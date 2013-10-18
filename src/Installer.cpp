
#include "Installer.hpp"

namespace cloudos {
namespace installer {
  
  /**
   * FIXME TODO => check system capabilities first (HDD space, KVM ability, amount of RAM)
   */
  
  Installer::Installer() {
    c_settings_directory      = "/etc/cloudos/installer/settings";
    c_temp_directory          = "/tmp/cloudos/installer";
    c_temp_settings_directory = c_temp_directory/"settings";
    c_temp_mountpoint         = c_temp_directory/"systemToBeInstalled";
    
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
    c_storage_config   = tools::StorageLocalConfigPointer( new config::os::InstallerDisk );
    
    c_neutron_config   = boost::shared_ptr<config::openstack::NeutronServer>( new config::openstack::NeutronServer );
    
    createTempDirs();
    
    loadSettingsFromFilesystem();
  }
  
  Installer::~Installer() {
    // clean up temp directories
    fs::remove_all(c_temp_settings_directory);
    
    // 
    // check, if our "systemToBeInstalled" is mounted
    // 
    std::string mountpoint_cmd = "/bin/mountpoint";
    std::vector<std::string> mountpoint_args;
    mountpoint_args.push_back(""); // the first arg has to empty
    mountpoint_args.push_back( c_temp_mountpoint.string() );
    
    ps::context moountpoint_cxt;
    moountpoint_cxt.stdout_behavior = ps::silence_stream();
    ps::child mountpoint_exe = ps::launch(mountpoint_cmd, mountpoint_args, moountpoint_cxt);
    
    ps::status mountpoint_status = mountpoint_exe.wait();
    if( mountpoint_status.exit_status() == 0 ) { // == is mounted
      umount( c_temp_mountpoint.c_str() );
    }
  }
  
  unsigned int Installer::run() {
    ui::DialogUserDecision return_value;
    
    //std::vector<WIZZARD_SCREEN_TYPE>::iterator wi = c_wizzard_order.begin();
    int last_element = c_wizzard_order.size() - 1;
    short int dialog_flags;
    for( int i = 0; i <= last_element; ) {
      dialog_flags = ui::NO_FLAG;
      if( i != 0 ) {
        dialog_flags = dialog_flags | ui::SHOW_BACK_BTN;
      }
      if( i == last_element ) {
        dialog_flags = dialog_flags | ui::SHOW_FINISHING_BTN;
      }
      //dialog_flags = ( wi != c_wizzard_order.begin() ); // if not first, there is a previous dialog
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
          last_element = c_wizzard_order.size() - 1; // c_wizzard_order may be changed
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
          std::cerr << "Error in " << __FILE__ << " Line " << __LINE__ << " Function " << __FUNCTION__ << std::endl;
          return 1;
      }
      
      if( return_value == ui::DIALOG_DECISION_BTN_BACK ) {
        --i;
      } else if( return_value == ui::DIALOG_DECISION_BTN_NEXT ) {
        ++i;
      } else {
        i = last_element+1;
      }
    }
    
    //} while( wi != c_wizzard_order.end() );
    if( return_value == ui::DIALOG_DECISION_BTN_NEXT ) {
      return install();
    } else {
      return return_value;
    }
  }
  
  void Installer::loadSettingsFromFilesystem() {
    // language and gernal settings
    tools::System::readMessage( c_settings_directory / "system.proto.conf", c_system_config );
    
    // Installer configuration
    tools::System::readMessage( c_settings_directory / "installer.proto.conf", c_installer_config );
    
    // Local Disk configuration
    tools::System::readMessage( c_settings_directory / "storage.proto.conf", c_storage_config );
    
    // NIC configuration and routes/dns
    tools::NetworkInterfaceConfigPointer nic( new config::os::NetworkInterface );
    tools::System::readMessage( c_settings_directory / "network.proto.conf", nic );
    c_network_config.clear();
    c_network_config.insert(nic);
    
    // 
    // OpenStack
    // 
    
    // Neutron
    tools::System::readMessage( c_settings_directory / "neutron.proto.conf", c_neutron_config );
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
    dialog->setSettings( c_network_config );
    
    dialog->show();
    //c_network_config = dialog->getSettings();
    c_selected_interface = dialog->getSelectedInterface();
    return dialog->getPushedBtn();
  }
  
  ui::DialogUserDecision Installer::showScreenStorage ( short int p_dialog_flags ) {
    ui::DialogStoragePointer dialog( new ui::DialogStorage( p_dialog_flags ) );
    dialog->setDialogTitle("Storage space for Installation");
    dialog->setSettings( c_storage_config );
    
    dialog->show();
    c_storage_config = dialog->getSettings();
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
    
    // calculate the lvm service data volume size
    int service_data_size = c_storage_config->size() - 6;
    // use rest of available storage, if there are less than 20GB available
    if( service_data_size > 20 ) { // if there are more than 20GiB available, use 80% of the storage
      service_data_size = (int) service_data_size * 0.8;
    }
    
    std::stringstream ss;
    ss << service_data_size;
    std::string sds = ss.str();
    
    std::string part_mode = "msdos"; // partition table layout
    if( c_storage_config->size() > 2000 ) {
      part_mode = "gpt";
    }
    
    c_system_config->set_hostname("cloudos-beta1");
    //c_neutron_config->set_public_ip_pool("10.150.0.0/24");
    
    
    std::string main_ip, default_interface;
    default_interface = c_selected_interface;
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
    
    // env to build
    //   - SERVICE_DATA_SIZE
    //   - LOGFILE
    //   - DEST_DISK
    //   - HOST_IP
    //   - 
    //   - INSTALL_DIR
    // 
    // 
    // 
    // 
    // 
    // 
    
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
    std::string install_sh( installer_location + "install.sh" );
    std::vector<std::string> install_sh_args;
    install_sh_args.push_back(""); // the first arg has to empty
    install_sh_args.push_back( c_temp_mountpoint.string() );
    
    ps::context install_sh_ctx;
    install_sh_ctx.stdout_behavior = ps::silence_stream();
    install_sh_ctx.environment = ps::self::get_environment(); 
    //install_sh_ctx.environment.erase("TMP"); 
    
    // 
    //  Calculate IP addresses for HOST ENV
    // 
    
    cloudos::tools::IPAddress ip_origin, ip_gateway, ip_host, ip_pool_start, ip_pool_end;
    
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
    
    // 
    // P R E P A R E   H O S T   E N V
    // 
    std::string host_prepare_sh( installer_location + "check_disk.sh" );
    install_sh_ctx.environment["DEST_DISK"] = c_storage_config->device_path();
    if( ps::launch(host_prepare_sh, install_sh_args, install_sh_ctx).wait().exit_status() != 0 ) {
      std::cerr << "Faild to prepare the host... see /tmp/check_disk.log for more information" << std::endl;
      return 1;
    }
    
    std::string host_install_dir("/tmp/cloudos/installer/host-disk");
    install_sh_ctx.environment.insert(ps::environment::value_type("DEST_DISK",                c_storage_config->device_path()));
    install_sh_ctx.environment.insert(ps::environment::value_type("LOGFILE",                  "/tmp/installer.log"));
    install_sh_ctx.environment.insert(ps::environment::value_type("PW_FILE",                  "/tmp/pws.txt"));
    install_sh_ctx.environment.insert(ps::environment::value_type("INSTALL_DIR",              host_install_dir));
    install_sh_ctx.environment.insert(ps::environment::value_type("SERVICE_DATA_SIZE",        sds));
    
    install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_PART_MODE",         part_mode));
    install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_HOST_IP",           main_ip));
    install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_KEYMAP",            c_system_config->keyboard_layout()));
    install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_CHARSET",           c_system_config->locale_charset()));
    install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_HOSTNAME",          c_system_config->hostname()));
    install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_PRIMARY_INTERFACE", default_interface));
    install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_PUBLIC_IP_POOL",    c_neutron_config->public_ip_pool()));
    
    if( c_installer_config->install_keystone() ) {
      install_sh_ctx.environment.insert(ps::environment::value_type("HOST_MODE",              "on"));
      
      install_sh_ctx.environment["CONFIG_IP_GATEWAY"] = ip_gateway.ip();
      install_sh_ctx.environment["CONFIG_IP_HOST"]    = ip_host.cidr();
      install_sh_ctx.environment["CONFIG_IP_POOL_START"] = ip_pool_start.ip();
      install_sh_ctx.environment["CONFIG_IP_POOL_END"] = ip_pool_end.ip();
    }
    
    ps::child install_sh_exe = ps::launch(install_sh, install_sh_args, install_sh_ctx);
    
    
    // 
    // M A N A G E M E N T
    // 
    ps::context mgt_install_sh_ctx;
    if( c_installer_config->install_management() ) {
      // 
      // P R E P A R E   M A N A G E M E N T   D I S K
      //
      tools::StorageLocalConfigPointer mgt_storage_config( new config::os::InstallerDisk );
      mgt_storage_config->set_device_path( "/dev/loop0" );
      
      std::string create_vdisk_sh( installer_location + "create_vdisk.sh" );
      main_ip = "";
      mgt_install_sh_ctx.stdout_behavior = ps::silence_stream();
      mgt_install_sh_ctx.environment = ps::self::get_environment(); 
      
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("MGT_MODE",                 "on"));
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("DEST_DISK",                mgt_storage_config->device_path()));
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("LOGFILE",                  "/tmp/installer_mgt.log"));
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("PW_FILE",                  "/tmp/pws_mgt.txt"));
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("INSTALL_DIR",              "/tmp/cloudos/installer/mgt-disk"));
      
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_PART_MODE",         part_mode));
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_IP_HOST",           ip_host.cidr()));
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_DST_IP",            ip_host.cidr()));
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_KEYMAP",            c_system_config->keyboard_layout()));
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_CHARSET",           c_system_config->locale_charset()));
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_HOSTNAME",          c_system_config->hostname()));
      mgt_install_sh_ctx.environment["CONFIG_PRIMARY_INTERFACE"] = "eth0";
      
      ps::child create_vdisk_sh_exe = ps::launch(create_vdisk_sh, install_sh_args, mgt_install_sh_ctx);
      ps::status create_vdisk_sh_status = create_vdisk_sh_exe.wait();
      
      // partitioning disk
      if( create_vdisk_sh_status.exit_status() == 0 ) {
        /*tools::StorageLocalPointer mgt_storage( new tools::StorageLocal );
        mgt_storage->setSettings(mgt_storage_config);
        if( mgt_storage->createPartitionTable() == false ) {
          std::cerr << "error while creating the partition table - abort" << std::endl;
          return 1;
        }
        if( mgt_storage->addPartition(100, 'M') == false ) {
          std::cerr << "error while creating the swap partition - abort" << std::endl;
          return 1;
        }
        if( mgt_storage->addPartition(0) == false ) {
          std::cerr << "error while creating the root partition - abort" << std::endl;
          return 1;
        }
        mgt_storage->applyToSystem();*/
        
        std::string install_mgt( installer_location + "install.sh" );
        ps::child install_mgt_exe = ps::launch(install_mgt, install_sh_args, mgt_install_sh_ctx);
        
        ps::status install_mgt_status = install_mgt_exe.wait();
      }
      
      
    }
    
    ps::status install_sh_status = install_sh_exe.wait();
    
    if( install_sh_status.exit_status() == 0 && c_installer_config->install_management() ) {
      // 
      // Copy management to host and activate it
      // 
      std::string mgt_finish_sh( installer_location + "finish_management.sh" );
      mgt_install_sh_ctx.environment.insert(ps::environment::value_type("CONFIG_HOST_ROOT", host_install_dir));
      ps::child mgt_finish_sh_exe = ps::launch(mgt_finish_sh, install_sh_args, mgt_install_sh_ctx);
      
      mgt_finish_sh_exe.wait();
    }
    
    return install_sh_status.exit_status();
  }

  void Installer::createTempDirs() {
    fs::create_directories(c_temp_settings_directory); // will create c_temp_directory too
    fs::create_directory(c_temp_mountpoint);
  }
  
  std::vector<WIZZARD_SCREEN_TYPE>::iterator Installer::isScreenSet( WIZZARD_SCREEN_TYPE p_screen ) {
    std::vector<WIZZARD_SCREEN_TYPE>::iterator i = c_wizzard_order.begin();
    while( i != c_wizzard_order.end() && *i != p_screen ) {
      ++i;
    }
    return i;
  }
  
  void Installer::configureScreensBasedOnModuleSelection() {
    
    //eraseOrInsertScreen( KEYSTONE_SERVER_MAIN, c_installer_config->install_keystone() );
    //eraseOrInsertScreen( GLANCE_SERVER_MAIN,   c_installer_config->install_glance()   );
    eraseOrInsertScreen( SCREEN_NEUTRON_SERVER_MAIN,  c_installer_config->install_quantum()  );
    
  }
  
  inline void Installer::eraseOrInsertScreen ( WIZZARD_SCREEN_TYPE p_type, bool p_config_value ) {
    std::vector<WIZZARD_SCREEN_TYPE>::iterator pos = isScreenSet( p_type );
    
    // if pos != end AND config true,  do nothing
    // if pos != end AND config false, do something
    // if pos == end AND config true,  do something
    // if pos == end AND config false, do nothing
    if( (pos != c_wizzard_order.end()) xor p_config_value ) {
      if( p_config_value ) {
        c_wizzard_order.push_back( p_type );
      } else {
        c_wizzard_order.erase( pos );
      }
    }
  }

}} // namespace cloudos::installer
