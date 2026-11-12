#include "objects.h"
#include <boost/filesystem.hpp>

using std::string;
using boost::str;
using boost::format;

//
// Network objects (ported from Pilotfish; Ethernet only)
//

static uint32_t getPhyErrorCount()
{
  // assume external forces are adding a line to this file each time an error is detected (see phy-error-monitor.service)
  const string phyErrorFile = "/mnt/data/phy-errors.txt";
  uint32_t errorCount = 0;
  try
  {
    if (boost::filesystem::exists(phyErrorFile))
    {
      string result = system2("wc -l " + phyErrorFile + " | cut -d' ' -f1");
      errorCount = std::stoul(result);
    }
  }
  catch (...)
  {
  }
  return errorCount;
}

Ethernet::Ethernet(string path, Object *parent) : LeafObject(path, parent)
{
  class PasswordControl final : public StringControl
  {
  public:
    PasswordControl(std::string path, LeafObject *parent, std::string value, int minLength, int maxLength) :
      StringControl(path, parent, value, maxLength)
    {
      this->minLength = minLength;
    }
    std::string set(std::string value)
    {
      if (value.length() != 0 && (value.length() < this->minLength || value.length() > this->maxLength))
      {
        std::string err = str(format("invalid length: %d (%s)") % value.length() % path);
        throw std::runtime_error(err);
      }
      if (value != this->value)
      {
        this->value = value;
        setModified();
      }
      return this->value;
    }
    json processJson(string method, json j, int client)
    {
      json result = nullptr;
      if (method == "info")
      {
        result = getInfo();
      }
      else if (method == "set" && isControl())
      {
        set(j.get<string>());
      }
      else if (method == "get")
      {
        string s = get();
        if (client != -1) // return obfuscated value
          result = string(s.size(), '*');
        else // internal client gets actual value
          result = s;
      }
      else
      {
        throw std::runtime_error(std::string("unsupported method: ") + method + " (" + path + ") ");
      }
      return result;
    }
  protected:
    int minLength;
  };
  string name;
  name = "dhcp";                         elements[name] = dhcpPtr                         = std::make_shared<BoolControl>(path + "/" + name, this,  true/*default*/);
  name = "ip";                           elements[name] = ipPtr                           = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/);
  name = "gateway";                      elements[name] = gatewayPtr                      = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/);
  name = "dns1";                         elements[name] = dns1Ptr                         = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/);
  name = "dns2";                         elements[name] = dns2Ptr                         = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/);
  name = "mac";                          elements[name] = macPtr                          = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/, true/*publishAsControlFlag*/);
  name = "dot1xMode";                    elements[name] = dot1xModePtr                    = std::make_shared<EnumControl>(path + "/" + name, this, 0, (int)Ethernet::Dot1xMode::Max/*max*/, (int)Ethernet::Dot1xMode::Disabled/*default*/, Ethernet::Dot1xModeMap);
  name = "dot1xIdentity";                elements[name] = dot1xIdentityPtr                = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "dot1xPassword";                elements[name] = dot1xPasswordPtr                = std::make_shared<PasswordControl>(path + "/" + name, this, ""/*default*/, 8/*minLength*/, 128/*maxLength*/);
  name = "dot1xAnonymousIdentity";       elements[name] = dot1xAnonymousIdentityPtr       = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "dot1xDomain";                  elements[name] = dot1xDomainPtr                  = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "dot1xCACertValid";             elements[name] = dot1xCACertValidPtr             = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "dot1xClientCertValid";         elements[name] = dot1xClientCertValidPtr         = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "dot1xClientKeyCheck";          elements[name] = dot1xClientKeyCheckPtr          = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "dot1xUploadedCACertValid";     elements[name] = dot1xUploadedCACertValidPtr     = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "dot1xUploadedClientCertValid"; elements[name] = dot1xUploadedClientCertValidPtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "dot1xUploadedClientKeyCheck";  elements[name] = dot1xUploadedClientKeyCheckPtr  = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "dot1xSaveUploadedCerts";       elements[name] = dot1xSaveUploadedCertsPtr       = std::make_shared<BoolControl>(path + "/" + name, this,  false/*default*/, false/*persistent*/);
  name = "phyErrorCount";                elements[name] = phyErrorCountPtr                = std::make_shared<UInt32Sensor>(path + "/" + name, this, 0/*min*/, __UINT32_MAX__/*max*/, 0/*default*/, ""/*units*/);
}

/*
{
  "leaApi": "1.0",
  "url": "/network/ethernet",
  "method": "set",
  "params": {
    "dhcp": true,
    "ip": "192.168.100.111/24",
    "gateway": "192.168.100.1",
    "dns1": "8.8.8.8",
    "dns2": "8.8.4.4",
    "dot1xMode": "PEAP",
    "dot1xIdentity": "testing",
    "dot1xPassword": "password",
    "dot1xAnonymousIdentity": "anonymous@plunkware.com",
    "dot1xDomain": "plunkware.com"
  },
  "id": 4
}
{
  "leaApi": "1.0",
  "url": "/network/ethernet",
  "method": "set",
  "params": {
    "dot1xSaveUploadedCerts": true
  },
  "id": 4
}
*/

#ifdef __ARM_ARCH
// current IPv4 address of an interface in CIDR notation, e.g. "192.168.1.50/24" ("" if none)
static string getInterfaceIp(const char *interface)
{
  string result;
  ifaddrs *interfaces = nullptr;
  if (getifaddrs(&interfaces) != 0)
    return result;
  for (ifaddrs *ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next)
  {
    if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET || strcmp(ifa->ifa_name, interface) != 0)
      continue;
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &((sockaddr_in *)ifa->ifa_addr)->sin_addr, buf, sizeof(buf)))
    {
      result = buf;
      if (ifa->ifa_netmask != nullptr)
        result += "/" + std::to_string(__builtin_popcount(((sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr));
    }
    break;
  }
  freeifaddrs(interfaces);
  return result;
}
#endif

static void setCertSensor(const std::shared_ptr<StringSensor> &sensor, const string &file)
{
  if (boost::filesystem::exists(file))
    sensor->set(system2("openssl x509 -in " + file + " -startdate -enddate -noout | tr '\n' ' '"));
  else
    sensor->set("no cert");
}

static void setKeySensor(const std::shared_ptr<StringSensor> &sensor, const string &file, const string &password)
{
  if (boost::filesystem::exists(file) && password != "")
    sensor->set(system2("openssl rsa -in " + file + " -passin pass:" + password + " -check -noout 2>&1 | tr '\n' ' '"));
  else
    sensor->set("no key");
}

void Ethernet::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
  {
    #ifdef __ARM_ARCH
    string mac = getMac("eth0");
    #else
    string mac = "00:ca:fe:ba:be:01";
    #endif
    macPtr->set(mac);
    #ifdef __ARM_ARCH
    // when DHCP is on, 'ip' mirrors the live address; when off, it is the user's static setting
    if (dhcpPtr->get())
    {
      ipPtr->set(getInterfaceIp("eth0"));
      ipPtr->isModified(); // clear modified flag so this doesn't trigger doit()
    }
    #endif
    auto now = time(NULL);
    // limit the update rate for all sensors that follow
    static time_t lastSensorUpdateTime;
    if (now - lastSensorUpdateTime < 10)
      return;
    else
      lastSensorUpdateTime = now;
    string dot1xPassword = dot1xPasswordPtr->get();
    setCertSensor(dot1xCACertValidPtr, "/mnt/data/certs/ca.pem");
    setCertSensor(dot1xClientCertValidPtr, "/mnt/data/certs/client.pem");
    setKeySensor(dot1xClientKeyCheckPtr, "/mnt/data/certs/client.key", dot1xPassword);
    setCertSensor(dot1xUploadedCACertValidPtr, "/mnt/data/uploaded-certs/ca.pem");
    setCertSensor(dot1xUploadedClientCertValidPtr, "/mnt/data/uploaded-certs/client.pem");
    setKeySensor(dot1xUploadedClientKeyCheckPtr, "/mnt/data/uploaded-certs/client.key", dot1xPassword);
    phyErrorCountPtr->set(getPhyErrorCount());
  }
  else
  {
    #ifdef __ARM_ARCH
    if (dhcpPtr->isModified() ||
        ipPtr->isModified() ||
        gatewayPtr->isModified() ||
        dns1Ptr->isModified() ||
        dns2Ptr->isModified() ||
        dot1xModePtr->isModified() ||
        dot1xIdentityPtr->isModified() ||
        dot1xPasswordPtr->isModified() ||
        dot1xAnonymousIdentityPtr->isModified() ||
        dot1xDomainPtr->isModified() ||
        dot1xSaveUploadedCertsPtr->isModified())
    {
      doit();
    }
    #endif
  }
}

#ifdef __ARM_ARCH
void Ethernet::doit()
{
  bool dhcp = dhcpPtr->get();
  string ip = ipPtr->get();
  string gateway = gatewayPtr->get();
  string defaultGateway = "0.0.0.0";
  // if gateway is blank, use the default
  if (gateway.length() < 7)
    gateway = defaultGateway;
  string dns1 = dns1Ptr->get();
  string dns2 = dns2Ptr->get();
  Ethernet::Dot1xMode dot1xMode = (Ethernet::Dot1xMode)dot1xModePtr->getEnumValue();
  const char *dot1xModeString = dot1xModePtr->get();
  string dot1xIdentity = dot1xIdentityPtr->get();
  string dot1xPassword = dot1xPasswordPtr->get();
  string dot1xAnonymousIdentity = dot1xAnonymousIdentityPtr->get();
  string dot1xDomain = dot1xDomainPtr->get();
  bool dot1xSaveUploadedCerts = dot1xSaveUploadedCertsPtr->get();
  if (dot1xSaveUploadedCerts) // make dot1xSaveUploadedCertsPtr behave like an event
  {
    dot1xSaveUploadedCertsPtr->set(false);
    dot1xSaveUploadedCertsPtr->isModified(); // clear modified flag
  }
  printf("%s:%d(%s)\n", __FILE__, __LINE__, __func__);
  printf("  dhcp = %s\n", dhcp ? "true" : "false");
  printf("  ip = %s\n", ip.c_str());
  printf("  gateway = %s\n", gateway.c_str());
  printf("  dns1 = %s\n", dns1.c_str());
  printf("  dns2 = %s\n", dns2.c_str());
  printf("  dot1xMode = %s\n", dot1xModeString);
  printf("  dot1xIdentity = %s\n", dot1xIdentity.c_str());
  printf("  dot1xAnonymousIdentity = %s\n", dot1xAnonymousIdentity.c_str());
  printf("  dot1xDomain = %s\n", dot1xDomain.c_str());
  printf("  dot1xSaveUploadedCerts = %s\n", dot1xSaveUploadedCerts ? "true" : "false");

  // run script to make it happen
  string cmd = "/etc/systemd/network/wired-mode.sh";
  if (!dhcp) cmd += " -a " + ip + " -g " + gateway;
  if (dns1 != "") cmd += " -1 " + dns1;
  if (dns2 != "") cmd += " -2 " + dns2;
  if (dot1xMode != Ethernet::Dot1xMode::Disabled) cmd += " -m " + string(dot1xModeString) + " -i " + dot1xIdentity + " -p " + dot1xPassword;
  if (dot1xMode != Ethernet::Dot1xMode::Disabled && dot1xAnonymousIdentity != "") cmd += " -u " + dot1xAnonymousIdentity;
  if (dot1xMode != Ethernet::Dot1xMode::Disabled && dot1xDomain != "") cmd += " -d " + dot1xDomain;
  if (dot1xSaveUploadedCerts) cmd += " -s ";
  cmd += " &";
  printf("%s:%d(%s) - executing shell command '%s'\n", __FILE__, __LINE__, __func__, cmd.c_str());
  system(cmd.c_str());
  std::shared_ptr<Discovery> discovery = std::dynamic_pointer_cast<Discovery>(getRoot()->leafFromPath("/discovery"));
  if (discovery)
    discovery->reset();
}
#endif
