#include "lvgl_ui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/steady_timer.hpp>

#include <lvgl.h>
#include <driver_backends.h>
#include <simulator_settings.h>

#include "ui/components/header.h"
#include "ui/components/info_list.h"
#include "ui/components/level_gauge.h"
#include "ui/components/picker_wheel.h"
#include "ui/components/scroll_pane.h"
#include "ui/nav.h"
#include "ui/components/tile.h"
#include "ui/ui_design_tokens.h"

#include <object.h>
#include <Bluetooth.h>
#include <Appearance.h>
#include <DeviceInfo.h>
#include <Misc.h>
#include <Themes.h>
#include <Screens.h>
#include <Tiles.h>

// Global simulator settings, defined in driver_backends.c. Must be populated
// (window size, at minimum) before driver_backends_init_backend() is called,
// otherwise the SDL backend creates a 0x0 display and lv_display_create()
// fails its hor_res/ver_res checks.
extern simulator_settings_t settings;

// Home icon in the header: snap back to the first screen of the scroll pane.
static void homeIconCb(lv_event_t *e)
{
  lv_obj_t *pane = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
  scroll_pane_set_screen_anim(pane, 0, false);
}

// Gear icon in the header: go to the settings destination (see buildSettings).
static nav_dest_t settingsDest;
static void settingsIconCb(lv_event_t *)
{
  nav_goto(settingsDest);
}

// Home header title: name of the screen currently shown in the scroll pane.
// Polled each tick; header_set_title() does nothing when the text is unchanged.
static void updateScreenTitle(lv_obj_t *header, lv_obj_t *pane)
{
  header_set_title(header, scroll_pane_get_screen_name(pane));
}

// Settings rows, in picker order.
enum SettingsRow
{
  SETTINGS_ROW_BRIGHTNESS,
  SETTINGS_ROW_BLUETOOTH,
  SETTINGS_ROW_STREAMING,
  SETTINGS_ROW_NETWORK,
  SETTINGS_ROW_APPEARANCE,
  SETTINGS_ROW_ADVANCED,
  SETTINGS_ROW_ABOUT,
  SETTINGS_ROW_REBOOT,
  SETTINGS_ROW_COUNT
};
static const char *const SETTINGS_ROW_NAMES[SETTINGS_ROW_COUNT] = {
  "Brightness", "Bluetooth", "Streaming", "Network", "Appearance", "Advanced", "About", "Reboot"};
static lv_obj_t *settingsPicker;

static std::string brightnessText()
{
  const TileNumber b = tile_read_number("/backlight", "brightness");
  return tile_format_number(b.value) + b.units;
}

static std::string bluetoothText()
{
  // connectedDevice is "Name (MAC)"; only the name is shown
  const std::string device = tile_read_string("/bluetooth", "connectedDevice");
  std::string name, mac;
  return Bluetooth::parseDevice(device, name, mac) ? name : device;
}

// Current value shown on the right of a Settings row.
static std::string settingsRowValue(int row)
{
  switch (row)
  {
  case SETTINGS_ROW_BRIGHTNESS: return brightnessText();
  case SETTINGS_ROW_BLUETOOTH:  return bluetoothText();
  }
  return std::string();
}

// Encoder rotation on the brightness page moves the level to the next multiple
// of BRIGHTNESS_STEP per tick (a value set elsewhere, e.g. 75, snaps to 70 / 80).
static const float BRIGHTNESS_STEP = 10.0f;
static void brightnessPageKeyCb(lv_event_t *e)
{
  const uint32_t key = lv_event_get_key(e);
  const int step = (key == LV_KEY_RIGHT) ? 1 : (key == LV_KEY_LEFT) ? -1 : 0;
  if (step == 0)
    return;
  const float value = tile_read_number("/backlight", "brightness").value;
  const float target = (step > 0 ? std::floor(value / BRIGHTNESS_STEP) + 1
                                 : std::ceil(value / BRIGHTNESS_STEP) - 1) * BRIGHTNESS_STEP;
  tile_step_number("/backlight", "brightness", (int)std::lround(target - value));   // clamped to 0..100
}

static void openBrightnessPage()
{
  nav_page_t page = nav_screen_create();

  const header_slot_t back  = {true,  UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
  const header_slot_t empty = {false, UI_ICON_SETTINGS,   nullptr,     nullptr};
  header_create(page.screen, back, "Brightness", empty);

  level_gauge_create(page.screen, "/backlight", "brightness");

  lv_obj_add_event_cb(page.focus, brightnessPageKeyCb, LV_EVENT_KEY, nullptr);
  nav_push(page);
}

// Network page: read-only rows from /network/ethernet. Anything the Network
// object doesn't have shows "-".
enum NetworkRow
{
  NETWORK_ROW_IP,
  NETWORK_ROW_ROUTER,
  NETWORK_ROW_DNS1,
  NETWORK_ROW_DNS2,
  NETWORK_ROW_SUBNET_MASK,
  NETWORK_ROW_MAC,
  NETWORK_ROW_DANTE_IP,
  NETWORK_ROW_DANTE_MAC,
  NETWORK_ROW_COUNT
};
static const char *const NETWORK_ROW_LABELS[NETWORK_ROW_COUNT] = {
  "IP Address", "Router", "DNS1", "DNS2", "Subnet Mask", "MAC Address", "Dante Address", "Dante MAC Address"};
static const char *const ETHERNET = "/network/ethernet";
static lv_obj_t *networkList;   // only while the Network page is open

// "192.168.1.50/24" -> "255.255.255.0"; "-" if there is no valid prefix length.
static std::string subnetMaskFromCidr(const std::string &cidr)
{
  const size_t slash = cidr.find('/');
  if (slash == std::string::npos)
    return "-";
  char *end = nullptr;
  const long bits = std::strtol(cidr.c_str() + slash + 1, &end, 10);
  if (end == cidr.c_str() + slash + 1 || *end != '\0' || bits < 0 || bits > 32)
    return "-";
  const uint32_t mask = bits == 0 ? 0u : 0xFFFFFFFFu << (32 - bits);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", mask >> 24, (mask >> 16) & 0xFF, (mask >> 8) & 0xFF, mask & 0xFF);
  return buf;
}

static std::string networkRowValue(int row)
{
  switch (row)
  {
  case NETWORK_ROW_IP:
  {
    // 'ip' is in CIDR notation; the prefix length is shown as the subnet mask instead.
    const std::string cidr = tile_read_string(ETHERNET, "ip");
    const std::string ip = cidr.substr(0, cidr.find('/'));
    if (ip == "-")
      return ip;
    return tile_read_bool(ETHERNET, "dhcp") ? ip + " (DHCP)" : ip;
  }
  case NETWORK_ROW_ROUTER:      return tile_read_string(ETHERNET, "gateway");
  case NETWORK_ROW_DNS1:        return tile_read_string(ETHERNET, "dns1");
  case NETWORK_ROW_DNS2:        return tile_read_string(ETHERNET, "dns2");
  case NETWORK_ROW_SUBNET_MASK: return subnetMaskFromCidr(tile_read_string(ETHERNET, "ip"));
  case NETWORK_ROW_MAC:         return tile_read_string(ETHERNET, "mac");
  default:                      return "-";   // Dante address / MAC: not in the Network object
  }
}

static void openNetworkPage()
{
  nav_page_t page = nav_screen_create();

  const header_slot_t back  = {true,  UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
  const header_slot_t empty = {false, UI_ICON_SETTINGS,   nullptr,     nullptr};
  header_create(page.screen, back, "Network", empty);

  std::string values[NETWORK_ROW_COUNT];   // info_list_create copies the text it shows
  info_row_t rows[NETWORK_ROW_COUNT];
  for (int row = 0; row < NETWORK_ROW_COUNT; row++)
  {
    values[row] = networkRowValue(row);
    rows[row] = {NETWORK_ROW_LABELS[row], values[row].c_str()};
  }
  networkList = info_list_create(page.screen, rows, NETWORK_ROW_COUNT);
  lv_obj_add_event_cb(networkList, [](lv_event_t *) { networkList = nullptr; }, LV_EVENT_DELETE, nullptr);

  nav_push(page);
}

// Keeps the Network page current (e.g. a DHCP lease arriving) while it is shown.
static void updateNetworkValues()
{
  if (!networkList || lv_screen_active() != lv_obj_get_screen(networkList))
    return;
  for (int row = 0; row < NETWORK_ROW_COUNT; row++)
    info_list_set_value(networkList, row, networkRowValue(row).c_str());
}

// Appearance page: a dropdown per setting. Picking an option writes the element
// (Appearance::themePtr etc.), which is what the rest of the system reacts to; the
// page follows the elements, so a change made elsewhere (e.g. the web UI) shows too.
enum AppearanceRow
{
  APPEARANCE_ROW_THEME,
  APPEARANCE_ROW_ORIENTATION,
  APPEARANCE_ROW_TEMPERATURE_UNITS,
  APPEARANCE_ROW_VOLUME_UNITS,
  APPEARANCE_ROW_LEVEL_GAUGE_STYLE,
  APPEARANCE_ROW_COUNT
};
static const char *const APPEARANCE_ROW_LABELS[APPEARANCE_ROW_COUNT] = {
  "Theme", "Orientation", "Temperature Units", "Volume Units", "Level Gauge Style"};

// One option of a dropdown: the value stored in the element, and the text shown.
struct Choice
{
  std::string value;
  std::string label;
  bool operator==(const Choice &o) const { return value == o.value && label == o.label; }
};

static std::shared_ptr<Appearance> appearanceLeaf;
static std::shared_ptr<Themes> themesLeaf;
static std::shared_ptr<Misc> miscLeaf;
static std::vector<Choice> appearanceChoices[APPEARANCE_ROW_COUNT];   // only while the page is open
static lv_obj_t *appearanceList;                                     // only while the page is open

static std::shared_ptr<StringControl> appearanceElement(int row)
{
  if (!appearanceLeaf)
    return nullptr;
  switch (row)
  {
  case APPEARANCE_ROW_THEME:             return appearanceLeaf->themePtr;
  case APPEARANCE_ROW_ORIENTATION:       return appearanceLeaf->orientationPtr;
  case APPEARANCE_ROW_TEMPERATURE_UNITS: return appearanceLeaf->temperatureUnitsPtr;
  case APPEARANCE_ROW_VOLUME_UNITS:      return appearanceLeaf->volumeUnitsPtr;
  case APPEARANCE_ROW_LEVEL_GAUGE_STYLE: return appearanceLeaf->levelGaugeStylePtr;
  }
  return nullptr;
}

static std::string jsonString(const json &object, const char *key)
{
  if (!object.is_object())
    return std::string();
  const auto it = object.find(key);
  return (it != object.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

// Every theme in the Themes config: presets first (the config is key-ordered, so dark
// then light), then custom themes. A theme shows its "name"; one without a name shows its
// key.
static std::vector<Choice> themeChoices()
{
  std::vector<Choice> choices;
  if (!themesLeaf || !themesLeaf->configPtr)
    return choices;
  const json config = themesLeaf->configPtr->get();
  if (!config.is_object())
    return choices;
  for (const bool presets : {true, false})
  {
    for (auto it = config.begin(); it != config.end(); ++it)
    {
      if ((jsonString(it.value(), "type") == "preset") != presets)
        continue;
      std::string name = jsonString(it.value(), "name");
      if (name.empty())
        name = it.key();
      choices.push_back({it.key(), name});
    }
  }
  return choices;
}

// Choices of the settings whose values are fixed. The values are what the web UI stores.
static std::vector<Choice> fixedChoices(int row)
{
  switch (row)
  {
  case APPEARANCE_ROW_ORIENTATION:       return {{"portrait", "Portrait"}, {"landscape", "Landscape"}};
  case APPEARANCE_ROW_TEMPERATURE_UNITS: return {{"f", "Fahrenheit"}, {"c", "Celsius"}};
  case APPEARANCE_ROW_VOLUME_UNITS:      return {{"db", "dB"}, {"percent", "Percent"}};
  case APPEARANCE_ROW_LEVEL_GAUGE_STYLE: return {{"anchored", "Anchored"}, {"floating", "Floating"}, {"ellipse", "Ellipse"}};
  }
  return {};
}

// The dropdown's option texts, '\n' separated.
static std::string choiceOptions(const std::vector<Choice> &choices)
{
  std::string options;
  for (const Choice &choice : choices)
  {
    std::string label = choice.label;
    std::replace(label.begin(), label.end(), '\n', ' ');
    options += (options.empty() ? "" : "\n") + label;
  }
  return options.empty() ? "-" : options;
}

// Text of the option whose value is the element's current one; "-" if there is none.
static std::string currentChoiceLabel(int row)
{
  const auto element = appearanceElement(row);
  if (!element)
    return "-";
  const std::string value = element->get();
  for (const Choice &choice : appearanceChoices[row])
    if (choice.value == value)
      return choice.label;
  return "-";
}

static void appearanceChoiceCb(int row, int option, void *)
{
  if (row < 0 || row >= APPEARANCE_ROW_COUNT || option < 0 || option >= (int)appearanceChoices[row].size())
    return;
  if (const auto element = appearanceElement(row))
    element->set(appearanceChoices[row][option].value);
}

static void openAppearancePage()
{
  nav_page_t page = nav_screen_create();

  const header_slot_t back  = {true,  UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
  const header_slot_t empty = {false, UI_ICON_SETTINGS,   nullptr,     nullptr};
  header_create(page.screen, back, "Appearance", empty);

  std::string options[APPEARANCE_ROW_COUNT];   // info_list_create copies the text it shows
  std::string values[APPEARANCE_ROW_COUNT];
  info_row_t rows[APPEARANCE_ROW_COUNT];
  for (int row = 0; row < APPEARANCE_ROW_COUNT; row++)
  {
    appearanceChoices[row] = row == APPEARANCE_ROW_THEME ? themeChoices() : fixedChoices(row);
    options[row] = choiceOptions(appearanceChoices[row]);
    values[row] = currentChoiceLabel(row);
    rows[row] = {APPEARANCE_ROW_LABELS[row], values[row].c_str(), options[row].c_str()};
  }
  appearanceList = info_list_create(page.screen, rows, APPEARANCE_ROW_COUNT);
  lv_obj_add_event_cb(appearanceList, [](lv_event_t *) { appearanceList = nullptr; }, LV_EVENT_DELETE, nullptr);
  info_list_set_change_cb(appearanceList, appearanceChoiceCb, nullptr);
  info_list_attach_encoder(appearanceList, page.focus);

  nav_push(page);
}

// Keeps the Appearance page current while it is shown: themes added or removed, and
// values changed from elsewhere.
static void updateAppearanceValues()
{
  if (!appearanceList || lv_screen_active() != lv_obj_get_screen(appearanceList))
    return;
  std::vector<Choice> themes = themeChoices();
  if (!(themes == appearanceChoices[APPEARANCE_ROW_THEME]))
  {
    appearanceChoices[APPEARANCE_ROW_THEME] = std::move(themes);
    info_list_set_options(appearanceList, APPEARANCE_ROW_THEME, choiceOptions(appearanceChoices[APPEARANCE_ROW_THEME]).c_str());
  }
  for (int row = 0; row < APPEARANCE_ROW_COUNT; row++)
    info_list_set_value(appearanceList, row, currentChoiceLabel(row).c_str());
}

// About page: read-only rows from the DeviceInfo leaf. Anything missing shows "-".
enum AboutRow
{
  ABOUT_ROW_NAME,
  ABOUT_ROW_SERIAL_NUMBER,
  ABOUT_ROW_FIRMWARE_VERSION,
  ABOUT_ROW_COUNT
};
static const char *const ABOUT_ROW_LABELS[ABOUT_ROW_COUNT] = {"Name", "Serial Number", "Firmware Version"};

static std::shared_ptr<DeviceInfo> deviceInfoLeaf;
static lv_obj_t *aboutList;   // only while the About page is open

static std::string aboutRowValue(int row)
{
  if (!deviceInfoLeaf)
    return "-";
  std::string value;
  switch (row)
  {
  case ABOUT_ROW_NAME:             value = deviceInfoLeaf->deviceNamePtr->get(); break;
  case ABOUT_ROW_SERIAL_NUMBER:    value = deviceInfoLeaf->serialNumPtr->get();  break;
  case ABOUT_ROW_FIRMWARE_VERSION: value = deviceInfoLeaf->arcFwVerPtr->get();   break;
  }
  return value.empty() ? "-" : value;
}

static void openAboutPage()
{
  nav_page_t page = nav_screen_create();

  const header_slot_t back  = {true,  UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
  const header_slot_t empty = {false, UI_ICON_SETTINGS,   nullptr,     nullptr};
  header_create(page.screen, back, "About", empty);

  std::string values[ABOUT_ROW_COUNT];   // info_list_create copies the text it shows
  info_row_t rows[ABOUT_ROW_COUNT];
  for (int row = 0; row < ABOUT_ROW_COUNT; row++)
  {
    values[row] = aboutRowValue(row);
    rows[row] = {ABOUT_ROW_LABELS[row], values[row].c_str()};
  }
  aboutList = info_list_create(page.screen, rows, ABOUT_ROW_COUNT);
  lv_obj_add_event_cb(aboutList, [](lv_event_t *) { aboutList = nullptr; }, LV_EVENT_DELETE, nullptr);

  nav_push(page);
}

// Keeps the About page current (e.g. a device rename from the web UI) while it is shown.
static void updateAboutValues()
{
  if (!aboutList || lv_screen_active() != lv_obj_get_screen(aboutList))
    return;
  for (int row = 0; row < ABOUT_ROW_COUNT; row++)
    info_list_set_value(aboutList, row, aboutRowValue(row).c_str());
}

// A tap on a row, or an encoder press released on it, opens that row's page.
static void settingsRowActivatedCb(int index, void *)
{
  switch (index)
  {
  case SETTINGS_ROW_BRIGHTNESS: openBrightnessPage(); break;
  case SETTINGS_ROW_BLUETOOTH:  tile_bluetooth_devices_open(SETTINGS_ROW_NAMES[SETTINGS_ROW_BLUETOOTH]); break;
  case SETTINGS_ROW_NETWORK:    openNetworkPage();    break;
  case SETTINGS_ROW_APPEARANCE: openAppearancePage(); break;
  case SETTINGS_ROW_ABOUT:      openAboutPage();      break;
  case SETTINGS_ROW_REBOOT:
    if (miscLeaf && miscLeaf->rebootPtr)
      miscLeaf->rebootPtr->set(true);
    break;
  }
}

// True while an encoder press that began on the Reboot row is still held there: the row is
// highlighted in its alternate color, and releasing the press reboots. Turning the encoder
// moves the selection off the row and cancels it.
static bool rebootArmed;

// Encoder on the Settings destination: each tick snaps the picker to the next or previous
// row (no animation), and the release of a press (LV_EVENT_CLICKED) activates the selected
// row. Pressing on the Reboot row highlights it; the release only activates it if the
// highlight is still showing.
static void settingsEncoderCb(lv_event_t *e)
{
  lv_obj_t *picker = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
  switch (lv_event_get_code(e))
  {
  case LV_EVENT_PRESSED:
    rebootArmed = picker_wheel_get_selected(picker) == SETTINGS_ROW_REBOOT;
    picker_wheel_set_press_active(picker, rebootArmed);
    break;
  case LV_EVENT_RELEASED:
  case LV_EVENT_PRESS_LOST:
    picker_wheel_set_press_active(picker, false);   // rebootArmed is consumed by CLICKED below
    break;
  case LV_EVENT_CLICKED:
  {
    const bool armed = rebootArmed;
    rebootArmed = false;
    if (picker_wheel_get_selected(picker) == SETTINGS_ROW_REBOOT && !armed)
      break;   // the highlight was lost before the release
    picker_wheel_click(picker);
    break;
  }
  case LV_EVENT_KEY:
  {
    rebootArmed = false;
    picker_wheel_set_press_active(picker, false);
    const uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_RIGHT)     picker_wheel_nudge(picker, 1, false);
    else if (key == LV_KEY_LEFT) picker_wheel_nudge(picker, -1, false);
    break;
  }
  default:
    break;
  }
}

// Row values shown on the Settings picker. Polled by value while Settings is
// the active screen; a row's text is only rewritten when it changes.
static std::string settingsShown[SETTINGS_ROW_COUNT];   // what the picker currently shows
static void updateSettingsValues()
{
  if (!settingsPicker || lv_screen_active() != settingsDest.screen)
    return;
  for (int row = 0; row < SETTINGS_ROW_COUNT; row++)
  {
    const std::string text = settingsRowValue(row);
    if (text == settingsShown[row])
      continue;
    settingsShown[row] = text;
    picker_wheel_set_secondary_text(settingsPicker, row, text.c_str());
  }
}

// The home screen (the display's first screen) and what is built on it. The pane and
// header are rebuilt, and the settings destination replaced, when the orientation
// changes; the screen itself lives for the whole run.
static lv_obj_t *homeScreen;
static lv_obj_t *homePane;
static lv_obj_t *homeHeader;

// Orientation setting -> display rotation. The panel is natively portrait; landscape is
// the same panel rotated 270 degrees in software, which only the framebuffer backend does
// (the SDL simulator stays portrait). Layout sizes follow the display, see ui_is_landscape().
#if LV_USE_LINUX_FBDEV
static lv_display_rotation_t wantedRotation()
{
  const bool landscape = appearanceLeaf && appearanceLeaf->orientationPtr
                         && appearanceLeaf->orientationPtr->get() == "landscape";
  return landscape ? LV_DISPLAY_ROTATION_270 : LV_DISPLAY_ROTATION_0;
}
#endif

// True if the Orientation setting no longer matches the display's rotation.
static bool orientationNeedsChange()
{
#if LV_USE_LINUX_FBDEV
  lv_display_t *display = lv_display_get_default();
  return display && lv_display_get_rotation(display) != wantedRotation();
#else
  return false;
#endif
}

static void applyOrientation()
{
#if LV_USE_LINUX_FBDEV
  lv_display_t *display = lv_display_get_default();
  if (!display || !orientationNeedsChange())
    return;
  lv_display_set_rotation(display, wantedRotation());
  if (homeScreen)
    lv_obj_set_size(homeScreen, lv_display_get_horizontal_resolution(display), lv_display_get_vertical_resolution(display));
#endif
}

// Home: the scroll pane below the header (the pane reads its screens and tiles from the
// Screens / Tiles leaves, and is refreshed by scheduleLvglTick() when their config
// changes), and the header: home icon (first screen), current screen name in the centre
// (kept current by scheduleLvglTick()), gear icon (settings destination).
static void buildHome()
{
  homePane = scroll_pane_create(homeScreen);
  lv_obj_align(homePane, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H_PX);
  nav_init(homePane);

  const header_slot_t homeSlot     = {true, UI_ICON_HOME,     homeIconCb,     homePane};
  const header_slot_t settingsSlot = {true, UI_ICON_SETTINGS, settingsIconCb, nullptr};
  homeHeader = header_create(homeScreen, homeSlot, scroll_pane_get_screen_name(homePane), settingsSlot,
                            &satoshi_medium_24);
}

// Settings destination: reached by the gear icon; its back button returns home. Below its
// header is a picker_wheel with one row per setting; each row opens its own page.
static void buildSettings()
{
  settingsDest = nav_screen_create();
  const header_slot_t backSlot  = {true,  UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
  const header_slot_t emptySlot = {false, UI_ICON_SETTINGS,   nullptr,     nullptr};
  header_create(settingsDest.screen, backSlot, "Settings", emptySlot);

  picker_wheel_item_t items[SETTINGS_ROW_COUNT] = {};
  for (int row = 0; row < SETTINGS_ROW_COUNT; row++)
  {
    settingsShown[row] = settingsRowValue(row);   // the picker copies the strings it shows
    items[row].primary_text         = SETTINGS_ROW_NAMES[row];
    items[row].primary_text_align   = LV_TEXT_ALIGN_LEFT;
    // Rows without a live value get no secondary label.
    items[row].secondary_text       = settingsShown[row].empty() ? nullptr : settingsShown[row].c_str();
    items[row].secondary_text_align = LV_TEXT_ALIGN_RIGHT;
    // Reboot is an action, not a subpage.
    items[row].accessory            = row == SETTINGS_ROW_REBOOT ? PICKER_WHEEL_ITEM_ACCESSORY_NONE
                                                                 : PICKER_WHEEL_ITEM_ACCESSORY_ARROW;
  }

  picker_wheel_props_t props = {};
  props.type               = PICKER_WHEEL_TYPE_CENTER_ARROW;   // accessory on the right
  props.items              = items;
  props.item_count         = SETTINGS_ROW_COUNT;
  props.selected_index     = 0;
  props.activated_callback = settingsRowActivatedCb;
  props.pressed_alt_enabled = true;                    // Reboot highlights in red while pressed
  props.pressed_alt_index   = SETTINGS_ROW_REBOOT;
  props.pressed_alt_color   = lv_color_hex(0xE5484D);
  settingsPicker = picker_wheel_create(settingsDest.screen, &props);
  lv_obj_add_event_cb(settingsPicker, [](lv_event_t *) { settingsPicker = nullptr; }, LV_EVENT_DELETE, nullptr);
  lv_obj_align(settingsPicker, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H_PX);

  lv_obj_add_event_cb(settingsDest.focus, settingsEncoderCb, LV_EVENT_KEY,     settingsPicker);
  lv_obj_add_event_cb(settingsDest.focus, settingsEncoderCb, LV_EVENT_CLICKED, settingsPicker);
  lv_obj_add_event_cb(settingsDest.focus, settingsEncoderCb, LV_EVENT_PRESSED, settingsPicker);
  lv_obj_add_event_cb(settingsDest.focus, settingsEncoderCb, LV_EVENT_RELEASED, settingsPicker);
  lv_obj_add_event_cb(settingsDest.focus, settingsEncoderCb, LV_EVENT_PRESS_LOST, settingsPicker);
}

// Applies a changed orientation: closes any open page, tears down everything whose
// layout depends on the orientation, rotates the display, and builds it again.
static void rebuildForOrientation()
{
  nav_goto({homeScreen, homePane}, false);

  if (settingsDest.screen)
  {
    lv_obj_delete(settingsDest.screen);
    settingsDest = {nullptr, nullptr};
  }
  scroll_pane_destroy(homePane);
  lv_obj_delete(homeHeader);
  homePane = nullptr;
  homeHeader = nullptr;

  applyOrientation();
  buildHome();
  buildSettings();
}

// Runs one LVGL timer/task tick, then reschedules itself on the timer's io_context after
// the delay LVGL itself recommends (lv_timer_handler()'s return value),
// taking the place of the old dedicated thread's `lv_timer_handler(); usleep();`
// loop. The timer is kept alive by the shared_ptr captured in its own
// completion handler, so no separate owner is needed.
//
// Also owns live config-change detection: LeafObject::isModified({elements...})
// checks (and clears) just the given elements' dirty flags - same global
// Element::modified mechanism NSTProxy.cpp uses to detect DSP-bound control
// changes, scoped here to themePtr/configPtr (theme), the Screens/Tiles
// config elements (pane) and orientationPtr (display rotation + relayout) only,
// so unrelated Appearance settings (units, etc.) don't trigger a repaint. Doesn't
// interfere with the websocket subscription system, which tracks modification
// per-subscriber in a separate map (Element::modifiedState).
static void scheduleLvglTick(std::shared_ptr<boost::asio::steady_timer> timer,
                             std::shared_ptr<Appearance> appearance, std::shared_ptr<Themes> themes,
                             std::shared_ptr<Screens> screens, std::shared_ptr<Tiles> tiles)
{
  if (appearance && appearance->isModified({appearance->orientationPtr}) && orientationNeedsChange())
  {
    rebuildForOrientation();
  }

  // isModified() clears the flag it reads, so every check must run (no short-circuit between them).
  bool themeChanged = appearance && appearance->isModified({appearance->themePtr});
  themeChanged |= themes && themes->isModified({themes->configPtr});
  if (themeChanged)
  {
    ui_design_tokens_refresh();
  }

  bool screensChanged = screens && screens->isModified({screens->configPtr, screens->screenOrderPtr});
  screensChanged |= tiles && tiles->isModified({tiles->configPtr});
  if (screensChanged && homePane)
  {
    nav_close_pages(false);   // an open page may belong to a tile that no longer exists
    scroll_pane_refresh(homePane);
  }
  if (homePane && homeHeader)
  {
    updateScreenTitle(homeHeader, homePane);
  }
  tile_live_update();
  updateSettingsValues();
  updateNetworkValues();
  updateAppearanceValues();
  updateAboutValues();

  uint32_t ms = lv_timer_handler();
  if (ms == LV_NO_TIMER_READY)
  {
    ms = LV_DEF_REFR_PERIOD;
  }
  timer->expires_after(std::chrono::milliseconds(ms));
  timer->async_wait([timer, appearance, themes, screens, tiles](const boost::system::error_code &ec)
  {
    if (ec) // timer cancelled, or io_context shutting down
    {
      return;
    }
    scheduleLvglTick(timer, appearance, themes, screens, tiles);
  });
}

void lvglUiInit(boost::asio::io_context &io_context, std::shared_ptr<RootObject> root)
{
  lv_init();

  // Match the Trevally target's physical panel so the simulator is
  // representative of the real device.
  settings.window_width = UI_PANEL_W_PX;
  settings.window_height = UI_PANEL_H_PX;

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
  appearanceLeaf = appearance;
  themesLeaf = themes;
  miscLeaf = std::dynamic_pointer_cast<Misc>(root->leafFromPath("/misc"));
  deviceInfoLeaf = std::dynamic_pointer_cast<DeviceInfo>(root->leafFromPath("/deviceInfo"));
  ui_design_tokens_init(appearance, themes);

  // The screen itself is a plain lv_obj and never picks up a theme's
  // background on its own - without this, the dark palette's near-white
  // icon/text colors render invisibly against LVGL's default white bg.
  homeScreen = lv_screen_active();
  lv_obj_add_style(homeScreen, ui_theme_get_style_bg(), 0);

  // Rotate first: the layout sizes are read from the display when the UI is built.
  applyOrientation();
  buildHome();
  buildSettings();

  auto timer = std::make_shared<boost::asio::steady_timer>(io_context);
  scheduleLvglTick(timer, appearance, themes, screens, tiles);
}
