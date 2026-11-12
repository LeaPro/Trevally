#include "objects.h"

#include <chrono> 
#include <ctime> 

using std::string;
using std::map;
using std::set;
using std::list;
using std::vector;
using std::shared_ptr;

const int maxLogSize = 65536;

class LogStringSensor final : public StringSensor
{
public:
  LogStringSensor(string path, LeafObject *parent, string value, int maxLength, bool publishAsControlFlag = false) :
    StringSensor(path, parent, value, maxLength, publishAsControlFlag)
  { 
    persistent = true;
  }
  json processJson(string method, json j, int client = -1) override final
  { 
    json result = json({});
    if (method == "set" && client == -1) // required for LeafObject::restore()
    {
      set(j.get<string>());
    }
    else if (method == "clear")
    {
      set(string(""));
      parent->dirty = true;
    }
    else 
    {
      result = StringSensor::processJson(method, j, client);
    }
    return result;
  }
  std::string set(std::string  value) override final
  {
    auto result = StringSensor::set(value);
    return result;
  }
  std::string get() override final
  {
    return StringSensor::get();
  }
};

//
// Log objects
//
Logs::Logs(string path, Object *parent) : LeafObject(path, parent)
{
  methods = {"get", "clear", "subscribe", "unsubscribe"};
  string name;
  name = "faults"; elements[name] = std::make_shared<LogStringSensor>(path + "/" + name, this, ""/*default*/, maxLogSize/*maxLength*/, true/*publishAsControlFlag*/);
  name = "events"; elements[name] = std::make_shared<LogStringSensor>(path + "/" + name, this, ""/*default*/, maxLogSize/*maxLength*/, true/*publishAsControlFlag*/);
}

void Logs::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
  {
  }
  else
  {
  }
}


void Logs::append(std::string &logName /* "faults" or "events" */, std::string label/*Critical, Error, Warning, Info, Debug*/, std::string msg)
{
  assert(elements.count(logName) > 0);
  auto log = std::dynamic_pointer_cast<LogStringSensor>(elements[logName]);
  // generate timestamp
  time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm *timeinfo = localtime(&now);
  char s[80];
  strftime(s, sizeof(s), "%c", timeinfo);
  // make sure msg ends with '\n'
  if (msg.c_str()[msg.size()-1] != '\n')
    msg = msg + "\n";
  // append message, making room if necessary by trimming oldest messages
  string newMsg = string(s) + " [" + label + "] " + msg;
  string existingMsgs = log->get();
  while (existingMsgs.size() + newMsg.size() > maxLogSize)
  {
    auto pos = existingMsgs.find_first_of('\n');
    if (pos == string::npos)
      break;
    existingMsgs.erase(0, pos+1);
  }
  log->set(existingMsgs + newMsg);
}

void Logs::append(const char *logName /* "faults" or "events" */, std::string label, std::string msg)
{
  string tmp = string(logName);
  append(tmp, label, msg);
}

void Logs::clear(const char *logName /* "faults" or "events" */)
{
  string tmp = string(logName);
  std::string &clearLogName = tmp;
  assert(elements.count(clearLogName) > 0);
  auto log = std::dynamic_pointer_cast<LogStringSensor>(elements[clearLogName]);
  log->set("");
}