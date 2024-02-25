#ifndef SRC_FALCOR_SCENE_MESHLETBUILDER_H_
#define SRC_FALCOR_SCENE_MESHLETBUILDER_H_

#include <map>
#include <bitset>

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

		using UniquePtr = std::unique_ptr<MeshletBuilder>;

		static UniquePtr create();

		void generateMeshlets(SceneBuilder::MeshSpec& mesh, BuildMode mode = BuildMode::SCAN);
		void buildPrimitiveAdjacency(SceneBuilder::MeshSpec& mesh);

	protected:
		MeshletBuilder();

		void generateMeshletsScan(SceneBuilder::MeshSpec& mesh);
		void generateMeshletsMeshopt(SceneBuilder::MeshSpec& mesh);

		BS::multi_future<uint32_t> mTasks;

};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_SCENEBUILDER_H_
