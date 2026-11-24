#include "objects.h"
#include "led_controller.h"
#include <boost/filesystem.hpp>
#include <cfloat>

using std::string;
using std::map;
using std::set;
using std::list;
using std::vector;
using std::valarray;
using std::shared_ptr;
using boost::str;
using boost::format;
using namespace std::chrono_literals;
namespace fs = boost::filesystem;

Misc::Misc(string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "reboot"; elements[name] = rebootPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persistent*/);
  name = "factoryReset"; elements[name] = factoryResetPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persistent*/);
  name = "fwUpdateStatus"; elements[name] = fwUpdateStatusPtr = std::make_shared<EnumSensor>(path + "/" + name, this, 0, (int)Misc::FwUpdateStatesType::Max/*max*/, (int)Misc::FwUpdateStatesType::None/*default*/, Misc::fwUpdateStatesTypeMap, true/*publishAsControlFlag*/);
  name = "fwDownloadProgress"; elements[name] = fwDownloadProgressPtr = std::make_shared<FloatSensor>(path + "/" + name, this, 0.0f/*min*/, 100.0f/*max*/, 0.0f/*default*/, 1.0f/*multipleOf*/, "%"/*units*/, true/*publishAsControlFlag*/);
  name = "fwUpdateInProgress"; elements[name] = fwUpdateInProgressPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, true/*persistent*/);
  name = "ledTest"; elements[name] = ledTestPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persistent*/);
  name = "setupMode"; elements[name] = setupModePtr = std::make_shared<BoolControl>(path + "/" + name, this, true/*default*/, true/*persistent*/);
  class SubscriptionPeriodControl final : public FloatControl
  {
  public:
    SubscriptionPeriodControl(string path, LeafObject *parent, float min, float max, float value, float multipleOf, const char *units) :
      FloatControl(path, parent, min, max, value, multipleOf, units)
    { 
    }
    json processJson(string method, json j, int client = -1) override final
    { 
      auto result = FloatElement::processJson(method, j, client);
      if (method == "set")
      {
        this->getParent()->getRoot()->setSubscriptionPeriod(client, this->value);
      }
      else 
      {
        result = FloatControl::processJson(method, j, client);
      }
      return result;
    }
  };
  class passwordProtectedSensor final : public BoolSensor
  {
  public:
    passwordProtectedSensor(string path, LeafObject *parent, bool publishAsControlFlag = false) :
      BoolSensor(path, parent, false, publishAsControlFlag)
    { 
    }
    json processJson(const string method, json j, int client = -1) override final
    { 
      json result = json({});
      if (method == "get")
      {
        result = this->getParent()->getRoot()->isPasswordProtected();
      }
      else
      {
        result = BoolElement::processJson(method, j, client);
      }
      return result;
    }
  };
  class UserNameSensor final : public StringSensor
  {
  public:
    UserNameSensor(string path, LeafObject *parent, bool publishAsControlFlag = false) :
      StringSensor(path, parent, "", publishAsControlFlag)
    { 
    }
    json processJson(const string method, json j, int client = -1) override final
    { 
      json result = json({});
      if (method == "get")
      {
        result = this->getParent()->getRoot()->getUserName(client);
      }
      else
      {
        result = StringSensor::processJson(method, j, client);
      }
      return result;
    }
  };
  class VerifyPasswordControl final : public BoolControl
  {
  public:
    VerifyPasswordControl(string path, LeafObject *parent) :
      BoolControl(path, parent, false/*default*/, false/*persistent*/)
    { 
    }
    json processJson(const string method, json j, int client = -1) override final
    { 
      json result = json({});
      if (method == "set")
      {
        // update userName on success
        Misc *misc = dynamic_cast<Misc *>(this->getParent());
        std::dynamic_pointer_cast<UserNameSensor>(misc->elements["userName"])->setModified();
      }
      else
      {
        result = BoolControl::processJson(method, j, client);
      }
      return result;
    }
  };
  name = "subscriptionPeriod"; elements[name] = std::make_shared<SubscriptionPeriodControl>(path + "/" + name, this, 100.0f/*min*/, 10000.0f/*max*/, 100.0f/*default*/, 1.0f/*multipleOf*/, "milliseconds"/*units*/);

}

void Misc::initialize()
{
  // clear any LED state left behind by a previous process (e.g. killed mid-pulse by a FW update reboot)
  ledDriverReset.SetValue(1); // deassert reset (active-low)
  led_controller::reset();

  // fwUpdateInProgress is persisted; if it survived a reboot the update completed
  if (fwUpdateInProgressPtr->get())
  {
    fwUpdateStatusPtr->setEnumValue((int)FwUpdateStatesType::FinishedFlash);
    fwUpdateInProgressPtr->set(false);
    // write successful fw update message to log
    auto logs = std::dynamic_pointer_cast<Logs>(getRoot()->leafFromPath("/logs"));
    auto devInfo = std::dynamic_pointer_cast<DeviceInfo>(getRoot()->leafFromPath("/deviceInfo"));
    if (logs)
    {
      string newFwVer = devInfo ? devInfo->fwVerPtr->get() : "";
      logs->append("events", "Info", "Firmware Update to " + newFwVer + " was successful\n");
    }
  }
  LeafObject::initialize();
}

bool firCoeffsStringFormatIsValid(string s)
{
  if (s != "")
  {
    try
    {
      vector<string> v;
      boost::split(v, s, boost::is_any_of(", \t\n"), boost::token_compress_on);
      for (uint32_t i = 0; i < (uint32_t)v.size(); i++)
      {
        size_t chars_matched;
        float coeff = std::stof(v[i], &chars_matched);
        if (chars_matched != v[i].size())
          return false;
      }
    }
    catch (...)
    {
      return false;
    }
    
  }
  return true;
}

valarray<float> stringToFirCoeffs(string s, uint32_t len)
{
  // printf("stringToFirCoeffs('%s')\n", s.c_str());
  vector<string> v;
  boost::split(v, s, boost::is_any_of(", \t\n"), boost::token_compress_on);
  valarray<float> coeffs = valarray<float>(0.0f, len);
  for (uint32_t i = 0; i < std::min(len, (uint32_t)v.size()); i++)
  {
    coeffs[i] = std::stof(v[i]);
  }
  if (coeffs[0] == 0.0f)
    coeffs[0] = FLT_MIN;
  return coeffs;
}

string firCoeffsToString(valarray<float> coeffs)
{
  string s = "";
  for (uint32_t i = 0; i < coeffs.size(); i++)
  {
    s = s + str(format("%1.6e") % coeffs[i]);
    if (i != coeffs.size() - 1)
      s = s + ",";
  }
  return s;
}
void Misc::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
  {
    // poll the factory reset button; require it to be held down continuously
    // for more than 10 seconds before triggering (and to have started out
    // released, to avoid triggering immediately on boot if stuck pressed)
    static bool buttonInit = false;
    static bool resetInProgress = false;
    static time_t heldLongEnough = 0;
    int32_t pushButtonState = 1; // default to "not pressed" if GPIO read fails (e.g. host build)
    factoryResetButton.GetValue(&pushButtonState);
    auto rightNow = time(NULL);
    if (buttonInit &&
        !pushButtonState &&
        !resetInProgress &&
        ((rightNow - heldLongEnough) > 10))
    {
      printf("factory reset button pressed\n");
      resetInProgress = true;
      factoryResetPtr->set(true);
    }
    // when button is released, reset the timer, also require the button to start out not pressed
    else if (pushButtonState)
    {
      heldLongEnough = rightNow;
      buttonInit = true;
    }

    // firmware update state machine; timeouts are measured from entry into the current state
    static int lastState = -1;
    static time_t stateStartTime = 0;
    auto state = (FwUpdateStatesType)fwUpdateStatusPtr->getEnumValue();
    if ((int)state != lastState)
    {
      lastState = (int)state;
      stateStartTime = rightNow;
    }
    auto elapsed = rightNow - stateStartTime;
    switch (state)
    {
      case FwUpdateStatesType::Downloading:
      {
        // upload.cgi reports progress as the file arrives; if it stops (e.g. the script was killed) give up
        static float lastProgress = -1.0f;
        static time_t progressTime = 0;
        float progress = fwDownloadProgressPtr->get();
        if (progress != lastProgress || elapsed == 0)
        {
          lastProgress = progress;
          progressTime = rightNow;
        }
        else if (rightNow - progressTime > 60)
        {
          fwDownloadProgressPtr->set(0.0f);
          fwUpdateStatusPtr->setEnumValue((int)FwUpdateStatesType::DownloadingFail);
        }
        break;
      }
      case FwUpdateStatesType::Extraction:
        fwUpdateInProgressPtr->set(true);
        break;
      case FwUpdateStatesType::DownloadingFail:
      case FwUpdateStatesType::DecryptingFail:
      case FwUpdateStatesType::ExtractionFail: // received fw for other model
        if (elapsed > 15)
        {
          fwUpdateInProgressPtr->set(false);
          fwDownloadProgressPtr->set(0.0f);
          fwUpdateStatusPtr->setEnumValue((int)FwUpdateStatesType::None);
        }
        break;
      case FwUpdateStatesType::HwNotSupportedInFw:
        if (elapsed > 30)
        {
          auto logs = std::dynamic_pointer_cast<Logs>(getRoot()->leafFromPath("/logs"));
          if (logs)
            logs->append("events", "Warning", "Firmware Update Failure - Hardware Not Supported in Firmware. Try a newer firmware version.\n");
          fwUpdateInProgressPtr->set(false);
          fwDownloadProgressPtr->set(0.0f);
          fwUpdateStatusPtr->setEnumValue((int)FwUpdateStatesType::None);
        }
        break;
      case FwUpdateStatesType::FinishedFlash:
        if (elapsed > 60) // webUI and ATS require the FinishedFlash state
          fwUpdateStatusPtr->setEnumValue((int)FwUpdateStatesType::Finished);
        break;
      case FwUpdateStatesType::Finished:
        if (elapsed > 60) // reset fw update status after awhile
        {
          fwUpdateInProgressPtr->set(false);
          fwDownloadProgressPtr->set(0.0f);
          fwUpdateStatusPtr->setEnumValue((int)FwUpdateStatesType::None);
        }
        break;
      case FwUpdateStatesType::None:
        if (fwUpdateInProgressPtr->get())
          fwUpdateStatusPtr->setEnumValue((int)FwUpdateStatesType::Finished);
        break;
      default:
        break;
    }

    // LEDs: pulse purple while a firmware update is underway, blink red while it shows a failure
    enum class FwLeds { Off, Pulse, Blink };
    static FwLeds fwLeds = FwLeds::Off;
    bool fwUpdateFailed = state == FwUpdateStatesType::DownloadingFail ||
                          state == FwUpdateStatesType::DecryptingFail ||
                          state == FwUpdateStatesType::ExtractionFail ||
                          state == FwUpdateStatesType::HwNotSupportedInFw;
    bool fwUpdateActive = fwUpdateInProgressPtr->get() ||
                          state == FwUpdateStatesType::Downloading ||
                          // the upload (upload.cgi) only reports progress; the status stays None until it is decrypted
                          (state == FwUpdateStatesType::None && fwDownloadProgressPtr->get() > 0.0f) ||
                          state == FwUpdateStatesType::Decrypting ||
                          state == FwUpdateStatesType::Extraction ||
                          state == FwUpdateStatesType::ExtractionComplete;
    FwLeds wantedLeds = fwUpdateFailed ? FwLeds::Blink : fwUpdateActive ? FwLeds::Pulse : FwLeds::Off;
    if (wantedLeds != fwLeds)
    {
      fwLeds = wantedLeds;
      if (wantedLeds == FwLeds::Off)
      {
        led_controller::stopPulse();
        if (ledTestPtr->get())
          led_controller::setAllBrightness(128);
        else
          led_controller::shutdown(true);
      }
      else
      {
        ledDriverReset.SetValue(1); // deassert reset (active-low)
        if (led_controller::initialize())
        {
          if (wantedLeds == FwLeds::Blink)
            led_controller::startBlink(255/*r*/, 0/*g*/, 0/*b*/, 1000/*periodMs*/);
          else
            led_controller::startPulse(128/*r*/, 0/*g*/, 255/*b*/, 2000/*periodMs*/);
        }
      }
    }
  }
  else // non-sensors
  {
    if (rebootPtr->isModified())
    {
      if (rebootPtr->get()) // this test effectively bypasses the initial 'set' of the default value (false)
      {
        printf("Reboot Initiated\n"); fflush(stdout);
        reboot();
      }
    }
    if (factoryResetPtr->isModified())
    {
      if (factoryResetPtr->get()) // this test effectively bypasses the initial 'set' of the default value (false)
      {
        printf("Factory Reset Initiated\n"); fflush(stdout);
        factoryReset();
      }
    }
    if (ledTestPtr->isModified())
    {
      if (ledTestPtr->get())
      {
        printf("LED Test: bringing IS31FL3235A out of reset and lighting all channels\n"); fflush(stdout);
        ledDriverReset.SetValue(1); // deassert reset (active-low)
        if (led_controller::initialize())
          led_controller::setAllBrightness(128);
        else
          printf("LED Test: led_controller::initialize failed\n");
      }
      else
      {
        printf("LED Test: shutting down LED driver\n"); fflush(stdout);
        led_controller::shutdown(true);
      }
    }
  }
}

void Misc::reboot()
{
  fwUpdateStatusPtr->setEnumValue((int)FwUpdateStatesType::SoftwareReboot);
  printf("Reboot: rebooting (settings preserved)\n"); fflush(stdout);
  #ifdef __ARM_ARCH
  sync();
  system("reboot");
  #else
  printf("Reboot: skipping reboot on host build\n"); fflush(stdout);
  #endif
}

void Misc::factoryReset()
{
  fwUpdateStatusPtr->setEnumValue((int)FwUpdateStatesType::FactoryReset);
  printf("Factory Reset: clearing persisted settings and rebooting\n"); fflush(stdout);
  getRoot()->kvs->Nuke();
  #ifdef __ARM_ARCH
  sync();
  system("reboot");
  #else
  printf("Factory Reset: skipping reboot on host build\n"); fflush(stdout);
  #endif
}