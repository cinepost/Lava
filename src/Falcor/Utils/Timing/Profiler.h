/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
#ifndef SRC_FALCOR_UTILS_TIMING_PROFILER_H_
#define SRC_FALCOR_UTILS_TIMING_PROFILER_H_

#include <stack>
#include <unordered_map>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/json.hpp>
namespace fs = boost::filesystem;

#include "CpuTimer.h"
#include "FrameRate.h"
#include "Core/API/GpuTimer.h"
#include "Utils/Scripting/ScriptBindings.h"

namespace Falcor {

class Device;
class GpuTimer;

/** Container class for CPU/GPU profiling.
	This class uses the most accurately available CPU and GPU timers to profile given events.
	It automatically creates event hierarchies based on the order and nesting of the calls made.
	This class uses a double-buffering scheme for GPU profiling to avoid GPU stalls.
	ProfilerEvent is a wrapper class which together with scoping can simplify event profiling.
*/
class dlldecl Profiler {
  public:
	  using SharedPtr = std::shared_ptr<Profiler>;

	  enum class Flags {
		  None        = 0x0,
		  Internal    = 0x1,
		  Pix         = 0x2,
		  Default     = Internal | Pix
	  };

	  struct Stats {
		  float min;
		  float max;
		  float mean;
		  float stdDev;

		  pybind11::dict 			toPython() const;
		  boost::json::object toBoostJSON() const;

		  static Stats compute(const float* data, size_t len);
	  };

		class Event {
			public:
				const std::string getName() const { return mName; }

				float getCpuTime() const { return mCpuTime; }
				float getGpuTime() const { return mGpuTime; }

				float getCpuTimeAverage() const { return mCpuTimeAverage; }
				float getGpuTimeAverage() const { return mGpuTimeAverage; }

				Stats computeCpuTimeStats() const;
				Stats computeGpuTimeStats() const;

			private:
				Event(const std::string& name);

				void start(std::shared_ptr<Device> pDevice, uint32_t frameIndex);
				void end(uint32_t frameIndex);
				void endFrame(uint32_t frameIndex);

				std::string mName;                              ///< Nested event name.

				float mCpuTime = 0.0;                           ///< CPU time (previous frame).
				float mGpuTime = 0.0;                           ///< GPU time (previous frame).

				float mCpuTimeAverage = -1.f;                   ///< Average CPU time (negative value to signify invalid).
				float mGpuTimeAverage = -1.f;                   ///< Average GPU time (negative value to signify invalid).

				std::vector<float> mCpuTimeHistory;             ///< CPU time history (round-robin, used for computing stats).
				std::vector<float> mGpuTimeHistory;             ///< GPU time history (round-robin, used for computing stats).
				size_t mHistoryWriteIndex = 0;                  ///< History write index.
				size_t mHistorySize = 0;                        ///< History size.

				uint32_t mTriggered = 0;                        ///< Keeping track of nested calls to start().

				struct FrameData {
					CpuTimer::TimePoint cpuStartTime;           ///< Last event CPU start time.
					float cpuTotalTime = 0.0;                   ///< Total accumulated CPU time.

					std::vector<GpuTimer::SharedPtr> pTimers;   ///< Pool of GPU timers.
					size_t currentTimer = 0;                    ///< Next GPU timer to use from the pool.
					GpuTimer *pActiveTimer = nullptr;           ///< Currently active GPU timer.

					bool valid = false;                         ///< True when frame data is valid (after begin/end cycle).
				};
				FrameData mFrameData[2];                        ///< Double-buffered frame data to avoid GPU flushes.

				friend class Profiler;
		};

		class Capture {
			public:
				using SharedPtr = std::shared_ptr<Capture>;

				enum OuputFactory {
					PYTHON,
					BOOST_JSON
				};

				struct Lane {
					std::string name;
					Stats stats;
					std::vector<float> records;
				};

				size_t getFrameCount() const { return mFrameCount; }
				const std::vector<Lane>& getLanes() const { return mLanes; }

				pybind11::dict 			toPython() const;
				boost::json::object toBoostJSON() const;

				std::string toJsonString() const;
				void writeToFile(const fs::path& path, Capture::OuputFactory factory=Capture::OuputFactory::BOOST_JSON) const;

			private:
				Capture(size_t reservedEvents, size_t reservedFrames);

				static SharedPtr create(size_t reservedEvents, size_t reservedFrames);
				void captureEvents(const std::vector<Event*>& events);
				void finalize();

				size_t mReservedFrames = 0;
				size_t mFrameCount = 0;
				std::vector<Event*> mEvents;
				std::vector<Lane> mLanes;
				bool mFinalized = false;

				friend class Profiler;
		};

		/** Check if the profiler is enabled.
			\return Returns true if the profiler is enabled.
		*/
		bool isEnabled() const { return mEnabled; }

		/** Enable/disable the profiler.
			\param[in] enabled True to enable the profiler.
		*/
		void setEnabled(bool enabled) { mEnabled = enabled; }

		/** Check if the profiler is paused.
			\return Returns true if the profiler is paused.
		*/
		bool isPaused() const { return mPaused; }

		/** Pause/resume the profiler.
			\param[in] paused True to pause the profiler.
		*/
		void setPaused(bool paused) { mPaused = paused; }

		/** Start profile capture.
			\param[in] reservedFrames Number of frames to reserve memory for.
		*/
		void startCapture(size_t reservedFrames = 1024);

		/** End profile capture.
			\return Returns the captured data.
		*/
		Capture::SharedPtr endCapture();

		/** Check if the profiler is capturing.
			\return Return true if the profiler is capturing.
		*/
		bool isCapturing() const;

		/** Finish profiling for the entire frame.
			Note: Must be called once at the end of each frame.
		*/
		void endFrame();

		/** Start profiling a new event and update the events hierarchies.
			\param[in] name The event name.
			\param[in] flags The event flags.
		*/
		void startEvent(const std::string& name, Flags flags = Flags::Default);

		/** Finish profiling a new event and update the events hierarchies.
			\param[in] name The event name.
			\param[in] flags The event flags.
		*/
		void endEvent(const std::string& name, Flags flags = Flags::Default);

		/** Get the event, or create a new one if the event does not yet exist.
			This is a public interface to facilitate more complicated construction of event names and finegrained control over the profiled region.
			\param[in] name The event name.
			\return Returns a pointer to the event.
		*/
		Event* getEvent(const std::string& name);

		/** Get the profiler events (previous frame).
		*/
		const std::vector<Event*>& getEvents() const { return mLastFrameEvents; }

		/** Get the profiler events (previous frame) as a python dictionary.
		*/
		pybind11::dict getPythonEvents() const;

		/** Global profiler instance pointer.
		*/
		static const Profiler::SharedPtr& instancePtr(std::shared_ptr<Device> pDevice);

		/** Global profiler instance.
		*/
		static Profiler& instance(std::shared_ptr<Device> pDevice) { return *instancePtr(pDevice); }

		Profiler(std::shared_ptr<Device> pDevice);

	private:
		/** Create a new event.
			\param[in] name The event name.
			\return Returns the new event.
		*/
		Event* createEvent(const std::string& name);

		/** Find an event that was previously created.
			\param[in] name The event name.
			\return Returns the event or nullptr if none was found.
		*/
		Event* findEvent(const std::string& name);

		std::shared_ptr<Device> mpDevice = nullptr;

		bool mEnabled = false;
		bool mPaused = false;

		std::unordered_map<std::string, std::shared_ptr<Event>> mEvents; ///< Events by name.
		std::vector<Event*> mCurrentFrameEvents;            ///< Events registered for current frame.
		std::vector<Event*> mLastFrameEvents;               ///< Events from last frame.
		std::string mCurrentEventName;                      ///< Current nested event name.
		uint32_t mCurrentLevel = 0;                         ///< Current nesting level.
		uint32_t mFrameIndex = 0;                           ///< Current frame index.

		Capture::SharedPtr mpCapture;                       ///< Currently active capture.

		GpuFence::SharedPtr mpFence;
		uint64_t mFenceValue = uint64_t(-1);
};

//enum_class_operators(Profiler::Flags);
enum_class_operators(Profiler::Flags);

/** Helper class for starting and ending profiling events using RAII.
	The constructor and destructor call Profiler::StartEvent() and Profiler::EndEvent().
	The PROFILE macro wraps creation of local ProfilerEvent objects when profiling is enabled,
	and does nothing when profiling is disabled, so should be used instead of directly creating ProfilerEvent objects.
*/
class ProfilerEvent {
  public:
	  ProfilerEvent(std::shared_ptr<Device> pDevice, const std::string& name, Profiler::Flags flags = Profiler::Flags::Default);
		~ProfilerEvent() { Profiler::instance(mpDevice).endEvent(mName, mFlags); }

  private:
  	std::shared_ptr<Device> mpDevice = nullptr;
	  const std::string mName;
	  Profiler::Flags mFlags;
};

inline std::string to_string(Profiler::Capture::OuputFactory f) {
#define f2s(fa_) case Profiler::Capture::OuputFactory::fa_: return #fa_;
    switch (f) {
        f2s(PYTHON);
        f2s(BOOST_JSON);
        default:
          assert(false);
          return "";
    }
#undef f2s
}

}  // namespace Falcor

#ifdef FALCOR_ENABLE_PROFILER
#define PROFILE_DEFAULT(_pDevice, _name) Falcor::ProfilerEvent _profileEvent##__LINE__(_pDevice, _name)
#define PROFILE_SOME(_pDevice, _name, _flags) Falcor::ProfilerEvent _profileEvent##__LINE__(_pDevice, _name, _flags)


#define GET_PROFILE(_1, _2, _3, NAME, ...) NAME
#define PROFILE(...) GET_PROFILE(__VA_ARGS__, PROFILE_SOME, PROFILE_DEFAULT)(__VA_ARGS__)
#else
#define PROFILE(_pDevice, _name, ...)
#endif

#endif  // SRC_FALCOR_UTILS_TIMING_PROFILER_H_