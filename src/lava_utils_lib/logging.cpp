#include <fstream>
#include <ostream>
#include <iomanip>

//#include <boost/log/sources/severity_logger.hpp>

#include "lava_utils_lib/logging.h"

namespace lava { namespace ut { namespace log { 

BOOST_LOG_ATTRIBUTE_KEYWORD(line_id,    "LineID",       unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp,  "TimeStamp",    boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity,   "Severity",     boost::log::trivial::severity_level)

// Initialize console logger
void init_log() {
    // create sink to stdout
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

    sink->locked_backend()->add_stream(
        boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter()));

    // flush
    sink->locked_backend()->auto_flush(true);

    // format sink
    boost::log::formatter formatter = boost::log::expressions::stream
        << std::setw(7) << std::setfill('0') << line_id << std::setfill(' ') << " | "
        << boost::log::expressions::format_date_time(timestamp, "%Y-%m-%d, %H:%M:%S.%f") << " "
        << "[" << boost::log::trivial::severity << "]"
        << " - " << boost::log::expressions::smessage;
    sink->set_formatter(formatter);

    // filter
    // TODO: add any filters

    // register sink
    boost::log::core::get()->add_sink(sink);
}

// Initialize file logger
void init_file_log(const std::string& logfilename) {
    // create sink to logfile
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();
    sink->locked_backend()->add_stream(
        boost::make_shared<std::ofstream>(logfilename.c_str()));

    // flush
    sink->locked_backend()->auto_flush(true);

    // format sink
    boost::log::formatter formatter = boost::log::expressions::stream
        << std::setw(7) << std::setfill('0') << line_id << std::setfill(' ') << " | "
        << boost::log::expressions::format_date_time(timestamp, "%Y-%m-%d, %H:%M:%S.%f") << " "
        << "[" << boost::log::trivial::severity << "]"
        << " - " << boost::log::expressions::smessage;
    sink->set_formatter(formatter);

    // filter
    // TODO: add any filters

    // register sink
    boost::log::core::get()->add_sink(sink);
}

}}} // namespace lava::ut::log