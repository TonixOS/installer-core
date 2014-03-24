/*
 *   libcloudos
 *   Copyright (C) 2014  Tony Wolf <wolf@os-forge.net>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/



#include "Installer.hpp"

#include <cloudos/ui/DialogInstallerFinished.hpp>

namespace cloudos {
namespace installer {
  
  /**
   * FIXME TODO => check system capabilities first (HDD space, KVM ability, amount of RAM)
   */
  
  Installer::Installer() {
    c_temp_directory          = "/tmp/cloudos/installer";
    c_temp_settings_directory = c_temp_directory/"settings";
    
    // init config paths
    c_settings_directory    = "/etc/cloudos/installer/settings";
    c_config_file_system    = c_settings_directory / "system.proto.conf";
    c_config_file_network   = c_settings_directory / "network.proto.conf";
    c_config_file_storage   = c_settings_directory / "storage.proto.conf";
    c_config_file_neutron   = c_settings_directory / "neutron.proto.conf";
    c_config_file_installer = c_settings_directory / "installer.proto.conf";
    
    // create the order of main wizzard screens
    // NOTE: change order of screens HERE
    // we will just iterate over the vector later, so order matters
    //c_wizzard_order.push_back( SCREEN_LANGUAGE );
    c_wizzard_order.push_back( SCREEN_WELCOME );
    //c_wizzard_order.push_back( SCREEN_MODULE_SELECTION );
    c_wizzard_order.push_back( SCREEN_STORAGE );
    c_wizzard_order.push_back( SCREEN_NEUTRON_SERVER_MAIN );
    c_wizzard_order.push_back( SCREEN_NETWORK );
    //c_wizzard_order.push_back( SYSTEM );
    
    c_system_config    = boost::shared_ptr<config::os::System>( new config::os::System );
    c_installer_config = boost::shared_ptr<config::os::Installer>( new config::os::Installer );
    c_neutron_config   = boost::shared_ptr<config::openstack::NeutronServer>( new config::openstack::NeutronServer );
    
    if( loadSettingsFromFilesystem() == false ) {
      c_is_valid = false;
    }
    
    // start creating our system template while we are asking the user to configure his system
    c_start_install_template_success = startCreateInstallTemplate();
  }
  
  Installer::~Installer() {
    
    if( c_core_install_thread && c_core_install_thread->joinable() ) {
      //TODO: interrupt the running thread and clean up the mess...
      c_core_install_thread->join();
    }
    
    // clean up temp directories
    fs::remove_all(c_temp_settings_directory);
  }
  
  unsigned int Installer::run() {
    ui::DialogUserDecision return_value;
    
    int last_element = c_wizzard_order.size() - 1;
    short int dialog_flags;
    for( int i = 0; i <= last_element; ) {
      dialog_flags = ui::NO_FLAG;
      if( i != 0 ) { // if we are not at our first dialog element...
        dialog_flags = dialog_flags | ui::SHOW_BACK_BTN;
      }
      if( i == last_element ) {
        dialog_flags = dialog_flags | ui::SHOW_FINISHING_BTN;
      }
      
      switch( c_wizzard_order[i] ) {
        case SCREEN_LANGUAGE:
          return_value = showScreenLanguage( dialog_flags );
          break;
        case SCREEN_WELCOME:
          return_value = showScreenWelcome( dialog_flags );
          break;
        case SCREEN_MODULE_SELECTION:
          // TODO let the user configure this
          //return_value = showScreenModuleSelection( dialog_flags );
          configureScreensBasedOnModuleSelection();
          last_element = c_wizzard_order.size() - 1; // c_wizzard_order might have been changed
          break;
        case SCREEN_STORAGE:
          return_value = showScreenStorage( dialog_flags );
          break;
        case SCREEN_NETWORK:
          return_value = showScreenNetwork( dialog_flags );
          break;
        case SCREEN_SYSTEM:
          break;
        case SCREEN_KEYSTONE_SERVER_MAIN:
          break;
        case SCREEN_GLANCE_SERVER_MAIN:
          break;
        case SCREEN_CINDER_SERVER_MAIN:
          break;
        case SCREEN_NOVA_SERVER_MAIN:
          break;
        case SCREEN_NEUTRON_SERVER_MAIN:
          return_value = showScreenNeutronServerMain( dialog_flags );
          break;
        default:
          LOG_E() << "invalid screen requested... screen number: " << c_wizzard_order[i];
          return 1;
      }
      
      switch( return_value ) {
        case ui::DIALOG_DECISION_BTN_BACK:
          --i;
          break;
        case ui::DIALOG_DECISION_BTN_NEXT:
          ++i;
          break;
        default:
          i = last_element+1;
      }
      
    }
    
    LOG_I() << "user dialogs finished... got return_value: " << return_value;
    
    return ( return_value == ui::DIALOG_DECISION_BTN_NEXT ) ? install() : return_value;
  }
  
  bool Installer::loadSettingsFromFilesystem() {
    // language and gernal settings
    tools::System::readMessage( c_settings_directory / "system.proto.conf", c_system_config );
    
    // Installer configuration
    tools::System::readMessage( c_settings_directory / "installer.proto.conf", c_installer_config );
    
    // Local Disk configuration
    tools::System::readMessage( c_settings_directory / "storage.proto.conf", c_storage_config );
    
    // NIC configuration and routes/dns
    tools::NetworkInterface nic;
    if( fs::exists(c_config_file_network) && nic.loadConfig(c_config_file_network) == false ) {
      LOG_E() << "loading nic config failed... Abort!";
      return false;
    }
    
    // 
    // OpenStack
    // 
    
    // Neutron
    tools::System::readMessage( c_settings_directory / "neutron.proto.conf", c_neutron_config );
    
    return true;
  }
  
  void Installer::checkIsValid() {
    //FIXME check requirements
    c_is_valid = c_start_install_template_success;
  }

  bool Installer::startCreateInstallTemplate() {
    using namespace cloudos::system;
    fs::path tar_file("/usr/share/cloudos/installer/basesystem-rpms.tar");
    c_core_install = InstallerCoreSystemPointer( new InstallerCoreSystem( c_temp_directory / "template_core") );
    if( c_core_install->setPackagesTarFile( std::move(tar_file) ) == false ) {
      LOG_E() << "setting packages tar file failed... Abort!";
      return false;
    }
    
    if( c_core_install->isValid() == false ) {
      LOG_E() << "core installer object is invalid... Abort!";
      return false;
    }
    
    c_core_install_thread = ThreadPointer(new boost::thread(boost::bind(&InstallerCoreSystem::setup, c_core_install.get())));
    
    return true;
  }
  
  ui::DialogUserDecision Installer::showScreenLanguage( short int p_dialog_flags ) {
    ui::DialogLanguagePointer dialog = ui::DialogLanguagePointer( new ui::DialogLanguage( p_dialog_flags ) );
    dialog->setDialogTitle("Change your Settings");
    dialog->setSettings( c_system_config );
    
    dialog->show();
    c_system_config = dialog->getSettings();
    return dialog->getPushedBtn();
  }

  ui::DialogUserDecision Installer::showScreenWelcome( short int p_dialog_flags ) {
    ui::DialogWelcomePointer dialog = ui::DialogWelcomePointer( new ui::DialogWelcome( p_dialog_flags ) );
    dialog->setDialogTitle("Interactive Cloud OS Installer");
    dialog->setSettings( c_settings_directory / "welcome.txt" );
    
    dialog->show();
    return dialog->getPushedBtn();
  }
  
  ui::DialogUserDecision Installer::showScreenModuleSelection ( short int p_dialog_flags ) {
    ui::DialogModuleSelectionPointer dialog( new ui::DialogModuleSelection( p_dialog_flags ) );
    dialog->setDialogTitle("Select Software Modules");
    dialog->setSettings( c_installer_config );
    
    dialog->show();
    c_installer_config = dialog->getSettings();
    return dialog->getPushedBtn();
  }
  
  ui::DialogUserDecision Installer::showScreenNetwork ( short int p_dialog_flags ) {
    ui::DialogNetworkPointer dialog( new ui::DialogNetwork( p_dialog_flags ) );
    dialog->setDialogTitle("Select Network Adapter");
    
    //TODO use network config file...
    dialog->setConfigs( std::move(tools::System::getAvailableInterfaces()) );
    
    if( c_network_config ) {
      dialog->setSelectedInterface( c_network_config->index() );
    }
    
    dialog->show();
    
    c_network_config = dialog->getSelectedInterface();
    return dialog->getPushedBtn();
  }
  
  ui::DialogUserDecision Installer::showScreenStorage ( short int p_dialog_flags ) {
    ui::DialogStoragePointer dialog( new ui::DialogStorage( p_dialog_flags ) );
    dialog->setDialogTitle("Storage space for Installation");
    if( c_storage_config ) {
      LOG_D() << "got selected disk from previous session... configure it...";
      dialog->setSelectedDisk( c_storage_config->index() );
    }
    
    dialog->show();
    c_storage_config = dialog->getSelectedDisk();
    return dialog->getPushedBtn();
  }


  
  ui::DialogUserDecision Installer::showScreenNeutronServerMain ( short int p_dialog_flags ) {
    ui::DialogNeutronServerMainPointer dialog = ui::DialogNeutronServerMainPointer( new ui::DialogNeutronServerMain( p_dialog_flags ) );
    dialog->setDialogTitle("Neutron (Network) Server Settings");
    dialog->setSettings( c_neutron_config );
    
    dialog->show();
    c_neutron_config = dialog->getSettings();
    return dialog->getPushedBtn();
  }

  unsigned int Installer::install() {
    using namespace cloudos::system;
    
    LOG_I() << "starting setup after the user finished his settings";
    
    // show the process screen
    ui::DialogInstallerProcessPointer install_process_dialog = ui::DialogInstallerProcessPointer( new ui::DialogInstallerProcess( ui::SHOW_NO_BUTTONS ) );
    install_process_dialog->setDialogTitle("Interactive Cloud OS Installation");
    install_process_dialog->show();
    
    //TODO check if installation was successful
    // wait until our template finished...
    if( c_core_install_thread ) {
      LOG_I() << "join core installer...";
      c_core_install_thread->join();
    }
    
    //TODO: implement host install
    if( c_installer_config->install_keystone() ) {
      LOG_I() << "starting host system INSTALLER...";
      c_host_install = InstallerHostSystemPointer( new InstallerHostSystem( c_temp_directory / "host" ) );
      
      // set core template, if we got one...
      if( c_core_install_thread ) {
        c_host_install->setCoreSystem( c_core_install );
      }
      
      // set configs
      c_host_install->setConfigSystem(           c_system_config    );
      c_host_install->setConfigNetwork(          c_network_config   );
      c_host_install->setConfigStorage(          c_storage_config   );
      c_host_install->setConfigInstaller(        c_installer_config );
      c_host_install->setConfigOpenStackNeutron( c_neutron_config   );
      
      // TODO: don't always install the management vm
      c_mgt_install = InstallerManagementSystemPointer( new InstallerManagementSystem( c_temp_directory / "mgt" ) );
      
      if( c_core_install_thread ) {
        c_mgt_install->setCoreSystem( c_core_install );
      }
      
      c_mgt_install->setConfigSystem( c_system_config );
      c_mgt_install->setConfigNetwork( c_network_config );
      c_mgt_install->setConfigInstaller( c_installer_config );
      c_host_install->setManagementInstaller( c_mgt_install );
      
      // launch host installer
      c_host_install_thread = ThreadPointer(new boost::thread(boost::bind(&InstallerHostSystem::setup, c_host_install.get())));
    }
    
    
    // merge host installer, if used...
    if( c_host_install_thread ) {
      LOG_I() << "join back host INSTALLER...";
      c_host_install_thread->join();
      //TODO check if installation was successful
    }
    
    ui::DialogInstallerFinishedPointer dialog = ui::DialogInstallerFinishedPointer( new ui::DialogInstallerFinished( ui::SHOW_REBOOT_BTN ) );
    dialog->setDialogTitle("Interactive Cloud OS Installer FINISHED");
    //dialog->setManagementIP( installer->getManagementIP() );
    
    dialog->show();
    ui::DialogUserDecision btn = dialog->getPushedBtn();
    
    // return specific return value...
    return btn;
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    // calculate the lvm service data volume size
    // size is stored in GB, and we alocate 6GB for swap/root
    // FIXME: we don't need service_data_size anymore... remove it, but check if there are dependencies within installer.sh
    //int service_data_size = c_storage_config->size() - 6;
    // use rest of available storage, if there are less than 20GB available
    //if( service_data_size > 20 ) { // if there are more than 20GiB available, use 80% of the storage
    //  service_data_size = (int) service_data_size * 0.8;
    //}
    /*
    std::stringstream ss;
    ss << service_data_size;
    std::string sds = ss.str();
    
    std::string part_mode = "msdos"; // partition table layout
    if( c_storage_config->size() > 2000 ) {
      part_mode = "gpt";
    }
    */
    //c_system_config->set_hostname("cloudos-beta1");
    
    /*
    std::string main_ip, default_interface;
    default_interface = c_selected_interface;
    
    // find the right network config
    boost::shared_ptr< config::os::NetworkInterface > network_config;
    for( auto& config : c_network_config ) {
      if( config->name() == c_selected_interface ) {
        network_config = config;
      }
    }
    if( !network_config ) {
      //FIXME... report error
    }
    
    c_network_config;*/
    /*
    std::set<tools::NetworkInterfaceConfigPointer>::iterator nit;
    for( nit = c_network_config.begin(); nit != c_network_config.end(); ++nit ) {
      tools::NetworkInterface nic;
      nic.setSettings( *nit );
      if(nic.hasRoute( std::string("0.0.0.0/0") )) {
        tools::IPAddress ip;
        ip.setValue( nic.getSettings()->ip_cidr(0) );
        main_ip = ip.ip();
        
        default_interface = nic.getSettings()->name();
      }
    }*/
    /*
    // TODO: use command description and command vector.count() for Process Screen
    install_process_dialog->setNextState();
    // 
    // P R E P A R E   H O S T   D I S K
    // 
    tools::StorageLocalPointer storage( new tools::StorageLocal );
    storage->setSettings(c_storage_config);
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
    
    std::string installer_location("/usr/share/cloudos/installer/");
    */
    // 
    //  Calculate IP addresses for HOST ENV
    // 
    /*
    tools::IPAddress ip_origin, ip_gateway, ip_host, ip_pool_start, ip_pool_end, ip_mgt;
    
    ip_origin.setValue( c_neutron_config->public_ip_pool() );
    
    // get the right base
    ip_origin.setValue( ip_origin.netaddress() + '/' + ip_origin.prefix() );
    
    // our gateway
    ip_gateway.setValue( ip_origin.cidr() );
    ip_gateway.setIncrementValue(1);
    
    // our own host
    ip_host.setValue( ip_gateway.cidr() );
    ip_host.setIncrementValue(1);
    
    ip_pool_start.setValue( ip_host.cidr() );
    ip_pool_start.setIncrementValue(1);
    
    ip_pool_end.setValue( ip_origin.broadcast() );
    ip_pool_end.setIncrementValue(-1);
    
    // our web management VM
    ip_mgt.setValue( ip_host.cidr() );
    ip_mgt.setIncrementValue(2); // increment 2, as the SDN router is between
    c_mgt_ip = ip_mgt.ip();*/
    /*
    // 
    // P R E P A R E   H O S T   E N V
    // 
    system::Command cmd_check_disk( installer_location + "check_disk.sh" );
    cmd_check_disk.setDescription("check host storage environment");
    cmd_check_disk.setEnvironmentVar("DEST_DISK", c_storage_config->index());
    if( cmd_check_disk.waitUntilFinished() != 0 ) {
      std::cerr << "Faild to prepare the host... see /tmp/check_disk.log for more information" << std::endl;
      return 1;
    }
    
    system::Command cmd_install_sh;
    cmd_install_sh.setDescription("install host system");
    cmd_install_sh << installer_location + "install.sh" << c_temp_mountpoint.string();
    
    cmd_install_sh.setEnvironmentVar("DEST_DISK",                c_storage_config->index());
    cmd_install_sh.setEnvironmentVar("LOGFILE",                  "/tmp/installer.log");
    cmd_install_sh.setEnvironmentVar("PW_FILE",                  "/tmp/pws.txt");
    cmd_install_sh.setEnvironmentVar("INSTALL_DIR",              "/tmp/cloudos/installer/host-disk");
    cmd_install_sh.setEnvironmentVar("SERVICE_DATA_SIZE",        sds);
    cmd_install_sh.setEnvironmentVar("CONFIG_PART_MODE",         part_mode);
    cmd_install_sh.setEnvironmentVar("CONFIG_HOST_IP",           main_ip);
    cmd_install_sh.setEnvironmentVar("CONFIG_KEYMAP",            c_system_config->keyboard_layout());
    cmd_install_sh.setEnvironmentVar("CONFIG_CHARSET",           c_system_config->locale_charset());
    cmd_install_sh.setEnvironmentVar("CONFIG_HOSTNAME",          c_system_config->hostname());
    cmd_install_sh.setEnvironmentVar("CONFIG_PRIMARY_INTERFACE", default_interface);
    cmd_install_sh.setEnvironmentVar("CONFIG_PUBLIC_IP_POOL",    c_neutron_config->public_ip_pool());
    
    if( c_installer_config->install_keystone() ) {
      cmd_install_sh.setEnvironmentVar("HOST_MODE",            "on");
      cmd_install_sh.setEnvironmentVar("CONFIG_IP_GATEWAY",    ip_gateway.ip());
      cmd_install_sh.setEnvironmentVar("CONFIG_IP_HOST",       ip_host.ip());
      cmd_install_sh.setEnvironmentVar("CONFIG_IP_POOL_START", ip_pool_start.ip());
      cmd_install_sh.setEnvironmentVar("CONFIG_IP_POOL_END",   ip_pool_end.ip());
      cmd_install_sh.setEnvironmentVar("CONFIG_IP_MGT",        ip_mgt.ip());
    }
    
    // 
    // okay, now it's time to wait, until the tar file is copied
    // 
    c_cmd_copy_tar.waitUntilFinished();
    
    install_process_dialog->setNextState();
    cmd_install_sh.start();
    
    // 
    // M A N A G E M E N T
    // 
    if( c_installer_config->install_management() ) {
      // 
      // P R E P A R E   M A N A G E M E N T   D I S K
      //
      tools::StorageLocalConfigPointer mgt_storage_config( new config::os::InstallerDisk );
      mgt_storage_config->set_device_path( "/dev/loop0" );
      
      system::Command cmd_vdisk_create( installer_location + "create_vdisk.sh" );
      cmd_vdisk_create.setDescription("prepare management VM disk");
      cmd_vdisk_create.setEnvironment( cmd_install_sh.getEnvironment() );
      cmd_vdisk_create.setEnvironmentVar("MGT_MODE",                 "on");
      cmd_vdisk_create.setEnvironmentVar("DEST_DISK",                mgt_storage_config->index());
      cmd_vdisk_create.setEnvironmentVar("LOGFILE",                  "/tmp/installer_mgt.log");
      cmd_vdisk_create.setEnvironmentVar("PW_FILE",                  "/tmp/pws_mgt.txt");
      cmd_vdisk_create.setEnvironmentVar("INSTALL_DIR",              "/tmp/cloudos/installer/mgt-disk");
      cmd_vdisk_create.setEnvironmentVar("CONFIG_IP_HOST",           ip_host.cidr());
      cmd_vdisk_create.setEnvironmentVar("CONFIG_DST_IP",            ip_host.cidr());
      cmd_vdisk_create.setEnvironmentVar("CONFIG_PRIMARY_INTERFACE", "eth0");
      cmd_vdisk_create.removeEnvironmentVar("HOST_MODE");
      
      // partitioning disk
      //if( cmd_vdisk_create.waitUntilFinished() == 0 ) {
        
        system::Command cmd_install_mgt( installer_location + "install.sh" );
        cmd_install_mgt.setDescription("install management VM system");
        cmd_install_mgt.setEnvironment( cmd_vdisk_create.getEnvironment() );
        install_process_dialog->setNextState();
        cmd_install_mgt.waitUntilFinished();
      //} // FIXME: report, if command failed...
    }
    
    unsigned short install_retval = cmd_install_sh.waitUntilFinished();
    
    if( install_retval == 0 && c_installer_config->install_management() ) {
      install_process_dialog->setNextState();
      // 
      // Copy management to host and activate it
      // 
      system::Command cmd_finish( installer_location + "finish_management.sh" );
      cmd_finish.setDescription("finish the management installation");
      cmd_finish.setEnvironmentVar("CONFIG_HOST_ROOT", "/tmp/cloudos/installer/mgt-disk");
      
      cmd_finish.waitUntilFinished();
      install_process_dialog->setNextState();
    }
    
    return install_retval;*/
  }
  
  Installer::ScreenTypeVector::iterator Installer::isScreenSet( WIZZARD_SCREEN_TYPE p_screen ) {
    ScreenTypeVector::iterator it = c_wizzard_order.begin(),
                               end = c_wizzard_order.end();
    while( it != end && *it != p_screen ) {
      ++it;
    }
    return it;
  }
  
  void Installer::configureScreensBasedOnModuleSelection() {
    
    //eraseOrInsertScreen( KEYSTONE_SERVER_MAIN, c_installer_config->install_keystone() );
    //eraseOrInsertScreen( GLANCE_SERVER_MAIN,   c_installer_config->install_glance()   );
    eraseOrInsertScreen( SCREEN_NEUTRON_SERVER_MAIN,  c_installer_config->install_quantum()  );
    
  }
  
  inline void Installer::eraseOrInsertScreen ( WIZZARD_SCREEN_TYPE p_type, bool p_enable_screen ) {
    ScreenTypeVector::iterator pos = isScreenSet( p_type ),
                               end = c_wizzard_order.end();
    
    // if pos != end AND config true,  do nothing
    // if pos != end AND config false, do something
    // if pos == end AND config true,  do something
    // if pos == end AND config false, do nothing
    if( pos != end && p_enable_screen == false ) {
      c_wizzard_order.push_back( p_type );
      return;
    }
    
    if( pos == end && p_enable_screen == true ) {
      c_wizzard_order.erase( pos );
      return;
    }
  }

}} // namespace cloudos::installer
