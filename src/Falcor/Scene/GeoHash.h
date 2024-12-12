#ifndef SRC_FALCOR_SCENE_GEOHASH_H_
#define SRC_FALCOR_SCENE_GEOHASH_H_

#include <map>
#include <bitset>
#include <string>
#include <unordered_map>

namespace Falcor {

namespace Geometry {

class Mesh;

class Hash {
    public:
        enum class Comparison {
            Equal,                  ///< If source mesh is equal to the destination mesh
            NotEqualTopology,       ///< Source-destination meshes topologies are different
            NotEqualPositions,      ///< Source-destinatiom meshes point positions are different
            NotEqualAttributes,     ///< Source-destinatiom meshes point or primitive attributes are different
        };

        Hash();
    
    private:
        bool        mInitialized;
        uint64_t    mTopologyHash;
        uint64_t    mPositionsDataHash;
        uint64_t    mNormalsDataHash;
        uint64_t    mTexCoordDataHash;
        std::unordered_map<std::string, uint64_t>   mPtAttribHashes;
        std::unordered_map<std::string, uint64_t>   mPrimAttribHashes;

    public:
        static Hash create(const Mesh& mesh);
};

}  // namespace Geometry

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_GEOHASH_H_
