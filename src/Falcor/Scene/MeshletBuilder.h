#ifndef SRC_FALCOR_SCENE_MESHLETBUILDER_H_
#define SRC_FALCOR_SCENE_MESHLETBUILDER_H_

#include <map>
#include <bitset>

#include "Falcor/Utils/ThreadPool.h"
#include "Falcor/Scene/SceneBuilder.h"

namespace Falcor {

class dlldecl MeshletBuilder {
	public:
		using UniquePtr = std::unique_ptr<MeshletBuilder>;

		static UniquePtr create();

		void generateMeshlets(SceneBuilder::ProcessedMesh& mesh);

	protected:
		MeshletBuilder();

		BS::multi_future<uint32_t> mTasks;

};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_SCENEBUILDER_H_
