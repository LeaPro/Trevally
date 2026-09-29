#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <sys/mman.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <WebsocketServer.h>
#include <TcpServer.h>
#include "objects/objects.h"
#include "gpio.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

using std::string;
using std::shared_ptr;
using std::make_shared;
using boost::str;
using boost::format;
using namespace std::chrono_literals;

std::shared_ptr<RootObject> root;
boost::asio::io_context io_context;
static unsigned short port = 1234;

boost::posix_time::millisec timerPeriod(100);
void timerTick(const boost::system::error_code &, boost::asio::deadline_timer *t);

void timerTick(const boost::system::error_code &, boost::asio::deadline_timer *t)
{
  t->expires_at(t->expires_at() + timerPeriod);
  t->async_wait(boost::bind(timerTick, boost::asio::placeholders::error, t));
}
// gpioTool --set --pin GPIO0_5
// gpioTool --set --pin GPIO0_6
// gpioTool --set --pin GPIO0_12

int main(int argc, char *argv[])
{
  try
  {
    boost::asio::ip::address address;

    if (argc == 1)
    {
      address = boost::asio::ip::make_address("0.0.0.0");
    }
    else if (argc == 2)
    {
      address = boost::asio::ip::make_address(argv[1]);
    }
    else
    {
      std::cerr << "Usage: " << argv[0] << " <address>\n"
                << "Example: " << argv[0] << " 0.0.0.0\n";
      return -1;
    }

    // create key/value store
    string kvsDirectory = "/tmp/kvs.mdb"; // default
    #ifdef __ARM_ARCH
    // use data partition when the expected eMMC data block device exists
    if (system("test -b /dev/mmcblk0p4 >/dev/null 2>&1") == 0)
    {
      if (system("grep -q ' /mnt/data ' /proc/mounts") != 0)
      {
        system("mkdir -p /mnt/data >/dev/null 2>&1; mount /dev/mmcblk0p4 /mnt/data >/dev/null 2>&1");
      }

      if (system("grep -q ' /mnt/data ' /proc/mounts") == 0)
      {
        system("mkdir -p /mnt/data/audio-files >/dev/null 2>&1");
        system("mkdir -p /mnt/data/image-files >/dev/null 2>&1");
        system("mkdir -p /mnt/data/misc-files >/dev/null 2>&1");
        kvsDirectory = "/mnt/data/kvs.mdb";
      }
    }
    printf("Using KVS directory %s\n", kvsDirectory.c_str());
    #endif
    KeyValueStore kvs(kvsDirectory);

    // create the root of the object hierarchy tree 
    root = make_shared<RootObject>(&kvs);
    // populate the leaves of the tree
    root->addLeaf<Discovery>("/discovery");
    root->addLeaf<Bluetooth>("/bluetooth");
    root->addLeaf<Misc>("/misc");
    root->addLeaf<Test>("/test");
    root->addLeaf<Files>("/files");
    root->addLeaf<CustomChannels>("/customchannels");
    #if SOUNDTRACK_ENABLED
    root->addLeaf<Soundtrack>("/soundtrack");
    #endif
    // initialize object hierarchy (includes RootObject::restore())
    root->initialize();

    // create servers
    WebsocketServer server1(root, io_context, address, port);
    TcpServer server2(root, io_context, address, port+1);
    TcpServer server3(root, io_context, address, port+2); // create port for ipcTool for internal use

    boost::asio::deadline_timer timer(io_context, timerPeriod);
    timer.async_wait(boost::bind(timerTick, boost::asio::placeholders::error, &timer));

    // run our boost::asio io_context, exiting when any of the following signals occur:
    boost::asio::signal_set signals(io_context, SIGINT);
    signals.add(SIGQUIT);
    signals.add(SIGABRT);
    signals.add(SIGSEGV);
    signals.add(SIGTERM);
    signals.add(SIGPWR);
    signals.async_wait(
      [](const boost::system::error_code& ec, int signal) {
        if (ec.value() != 0)
          throw std::runtime_error("boost::asio::signal_set.async_wait() failed");
        else
          printf("%s:%d(%s) caught signal %d (%s)\n", __FILE__, __LINE__, __func__, signal, strsignal(signal));
        io_context.stop();
      });
    if (io_context.stopped())
      io_context.restart();
    io_context.run();
  }
  catch (std::exception &e)
  {
    std::cerr << "Server exception: " << e.what() << "\n";
    return 1;
  }
  std::cout << "Server exiting." << std::endl;
  return 0;
}

