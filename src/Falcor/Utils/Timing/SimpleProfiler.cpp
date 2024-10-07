#include "SimpleProfiler.h"

namespace Falcor {

namespace {

}

SimpleProfiler::SimpleProfiler( const char* name ) {
	mName = std::string( name );
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
	printf("%30s Calls\tMean (secs)\tStdDev\n","Scope");
	for(std::map<std::string, acc_t>::iterator p = mMap.begin(); p != mMap.end(); p++ ) {
		float av = ba::mean(p->second);
		float stdev = sqrt((double)(ba::variance(p->second)));
		//float worst = ba::extract_result<ba::tag::max>(p->second);
		//float best = ba::extract_result<ba::tag::min>(p->second);

		printf("%30s %lu\t%f\t%f\n",p->first.c_str(), ba::count(p->second), av * 0.001f, stdev * 0.001f);
	}
	printf("\n");
}

std::map<std::string, ba::accumulator_set<uint64_t, ba::stats<ba::tag::variance(ba::lazy)> >> SimpleProfiler::mMap;

}  // namespace Falcor