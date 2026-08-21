#include "objects.h"
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
    static float minSecBetweenUpdates = 1e6f; // minimum time between updates in seconds
    static float maxSecBetweenUpdates = 0.0f; // maximum time between updates in seconds
    static Timer updateTimer;
    static bool firstTime = true;
    if (firstTime)
    {
      firstTime = false;
    }
    else
    {
      float secSinceLastUpdate = updateTimer.elapsedSec();
      if (secSinceLastUpdate < minSecBetweenUpdates)
      {
        minSecBetweenUpdates = secSinceLastUpdate;
        printf("New minimum time between updates: %f seconds\n", minSecBetweenUpdates);
      }
      if (secSinceLastUpdate > maxSecBetweenUpdates)
      {
        maxSecBetweenUpdates = secSinceLastUpdate;
        printf("New maximum time between updates: %f seconds\n", maxSecBetweenUpdates);
      }
    }
    updateTimer.reset();
  }
  else // non-sensors
  {
  }
}