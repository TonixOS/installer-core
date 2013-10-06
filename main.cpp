#include <iostream>

// #define YUILogComponent "cloudos-installer"
// #include <yui/YUILog.h>

#include "src/Installer.hpp"

#include <cloudos/tools/StorageLocal.hpp>

using namespace cloudos;

int main(int argc, char **argv) {
  
  unsigned int retval = 0;
  
  //YUILog::setLogFileName("test.log");
  
  try {
  
    //cloudos::installer::Installer *installer = new cloudos::installer::Installer();
    //retval = installer->run();
    //delete installer;
    
    tools::StorageLocalConfigPointer config( new config::os::InstallerDisk );
    config->set_device_path( "/dev/vdb" );
    
    tools::StorageLocalPointer storage( new tools::StorageLocal );
    storage->setSettings(config);
    storage->getAvailableDisks();
    if( storage->createPartitionTable() == false ) {
      std::cerr << "error while creating the partition table - abort" << std::endl;
      return 1;
    }
    if( storage->addPartition(100, 'M') == false ) {
      std::cerr << "error while creating the swap partition - abort" << std::endl;
      return 1;
    }
    if( storage->addPartition(5) == false ) {
      std::cerr << "error while creating the root partition - abort" << std::endl;
      return 1;
    }
    if( storage->addPartition(0, 'G', true) == false ) { // apply rest of the disk for lvm
      std::cerr << "error while creating the srv partition - abort" << std::endl;
      return 1;
    }
    storage->applyToSystem();
    
  
  } catch(std::exception& e) {
    //YUILog::milestone(YUILogComponent, __FILE__, __LINE__, __FUNCTION__) << "std::exception: " << e.what() << std::endl;
  }
  
  if( retval == 1 ) {
    //YUILog::milestone(YUILogComponent, __FILE__, __LINE__, __FUNCTION__) << "Installation process aborted, requested by user" << std::endl;
  }
  
  return retval;
}
