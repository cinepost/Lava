#include "Falcor/Utils/Geometry/GeometryHash.h"


namespace Falcor {

GeometryHash GeometryHash::create(const ika::bgeo::Bgeo& bgeo) {
	return GeometryHash();
}

GeometryHash GeometryHash::create(const Geometry::Mesh& mesh) {
	return GeometryHash();
}

GeometryHash::ComparisonStatus GeometryHash::compare(const GeometryHash& lhash, const GeometryHash& rhash) {
	return GeometryHash::ComparisonStatus::SAME;
}


}  // namespace Falcor
