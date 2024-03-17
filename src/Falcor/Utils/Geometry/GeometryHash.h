#ifndef SRC_FALCOR_UTILS_GEOMETRY_GEOHASH_H_
#define SRC_FALCOR_UTILS_GEOMETRY_GEOHASH_H_

#include "Falcor/Scene/Geometry.h"
#include "lava_lib/reader_bgeo/bgeo/Bgeo.h"


namespace Falcor {

class GeometryHash {
	public:
		enum ComparisonStatus {
			SAME,						// Exact same geometry
			ATTR_CHANGED,   // Same topology but some attributes (vertex positions, normals or texture coordinates) changed
			TOPO_CHANGED    // DIfferent topology
		};

	public:
		static GeometryHash create(const ika::bgeo::Bgeo& bgeo);
		static GeometryHash create(const Geometry::Mesh& mesh);

		static ComparisonStatus    compare(const GeometryHash& lhash, const GeometryHash& rhash);
	
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_GEOMETRY_GEOHASH_H_