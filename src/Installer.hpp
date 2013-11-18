
#ifndef CLOUDOS_INSTALLER_INSTALLER_HPP__
#define CLOUDOS_INSTALLER_INSTALLER_HPP__

// general headers
#include <map>
#include <vector>
#include <string>
#include <sstream>

// #include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/optional.hpp>

#include <cloudos/ui/Dialog.hpp>
#include <cloudos/ui/DialogLanguage.hpp>
#include <cloudos/ui/DialogWelcome.hpp>
#include <cloudos/ui/DialogModuleSelection.hpp>
#include <cloudos/ui/DialogNetwork.hpp>
#include <cloudos/ui/DialogNeutronServerMain.hpp>
#include <cloudos/ui/DialogStorage.hpp>
#include <cloudos/ui/DialogInstallerProcess.hpp>
#include <cloudos/ui/DialogInstallerFinished.hpp>


#include <cloudos/proto/OS.System.pb.h>
#include <cloudos/proto/OS.Network.pb.h>
#include <cloudos/proto/OpenStack.NeutronServer.pb.h>

// system related headers

/**
 * we need to mount/unmount our destination disk...
 */
extern "C" {
#include <sys/mount.h>
}

//namespace po = boost::program_options;
namespace fs = boost::filesystem;
namespace ps = boost::process;

namespace cloudos {
namespace installer {
  
  typedef boost::shared_ptr<pb::Message> MessagePointer;
  
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
  
  class Installer {
  public:
    Installer();
    ~Installer();
    
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
    
    std::string getManagementIP() { return c_mgt_ip; }
    
  private:
    
    std::string c_mgt_ip;
    
    fs::path c_settings_directory;
    
    fs::path c_temp_directory;
    fs::path c_temp_settings_directory;
    
    /**
     * Our mountpoint where we will mount the root disk
     * Here, we will install our system.
     */
    fs::path c_temp_mountpoint;
    
    boost::optional<ps::child> c_copy_tar;
    
    // 
    // Wizzard order element
    // 
    std::vector<WIZZARD_SCREEN_TYPE> c_wizzard_order;
    
    // 
    // configuration objects
    // 
    boost::shared_ptr<config::os::System>               c_system_config;
    boost::shared_ptr<config::os::Installer>            c_installer_config;
    tools::StorageLocalConfigPointer                    c_storage_config;
    std::set<tools::NetworkInterfaceConfigPointer>      c_network_config;
    
    boost::shared_ptr<config::openstack::NeutronServer> c_neutron_config;
    
    std::string c_selected_interface; //TODO
    
    // 
    // init some
    // 
    void loadSettingsFromFilesystem();
    
    /**
     * creates the /tmp/installer/... directories
     */
    void createTempDirs();
    
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
    
    void copyRpmsToTmp();
    
    /**
     * installs the system, based on the givven configuration
     */
    unsigned int install();
    
    /**
     * checks, if the givven wizzard is set in c_wizzard_order
     * returns c_wizzard_order.end() if not found
     */
    std::vector<WIZZARD_SCREEN_TYPE>::iterator isScreenSet( WIZZARD_SCREEN_TYPE p_screen );
    
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
