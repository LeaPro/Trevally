#include "AmpFader.h"
#include <algorithm>
#include <cmath>
#include <set>

using std::string;
using Clock = std::chrono::steady_clock;

// After a local set the amps echo each value back; ignore their reports this long so a quickly
// turned encoder doesn't jump back to older values.
static const auto echoHold = std::chrono::milliseconds(1000);

AmpFaderElement::AmpFaderElement(string path, LeafObject *parent)
  : FloatElement(path, parent, -80.0f/*min*/, 0.0f/*max*/, -80.0f/*default*/, 0.1f/*multipleOf*/, "dB"/*units*/)
{
  // a control, like FloatControl (whose set()/get() are final), but not persisted: the amps hold the real values
  control = true;
  persistent = false;
  accessControlled = false;
}

static float clampLevel(float value)
{
  return std::fmin(std::fmax(value, -80.0f), 0.0f);
}

// reported vs sent levels: the amp keeps what it's sent, but don't let float noise count as a change
static bool sameLevel(float a, float b)
{
  return std::fabs(a - b) < 0.05f;
}

float AmpFaderElement::window(const Member &m, float level)
{
  if (m.target.topElement.empty() || !m.reportedTop)
    return clampLevel(level);
  const float top = std::fmin(*m.reportedTop, 0.0f);
  return std::fmin(std::fmax(level, top - 80.0f), top);
}

bool AmpFaderElement::known() const
{
  return std::any_of(members.begin(), members.end(), [](const Member &m) { return m.intended.has_value(); });
}

float AmpFaderElement::loudest() const
{
  float result = -INFINITY;
  for (const Member &m : members)
    if (m.intended)
      result = std::fmax(result, *m.intended);
  return clampLevel(result);
}

float AmpFaderElement::get()
{
  return known() ? FloatElement::get() : NAN;
}

float AmpFaderElement::set(float value)
{
  // until the amps report their levels a relative change (the encoder) has nothing to start from
  if (!known() || std::isnan(value))
    return get();
  // the loudest member reaches the value; every member moves the same amount (its offset is kept)
  const float delta = clampLevel(value) - loudest();
  if (delta == 0.0f)
    return get();
  for (Member &m : members)
    if (m.intended)
      *m.intended += delta;
  refresh();
  sendPending = true;
  holdUntil = Clock::now() + echoHold;
  return get();
}

void AmpFaderElement::refresh()
{
  if (known())
    FloatElement::set(loudest());
}

void AmpFaderElement::report(const string &mac, const string &leafUrl, const json &elements)
{
  const bool wasKnown = known();
  bool changed = false;
  for (Member &m : members)
  {
    if (AmpLinks::normalizeMac(m.target.mac) != mac || m.target.leafUrl != leafUrl)
      continue;
    if (!m.target.topElement.empty())
    {
      auto top = elements.find(m.target.topElement);
      if (top != elements.end() && top->is_number())
        m.reportedTop = top->get<float>();
    }
    auto it = elements.find(m.target.element);
    if (it == elements.end() || !it->is_number())
      continue;
    m.reported = it->get<float>();
    if (!m.intended)
    {
      m.intended = m.sent = m.reported;
      changed = true;
    }
  }
  if (!sendPending && Clock::now() >= holdUntil)
    adoptReported();
  else if (changed)
    refresh();
  if (!wasKnown && known())
    setModified(); // null -> value, even if it equals the stored default
}

void AmpFaderElement::adoptReported()
{
  for (Member &m : members)
  {
    if (!m.reported || !m.intended)
      continue;
    if (!sameLevel(*m.reported, window(m, *m.intended)))
      m.intended = m.reported;
    m.sent = m.reported; // bumps are relative to what the amp has
  }
  refresh();
}

void AmpFaderElement::tick()
{
  if (sendPending)
  {
    sendPending = false;
    holdUntil = Clock::now() + echoHold;
    for (Member &m : members)
    {
      if (!m.intended)
        continue;
      const float level = window(m, *m.intended);
      if (!m.target.bump)
      {
        AmpLinks::instance().set(m.target.mac, m.target.leafUrl, json{{m.target.element, level}});
      }
      else if (m.sent && std::fabs(level - *m.sent) > 0.001f)
      {
        // the amp moves it by the difference (clamped to its range like we did); a lost bump is corrected when the
        // amp's report is adopted after the echo hold
        AmpLinks::instance().bump(m.target.mac, m.target.leafUrl, json{{m.target.element, level - *m.sent}});
      }
      m.sent = level;
    }
  }
  else if (Clock::now() >= holdUntil)
  {
    adoptReported(); // settle on what the amps reported last
  }
}

void AmpFaderElement::retarget(const std::vector<Target> &targets)
{
  std::vector<Target> current;
  for (const Member &m : members)
    current.push_back(m.target);
  if (targets == current)
    return;

  const bool wasKnown = known();
  std::vector<Member> next;
  for (const Target &t : targets)
  {
    auto same = std::find_if(members.begin(), members.end(), [&](const Member &m) { return m.target == t; });
    next.push_back(same != members.end() ? *same : Member{t}); // keep known offsets
  }
  members = std::move(next);
  sendPending = false;

  // new subscriptions before dropping the old ones, so leaves still in use aren't unsubscribed in between
  std::vector<AmpLinks::Handle> nextHandles;
  std::set<std::pair<string, string>> leaves;
  for (const Member &m : members)
  {
    const string mac = AmpLinks::normalizeMac(m.target.mac), leafUrl = m.target.leafUrl;
    if (!leaves.insert({mac, leafUrl}).second)
      continue;
    nextHandles.push_back(AmpLinks::instance().subscribe(mac, leafUrl, [this, mac, leafUrl](const json &elements) {
      report(mac, leafUrl, elements);
    }));
  }
  handles = std::move(nextHandles);

  refresh();
  if (wasKnown != known())
    setModified();
}

void AmpFaderLeaf::update(bool sensorsOnly, bool refreshVolatileElements)
{
  for (auto &[name, element] : elements)
    if (auto fader = std::dynamic_pointer_cast<AmpFaderElement>(element))
      fader->tick();
}

void AmpFaderLeaf::sync(const Wanted &wanted)
{
  std::set<string> names;
  for (const auto &[name, targets] : wanted)
  {
    if (name.empty() || name.find('/') != string::npos || !names.insert(name).second)
      continue;
    auto fader = elements.count(name) ? std::dynamic_pointer_cast<AmpFaderElement>(elements[name]) : nullptr;
    if (!fader)
    {
      fader = std::make_shared<AmpFaderElement>(path + "/" + name, this);
      elements[name] = fader;
      for (int client : subscribers)
        fader->addSubscriber(client);
    }
    fader->retarget(targets);
  }

  // drop elements that left the config (the UI may still hold one; it just stops changing)
  std::vector<string> gone;
  for (auto &[name, element] : elements)
    if (!names.count(name))
      gone.push_back(name);
  for (const auto &name : gone)
  {
    if (auto fader = std::dynamic_pointer_cast<AmpFaderElement>(elements[name]))
      fader->detach();
    elements.erase(name);
  }
}
