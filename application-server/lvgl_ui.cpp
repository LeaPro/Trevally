#include "lvgl_ui.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <boost/asio/steady_timer.hpp>

#include <lvgl.h>
#include <driver_backends.h>
#include <simulator_settings.h>

#include "ui/components/scroll_pane.h"
#include "ui/nav.h"
#include "ui/components/tile.h"
#include "ui/ui_design_tokens.h"

#include <object.h>
#include <Appearance.h>
#include <Themes.h>
#include <Screens.h>
#include <Tiles.h>
#include <Network.h>

// Global simulator settings, defined in driver_backends.c. Must be populated
// (window size, at minimum) before driver_backends_init_backend() is called,
// otherwise the SDL backend creates a 0x0 display and lv_display_create()
// fails its hor_res/ver_res checks.
extern simulator_settings_t settings;

// Encoder rotation arrives at the scroll_pane (the focused object, in edit
// mode) as LV_KEY_LEFT / LV_KEY_RIGHT. This extra listener on the pane moves the
// decorative encoder arc's fill by one step per tick, clamped to its range.
static const int ENCODER_ARC_STEP = 4;
static void encoderArcKeyCb(lv_event_t *e)
{
  lv_obj_t *arc = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
  uint32_t key = lv_event_get_key(e);
  int step = (key == LV_KEY_RIGHT) ? ENCODER_ARC_STEP : (key == LV_KEY_LEFT) ? -ENCODER_ARC_STEP : 0;
  if (step == 0)
    return;
  int value = lv_arc_get_value(arc) + step;
  int lo = lv_arc_get_min_value(arc), hi = lv_arc_get_max_value(arc);
  lv_arc_set_value(arc, value < lo ? lo : value > hi ? hi : value);
}

// Shows the Ethernet IP (without the "/prefix" length) in the label left of
// the gear icon, or "-" when there is none. Polled each tick rather than using
// isModified(), because Ethernet::update() clears the ip modified flag itself.
static void updateIpLabel(std::shared_ptr<Ethernet> ethernet, lv_obj_t *ipLabel, lv_obj_t *gear)
{
  std::string ip = ethernet->ipPtr->get();
  ip = ip.substr(0, ip.find('/'));
  if (ip.empty())
  {
    ip = "-";
  }
  if (strcmp(lv_label_get_text(ipLabel), ip.c_str()) != 0)
  {
    lv_label_set_text(ipLabel, ip.c_str());
    lv_obj_update_layout(ipLabel);
    lv_obj_align_to(ipLabel, gear, LV_ALIGN_OUT_LEFT_MID, -12, 0);
  }
}

// Runs one LVGL timer/task tick, then reschedules itself on io_context after
// the delay LVGL itself recommends (lv_timer_handler()'s return value),
// taking the place of the old dedicated thread's `lv_timer_handler(); usleep();`
// loop. The timer is kept alive by the shared_ptr captured in its own
// completion handler, so no separate owner is needed.
//
// Also owns live config-change detection: LeafObject::isModified({elements...})
// checks (and clears) just the given elements' dirty flags - same global
// Element::modified mechanism NSTProxy.cpp uses to detect DSP-bound control
// changes, scoped here to themePtr/configPtr (theme) and the Screens/Tiles
// config elements (pane) only, so unrelated Appearance settings (orientation,
// units, etc.) don't trigger a repaint. Doesn't interfere with the websocket
// subscription system, which tracks modification per-subscriber in a separate
// map (Element::modifiedState).
static void scheduleLvglTick(boost::asio::io_context &io_context, std::shared_ptr<boost::asio::steady_timer> timer,
                             std::shared_ptr<Appearance> appearance, std::shared_ptr<Themes> themes,
                             std::shared_ptr<Screens> screens, std::shared_ptr<Tiles> tiles, lv_obj_t *pane,
                             std::shared_ptr<Ethernet> ethernet, lv_obj_t *ipLabel, lv_obj_t *gear)
{
  if (ethernet && ipLabel)
  {
    updateIpLabel(ethernet, ipLabel, gear);
  }

  bool themeChanged = false;
  if (appearance && appearance->isModified({appearance->themePtr}))
  {
    themeChanged = true;
  }
  if (themes && themes->isModified({themes->configPtr}))
  {
    themeChanged = true;
  }
  if (themeChanged)
  {
    ui_design_tokens_refresh();
  }

  bool screensChanged = false;
  if (screens && screens->isModified({screens->configPtr, screens->screenOrderPtr}))
  {
    screensChanged = true;
  }
  if (tiles && tiles->isModified({tiles->configPtr}))
  {
    screensChanged = true;
  }
  if (screensChanged && pane)
  {
    nav_home(false);   // an open page may belong to a tile that no longer exists
    scroll_pane_refresh(pane);
  }
  tile_live_update();

  uint32_t ms = lv_timer_handler();
  if (ms == LV_NO_TIMER_READY)
  {
    ms = LV_DEF_REFR_PERIOD;
  }
  timer->expires_after(std::chrono::milliseconds(ms));
  timer->async_wait([&io_context, timer, appearance, themes, screens, tiles, pane, ethernet, ipLabel, gear](const boost::system::error_code &ec)
  {
    if (ec) // timer cancelled, or io_context shutting down
    {
      return;
    }
    scheduleLvglTick(io_context, timer, appearance, themes, screens, tiles, pane, ethernet, ipLabel, gear);
  });
}

void lvglUiInit(boost::asio::io_context &io_context, std::shared_ptr<RootObject> root)
{
  lv_init();

  // Match the Trevally target's physical panel (Densitron DMT049W1NTCMI-1A,
  // 480x854 portrait) so the simulator is representative of the real device.
  settings.window_width = 480;
  settings.window_height = 854;

  // Must be created and set as the default group *before* the display/input
  // backends are initialized below: that's when display_backends/sdl.c's
  // mousewheel indev (host) and hid_encoder.c's indev (target) each grab
  // lv_group_get_default() once, at init time, and latch onto whatever it
  // returns. Without this, both encoders would be wired to a NULL group and
  // silently do nothing.
  lv_group_t *encoderGroup = lv_group_create();
  lv_group_set_default(encoderGroup);

  driver_backends_register();
  if (driver_backends_init_backend(nullptr) == -1)
  {
    fprintf(stderr, "lvgl: failed to initialize display backend\n");
    return;
  }

#if LV_USE_EVDEV
  // LVGL's evdev auto-discovery only classifies devices exposing legacy
  // ABS_X/ABS_Y as pointers; the Trevally sitronix touchscreen only reports
  // the modern ABS_MT_* multi-touch protocol and would otherwise be
  // misclassified as a keypad and ignored. Force it into pointer mode via
  // its stable by-path symlink (tied to the i2c bus address, unlike
  // /dev/input/eventN which depends on driver probe order). Set the
  // default here, in the binary itself, rather than only in
  // application-server.service's Environment= line, so it takes effect no
  // matter how the process is launched - systemd, or a manual
  // `./application-server` after `systemctl stop` - without depending on
  // the caller's shell/session environment. overwrite=0 still lets an
  // explicit environment variable take precedence over this default.
  setenv("LV_LINUX_EVDEV_POINTER_DEVICE",
         "/dev/input/by-path/platform-20020000.i2c-event", 0);

  if (driver_backends_init_backend((char *)"EVDEV") == -1)
  {
    fprintf(stderr, "lvgl: failed to initialize evdev input backend\n");
  }
#endif

  // Target only: the host build gets its encoder for free from LVGL's own
  // SDL backend (display_backends/sdl.c's lv_sdl_mousewheel_create(), wired
  // to the default group already), simulated via scroll wheel + middle
  // click. This registers the physical HID dial as the same kind of
  // LV_INDEV_TYPE_ENCODER device, using LVGL's native group/focus
  // semantics: rotate moves focus, press selects.
#if LV_USE_HID_ENCODER
  if (driver_backends_init_backend((char *)"HID_ENCODER") == -1)
  {
    fprintf(stderr, "lvgl: failed to initialize HID encoder input backend\n");
  }
#endif

  auto appearance = std::dynamic_pointer_cast<Appearance>(root->leafFromPath("/appearance"));
  auto themes = std::dynamic_pointer_cast<Themes>(root->leafFromPath("/themes"));
  auto screens = std::dynamic_pointer_cast<Screens>(root->leafFromPath("/screens"));
  auto tiles = std::dynamic_pointer_cast<Tiles>(root->leafFromPath("/tiles"));
  auto ethernet = std::dynamic_pointer_cast<Ethernet>(root->leafFromPath("/network/ethernet"));
  ui_design_tokens_init(appearance, themes);

  // The screen itself is a plain lv_obj and never picks up a theme's
  // background on its own - without this, the dark palette's near-white
  // icon/text colors render invisibly against LVGL's default white bg.
  lv_obj_add_style(lv_screen_active(), ui_theme_get_style_bg(), 0);

  // Standalone themed icon in the top-right corner, with the scroll_pane
  // directly below it.
  lv_obj_t *testIcon = UI_ICON_CREATE_DEFAULT(lv_screen_active(), UI_ICON_SETTINGS);
  lv_obj_align(testIcon, LV_ALIGN_TOP_RIGHT, -20, 8);
  lv_obj_update_layout(testIcon);

  // Ethernet IP to the left of the gear, kept current by scheduleLvglTick().
  lv_obj_t *ipLabel = lv_label_create(lv_screen_active());
  lv_obj_add_style(ipLabel, ui_theme_get_style_text_secondary(), 0);
  lv_obj_set_style_text_font(ipLabel, &satoshi_medium_20, 0);
  lv_label_set_text(ipLabel, "");
  // The scroll_pane reads its screens and tiles straight from the Screens /
  // Tiles leaves, and is refreshed by scheduleLvglTick() when their config
  // changes.
  lv_obj_t *pane = scroll_pane_create(lv_screen_active());
  lv_obj_align(pane, LV_ALIGN_TOP_MID, 0, 8 + lv_obj_get_height(testIcon) + 4);
  nav_init(pane);

  // // Just for fun: a decorative gauge-style arc ringing the physical encoder,
  // // its fill following encoder turns (via the pane's key events, see
  // // encoderArcKeyCb). The encoder's centre sits at (240, 676) on the 480x854
  // // panel (from the "Knob" in the Figma Settings Screen: 282px wide, centred
  // // horizontally, top at y=535). The arc's outer radius is bigger than the
  // // knob's (141) so the encoder doesn't cover it. Created last so it draws
  // // above the pane.
  // {
  //   const int encoderCx = 240, encoderCy = 676;
  //   const int arcOuterRadius = 187, arcWidth = 6;
  //   lv_obj_t *arc = lv_arc_create(lv_screen_active());
  //   lv_obj_remove_style_all(arc);
  //   // lv_arc_create() registers with the default (encoder) group; keep it out
  //   // so encoder rotation doesn't land on a decorative widget.
  //   lv_group_remove_obj(arc);
  //   lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  //   lv_obj_set_size(arc, 2 * arcOuterRadius, 2 * arcOuterRadius);
  //   lv_obj_set_pos(arc, encoderCx - arcOuterRadius, encoderCy - arcOuterRadius);
  //   lv_arc_set_rotation(arc, 135);
  //   lv_arc_set_bg_angles(arc, 0, 270);
  //   lv_arc_set_range(arc, 0, 100);
  //   lv_arc_set_value(arc, 0);
  //   lv_obj_set_style_arc_width(arc, arcWidth, LV_PART_MAIN);
  //   lv_obj_set_style_arc_color(arc, ui_get_bg_tertiary_color(), LV_PART_MAIN);
  //   lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  //   lv_obj_set_style_arc_width(arc, arcWidth, LV_PART_INDICATOR);
  //   lv_obj_set_style_arc_color(arc, ui_get_brand_color(), LV_PART_INDICATOR);
  //   lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  //   lv_obj_add_event_cb(pane, encoderArcKeyCb, LV_EVENT_KEY, arc);
  // }

  auto timer = std::make_shared<boost::asio::steady_timer>(io_context);
  scheduleLvglTick(io_context, timer, appearance, themes, screens, tiles, pane, ethernet, ipLabel, testIcon);
}
