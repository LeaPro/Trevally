/*

  A friendly command-line utility for interacting with gpio pin settings

*/
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <tuple>
#include <unistd.h>
#include <getopt.h>

#include "../gpio.h"

using std::string;
namespace fs = std::filesystem;

static const char *SYSFS_GPIO_DIR = "/sys/class/gpio";

enum class Action
{
  None,
  Status,
  Set,
  Clr,
  Get,
  Nuke,
};

void usage(const char *progName)
{
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s --port <port> --pin <pin> [--status|--set|--clr|--get|--nuke]\n", progName);
  fprintf(stderr, "  %s --pin <PORTNAME_pin> [--status|--set|--clr|--get|--nuke]\n", progName);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  --pin <pin>    specify pin as an integer >= 0, or as a symbolic name\n");
  fprintf(stderr, "                 e.g. GPIO0_12, GPIO1_5, MCU_GPIO0_3\n");
  fprintf(stderr, "                 (symbolic form implies --port; conflicts with explicit --port are rejected)\n");
  fprintf(stderr, "  --port <port>  specify port, e.g. GPIO0, GPIO1, MCU_GPIO0\n");
  fprintf(stderr, "  --status       report GPIO configuration (in/out/unconfigured) and value\n");
  fprintf(stderr, "  --set          configure if needed and set value to 1 (error if input)\n");
  fprintf(stderr, "  --clr          configure if needed and clear value to 0 (error if input)\n");
  fprintf(stderr, "  --get          report pin value; if unconfigured, configure as input then read\n");
  fprintf(stderr, "  --nuke         unexport GPIO if currently configured\n");
  fprintf(stderr, "  --help         print help\n");
}

static bool read_file_trimmed(const std::string &path, std::string *out)
{
  std::ifstream f(path);
  if (!f.is_open())
    return false;

  std::string line;
  std::getline(f, line);
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
    line.pop_back();
  *out = line;
  return true;
}

static std::string gpio_path(int gpioNum, const char *node)
{
  return std::string(SYSFS_GPIO_DIR) + "/gpio" + std::to_string(gpioNum) + "/" + node;
}

static bool is_configured(int gpioNum)
{
  return fs::exists(gpio_path(gpioNum, "direction"));
}

static bool read_direction(int gpioNum, std::string *direction)
{
  std::string raw;
  if (!read_file_trimmed(gpio_path(gpioNum, "direction"), &raw))
    return false;

  if (raw == "in")
    *direction = "in";
  else
    *direction = "out";

  return true;
}

static bool read_value(int gpioNum, int *value)
{
  std::string raw;
  if (!read_file_trimmed(gpio_path(gpioNum, "value"), &raw))
    return false;

  *value = (raw != "0") ? 1 : 0;
  return true;
}

static bool write_node(int gpioNum, const char *node, const char *value)
{
  std::ofstream f(gpio_path(gpioNum, node));
  if (!f.is_open())
    return false;
  f << value;
  return f.good();
}

static bool export_gpio(int gpioNum)
{
  std::ofstream f(std::string(SYSFS_GPIO_DIR) + "/export");
  if (!f.is_open())
    return false;
  f << gpioNum;
  return f.good();
}

static bool unexport_gpio(int gpioNum)
{
  std::ofstream f(std::string(SYSFS_GPIO_DIR) + "/unexport");
  if (!f.is_open())
    return false;
  f << gpioNum;
  return f.good();
}

static bool parse_non_negative_int(const std::string &s, int *value)
{
  if (s.empty())
    return false;
  for (char c : s)
  {
    if (c < '0' || c > '9')
      return false;
  }
  *value = atoi(s.c_str());
  return true;
}

static std::vector<int> list_configured_gpio_numbers()
{
  std::vector<int> out;
  if (!fs::exists(SYSFS_GPIO_DIR))
    return out;

  for (const auto &entry : fs::directory_iterator(SYSFS_GPIO_DIR))
  {
    const std::string name = entry.path().filename().string();
    if (name.rfind("gpio", 0) != 0)
      continue;

    const std::string suffix = name.substr(4);
    int gpioNum = -1;
    if (!parse_non_negative_int(suffix, &gpioNum))
      continue;

    if (is_configured(gpioNum))
      out.push_back(gpioNum);
  }

  std::sort(out.begin(), out.end());
  return out;
}

static bool count_pins_for_port(const std::string &port, int *ngpio)
{
  int count = 0;
  for (int p = 0; p < 512; ++p)
  {
    try
    {
      (void)am62x_gpio_num(port, p);
      count++;
    }
    catch (...)
    {
      break;
    }
  }

  if (count <= 0)
    return false;

  *ngpio = count;
  return true;
}

static std::string gpio_symbol_name(const std::string &port, int pin)
{
  if (pin < 0)
    return "UNKNOWN";
  return port + "_" + std::to_string(pin);
}

static int port_sort_rank(const std::string &port)
{
  if (port == "GPIO0")
    return 0;
  if (port == "GPIO1")
    return 1;
  if (port == "MCU_GPIO0")
    return 2;
  return 99;
}

static void print_status_line(const std::string &port, int pin, int gpioNum)
{
  const std::string symbol = gpio_symbol_name(port, pin);
  if (!is_configured(gpioNum))
  {
    printf("name=%s port=%s pin=%d gpio#=%d config=unconfigured value=N/A\n", symbol.c_str(), port.c_str(), pin, gpioNum);
    return;
  }

  std::string direction;
  int value = 0;
  if (!read_direction(gpioNum, &direction) || !read_value(gpioNum, &value))
  {
    fprintf(stderr, "Failed to read GPIO status for gpio%d: %s\n", gpioNum, strerror(errno));
    return;
  }
  printf("name=%s port=%s pin=%d gpio#=%d config=%s value=%d\n", symbol.c_str(), port.c_str(), pin, gpioNum, direction.c_str(), value);
}

// Resolves a --pin argument that may be either a plain integer or a symbolic
// name like GPIO0_12, GPIO1_5, MCU_GPIO0_3.  On success returns true and sets
// *pin (and optionally *port).  Reports an error and returns false on failure
// or when the implied port conflicts with an already-specified --port value.
static bool resolve_pin_arg(const std::string &arg, std::string *port, int *pin)
{
  std::string s = arg;
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });

  // Try longest prefix first so MCU_GPIO0 is not mistaken for a GPIO0 variant.
  static const struct { const char *prefix; const char *portName; } patterns[] = {
    {"MCU_GPIO0_", "MCU_GPIO0"},
    {"GPIO0_",     "GPIO0"},
    {"GPIO1_",     "GPIO1"},
  };

  for (const auto &pat : patterns)
  {
    const std::string prefix = pat.prefix;
    if (s.size() > prefix.size() && s.substr(0, prefix.size()) == prefix)
    {
      const std::string suffix = s.substr(prefix.size());
      int val = -1;
      if (parse_non_negative_int(suffix, &val))
      {
        if (!port->empty() && *port != pat.portName)
        {
          fprintf(stderr, "--pin=%s implies port %s, but --port=%s was already specified\n",
                  arg.c_str(), pat.portName, port->c_str());
          return false;
        }
        *port = pat.portName;
        *pin = val;
        return true;
      }
    }
  }

  // Plain non-negative integer
  int val = -1;
  if (parse_non_negative_int(s, &val))
  {
    *pin = val;
    return true;
  }

  fprintf(stderr, "Invalid --pin value: %s (expected integer or e.g. GPIO0_12)\n", arg.c_str());
  return false;
}

static bool gpio_to_port_pin(int gpioNum, std::string *port, int *pin)
{
  const char *ports[] = {"GPIO0", "GPIO1", "MCU_GPIO0"};
  for (const char *candidate : ports)
  {
    int base = -1;
    try
    {
      base = am62x_gpio_num(candidate, 0);
    }
    catch (...)
    {
      continue;
    }

    int ngpio = 0;
    if (!count_pins_for_port(candidate, &ngpio))
      continue;

    if (gpioNum >= base && gpioNum < (base + ngpio))
    {
      *port = candidate;
      *pin = gpioNum - base;
      return true;
    }
  }
  return false;
}

int main(int argc, char *argv[])
{
  static struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {"port", required_argument, NULL, 'p'},
      {"pin", required_argument, NULL, 'n'},
      {"status", no_argument, NULL, 's'},
      {"set", no_argument, NULL, '1'},
      {"clr", no_argument, NULL, '0'},
      {"get", no_argument, NULL, 'g'},
      {"nuke", no_argument, NULL, 'u'},
      {NULL, 0, NULL, 0}};

  string port;
  int pin = -1;
  std::string pin_arg;
  Action action = Action::None;
  int actionCount = 0;

  int c;
  int option_index = 0;
  while ((c = getopt_long(argc, argv, "hp:n:", long_options, &option_index)) != -1)
  {
    switch (c)
    {
    case 'h':
      usage(argv[0]);
      return 0;
    case 'p':
      port = optarg;
      break;
    case 'n':
      pin_arg = optarg;
      break;
    case 's':
      action = Action::Status;
      actionCount++;
      break;
    case '1':
      action = Action::Set;
      actionCount++;
      break;
    case '0':
      action = Action::Clr;
      actionCount++;
      break;
    case 'g':
      action = Action::Get;
      actionCount++;
      break;
    case 'u':
      action = Action::Nuke;
      actionCount++;
      break;
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (optind < argc)
  {
    fprintf(stderr, "%s: unexpected argument: %s\n", argv[0], argv[optind]);
    return 1;
  }

  if (!pin_arg.empty())
  {
    if (!resolve_pin_arg(pin_arg, &port, &pin))
      return 1;
  }

  if (actionCount != 1)
  {
    fprintf(stderr, "Specify exactly one action: --status, --set, --clr, --get, or --nuke\n");
    return 1;
  }

  if (action == Action::Status)
  {
    if (pin >= 0 && port.empty())
    {
      fprintf(stderr, "--pin requires --port\n");
      return 1;
    }
  }
  else
  {
    if (port.empty())
    {
      fprintf(stderr, "Missing required option --port\n");
      return 1;
    }
    if (pin < 0)
    {
      fprintf(stderr, "Missing or invalid --pin (must be >= 0)\n");
      return 1;
    }
  }

  if (action == Action::Status)
  {
    if (!port.empty() && pin >= 0)
    {
      int gpioNum = -1;
      try
      {
        gpioNum = am62x_gpio_num(port, pin);
      }
      catch (...)
      {
        fprintf(stderr, "Invalid GPIO selection: port=%s pin=%d\n", port.c_str(), pin);
        return 1;
      }
      print_status_line(port, pin, gpioNum);
      return 0;
    }

    if (!port.empty())
    {
      int base = -1;
      try
      {
        base = am62x_gpio_num(port, 0);
      }
      catch (...)
      {
        fprintf(stderr, "Could not determine gpiochip size for %s\n", port.c_str());
        return 1;
      }

      int ngpio = 0;
      if (!count_pins_for_port(port, &ngpio))
      {
        fprintf(stderr, "Could not determine gpiochip size for %s\n", port.c_str());
        return 1;
      }

      for (int p = 0; p < ngpio; ++p)
      {
        print_status_line(port, p, base + p);
      }
      return 0;
    }

    const std::vector<int> configured = list_configured_gpio_numbers();
    std::vector<std::tuple<int, int, int, std::string, int>> rows;
    for (int gpioNum : configured)
    {
      std::string foundPort = "UNKNOWN";
      int foundPin = -1;
      gpio_to_port_pin(gpioNum, &foundPort, &foundPin);
      rows.emplace_back(port_sort_rank(foundPort), foundPin, gpioNum, foundPort, foundPin);
    }

    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
      if (std::get<0>(a) != std::get<0>(b))
        return std::get<0>(a) < std::get<0>(b);
      if (std::get<1>(a) != std::get<1>(b))
        return std::get<1>(a) < std::get<1>(b);
      return std::get<2>(a) < std::get<2>(b);
    });

    for (const auto &row : rows)
    {
      print_status_line(std::get<3>(row), std::get<4>(row), std::get<2>(row));
    }
    return 0;
  }

  int gpioNum = -1;
  try
  {
    gpioNum = am62x_gpio_num(port, pin);
  }
  catch (...)
  {
    fprintf(stderr, "Invalid GPIO selection: port=%s pin=%d\n", port.c_str(), pin);
    return 1;
  }

  if (action == Action::Get)
  {
    if (!is_configured(gpioNum))
    {
      if (!export_gpio(gpioNum))
      {
        fprintf(stderr, "Failed to export gpio%d: %s\n", gpioNum, strerror(errno));
        return 1;
      }

      // Wait briefly for sysfs node creation.
      for (int i = 0; i < 100; ++i)
      {
        if (is_configured(gpioNum))
          break;
        usleep(10000);
      }
      if (!is_configured(gpioNum))
      {
        fprintf(stderr, "Timeout waiting for gpio%d sysfs nodes\n", gpioNum);
        return 1;
      }

      if (!write_node(gpioNum, "direction", "in"))
      {
        fprintf(stderr, "Failed to set gpio%d direction to in: %s\n", gpioNum, strerror(errno));
        return 1;
      }
    }

    int value = 0;
    if (!read_value(gpioNum, &value))
    {
      fprintf(stderr, "Failed to read gpio%d value: %s\n", gpioNum, strerror(errno));
      return 1;
    }
    printf("%d\n", value);
    return 0;
  }

  if (action == Action::Nuke)
  {
    if (!is_configured(gpioNum))
      return 0;

    if (!unexport_gpio(gpioNum))
    {
      fprintf(stderr, "Failed to unexport gpio%d: %s\n", gpioNum, strerror(errno));
      return 1;
    }
    return 0;
  }

  // --set / --clr
  if (!is_configured(gpioNum))
  {
    if (!export_gpio(gpioNum))
    {
      fprintf(stderr, "Failed to export gpio%d: %s\n", gpioNum, strerror(errno));
      return 1;
    }

    // Wait briefly for sysfs node creation.
    for (int i = 0; i < 100; ++i)
    {
      if (is_configured(gpioNum))
        break;
      usleep(10000);
    }
    if (!is_configured(gpioNum))
    {
      fprintf(stderr, "Timeout waiting for gpio%d sysfs nodes\n", gpioNum);
      return 1;
    }

    if (!write_node(gpioNum, "direction", "out"))
    {
      fprintf(stderr, "Failed to set gpio%d direction to out: %s\n", gpioNum, strerror(errno));
      return 1;
    }
  }

  std::string direction;
  if (!read_direction(gpioNum, &direction))
  {
    fprintf(stderr, "Failed to read gpio%d direction: %s\n", gpioNum, strerror(errno));
    return 1;
  }
  if (direction == "in")
  {
    fprintf(stderr, "GPIO is configured as input; refusing to drive output: port=%s pin=%d gpio=%d\n", port.c_str(), pin, gpioNum);
    return 1;
  }

  const char *out = (action == Action::Set) ? "1" : "0";
  if (!write_node(gpioNum, "value", out))
  {
    fprintf(stderr, "Failed to write gpio%d value: %s\n", gpioNum, strerror(errno));
    return 1;
  }

  return 0;
}