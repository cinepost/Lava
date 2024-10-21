#include "SimpleProfiler.h"

namespace Falcor {

namespace {

template<typename T>
void updateMaximum(std::atomic<T>& maximum_value, T const& value) noexcept {
    T prev_value = maximum_value;
    while(prev_value < value && !maximum_value.compare_exchange_weak(prev_value, value)) {}
}

}

SimpleProfiler::SimpleProfiler( const char* name ) {
	mName = std::string( name );
	updateMaximum(mCallerNameWidth, mName.size());
  mTimeStart = Clock::now();
}

SimpleProfiler::~SimpleProfiler() {
	TimePoint t = Clock::now();

	std::map<std::string, acc_t >::iterator p = mMap.find( mName );
	if( p == mMap.end() ) {
		// this is the first time this scope has run
		acc_t acc;
		std::pair<std::string,acc_t> pr(mName,acc);
		p = mMap.insert(pr).first;
	}
	// add the time of running to the accumulator for this scope
	(p->second)( std::chrono::duration_cast<std::chrono::milliseconds>(t - mTimeStart).count() );
}

// Generate profile report
void SimpleProfiler::printReport() {
	//TimePoint f = Clock::now();

	printf("SimpleProfiler report...\n");
	printf("%42s Calls\tMean (secs)\tStdDev\tTotal (secs)\n","Scope");
	for(std::map<std::string, acc_t>::iterator p = mMap.begin(); p != mMap.end(); p++ ) {
		float av = ba::mean(p->second);
		float stdev = sqrt((double)(ba::variance(p->second)));

		size_t sum = ba::sum(p->second);
    size_t cnt = ba::count(p->second);
		//float worst = ba::extract_result<ba::tag::max>(p->second);
		//float best = ba::extract_result<ba::tag::min>(p->second);

		printf("%42s %lu\t%f\t%f\t%f\n", p->first.c_str(), ba::count(p->second), av * 0.001f, stdev * 0.001f, float(sum) * 0.001f);
	}
	printf("\n");
}

std::map<std::string, ba::accumulator_set<uint64_t, ba::stats<ba::tag::variance(ba::lazy)> >> SimpleProfiler::mMap;

std::atomic<size_t> SimpleProfiler::mCallerNameWidth = 30; 

}  // namespace Falcor