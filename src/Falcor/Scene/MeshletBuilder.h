#ifndef SRC_FALCOR_SCENE_MESHLETBUILDER_H_
#define SRC_FALCOR_SCENE_MESHLETBUILDER_H_

#include <map>
#include <bitset>
#include <atomic>
#include <chrono>

#include "Falcor/Utils/ThreadPool.h"
#include "Falcor/Scene/SceneBuilder.h"

namespace Falcor {

class dlldecl MeshletBuilder {
	public:
		enum class BuildMode: uint8_t {
			SCAN,
			GREEDY,
			MESHOPT
		};

		class Stats {
			public:
				Stats ();

				void appendTotalMeshletsBuildCount(const std::vector<SceneBuilder::MeshletSpec>& meshletSpecs);
				void appendTotalMeshletsBuildDuration(const std::chrono::duration<double>& duration);
				void appendTotalAdjacencyDataBuildDuration(const std::chrono::duration<double>& duration);

				size_t totalMeshletsBuildCount() const { std::scoped_lock lock(mMutex); return mTotalMeshletsBuildCount; }
				std::chrono::duration<double> totalMeshletsBuildDuration() const { std::scoped_lock lock(mMutex); return mTotalMeshletsBuildDuration; }
				std::chrono::duration<double> totalAdjacencyDataBuildDuration() const { std::scoped_lock lock(mMutex); return mTotalAdjacencyDataBuildDuration; }

			private:
				size_t mTotalMeshletsBuildCount;
				std::chrono::duration<double> mTotalMeshletsBuildDuration;
				std::chrono::duration<double> mTotalAdjacencyDataBuildDuration;

  	    mutable std::mutex mMutex;
		};

		using UniquePtr = std::unique_ptr<MeshletBuilder>;

		static UniquePtr create();
		~MeshletBuilder();

		void generateMeshlets(SceneBuilder::MeshSpec& mesh, BuildMode mode = BuildMode::SCAN);
		void buildPrimitiveAdjacency(SceneBuilder::MeshSpec& mesh);

		void printStats() const;

	private:
		MeshletBuilder();

		void generateMeshletsScan(SceneBuilder::MeshSpec& mesh);
		void generateMeshletsGreedy(SceneBuilder::MeshSpec& mesh);
		void generateMeshletsMeshopt(SceneBuilder::MeshSpec& mesh);

		void buildPrimitiveAdjacencyNoPoints(SceneBuilder::MeshSpec& mesh);
		void buildPrimitiveAdjacencyByPointIndices(SceneBuilder::MeshSpec& mesh);

		Stats mStats;
		BS::multi_future<uint32_t> mTasks;

};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_SCENEBUILDER_H_
