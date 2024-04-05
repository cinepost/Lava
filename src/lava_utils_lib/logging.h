#ifndef LAVA_UTILS_LOGGING_H_
#define LAVA_UTILS_LOGGING_H_


#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/async_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#define USE_ASYNCRONOUS_CONSOLE_SINK 1

// Logging macro or static inline to stay within namespace boundaries

#ifdef _DEPLOY_BUILD
#define LLOG_TRC if (false) BOOST_LOG_SEV(lava::ut::log::global_logger::get(), boost::log::trivial::trace)
#else
#define LLOG_TRC BOOST_LOG_SEV(lava::ut::log::global_logger::get(), boost::log::trivial::trace)
#endif

#define LLOG_DBG BOOST_LOG_SEV(lava::ut::log::global_logger::get(), boost::log::trivial::debug)
#define LLOG_INF BOOST_LOG_SEV(lava::ut::log::global_logger::get(), boost::log::trivial::info)
#define LLOG_WRN BOOST_LOG_SEV(lava::ut::log::global_logger::get(), boost::log::trivial::warning)
#define LLOG_ERR BOOST_LOG_SEV(lava::ut::log::global_logger::get(), boost::log::trivial::error)
#define LLOG_FTL BOOST_LOG_SEV(lava::ut::log::global_logger::get(), boost::log::trivial::fatal)

#define LLOG_FN_DBG LLOG_DBG << boost::log::add_value("Function", __FUNCTION__)
#define LLOG_FN_INF LLOG_INF << boost::log::add_value("Function", __FUNCTION__)
#define LLOG_FN_WRN LLOG_WRN << boost::log::add_value("Function", __FUNCTION__)
#define LLOG_FN_ERR LLOG_ERR << boost::log::add_value("Function", __FUNCTION__)
#define LLOG_FN_FTL LLOG_FTL << boost::log::add_value("Function", __FUNCTION__)
#define LLOG_FN_TRC LLOG_TRC << boost::log::add_value("Function", __FUNCTION__)

namespace lava { 

namespace ut { namespace log {

// Initializing global boost::log logger
typedef boost::log::sources::severity_channel_logger_mt<boost::log::trivial::severity_level, std::string> global_logger_type;

// basic text based sink. TODO: asyncronous sink
#if USE_ASYNCRONOUS_CONSOLE_SINK
    typedef boost::log::sinks::asynchronous_sink<boost::log::sinks::text_ostream_backend> console_sink;
#else
    // We use synchronous sink mainly for console log in debug build
    typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend> console_sink;
#endif

typedef boost::log::sinks::asynchronous_sink<boost::log::sinks::text_ostream_backend> file_sink;

BOOST_LOG_INLINE_GLOBAL_LOGGER_INIT(global_logger, global_logger_type) {
	global_logger_type logger = global_logger_type(boost::log::keywords::channel = "global_logger");

	// add attributes
    logger.add_attribute("LineID", boost::log::attributes::counter<unsigned int>(1));     // lines are sequentially numbered
    logger.add_attribute("TimeStamp", boost::log::attributes::local_clock());             // each log line gets a timestamp

    return logger;
}

// Initialize/add file logger sink
void init_log();
void init_file_log(const std::string& logfilename, bool autoFlush = false);
void shutdown_log();
void flush();

}}} // namespace lava::ut::log

#endif // LAVA_UTILS_LOGGING_H_