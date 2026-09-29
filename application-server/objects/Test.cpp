#include <Test.h>
#include "LeaApiException.h"
#include <string>
#include <vector>
#include <algorithm>
#include <boost/filesystem.hpp>

using std::string;
using std::vector;

namespace {

std::vector<std::string> parsePathSegments(const std::string &path)
{
  std::vector<std::string> segments;
  boost::split(segments, path, [](char c){ return c == '/'; });
  segments.erase(std::remove(segments.begin(), segments.end(), ""), segments.end());
  return segments;
}

}

Test::Test(string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "test"; elements[name] = testPtr = std::make_shared<JsonControl>(path + "/" + name, this, json({}));
  name = "testHook"; elements[name] = testHookPtr = std::make_shared<JsonControl>(path + "/" + name, this, json({}), false/*persist*/);
  name = "foo"; elements[name] = std::make_shared<BoolControl>(path + "/" + name, this, false);
}

void Test::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (!sensorsOnly && !refreshVolatileElements)
  {
    if (!testHookPtr->isModified())
      return;

    auto hook = testHookPtr->get();
    if (!hook.is_object())
      return;

    auto opIt = hook.find("op");
    if (opIt == hook.end() || !opIt->is_string())
      return;

    const std::string op = opIt->get<std::string>();
    try
    {
      if (op == "set")
      {
        auto valueIt = hook.find("value");
        if (valueIt == hook.end())
          throw std::runtime_error("testHook set requires 'value'");
        testPtr->set(*valueIt);
      }
      else if (op == "add" || op == "update" || op == "remove")
      {
        auto pathIt = hook.find("path");
        if (pathIt == hook.end() || !pathIt->is_string())
          throw std::runtime_error("testHook " + op + " requires string 'path'");

        auto relativePath = parsePathSegments(pathIt->get<std::string>());
        if (relativePath.empty())
          throw std::runtime_error("testHook " + op + " path must not be empty");

        if (op == "add")
        {
          auto valueIt = hook.find("value");
          if (valueIt == hook.end())
            throw std::runtime_error("testHook add requires 'value'");
          auto updated = testPtr->add(relativePath, *valueIt);
          testPtr->set(updated.updatedValue);
        }
        else if (op == "update")
        {
          auto valueIt = hook.find("value");
          if (valueIt == hook.end())
            throw std::runtime_error("testHook update requires 'value'");
          auto updated = testPtr->update(relativePath, *valueIt);
          testPtr->set(updated.updatedValue);
        }
        else
        {
          auto updated = testPtr->remove(relativePath);
          testPtr->set(updated.updatedValue);
        }
      }
      else
      {
        throw std::runtime_error("unsupported testHook op '" + op + "'");
      }
    }
    catch (const std::exception &e)
    {
      printf("testHook error: %s\n", e.what());
    }
  }
}

void Test::update(const std::string &elementName, const json &patchOps, int client)
{
  printf("update element: %s, patchOps: %s\n", elementName.c_str(), patchOps.dump().c_str());
}
