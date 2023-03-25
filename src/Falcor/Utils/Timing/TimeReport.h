/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#ifndef SRC_FALCOR_UTILS_TIMING_TIMEREPORT_H_
#define SRC_FALCOR_UTILS_TIMING_TIMEREPORT_H_

#include <string>
#include <vector>
#include <utility>
#include <numeric>

#include "Falcor/Core/Framework.h"
#include "Falcor/Utils/StringUtils.h"
#include "lava_utils_lib/logging.h"

#include "CpuTimer.h"

namespace Falcor {
    /** Utility class to record a number of timing measurements and print them afterwards.
        This is mainly intended for measuring longer running tasks on the CPU.
    */
    class dlldecl TimeReport {
     public:
        TimeReport();

        /** Resets the recorded measurements and the internal timer.
        */
        inline void reset() {
            mLastMeasureTime = CpuTimer::getCurrentTimePoint();
            mMeasurements.clear();
        }

        /** Prints the recorded measurements to the logfile.
        */
        inline void printToLog() const {
            for (const auto& [task, duration] : mMeasurements) {
                LLOG_INF << padStringToLength(task + ":", 25) << " " << std::to_string(duration)<<+ " s";
            }
        }

        /** Prints the recorded measurements to string.
        */
        inline std::string printToString() const {
            std::stringstream ss;
            for (const auto& [task, duration] : mMeasurements) {
                ss << padStringToLength(task + ":", 25) << " " << std::to_string(duration)<<+ " s";
            }
            return ss.str();
        }

        /** Records a time measurement.
            Measures time since last call to reset() or reportTime(), whichever happened more recently.
            \param[in] name Name of the record.
        */
        inline void measure(const std::string& name) {
            auto currentTime = CpuTimer::getCurrentTimePoint();
            std::chrono::duration<double> duration = currentTime - mLastMeasureTime;
            mLastMeasureTime = currentTime;
            mMeasurements.push_back({name, duration.count()});
        }

        /** Add a record containing the total of all measurements.
            \param[in] name Name of the record.
        */
        inline void addTotal(const std::string name = "Total") {
            double total = std::accumulate(mMeasurements.begin(), mMeasurements.end(), 0.0, [] (double t, auto &&m) { return t + m.second; });
            mMeasurements.push_back({"Total", total});
        }

     private:
        CpuTimer::TimePoint mLastMeasureTime;
        std::vector<std::pair<std::string, double>> mMeasurements;
    };

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_TIMING_TIMEREPORT_H_
