#include "SimpleProfiler.h"

namespace Falcor {

namespace {

}

SimpleProfiler::SimpleProfiler( const char* name ) {
	mName = std::string( name );

  //QueryPerformanceCounter( (LARGE_INTEGER *)&mTimeStart );
	mTimeStart = Clock::now();
}

SimpleProfiler::~SimpleProfiler() {
	TimePoint t = Clock::now();

	//QueryPerformanceCounter( (LARGE_INTEGER *)&t );
	//t -= mTimeStart;

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
	TimePoint f = Clock::now();

	//QueryPerformanceFrequency( (LARGE_INTEGER *)&f );

	printf("%20s Calls\tMean (secs)\tStdDev\n","Scope");
	for(std::map<std::string, ba::accumulator_set<uint64_t, ba::stats<ba::tag::variance(ba::lazy)>>>::iterator p = mMap.begin(); p != mMap.end(); p++ ) {
		//float av = mean(p->second) / f;
		//float stdev = sqrt( ((double) variance(p->second))  ) / f;

		float av = ba::mean(p->second);
		float stdev = sqrt((double)(ba::variance(p->second)));

		printf("%20s %lu\t%f\t%f\n",p->first.c_str(), ba::count(p->second), av, stdev);
	}
}

std::map<std::string, ba::accumulator_set<uint64_t, ba::stats<ba::tag::variance(ba::lazy)> >> SimpleProfiler::mMap;

}  // namespace Falcor