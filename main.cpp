#include <iostream>

#include <stdlib.h>

#define YUILogComponent "cloudos-installer"
#include <yui/YUILog.h>

#include "src/Installer.hpp"

#include <cloudos/tools/StorageLocal.hpp>
#include <cloudos/ui/DialogInstallerFinished.hpp>

using namespace cloudos;

int main(int argc, char **argv) {
  
  unsigned int retval = 0;
  
  YUILog::setLogFileName("/tmp/installer.tool.log");
  
  system("/bin/dmesg --console-off"); // disable kernel messages on console
  
  try {
  
    cloudos::installer::Installer *installer = new cloudos::installer::Installer();
    retval = installer->run();
    delete installer;
    
    ui::DialogInstallerFinishedPointer dialog = ui::DialogInstallerFinishedPointer( new ui::DialogInstallerFinished( ui::SHOW_REBOOT_BTN ) );
    dialog->setDialogTitle("Interactive Cloud OS Installer FINISHED");
    
    dialog->show();
    short btn = dialog->getPushedBtn();
    
    if( btn == ui::DIALOG_DECISION_BTN_NEXT ) {
      system("/bin/sync");
      system("/usr/bin/systemctl reboot");
    }
  
  } catch(std::exception& e) {
    YUILog::milestone(YUILogComponent, __FILE__, __LINE__, __FUNCTION__) << "std::exception: " << e.what() << std::endl;
  }
  
  if( retval == 1 ) {
    YUILog::milestone(YUILogComponent, __FILE__, __LINE__, __FUNCTION__) << "Installation process aborted, requested by user" << std::endl;
  }
  
  system("/bin/dmesg --console-on");
  
  return retval;
}
