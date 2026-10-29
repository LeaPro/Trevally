#include "objects.h"
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <Common.h>

using std::string;
using std::map;
using std::set;
using std::list;
using std::vector;
using std::shared_ptr;
using boost::str;
using boost::format;

namespace fs = boost::filesystem;


DeviceInfo::DeviceInfo(string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "deviceName";           elements[name] = deviceNamePtr           = std::make_shared<StringControl>(path + "/" + name, this, "DeviceName"/*default*/, 64/*maxLength*/);
  name = "venueName";            elements[name] = venueNamePtr            = std::make_shared<StringControl>(path + "/" + name, this, "VenueName"/*default*/, 64/*maxLength*/);
  name = "location";             elements[name] = locationPtr             = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/);
  name = "defaultPostalCode";    elements[name] = defaultPostalCodePtr    = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 16/*maxLength*/);
  name = "installerName";        elements[name] = installerNamePtr        = std::make_shared<StringControl>(path + "/" + name, this, "Installer Name"/*default*/, 64/*maxLength*/);
  name = "installerContactInfo"; elements[name] = installerContactInfoPtr = std::make_shared<StringControl>(path + "/" + name, this, "Installer Contact Info"/*default*/, 256/*maxLength*/);
  name = "numInputs";            elements[name] = numInputsPtr            = std::make_shared<FloatSensor>(path + "/" + name, this, 2.0f/*min*/,  8.0f/*max*/, 2.0f/*default*/,   2.0f/*multipleOf*/,  ""/*units*/, true/*publishAsControlFlag*/);
  name = "numOutputs";           elements[name] = numOutputsPtr           = std::make_shared<FloatSensor>(path + "/" + name, this, 2.0f/*min*/,  8.0f/*max*/, 2.0f/*default*/,   2.0f/*multipleOf*/,  ""/*units*/, true/*publishAsControlFlag*/);
  name = "modelID";              elements[name] = modelIdPtr              = std::make_shared<StringSensor>(path + "/" + name, this, "CS Glass Dante"/*default*/, 32/*maxLength*/);
  name = "family";               elements[name] = familyPtr               = std::make_shared<StringSensor>(path + "/" + name, this, "TV"/*default*/, 32/*maxLength*/, true/*publishAsControlFlag*/);
  name = "serialNumber";         elements[name] = serialNumPtr            = std::make_shared<StringSensor>(path + "/" + name, this, "123"/*default*/, 32/*maxLength*/, true/*publishAsControlFlag*/);
  name = "dateOfMfg";            elements[name] = dateOfMfgPtr            = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 32/*maxLength*/, true/*publishAsControlFlag*/);
  name = "dateOfInstallation";   elements[name] = dateOfInstallationPtr   = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 32/*maxLength*/);
  name = "hardwareID";           elements[name] = hardwareIDPtr           = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/, true/*publishAsControlFlag*/);
  name = "hardwareRevision";     elements[name] = hardwareRevisionPtr     = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/, true/*publishAsControlFlag*/);
  name = "ampHardwareRevision";  elements[name] = ampHardwareRevisionPtr  = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/, true/*publishAsControlFlag*/);
  name = "wifiHwSupport";        elements[name] = wifiHwSupportPtr        = std::make_shared<BoolSensor>(path + "/" + name, this, false/*default*/);
  name = "mfgTestModeControl";   elements[name] = mfgTestModeControlPtr   = std::make_shared<BoolControl>(path + "/" + name, this,  false/*default*/, false/*persistent*/);
  name = "mfgTestMode";          elements[name] = mfgTestModePtr          = std::make_shared<BoolSensor>(path + "/" + name, this, false/*default*/);
  name = "assetTagNumber";       elements[name] = assetTagNumberPtr       = std::make_shared<StringControl>(path + "/" + name, this, "Asset Tag ID"/*default*/, 64/*maxLength*/);
  name = "firmwareVersion";      elements[name] = arcFwVerPtr             = std::make_shared<StringSensor>(path + "/" + name, this, "0.0.0.0"/*default*/, 32/*maxLength*/);
  name = "RC";                   elements[name] = arcFwVerRcPtr           = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 8/*maxLength*/);
  name = "upTime";                     elements[name] = upTimePtr         = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/, true/*publishAsControlFlag*/);
  name = "mfgTest";                    elements[name] = mfgTestPtr        = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 8/*maxLength*/, true/*publishAsControlFlag*/);
  name = "mftResult";                  elements[name] = mftResultPtr      = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 8/*maxLength*/, true/*publishAsControlFlag*/);
  name = "netResult";                  elements[name] = netResultPtr      = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 8/*maxLength*/, true/*publishAsControlFlag*/);
  name = "burnInResult";               elements[name] = burnInResultPtr   = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 8/*maxLength*/, true/*publishAsControlFlag*/);
}

void DeviceInfo::update(bool sensorsOnly, bool refreshVolatileElements)
{
  #ifdef __ARM_ARCH
  if (sensorsOnly)
  {
    /* 
     * The upTime file is updated every minute here
     */
    #if true
    static time_t lastSensorUpdateTime;
    auto now = time(NULL);
    if (now - lastSensorUpdateTime > 60)
    {
      static uint32_t upTime;
      lastSensorUpdateTime = now;
      //printf("DeviceInfo upTime update\n");

      // make sure directory and file exists
      if ((boost::filesystem::exists(ampDataFilePath) == false) ||
          (boost::filesystem::is_directory(ampDataFilePath) == false))
      {
        printf("creating new amp folder\n");
        const std::string cmd = "mkdir -p " + ampDataFilePath + ";";
        system(cmd.c_str());
      }
      if (boost::filesystem::exists(upTimeFileName) == false) { // create the file if needed
        printf("creating new upTime file\n");
        const std::string cmd = "touch " + upTimeFileName;
        system(cmd.c_str());
        // change file permissions to write
        boost::filesystem::permissions(upTimeFileName, boost::filesystem::perms::owner_write);
        std::ofstream fileNew;
        fileNew.open(upTimeFileName);
        fileNew << "This file is generated automatically; don't modify. UpTime increments every 1-minute\n";
        fileNew << "upTime: " << std::to_string(upTime) << "\n";
        fileNew.close();
        upTimePtr->set("0 days  0 hours  0 minutes");
      }
      else 
      {
        std::ifstream fileRead(upTimeFileName, std::ios::out);
        if (!fileRead.is_open()) {
          printf("Error opening %s\n", upTimeFileName.c_str());
        }
        else 
        { // read data out of the file and erase it line by line
          std::string line;
          while (std::getline(fileRead, line))
          {
            if (line.find("upTime:") != std::string::npos)
            {
              std::string time = line.substr(7,string::npos);
              // printf("current upTime: %s\t", time.c_str());
              upTime = std::stoi(time);
            }
            line.clear();
          }
          fileRead.close();
          upTime += 1;
          // update object parameter
          int32_t days  = upTime / 1440; 
          int32_t hours = (upTime - (days * 1440)) / 60;
          int32_t minutes = upTime - (hours * 60) - (days * 1440);
          std::string upTimeFormatted = std::to_string(days) + " days  " + std::to_string(hours) + " hours  " + std::to_string(minutes) + " minutes";
          std::dynamic_pointer_cast<StringSensor>(elements["upTime"])->set(upTimeFormatted);

          // re-write the file
          boost::filesystem::permissions(upTimeFileName, boost::filesystem::perms::owner_write);
          std::ofstream fileWrite;
          fileWrite.open(upTimeFileName);
          fileWrite << "This file is generated automatically; don't modify. UpTime increments every 1-minute\n";
          fileWrite << "upTime: " << std::to_string(upTime) << "\n";
          //printf("new upTime: %d\t", upTime, upTimeFormatted.c_str());
          fileWrite.close();
        }
      }
    } // 60-sec update
    #endif
  }
  else // non-sensor data
  {
   if (serialNumPtr->isModified()
     || dateOfMfgPtr->isModified()
     || mfgTestPtr->isModified()
     || mftResultPtr->isModified()
     || netResultPtr->isModified()
     || burnInResultPtr->isModified()
       )
    {
      // Look for serial Number and dateOfMfg
      fileCleanUp();
      if(fs::exists("/mnt/data/mfg/mfgInfo.txt")) {
        DeviceInfo::readMfgDataFromFile();
      }
      else {
        printf("%s file doesn't exist, using default\n", mfgInfoFileName.c_str());
        serialNumPtr->set("1234");
        dateOfMfgPtr->set("");
        mfgTestPtr->set("9");
        mftResultPtr->set("");
        netResultPtr->set("");
        burnInResultPtr->set("");
      }
    }
  } // non-sensor data
  #endif // __ARM_ARCH
}

void DeviceInfo::fileCleanUp()
{
  #ifdef __ARM_ARCH
  boost::system::error_code ec;
  // Look to see if files accidently got placed here... see issue#181
  if(boost::filesystem::exists("/mnt/data/mfg/mfg/mfgInfo.txt"))
  {
    printf("Found mfgInfo in wrong place, move it\n");
    boost::filesystem::path sourceDir("/mnt/data/mfg/mfg");
    boost::filesystem::path targetDir("/mnt/data/mfg");
    // Loop through all files in the source directory
    if (boost::filesystem::exists(sourceDir) && boost::filesystem::is_directory(sourceDir))
    {
      for (boost::filesystem::directory_iterator it(sourceDir), end; it != end; ++it)
      {
        if (boost::filesystem::is_regular_file(it->path()) && it->path().extension() == ".txt")  // Check if it's a file and ends with .txt
        {
          boost::filesystem::path destPath = targetDir / it->path().filename();  // Create the destination path (e.g., /mnt/data/mfg/filename.txt)
          boost::filesystem::rename(it->path(), destPath, ec);  // Move the file
          if (ec)
            printf("Error moving %s: %s\n", it->path().filename().c_str(), ec.message().c_str());
        }
      }
    }
    boost::filesystem::remove_all(sourceDir, ec);  // Now that the files are moved, clean up and delete the extra empty 'mfg' directory
  }

  // Look to see if files accidently got placed here... see issue#181
  if(boost::filesystem::exists("/mnt/data/mfgInfo.txt"))
  {
    printf("Found mfgInfo in wrong place, move it\n");
    boost::filesystem::path sourceDir("/mnt/data");
    boost::filesystem::path targetDir("/mnt/data/mfg");
    // Loop through all files in the source directory
    if (boost::filesystem::exists(sourceDir) && boost::filesystem::is_directory(sourceDir))
    {
      for (boost::filesystem::directory_iterator it(sourceDir), end; it != end; ++it)
      {
        if (boost::filesystem::is_regular_file(it->path()) && it->path().extension() == ".txt")  // Check if it's a file and ends with .txt
        {
          boost::filesystem::path destPath = targetDir / it->path().filename();  // Create the destination path (e.g., /mnt/data/mfg/filename.txt)
          boost::filesystem::rename(it->path(), destPath, ec);  // Move the file
          if (ec)
            printf("Error moving %s: %s\n", it->path().filename().c_str(), ec.message().c_str());
        }
      }
    }
  }
  #endif // __ARM_ARCH
}

int DeviceInfo::readMfgDataFromFile()
{
  #ifdef __ARM_ARCH
  std::string fileName = mfgInfoFileName;
  std::ifstream in(fileName, std::ios::out);
  if (!in.is_open()) {
    printf("Error opening %s\n", mfgInfoFileName.c_str());
    return -1;
  }
  else { // read data out of the file
    std::string line;
    while (std::getline(in, line)) {
      if (line.find("serialNumber:") != std::string::npos) {
        std::string sn = line.substr(14);
        printf("found serial number %s\n", sn.c_str());
        serialNumPtr->set(sn);
      }
      if (line.find("dateOfMfg:") != std::string::npos) {
        std::string dom = line.substr(11);
        printf("found dateOfMfg %s\n", dom.c_str());
        dateOfMfgPtr->set(dom);
      }
      if (line.find("testResult:") != std::string::npos) {
        std::string tr = line.substr(12);
        printf("found testResult %s\n", tr.c_str());
        mfgTestPtr->set(tr);
      }
       if (line.find("mftResult:") != std::string::npos) {
        std::string tr = line.substr(11);
        printf("found mftResult %s\n", tr.c_str());
        mftResultPtr->set(tr);
      }
       if (line.find("netResult:") != std::string::npos) {
        std::string tr = line.substr(11);
        printf("found netResult %s\n", tr.c_str());
        netResultPtr->set(tr);
      }
      if (line.find("burnInResult:") != std::string::npos) {
        std::string tr = line.substr(14);
        printf("found burnInResult %s\n", tr.c_str());
        burnInResultPtr->set(tr);
      }
    }
  }
  in.close();
  // take a short nap here to prevent reading this file too fast.
  usleep(1000);
  #endif  // __ARM_ARCH
  return 0;
}
