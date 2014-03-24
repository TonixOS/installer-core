
#include <boost/system/error_code.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/log/detail/format.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/expressions/formatters.hpp>
#include <boost/thread.hpp>

#define YUILogComponent "cloudos-installer"
#include <yui/YUILog.h>

#include "src/Installer.hpp"

#include <cloudos/system/Command.hpp>
#include <cloudos/core/logging.hpp>

typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_file_backend > sink_t;

namespace expr = boost::log::expressions;
namespace attr = boost::log::attributes;

using namespace cloudos;

int main(int argc, char **argv) {
  
  unsigned int retval = 0;
  
  // enable backtrace
  cloudos::tools::System::enableBacktrace();
  
  blog::add_common_attributes();
  blog::core::get()->add_global_attribute("TimeStamp", blog::attributes::local_clock());
  blog::core::get()->add_global_attribute("ThreadID", blog::attributes::current_thread_id());
  
  auto backend = boost::make_shared< boost::log::sinks::text_file_backend >( boost::log::keywords::file_name = "/tmp/installer_%5N.log" );
  backend->auto_flush(true);
  boost::shared_ptr< sink_t > sink(new sink_t(backend));
  
  sink->set_formatter
      (
        expr::format("[%1%] [%2%] [%3%] %4%")
        % expr::attr< boost::posix_time::ptime >("TimeStamp")
        % expr::attr< boost::log::attributes::current_thread_id::value_type >("ThreadID")
        % expr::attr< blog::trivial::severity_level >("Severity")
        % expr::smessage
      );
  
      // % expr::attr< boost::thread::id >("ThreadID")
      // % blog::formatter::attr< boost::log::attributes::current_thread_id::value_type >("ThreadID")
      
  blog::core::get()->add_sink( sink );
  
  core::Logger::LogSourceType& lg = core::cloudos_log::get();
  
  YUILog::setLogFileName("/tmp/installer.ui.log");
  
  // disable kernel messages on console
  system::Command dmesg("dmesg");
  dmesg << "--console-off";
  dmesg.waitUntilFinished();
  
  try {
    
    boost::shared_ptr<installer::Installer> installer( new installer::Installer() );
    retval = installer->run();
    
    system::Command sync("/bin/sync");
    sync.waitUntilFinished();
    
    if( retval == ui::DIALOG_DECISION_BTN_NEXT ) {
      system::Command systemctl;
      systemctl << "/usr/bin/systemctl" << "reboot";
      systemctl.waitUntilFinished();
    }
  
  } catch(std::exception& e) {
    BOOST_LOG_SEV(lg, blog::trivial::fatal) << "UI, std exception occured: " << e.what();
    sink->flush();
  } catch(boost::system::error_code& ec) {
    BOOST_LOG_SEV(lg, blog::trivial::fatal) << "UI, boost exception occured: " << ec.message();
    sink->flush();
  } catch(...) {
    BOOST_LOG_SEV(lg, blog::trivial::fatal) << "UI, unknown exception occured :-(";
    sink->flush();
  }
  
  dmesg.clearArguments();
  dmesg << "--console-on";
  dmesg.waitUntilFinished();
  
  if( retval == 1 ) {
    YUILog::milestone(YUILogComponent, __FILE__, __LINE__, __FUNCTION__) << "Installation process aborted, requested by user" << std::endl;
  }
  
  return retval;
}
