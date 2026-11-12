#include "rtc.h"
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

bool Rtc::read(time_t &utc)
{
  int fd = open(DEVICE, O_RDONLY);
  if (fd < 0)
  {
    printf("Rtc::read: open %s: %s\n", DEVICE, strerror(errno)); fflush(stdout);
    return false;
  }
  struct rtc_time rt;
  memset(&rt, 0, sizeof(rt));
  int rc = ioctl(fd, RTC_RD_TIME, &rt);
  int err = errno;
  close(fd);
  if (rc < 0)
  {
    printf("Rtc::read: RTC_RD_TIME: %s\n", strerror(err)); fflush(stdout);
    return false;
  }
  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  tm.tm_sec = rt.tm_sec;
  tm.tm_min = rt.tm_min;
  tm.tm_hour = rt.tm_hour;
  tm.tm_mday = rt.tm_mday;
  tm.tm_mon = rt.tm_mon;
  tm.tm_year = rt.tm_year;
  utc = timegm(&tm);
  return true;
}

bool Rtc::write(time_t utc)
{
  struct tm tm;
  if (!gmtime_r(&utc, &tm))
    return false;
  struct rtc_time rt;
  memset(&rt, 0, sizeof(rt));
  rt.tm_sec = tm.tm_sec;
  rt.tm_min = tm.tm_min;
  rt.tm_hour = tm.tm_hour;
  rt.tm_mday = tm.tm_mday;
  rt.tm_mon = tm.tm_mon;
  rt.tm_year = tm.tm_year;
  rt.tm_wday = tm.tm_wday;
  rt.tm_yday = tm.tm_yday;
  rt.tm_isdst = 0;

  int fd = open(DEVICE, O_RDWR);
  if (fd < 0)
  {
    printf("Rtc::write: open %s: %s\n", DEVICE, strerror(errno)); fflush(stdout);
    return false;
  }
  int rc = ioctl(fd, RTC_SET_TIME, &rt);
  int err = errno;
  close(fd);
  if (rc < 0)
  {
    printf("Rtc::write: RTC_SET_TIME: %s\n", strerror(err)); fflush(stdout);
    return false;
  }
  return true;
}

bool Rtc::writeFromSystemClock()
{
  return write(time(nullptr));
}

bool Rtc::setSystemClock(time_t utc)
{
  struct timespec ts;
  ts.tv_sec = utc;
  ts.tv_nsec = 0;
  if (clock_settime(CLOCK_REALTIME, &ts) < 0)
  {
    printf("Rtc::setSystemClock: clock_settime: %s\n", strerror(errno)); fflush(stdout);
    return false;
  }
  return true;
}
