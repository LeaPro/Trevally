#include "lvgl_ui.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <boost/asio/steady_timer.hpp>

#include <lvgl.h>
#include <driver_backends.h>
#include <simulator_settings.h>

// Global simulator settings, defined in driver_backends.c. Must be populated
// (window size, at minimum) before driver_backends_init_backend() is called,
// otherwise the SDL backend creates a 0x0 display and lv_display_create()
// fails its hor_res/ver_res checks.
extern simulator_settings_t settings;

// Debug/test widget: an arc whose value tracks the encoder's rotation and
// whose indicator color toggles while the encoder button is held down. This
// exists purely to visually confirm the encoder indev (HID_ENCODER on
// target, SDL mousewheel-as-encoder on host) is wired up correctly.
static void encoderTestArcEventCb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *arc = static_cast<lv_obj_t *>(lv_event_get_target(e));

  switch (code)
  {
  case LV_EVENT_VALUE_CHANGED:
    printf("encoder: arc value = %d\n", (int)lv_arc_get_value(arc));
    break;
  case LV_EVENT_PRESSED:
    printf("encoder: pressed\n");
    lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    break;
  case LV_EVENT_RELEASED:
    printf("encoder: released\n");
    lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    break;
  default:
    break;
  }
}

// Runs one LVGL timer/task tick, then reschedules itself on io_context after
// the delay LVGL itself recommends (lv_timer_handler()'s return value),
// taking the place of the old dedicated thread's `lv_timer_handler(); usleep();`
// loop. The timer is kept alive by the shared_ptr captured in its own
// completion handler, so no separate owner is needed.
static void scheduleLvglTick(boost::asio::io_context &io_context, std::shared_ptr<boost::asio::steady_timer> timer)
{
  uint32_t ms = lv_timer_handler();
  if (ms == LV_NO_TIMER_READY)
  {
    ms = LV_DEF_REFR_PERIOD;
  }
  timer->expires_after(std::chrono::milliseconds(ms));
  timer->async_wait([&io_context, timer](const boost::system::error_code &ec)
  {
    if (ec) // timer cancelled, or io_context shutting down
    {
      return;
    }
    scheduleLvglTick(io_context, timer);
  });
}

void lvglUiInit(boost::asio::io_context &io_context)
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

  lv_obj_t *label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Hello maria");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

  // Encoder test widget: lv_arc doesn't auto-join the default group the way
  // some widgets do (it doesn't set group_def = TRUE), so it has to be added
  // explicitly. It's also the only member of the group, so LVGL's "long
  // press toggles edit mode" rule never applies (it refuses to leave edit
  // mode when there's nowhere else to navigate) - we preempt that by
  // starting the group in edit mode so rotation/press/release work from the
  // very first interaction instead of the first press just entering edit
  // mode with no visible effect.
  lv_obj_t *arc = lv_arc_create(lv_screen_active());
  lv_obj_set_size(arc, 200, 200);
  lv_obj_center(arc);
  lv_arc_set_range(arc, 0, 100);
  lv_arc_set_value(arc, 0);
  lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
  lv_obj_add_event_cb(arc, encoderTestArcEventCb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(arc, encoderTestArcEventCb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(arc, encoderTestArcEventCb, LV_EVENT_RELEASED, nullptr);
  lv_group_add_obj(encoderGroup, arc);
  lv_group_set_editing(encoderGroup, true);

  auto timer = std::make_shared<boost::asio::steady_timer>(io_context);
  scheduleLvglTick(io_context, timer);
}
