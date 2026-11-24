#include "objects.h"
#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>

using std::string;
using std::map;
using std::set;
using std::list;
using std::vector;
using std::shared_ptr;

namespace fs = boost::filesystem;

/* The elements in this object are used by mfg to store persistent data set at the factory, then never changed again (except by a service center replacing boards)
 * The data is stored in a file which is maintained in a persistent memory location so it doesn't change on firmware updates.
 * It requires the user have knowledge of a "key" entered in the "sm" elements.  If the key is correct, the data in the other elements is stored in the file.
 */
Tech::Tech(string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "tr";  /* mfg set test byte */     elements[name] = techTrPtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 3/*maxLength*/, false/*persist*/, false/*accessControlled*/);
  name = "sn";  /* mfg set serial number */ elements[name] = techSnPtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 12/*maxLength*/, false/*persist*/, false/*accessControlled*/); 
  name = "md";  /* mfg set date of mfg */   elements[name] = techMdPtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 12/*maxLength*/, false/*persist*/, false/*accessControlled*/); 
  name = "sm";  /* mfg set save key */      elements[name] = techSmPtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 12/*maxLength*/, false/*persist*/, false/*accessControlled*/); 
  name = "mr";  /* mfg set mft result */    elements[name] = techMrPtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 8/*maxLength*/, false/*persist*/, false/*accessControlled*/); 
  name = "nr";  /* mfg set net result */    elements[name] = techNrPtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 8/*maxLength*/, false/*persist*/, false/*accessControlled*/); 
  name = "br";  /* mfg set burn-in result */ elements[name]= techBrPtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 8/*maxLength*/, false/*persist*/, false/*accessControlled*/); 
}

void Tech::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
  {
  }
  else // non-sensors
  {
    #ifdef __ARM_ARCH
    if (techSmPtr->isModified())
    {
      std::string key = techSmPtr->get();
      // todo:  this code is pretty WET! it could be better, maybe even make it common in websocketAPI? -DB
      if (key.compare(serialNumKey) == 0) { // key matches! write the data
        bool snMod=0, mdMod=0, trMod=0, mrMod=0, nrMod=0, brMod=0;  // Indicates if field needs to be updated
        std::string serNum = techSnPtr->get();
        std::string mfgDate = techMdPtr->get();
        std::string testResult = techTrPtr->get();
        std::string mftResult = techMrPtr->get();
        std::string netResult = techNrPtr->get();
        std::string burnInResult = techBrPtr->get();
        std::shared_ptr<DeviceInfo> devInfo = std::dynamic_pointer_cast<DeviceInfo>(getRoot()->leafFromPath("/deviceInfo"));
        if (serNum.length() > 2) {
          snMod=true; printf("Writing new serial number %s\n", serNum.c_str());
        }
        if (mfgDate.length() > 5) {
          mdMod=true; printf("Writing new date of mfg %s\n", mfgDate.c_str());
        }
        if (testResult.length() > 0) {
          trMod=true; printf("Writing new mfg test byte %s\n", testResult.c_str());
        }
        if (mftResult.length() > 0) {
          mrMod=true; printf("Writing new mft test result %s\n", mftResult.c_str());
        }
        if (netResult.length() > 0) {
          nrMod=true; printf("Writing new net test result %s\n", netResult.c_str());
        }
        if (burnInResult.length() > 0) {
          brMod=true; printf("Writing new burn-in test result %s\n", burnInResult.c_str());
        }
        if (snMod || mdMod || trMod || mrMod || nrMod || brMod)  // is there anything to do?
        {
          printf("something in mfgInfo changed\n");
          if (system("mount | grep -q ' /mnt/data '") != 0) {
            printf("ERROR: /mnt/data not mounted; cannot update mfgInfo.txt\n");
            return;
          }
          if ((fs::exists(mfgInfoPath) == false) || (fs::is_directory(mfgInfoPath) == false))
          {
            printf("creating mfg folder\n");
            system("mkdir -p /mnt/data/mfg;");
          }
          if (fs::exists(mfgInfoFileName) == false) { // create the file if needed
            printf("creating mfgInfo file\n");
            system("touch /mnt/data/mfg/mfgInfo.txt;");
          }
          // change file permissions to write
          fs::permissions(mfgInfoFileName, fs::perms::owner_write);
          // open the file
          std::ofstream file;
          file.open(mfgInfoFileName);
          if (!file.is_open())
          {
            printf("ERROR: failed to open mfgInfo file for writing\n");
            return; // or handle error
          }
          file << "This file is generated automatically; don't modify\n";
          // new serial number, if !snMod, use existing deviceInfo data
          if(serNum.length() > 3) {
            file << "serialNumber: " + serNum + "\n";
            devInfo->serialNumPtr->set(serNum);
            devInfo->serialNumPtr->setModified();
          } // use existing data
          else {
            std::string serNumSaved = devInfo->serialNumPtr->get();
            file << "serialNumber: " + serNumSaved + "\n";
          }
          // new date of mfg
          if(mfgDate.length() > 5) {
            file << "dateOfMfg: " + mfgDate + "\n";
            devInfo->dateOfMfgPtr->set(mfgDate);
            devInfo->dateOfMfgPtr->setModified();
          } // use existing data
          else {
            std::string mfgDateSaved = devInfo->dateOfMfgPtr->get();
            file << "dateOfMfg: " + mfgDateSaved + "\n";
          }
          // new test result
          if(testResult.length() > 0) {
            file << "testResult: " + testResult + "\n";
            devInfo->mfgTestPtr->set(testResult);
            devInfo->mfgTestPtr->setModified();
          } // use existing data
          else {
            std::string testResultSaved = devInfo->mfgTestPtr->get();
            file << "testResult: " + testResultSaved + "\n";
          }
          // new mft result
          if(mftResult.length() > 0) {
            file << "mftResult: " + mftResult + "\n";
            devInfo->mftResultPtr->set(mftResult);
            devInfo->mftResultPtr->setModified();
          } // use existing data
          else {
            std::string mftResultSaved = devInfo->mftResultPtr->get();
            file << "mftResult: " + mftResultSaved + "\n";
          }
          // new net result
          if(netResult.length() > 0) {
            file << "netResult: " + netResult + "\n";
            devInfo->netResultPtr->set(netResult);
            devInfo->netResultPtr->setModified();
          } // use existing data
          else {
            std::string netResultSaved = devInfo->netResultPtr->get();
            file << "netResult: " + netResultSaved + "\n";
          }
          // new burnIn result
          if(burnInResult.length() > 0) {
            file << "burnInResult: " + burnInResult + "\n";
            devInfo->burnInResultPtr->set(burnInResult);
            devInfo->burnInResultPtr->setModified();
          } // use existing data
          else {
            std::string burnInResultSaved = devInfo->burnInResultPtr->get();
            file << "burnInResult: " + burnInResultSaved + "\n";
          }
          file.close();
          fs::permissions(mfgInfoFileName, fs::perms::others_read | fs::perms::owner_read);
          usleep(1000);// take a short nap here to prevent read/writing this file too fast and allow permission change.

        } // end of file update
        else { 
          printf("nothing to update in the file\n");
        }
        // finished, reset the key
        techSmPtr->set("0");
      }
      else if (key.compare(upTimeKey) == 0) { // matches! reset the uptime
        if (fs::exists(upTimeFileName) == true) { // reset time if file exists
          fs::permissions(upTimeFileName, fs::perms::owner_write);
          std::ofstream fileWrite;
          fileWrite.open(upTimeFileName);
          fileWrite << "This file is generated automatically; don't modify. UpTime increments every 1-minute\n";
          fileWrite << "upTime: 0\n";
          fileWrite.close();
        }
        // finished, reset the key
        techSmPtr->set("0");
      }
      else {
        // wrong key, reset it
        techSmPtr->set("");
      }
    }
    #endif // __ARM_ARCH
  }
}