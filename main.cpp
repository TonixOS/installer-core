#include <iostream>

#define YUILogComponent "cloudos-installer"
#include <yui/YUILog.h>

#include "src/Installer.hpp"

#include <cloudos/tools/StorageLocal.hpp>

using namespace cloudos;

int main(int argc, char **argv) {
  
  unsigned int retval = 0;
  
  YUILog::setLogFileName("/tmp/installer.tool.log");
  
  try {
  
    cloudos::installer::Installer *installer = new cloudos::installer::Installer();
    retval = installer->run();
    delete installer;
    
  
  } catch(std::exception& e) {
    YUILog::milestone(YUILogComponent, __FILE__, __LINE__, __FUNCTION__) << "std::exception: " << e.what() << std::endl;
  }
  
  if( retval == 1 ) {
    YUILog::milestone(YUILogComponent, __FILE__, __LINE__, __FUNCTION__) << "Installation process aborted, requested by user" << std::endl;
  }
  
  return retval;
}
