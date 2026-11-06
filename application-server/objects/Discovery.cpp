#include "objects.h"

using std::string;
using std::map;
using std::set;
using std::list;
using std::vector;
using std::shared_ptr;

extern boost::asio::io_context io_context;

//
// Discovery objects (see DiscoBase class)
//
Discovery::Discovery(string path, Object *parent) : DiscoBase(path, parent)
{
  methods = {"start"};
  string name;
  name = "devices"; elements[name] = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 65536/*maxLength*/);
  const short port = 1234;
  broadcaster = new Broadcaster(this, port, io_context);
  listener = new Listener(this, port, io_context);
}

Discovery::~Discovery()
{
  delete broadcaster;
  delete listener;
}

void Discovery::update(bool sensorsOnly, bool refreshVolatileElements)
{
  auto devInfo = std::dynamic_pointer_cast<DeviceInfo>(getRoot()->leafFromPath("/deviceInfo"));
  auto ethernet = std::dynamic_pointer_cast<Ethernet>(getRoot()->leafFromPath("/network/ethernet"));
  if (ethernet && !ethernet->macPtr->get().empty())
    macString = ethernet->macPtr->get();
  if (devInfo)
  {
    modelIDString = devInfo->modelIdPtr->get();
    serialNumberString = devInfo->serialNumPtr->get();
  }
}
