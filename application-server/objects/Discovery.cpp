#include "objects.h"
#include "AmpLinks.h"

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
  // amps found / gone / moved: AmpLinks connects to newly found ones right away rather than on its next retry
  if (auto devices = std::dynamic_pointer_cast<StringSensor>(elements["devices"]))
  {
    if (devices->get() != lastDevices)
    {
      lastDevices = devices->get();
      AmpLinks::instance().discoveryChanged();
    }
  }

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

void Discovery::requestAnnouncements()
{
  // flag our next broadcasts with requestInfo (which makes devices restart their fast announcing) and send the
  // first one now instead of on our own, possibly slow, broadcast schedule
  broadcaster->Restart();
  broadcaster->SendRequests();
  broadcaster->Broadcast();
}
