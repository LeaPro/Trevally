#pragma once
#include <object.h>

class Logs: public LeafObject
{
public:
  Logs(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
  void append(std::string &logName /* "faults" or "events" */, std::string label, std::string msg);
  void append(const char *logName/* "faults" or "events" */, std::string label, std::string msg);
  void clear(const char *logName/* "faults" or "events" */);
};

