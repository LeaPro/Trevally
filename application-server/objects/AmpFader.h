#pragma once
#include <object.h>
#include "AmpLinks.h"
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

/*
 * A local fader that drives the same dB element on one or more amp channels ("members"), e.g. "fader" on
 * several /amp/channels/<n>/output leaves, or primaryFader / analog1MixFader on /amp/channels/<n>/inputSelector.
 *
 * With several members it's a group control that keeps their offsets (like a SharkWare group volume):
 * - its value is the loudest member's level;
 * - a change moves every member by the same amount, so their differences stay; the group stops when its
 *   loudest member reaches -80 or 0 dB;
 * - a member held at -80 by a move remembers how far below it should be, so moving back up restores its offset;
 * - a member changed on its own (its own element, the amp, another UI) keeps that level as its new offset.
 * Members are followed through AmpLinks subscriptions to their leaves. Not persisted: the amps hold the real
 * values (so are the offsets of members held at -80 forgotten on restart).
 *
 * How a member is changed depends on the amp element:
 * - set (e.g. inputSelector primaryFader / mix faders): the new level, within -80..0 dB.
 * - bump (output fader / faderZoneAdjust, the amp's wall controller support, see Output::processJson()): the change.
 *   An output fader's range follows its channel's faderZoneAdjust ("topElement"): faderZoneAdjust..faderZoneAdjust-80
 *   (faderZoneOffset / faderZoneRange at their defaults), so below -80 when the zone is turned down (the amp's
 *   "footroom"); bumping faderZoneAdjust moves the channel's fader by the same amount.
 */
class AmpFaderElement final : public FloatElement
{
public:
  struct Target
  {
    std::string mac;
    std::string leafUrl; // e.g. "/amp/channels/1/output"
    std::string element; // e.g. "fader"
    bool bump = false;      // changed with the amp's "bump" method instead of "set"
    std::string topElement; // element in the same leaf whose value is this one's maximum (window: top-80..top); "" for -80..0
    bool operator==(const Target &other) const
    {
      return mac == other.mac && leafUrl == other.leafUrl && element == other.element && bump == other.bump &&
             topElement == other.topElement;
    }
  };

  AmpFaderElement(std::string path, LeafObject *parent);
  float set(float value) override;
  float get() override; // NAN (null over the API, "-" in the UI) until a member has reported its level
  void retarget(const std::vector<Target> &targets);
  void detach() { retarget({}); }
  void tick(); // from the leaf's update(): sends pending changes, adopts members' own changes
  bool known() const;

private:
  struct Member
  {
    Target target;
    std::optional<float> reported;    // last level the amp reported
    std::optional<float> reportedTop; // last value of target.topElement the amp reported
    std::optional<float> intended;    // where the group puts it; outside its range while held at an end
    std::optional<float> sent;        // the level the amp has (as far as we know): what bumps are relative to
  };
  static float window(const Member &m, float level); // @p level within the member's range
  void report(const std::string &mac, const std::string &leafUrl, const json &elements);
  void adoptReported();  // members whose reported level isn't where we put them were changed on their own
  void refresh();        // the element's value from the members
  float loudest() const; // the group's value; only valid while known()

  std::vector<Member> members;
  std::vector<AmpLinks::Handle> handles; // one subscription per member leaf
  bool sendPending = false;
  std::chrono::steady_clock::time_point holdUntil; // ignore amp echoes of our own recent sets until then
};

/* A leaf of AmpFaderElements whose set comes from a config (see OutputFaders, InputFaders). */
class AmpFaderLeaf : public LeafObject
{
public:
  using Wanted = std::vector<std::pair<std::string, std::vector<AmpFaderElement::Target>>>; // element name -> targets

  AmpFaderLeaf(std::string path, Object *parent) : LeafObject(path, parent) {}
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

protected:
  // Creates, re-points and drops elements so the leaf holds exactly @p wanted (first entry wins on a duplicate name).
  void sync(const Wanted &wanted);
};
