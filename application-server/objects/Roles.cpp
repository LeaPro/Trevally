#include "Roles.h"
#include <Aes256Gcm.h>
#include <cctype>

using std::string;

Roles::Roles(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config";     elements[name] = configPtr = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
  name = "activeRole"; elements[name] = activeRolePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 64);
}

/* A passcode as text; the web UI may send it as a number. "" if missing. */
static string passcodeOf(const json &role)
{
  auto it = role.find("passcode");
  if (it == role.end())
    return "";
  if (it->is_string())
    return it->get<string>();
  if (it->is_number_integer())
    return std::to_string(it->get<long long>());
  return "";
}

static int clampLevel(const json &value)
{
  if (!value.is_number())
    return 2;
  const int level = value.get<int>();
  return level < 0 ? 0 : level > 2 ? 2 : level;
}

void Roles::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
    return;
  // the logged-in role was removed (or lost its passcode): lock
  const string active = activeRolePtr->get();
  if (!active.empty())
  {
    const json config = configPtr->get();
    auto it = config.is_object() ? config.find(active) : config.end();
    if (!config.is_object() || it == config.end() || !it->is_object() || passcodeOf(*it).empty())
      logout();
  }
}

bool Roles::hasRoles()
{
  const json config = configPtr->get();
  if (!config.is_object())
    return false;
  for (auto it = config.begin(); it != config.end(); ++it)
    if (it->is_object() && !passcodeOf(*it).empty())
      return true;
  return false;
}

bool Roles::locked()
{
  return hasRoles() && activeRolePtr->get().empty();
}

/* Hex long enough to hold an IV, a tag and at least one byte: how the web UI stores an encrypted passcode */
static bool isCipherHex(const string &s)
{
  if (s.size() % 2 || s.size() < 2 * (Aes256Gcm::IV_SIZE + Aes256Gcm::TAG_SIZE + 1))
    return false;
  for (char c : s)
    if (!isxdigit((unsigned char)c))
      return false;
  return true;
}

/* The web UI's shared key for passcodes: AES-256-GCM, 12-byte IV prepended, 16-byte tag appended */
static const string passcodeKeyHexString = "967ca764c3d9fac358489028bbe36539b904e6a442049021f478359227920303";

string Roles::roleForPasscode(const string &passcode)
{
  const json config = configPtr->get();
  if (passcode.empty() || !config.is_object())
    return "";
  Aes256Gcm cipher(passcodeKeyHexString);
  for (auto it = config.begin(); it != config.end(); ++it)
  {
    if (!it->is_object())
      continue;
    const string stored = passcodeOf(*it);
    if (stored == passcode)
      return it.key();
    if (isCipherHex(stored))
    {
      try
      {
        if (cipher.decrypt(stored) == passcode)
          return it.key();
      }
      catch (const std::exception &) {} // not encrypted with the shared key
    }
  }
  return "";
}

void Roles::login(const string &roleId)
{
  activeRolePtr->set(roleId);
}

void Roles::logout()
{
  activeRolePtr->set("");
}

string Roles::activeRoleName()
{
  const string active = activeRolePtr->get();
  const json config = configPtr->get();
  if (active.empty() || !config.is_object())
    return "";
  auto it = config.find(active);
  if (it == config.end() || !it->is_object())
    return "";
  auto name = it->find("name");
  return (name != it->end() && name->is_string()) ? name->get<string>() : "";
}

json Roles::activePermissions()
{
  const string active = activeRolePtr->get();
  const json config = configPtr->get();
  if (active.empty() || !config.is_object())
    return nullptr;
  auto it = config.find(active);
  if (it == config.end() || !it->is_object())
    return nullptr;
  auto permissions = it->find("permissions");
  return (permissions != it->end() && permissions->is_object()) ? *permissions : json::object();
}

int Roles::globalLevel(const string &key)
{
  if (!hasRoles())
    return 2;
  const json permissions = activePermissions();
  if (permissions.is_null())
    return 0; // locked: nothing is usable until someone logs in
  auto global = permissions.find("global");
  if (global == permissions.end() || !global->is_object())
    return 2;
  auto it = global->find(key);
  return it != global->end() ? clampLevel(*it) : 2;
}

int Roles::tileLevel(const string &tileId, const string &key)
{
  if (!hasRoles())
    return 2;
  const json permissions = activePermissions();
  if (permissions.is_null())
    return 0;
  auto tiles = permissions.find("tile");
  if (tiles == permissions.end() || !tiles->is_object())
    return 2;
  auto tile = tiles->find(tileId);
  if (tile == tiles->end() || !tile->is_object())
    return 2;
  auto it = tile->find(key);
  if (it != tile->end())
    return clampLevel(*it);
  auto fallback = tile->find("default");
  return fallback != tile->end() ? clampLevel(*fallback) : 2;
}
