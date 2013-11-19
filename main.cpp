
#include <boost/shared_ptr.hpp>

#define YUILogComponent "cloudos-installer"
#include <yui/YUILog.h>

#include "src/Installer.hpp"

#include <cloudos/tools/StorageLocal.hpp>
#include <cloudos/ui/DialogInstallerFinished.hpp>
#include <cloudos/system/Command.hpp>

using namespace cloudos;

int main(int argc, char **argv) {
  
  unsigned int retval = 0;
  
  YUILog::setLogFileName("/tmp/installer.tool.log");
  
  // disable kernel messages on console
  system::Command dmesg;
  dmesg << "/bin/dmesg" << "--console-off";
  dmesg.waitUntilFinished();
  
  try {
    
    boost::shared_ptr<installer::Installer> installer( new installer::Installer() );
    retval = installer->run();
    
    
    ui::DialogInstallerFinishedPointer dialog = ui::DialogInstallerFinishedPointer( new ui::DialogInstallerFinished( ui::SHOW_REBOOT_BTN ) );
    dialog->setDialogTitle("Interactive Cloud OS Installer FINISHED");
    dialog->setManagementIP( installer->getManagementIP() );
    
    dialog->show();
    short btn = dialog->getPushedBtn();
    
    system::Command sync("/bin/sync");
    sync.waitUntilFinished();
    
    dmesg.clearArguments();
    dmesg << "--console-on";
    dmesg.waitUntilFinished();
    
    if( btn == ui::DIALOG_DECISION_BTN_NEXT ) {
      system::Command systemctl;
      systemctl << "/usr/bin/systemctl" << "reboot";
      systemctl.waitUntilFinished();
    }
  
  } catch(std::exception& e) {
    YUILog::milestone(YUILogComponent, __FILE__, __LINE__, __FUNCTION__) << "std::exception: " << e.what() << std::endl;
  }
  
  if( retval == 1 ) {
    YUILog::milestone(YUILogComponent, __FILE__, __LINE__, __FUNCTION__) << "Installation process aborted, requested by user" << std::endl;
  }
  
  return retval;
}
