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



#ifndef CLOUDOS_INSTALLER_INSTALLER_HPP__
#define CLOUDOS_INSTALLER_INSTALLER_HPP__

// general headers
#include <map>
#include <vector>
#include <string>
#include <sstream>

// #include <boost/program_options.hpp>
#include <boost/container/stable_vector.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/thread.hpp>

#include <cloudos/ui/Dialog.hpp>
#include <cloudos/ui/DialogLanguage.hpp>
#include <cloudos/ui/DialogWelcome.hpp>
#include <cloudos/ui/DialogModuleSelection.hpp>
#include <cloudos/ui/DialogNetwork.hpp>
#include <cloudos/ui/DialogNeutronServerMain.hpp>
#include <cloudos/ui/DialogStorage.hpp>
#include <cloudos/ui/DialogInstallerProcess.hpp>
#include <cloudos/ui/DialogInstallerFinished.hpp>


#include <cloudos/core/Object.hpp>
#include <cloudos/proto/OS.System.pb.h>
#include <cloudos/proto/OS.Network.pb.h>
#include <cloudos/proto/OpenStack.NeutronServer.pb.h>
#include <cloudos/system/Command.hpp>
#include <cloudos/system/InstallerHostSystem.hpp>

//namespace po = boost::program_options;
namespace fs = boost::filesystem;
namespace ps = boost::process;

namespace cloudos {
namespace installer {
  
  enum WIZZARD_SCREEN_TYPE {
    SCREEN_LANGUAGE         =  0,
    SCREEN_WELCOME          =  4,
    SCREEN_MODULE_SELECTION =  8,
    SCREEN_STORAGE          = 12,
    SCREEN_NETWORK          = 16,
    SCREEN_SYSTEM           = 20,
    
    SCREEN_KEYSTONE_SERVER_MAIN    = 40,
    SCREEN_KEYSTONE_SERVER_DETAILS = 41,
    SCREEN_GLANCE_SERVER_MAIN      = 44,
    SCREEN_GLANCE_SERVER_DETAILS   = 45,
    SCREEN_CINDER_SERVER_MAIN      = 48,
    SCREEN_CINDER_SERVER_DETAILS   = 49,
    SCREEN_NOVA_SERVER_MAIN        = 52,
    SCREEN_NOVA_SERVER_DETAILS     = 53,
    SCREEN_NEUTRON_SERVER_MAIN     = 56,
    SCREEN_NEUTRON_SERVER_DETAILS  = 57
  };
  
  class Installer : public core::Object {
  public:
    Installer();
    ~Installer();
    
    typedef boost::container::stable_vector<WIZZARD_SCREEN_TYPE> ScreenTypeVector;
    typedef boost::shared_ptr<boost::thread>                     ThreadPointer;
    
    /**
     * - launches the installer GUI which asks the user about the settings.
     * - stores all givven settings in /tmp/installer/settings
     * - starts the installing process
     * After return, the system may be installed; if not
     *   - the user may request an "abort"
     *   - the installation failed
     * 
     * @return 0 on success, else error code
     */
    unsigned int run();
    
  private:
    /**
     * Our core (template) installer instance
     */
    system::InstallerCoreSystemPointer c_core_install;
    
    /**
     * Pointer to the thread, which will create our install template
     */
    ThreadPointer c_core_install_thread;
    
    /**
     * Our host installer instance
     */
    system::InstallerHostSystemPointer c_host_install;
    
    /**
     * Pointer to the thread, which will install a host system
     */
    ThreadPointer c_host_install_thread;
    
    system::InstallerManagementSystemPointer c_mgt_install;
    
    /**
     * Pointer to the thread, which will install a management system
     */
    ThreadPointer c_mgt_install_thread;
    
    std::string c_mgt_ip;
    
    fs::path c_settings_directory;
    
    /**
     * Our default network config file
     */
    fs::path c_config_file_network;
    
    /**
     * Our default system config file
     */
    fs::path c_config_file_system;
    
    /**
     * Our default installer config file
     */
    fs::path c_config_file_installer;
    
    /**
     * Our default storage config file
     */
    fs::path c_config_file_storage;
    
    /**
     * Our default storage config file
     */
    fs::path c_config_file_neutron;
    
    fs::path c_temp_directory;
    fs::path c_temp_settings_directory;
    
    // 
    // Wizzard order element
    // 
    ScreenTypeVector c_wizzard_order;
    
    // 
    // configuration objects
    // 
    boost::shared_ptr<config::os::System>               c_system_config;
    boost::shared_ptr<config::os::Installer>            c_installer_config;
    system::HDDisk::ConfigPointer                       c_storage_config;
    tools::NetworkInterface::ConfigPointer              c_network_config;
    
    boost::shared_ptr<config::openstack::NeutronServer> c_neutron_config;
    
    /**
     * If our launch of the core template installation prepared successful
     * 
     * Required for isValid() == true
     */
    bool c_start_install_template_success;
    
    // 
    // init some
    // 
    bool loadSettingsFromFilesystem();
    
    /**
     * Checks, if we met all requirements
     */
    virtual void checkIsValid() override;
    
    /**
     * Starts the template installation process in the background
     */
    bool startCreateInstallTemplate();
    
    // 
    // wizzard screens
    // 
    ui::DialogUserDecision showScreenLanguage( short int p_dialog_flags );
    ui::DialogUserDecision showScreenWelcome( short int p_dialog_flags );
    ui::DialogUserDecision showScreenModuleSelection( short int p_dialog_flags );
    ui::DialogUserDecision showScreenStorage( short int p_dialog_flags );
    ui::DialogUserDecision showScreenNetwork( short int p_dialog_flags );
    ui::DialogUserDecision showScreenSystem( short int p_dialog_flags );
    
    ui::DialogUserDecision showScreenNeutronServerMain( short int p_dialog_flags );
    
    /**
     * installs the system, based on the givven configuration
     */
    unsigned int install();
    
    /**
     * checks, if the givven wizzard is set in c_wizzard_order
     * returns c_wizzard_order.end() if not found
     */
    ScreenTypeVector::iterator isScreenSet( WIZZARD_SCREEN_TYPE p_screen );
    
    /**
     * Will insert/remove configuration screens from the wizzard
     * based on what modules where selected.
     */
    void configureScreensBasedOnModuleSelection();
    
    /**
     * An assistent for configureScreensBasedOnModuleSelection()
     * Will do the erase/insert operation
     */
  inline void eraseOrInsertScreen( WIZZARD_SCREEN_TYPE p_type, bool p_config_value);
    
  };
  
}} // namespace cloudos::installer


#endif
