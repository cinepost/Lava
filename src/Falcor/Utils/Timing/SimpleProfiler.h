#ifndef SRC_FALCOR_UTILS_TIMING_SIMPLEPROFILER_H_
#define SRC_FALCOR_UTILS_TIMING_SIMPLEPROFILER_H_

#include <string>
#include <vector>
#include <utility>
#include <numeric>
#include <map>
#include <chrono>

#include "Falcor/Core/Framework.h"
#include "Falcor/Utils/Timing/CpuTimer.h"
#include "lava_utils_lib/logging.h"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>


namespace ba = boost::accumulators;

namespace Falcor {

class dlldecl SimpleProfiler {
	public:
		using Clock = CpuTimer::Clock;
		using TimePoint = CpuTimer::TimePoint;
		using TimeDuration = std::chrono::duration<double, std::milli>;

		SimpleProfiler( const char* name );
		~SimpleProfiler();

		// produces report when called without parameters
		static void printReport();


	private:
		typedef ba::accumulator_set<uint64_t, ba::stats<ba::tag::variance(ba::lazy)> > acc_t;
		static std::map <std::string, acc_t> mMap;
		std::string mName;
		TimePoint mTimeStart;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_TIMING_SIMPLEPROFILER_H_
