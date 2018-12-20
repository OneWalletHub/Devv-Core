/*
 * logger.h logging system for Devv
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef DEVV_LOGGER_H
#define DEVV_LOGGER_H

#include <cstdlib>
#include <thread>

#define BOOST_LOG_DYN_LINK \
  1  // necessary when linking the boost_log library dynamically

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/attributes/current_thread_id.hpp>

#include "common/minitrace.h"
#include "common/devv_version.h"

// the logs are also written to LOGFILE
#define LOGFILE "/tmp/devv.log"

//Narrow-char thread-safe logger.
typedef boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> logger_t;

static const unsigned int kDEFAULT_SYSLOG_PORT = 514;

// register a global logger
BOOST_LOG_GLOBAL_LOGGER(logger, logger_t)

struct LoggerContext {
  LoggerContext() = default;

  LoggerContext(const std::string& log_hostname, unsigned int log_port)
      : syslog_hostname(log_hostname)
      , syslog_port(log_port) {}

  std::string syslog_hostname = "";
  unsigned int syslog_port = kDEFAULT_SYSLOG_PORT;
};

static boost::log::trivial::severity_level GetLogLevel() {
  const char* env_p = std::getenv("DEVV_OUTPUT_LOGLEVEL");

  if ( env_p == nullptr) {
    return boost::log::trivial::info;
  }

  std::string env_ps(env_p);
  std::transform(env_ps.begin(), env_ps.end(), env_ps.begin(), ::tolower);

  if (env_ps.find("trace") != std::string::npos) {
    return boost::log::trivial::trace;
  } else if (env_ps.find("debug") != std::string::npos) {
    return boost::log::trivial::debug;
  } else if (env_ps.find("info") != std::string::npos) {
    return boost::log::trivial::info;
  } else if (env_ps.find("warn") != std::string::npos) {
    return boost::log::trivial::warning;
  } else if (env_ps.find("error") != std::string::npos) {
    return boost::log::trivial::error;
  } else if (env_ps.find("fatal") != std::string::npos) {
    return boost::log::trivial::fatal;
  } else {
    return boost::log::trivial::info;
  }
}

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;

BOOST_LOG_ATTRIBUTE_KEYWORD(line_id, "LineID", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(thread_id, "ThreadID", attrs::current_thread_id::value_type)

static void init_log(const LoggerContext& context) {
  /* init boost log
   * 1. Add common attributes
   * 2. set log filter to trace
   */
  logger_t logger;

  boost::shared_ptr< logging::core > core = logging::core::get();

  bool add_ostream_sink = true;
  if (add_ostream_sink) {
    // Create a backend and attach a couple of streams to it
    boost::shared_ptr<sinks::text_ostream_backend> backend =
        boost::make_shared<sinks::text_ostream_backend>();

    typedef boost::shared_ptr<std::ostream> OStreamPtr;
    backend->add_stream(OStreamPtr(&std::clog, boost::null_deleter()));
    //backend->add_stream(OStreamPtr(new std::ofstream(LOGFILE)));

    // Enable auto-flushing after each log record written
    backend->auto_flush(true);

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    typedef sinks::synchronous_sink<sinks::text_ostream_backend> sink_t;
    boost::shared_ptr<sink_t> sink(new sink_t(backend));

    sink->set_formatter
        (
            expr::stream
                << Devv::Version::GIT_SHA1
                << " " << std::hex << std::setw(6) << std::setfill('0') << line_id << std::dec << std::setfill(' ')
                << " " << timestamp
                << " " << severity
                << " " << std::hex << std::setw(6) << thread_id
                << " " << expr::smessage
        );

    core->add_sink(sink);
  }

  // Add syslog sink
  if (context.syslog_hostname.size() > 1) {
    // Create a new backend
    boost::shared_ptr< sinks::syslog_backend > backend(new sinks::syslog_backend(
        logging::keywords::facility = sinks::syslog::local0,
        logging::keywords::use_impl = sinks::syslog::udp_socket_based
    ));

    // Setup the target address and port to send syslog messages to
    backend->set_target_address(context.syslog_hostname, context.syslog_port);

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    typedef sinks::synchronous_sink<sinks::syslog_backend> sink_t;
    boost::shared_ptr<sink_t> sink(new sink_t(backend));

    core->add_sink(sink);
  }
  boost::log::add_common_attributes();
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= GetLogLevel());
}

void init_log();

static inline std::string file_cut(const char* file) {
  std::string s(file);
  auto p = s.rfind("src");
  auto st = s.substr(p);
  return st;
}

// just a helper macro used by the macros below - don't use it in your code
#define LOG(severity) \
  BOOST_LOG_SEV(logger::get(), boost::log::trivial::severity) \
  << file_cut(__FILE__) << ":" << __LINE__ << " " << __FUNCTION__ << " -> "

// ===== log macros =====
/// Very verbose - used for profiling
#define LOG_TRACE LOG(trace)
/// Highly verbose - good for debugging
#define LOG_DEBUG LOG(debug)
#define LOG_INFO LOG(info)
// FIXME(spm): Add custom log levels including notice
#define LOG_NOTICE LOG(warning)
#define LOG_WARNING LOG(warning)
#define LOG_ERROR LOG(error)
#define LOG_FATAL LOG(fatal)

//toggle exceptions on/off
#if true
    #define CASH_THROW(exception) throw exception
    #define CASH_TRY try
    #define CASH_CATCH(exception) catch(exception)
#else
    #define CASH_THROW(exception) std::abort()
    #define CASH_TRY if(true)
    #define CASH_CATCH(exception) if(false)
#endif

#define NOW std::chrono::high_resolution_clock::now()
#define MILLI_SINCE(start) std::chrono::duration_cast<std::chrono::milliseconds>(NOW - start).count()

#endif  // DEVV_LOGGER_H
