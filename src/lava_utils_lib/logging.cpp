#include <fstream>
#include <ostream>
#include <iomanip>

#include <boost/filesystem.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/phoenix/bind/bind_function.hpp>
#include <boost/log/expressions/attr_fwd.hpp>
#include <boost/log/expressions/attr.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>

// Supporting headers
#include <boost/log/support/date_time.hpp>

#include "lava_utils_lib/logging.h"


static bool g_logger_initialized = false;
static bool g_logger_shutted_down = false;
std::vector< std::function< void() > > g_log_stop_functions;


namespace lava { namespace ut { namespace log { 

BOOST_LOG_ATTRIBUTE_KEYWORD(line_id,    "LineID",       unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp,  "TimeStamp",    boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity,   "Severity",     boost::log::trivial::severity_level)

static void init_log_common() {
    if (g_logger_initialized) return;
    g_log_stop_functions.clear();
    boost::filesystem::path::imbue(std::locale("C"));
    g_logger_initialized = true;
}

void console_color_formatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm) {
    auto severity = rec[boost::log::trivial::severity];

#ifndef _WIN32
    if (severity) {
        // Set the color
        switch (severity.get()) {
            case boost::log::trivial::severity_level::info:
                strm << "\033[32m";
                break;
            case boost::log::trivial::severity_level::warning:
                strm << "\033[33m";
                break;
            case boost::log::trivial::severity_level::error:
            case boost::log::trivial::severity_level::fatal:
                strm << "\033[31m";
                break;
            default:
                break;
        }
    }
#endif

    // Shorter console time format
    auto date_time_formatter = boost::log::expressions::stream << std::setw(5) << std::setfill('0') << rec[line_id] << std::setfill(' ') << " | "
        << boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%H:%M:%S.%f");
    
    date_time_formatter(rec, strm);

    // Rest of the message
    strm << " [" << rec[boost::log::trivial::severity] << "]"
        << " - " << rec[boost::log::expressions::smessage];

#ifndef _WIN32
    if (severity) {
        // Restore the default color
        strm << "\033[0m";
    }
#endif
}

// Initialize console logger
void init_log() {
    init_log_common();

    // create sink to stdout
    boost::shared_ptr<console_sink> sink = boost::make_shared<console_sink>();

    sink->locked_backend()->add_stream(
        boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter()));

    // flush
    sink->locked_backend()->auto_flush(true);

    sink->set_formatter(&console_color_formatter);

    // filter
    // TODO: add any filters

    // register sink
    boost::log::core::get()->add_sink(sink);

    // add sink stop functions
#if USE_ASYNCRONOUS_CONSOLE_SINK
    g_log_stop_functions.emplace_back([sink]() {
        sink->stop();   
        sink->flush();
    });
#endif
}

void shutdown_log() {
    if (g_logger_shutted_down) return;

    if (g_log_stop_functions.size() > 0) {
        for (auto& stop : g_log_stop_functions) {
            stop();
        }
    }
    g_log_stop_functions.empty();
    //boost::log::core::get()->flush();
    //boost::log::core::get()->remove_all_sinks();
}

// Initialize file logger
void init_file_log(const std::string& logfilename, bool autoFlush) {
    init_log_common();

    // create sink to logfile
    boost::shared_ptr<file_sink> sink = boost::make_shared<file_sink>();

    if(logfilename != "") {
        // file log
        sink->locked_backend()->add_stream(boost::make_shared<std::ofstream>(logfilename.c_str()));
    } else {

    }

    // set automatic flushing
    sink->locked_backend()->auto_flush(autoFlush);

    // format sink
    boost::log::formatter formatter = boost::log::expressions::stream
        << std::setw(5) << std::setfill('0') << line_id << std::setfill(' ') << " | "
        << boost::log::expressions::format_date_time(timestamp, "%Y-%m-%d, %H:%M:%S.%f") << " "
        << "[" << boost::log::trivial::severity << "]"
        << " - " << boost::log::expressions::smessage;
    sink->set_formatter(formatter);

    // filter
    // TODO: add any filters

    // register sink
    boost::log::core::get()->add_sink(sink);

    // add sink stop functions
    g_log_stop_functions.emplace_back([sink]() {
        sink->flush();
        sink->stop();       
    });
}

}}} // namespace lava::ut::log