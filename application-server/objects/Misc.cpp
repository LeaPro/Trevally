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
  name = "factoryReset"; elements[name] = factoryResetPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persistent*/);
  name = "ledTest"; elements[name] = ledTestPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persistent*/);
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
  }
  else // non-sensors
  {
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

void Misc::factoryReset()
{
  printf("Factory Reset: clearing persisted settings and rebooting\n"); fflush(stdout);
  getRoot()->kvs->Nuke();
  #ifdef __ARM_ARCH
  sync();
  system("reboot");
  #else
  printf("Factory Reset: skipping reboot on host build\n"); fflush(stdout);
  #endif
}