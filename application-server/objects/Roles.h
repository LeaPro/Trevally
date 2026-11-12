#pragma once
#include <object.h>
#include <string>

/*
 * Roles and their permissions, set up from the web UI ("config"), and who is logged in on the panel
 * ("activeRole", a sensor: the panel logs in with a role's passcode on its Enter PIN page and out when it
 * sleeps, see lock_screen.h).
 *
 * config: { "<role uuid>": { "id", "name", "passcode": "1234" (or the web UI's AES-256-GCM hex of it, see Roles.cpp),
 *   "permissions": { "global": { "<key>": 0|1|2, ... },
 *                    "tile": { "<tile uuid>": { "default": 0|1|2, "<key>": 0|1|2, ... }, ... } } }, ... }
 * Levels: 0 = no access (hidden), 1 = view only, 2 = full control.
 * Global keys: display, advanced, reboot, bluetooth-admin, bluetooth-control, streaming-admin, streaming-control.
 * Tile keys: "default" for the whole tile; the input tile also has input-source-select and input-volume-control.
 *
 * With no roles configured nothing is locked and everything is allowed. Otherwise the panel is locked until a
 * role logs in, and then has that role's permissions; a key a role doesn't list is allowed (2).
 */
class Roles final : public LeafObject
{
public:
  Roles(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

  std::shared_ptr<JsonControl> configPtr;
  std::shared_ptr<StringSensor> activeRolePtr; // role uuid; "" while locked (or with no roles)

  bool hasRoles();                                     // at least one role with a passcode
  bool locked();                                       // roles exist and none is logged in
  std::string roleForPasscode(const std::string &passcode); // matching role uuid, "" if none
  void login(const std::string &roleId);
  void logout();
  std::string activeRoleName();                        // "" while none is logged in

  int globalLevel(const std::string &key);
  int tileLevel(const std::string &tileId, const std::string &key = "default");

private:
  json activePermissions(); // the logged-in role's "permissions"; null with no role
};
