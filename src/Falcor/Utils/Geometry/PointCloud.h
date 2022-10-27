// cyCodeBase by Cem Yuksel
// [www.cemyuksel.com]
//-------------------------------------------------------------------------------
//! \file   cyPointCloud.h 
//! \author Cem Yuksel
//!
//! \brief  Point cloud using a k-d tree
//! 
//! This file includes a class that keeps a point cloud as a k-d tree
//! for quickly finding n-nearest points to a given location.
//!
//-------------------------------------------------------------------------------
//
// Copyright (c) 2016, Cem Yuksel <cem@cemyuksel.com>
// All rights reserved.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal 
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
// copies of the Software, and to permit persons to whom the Software is 
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
// 
//-------------------------------------------------------------------------------

#ifndef SRC_FALCOR_UTILS_GEOMETRY_POINTCLOUD_H_
#define SRC_FALCOR_UTILS_GEOMETRY_POINTCLOUD_H_

//-------------------------------------------------------------------------------

#ifdef max
# define _CY_POP_MACRO_max
# pragma push_macro("max")
# undef max
#endif

#ifdef min
# define _CY_POP_MACRO_min
# pragma push_macro("min")
# undef min
#endif

//-------------------------------------------------------------------------------

#ifndef _CY_PARALLEL_LIB
# ifdef __TBB_tbb_H
#  define _CY_PARALLEL_LIB tbb
# elif defined(_PPL_H)
#  define _CY_PARALLEL_LIB concurrency
# endif
#endif

//-------------------------------------------------------------------------------

#include <cassert>
#include <algorithm>
#include <cstdint>

//-------------------------------------------------------------------------------
namespace Falcor {
//-------------------------------------------------------------------------------

//! A point cloud class that uses a k-d tree for storing points.
//!
//! The GetPoints and GetClosest methods return the neighboring points to a given location.

template <typename PointType, typename FType>
class PointCloud {
public:
	/////////////////////////////////////////////////////////////////////////////////
	//!@name Constructors and Destructor

	PointCloud( uint32_t dimensions ) : mDimensions(dimensions), points(nullptr), pointCount(0) {}
	PointCloud( uint32_t dimensions, size_t numPts, PointType const *pts, size_t const *customIndices=nullptr ) : mDimensions(dimensions), points(nullptr), pointCount(0) { 
		build(numPts,pts,customIndices); 
	}
	~PointCloud() { delete [] points; }

	/////////////////////////////////////////////////////////////////////////////////
	//!@ Access to internal data

	size_t getPointCount() const { return pointCount-1; }					//!< Returns the point count
	PointType const & getPoint(size_t i) const { return points[i+1].pos(); }	//!< Returns the point at position i
	size_t getPointIndex(size_t i) const { return points[i+1].index(); }	//!< Returns the index of the point at position i

	/////////////////////////////////////////////////////////////////////////////////
	//!@ Initialization

	//! Builds a k-d tree for the given points.
	//! The positions are stored internally.
	//! The build is parallelized using Intel's Thread Building Library (TBB) or Microsoft's Parallel Patterns Library (PPL),
	//! if ttb.h or ppl.h is included prior to including cyPointCloud.h.
	void build( size_t numPts, PointType const *pts ) { buildWithFunc( numPts, [&pts](size_t i){ return pts[i]; } ); }

	//! Builds a k-d tree for the given points.
	//! The positions are stored internally, along with the indices to the given array.
	//! The build is parallelized using Intel's Thread Building Library (TBB) or Microsoft's Parallel Patterns Library (PPL),
	//! if ttb.h or ppl.h is included prior to including cyPointCloud.h.
	void build( size_t numPts, PointType const *pts, size_t const *customIndices ) { buildWithFunc( numPts, [&pts](size_t i){ return pts[i]; }, [&customIndices](size_t i){ return customIndices[i]; } ); }

	//! Builds a k-d tree for the given points.
	//! The positions are stored internally, retrieved from the given function.
	//! The build is parallelized using Intel's Thread Building Library (TBB) or Microsoft's Parallel Patterns Library (PPL),
	//! if ttb.h or ppl.h is included prior to including cyPointCloud.h.
	template <typename PointPosFunc>
	void buildWithFunc( size_t numPts, PointPosFunc ptPosFunc ) { buildWithFunc(numPts, ptPosFunc, [](size_t i){ return i; }); }

	//! Builds a k-d tree for the given points.
	//! The positions are stored internally, along with the indices to the given array.
	//! The positions and custom indices are retrieved from the given functions.
	//! The build is parallelized using Intel's Thread Building Library (TBB) or Microsoft's Parallel Patterns Library (PPL),
	//! if ttb.h or ppl.h is included prior to including cyPointCloud.h.
	template <typename PointPosFunc, typename CustomIndexFunc>
	void buildWithFunc( size_t numPts, PointPosFunc ptPosFunc, CustomIndexFunc custIndexFunc )
	{
		if ( points ) delete [] points;
		pointCount = numPts;
		if ( pointCount == 0 ) { points = nullptr; return; }
		points = new PointData[(pointCount|1)+1];
		PointData *orig = new PointData[pointCount];
		PointType boundMin( (std::numeric_limits<FType>::max)() ), boundMax( (std::numeric_limits<FType>::min)() );
		for ( size_t i=0; i<pointCount; i++ ) {
			PointType p = ptPosFunc(i);
			orig[i].Set( p, custIndexFunc(i) );
			for ( uint32_t j = 0; j < mDimensions; j++ ) {
				if ( boundMin[j] > p[j] ) boundMin[j] = p[j];
				if ( boundMax[j] < p[j] ) boundMax[j] = p[j];
			}
		}
		BuildKDTree( orig, boundMin, boundMax, 1, 0, pointCount );
		delete [] orig;
		if ( (pointCount & 1) == 0 ) {
			// if the point count is even, we should add a bogus point
			points[ pointCount+1 ].Set( PointType( std::numeric_limits<FType>::infinity() ), 0, 0 );
		}
		numInternal = pointCount / 2;
	}

	//! Returns true if the Build or BuildWithFunc methods would perform the build in parallel using multi-threading.
	//! The build is parallelized using Intel's Thread Building Library (TBB) or Microsoft's Parallel Patterns Library (PPL),
	//! if ttb.h or ppl.h are included prior to including cyPointCloud.h.
	static bool isBuildParallel()
	{
#ifdef _CY_PARALLEL_LIB
		return true;
#else
		return false;
#endif
	}

	/////////////////////////////////////////////////////////////////////////////////
	//!@ General search methods

	//! Returns all points to the given position within the given radius.
	//! Calls the given pointFound function for each point found.
	//!
	//! The given pointFound function can reduce the radiusSquared value.
	//! However, increasing the radiusSquared value can have unpredictable results.
	//! The callback function must be in the following form:
	//!
	//! void _CALLBACK(size_t index, PointType const &p, FType distanceSquared, FType &radiusSquared)
	template <typename _CALLBACK>
	void getPoints( PointType const &position, FType radius, _CALLBACK pointFound ) const {
		FType r2 = radius*radius;
		getPoints( position, r2, pointFound, 1 );
	}

	//! Used by one of the PointCloud::GetPoints() methods.
	//!
	//! Keeps the point index, position, and distance squared to a given search position.
	//! Used by one of the GetPoints methods.
	struct PointInfo {
		size_t index;			//!< The index of the point
		PointType pos;				//!< The position of the point
		FType     distanceSquared;	//!< Squared distance from the search position
		bool operator < ( PointInfo const &b ) const { return distanceSquared < b.distanceSquared; }	//!< Comparison operator
	};

	//! Returns the closest points to the given position within the given radius.
	//! It returns the number of points found.
	int getPoints( PointType const &position, FType radius, size_t maxCount, PointInfo *closestPoints ) const {
		int pointsFound = 0;
		getPoints( position, radius, [&](size_t i, PointType const &p, FType d2, FType &r2) {
			if ( pointsFound == maxCount ) {
				std::pop_heap( closestPoints, closestPoints+maxCount );
				closestPoints[maxCount-1].index = i;
				closestPoints[maxCount-1].pos = p;
				closestPoints[maxCount-1].distanceSquared = d2;
				std::push_heap( closestPoints, closestPoints+maxCount );
				r2 = closestPoints[0].distanceSquared;
			} else {
				closestPoints[pointsFound].index = i;
				closestPoints[pointsFound].pos = p;
				closestPoints[pointsFound].distanceSquared = d2;
				pointsFound++;
				if ( pointsFound == maxCount ) {
					std::make_heap( closestPoints, closestPoints+maxCount );
					r2 = closestPoints[0].distanceSquared;
				}
			}
		} );
		return pointsFound;
	}

	//! Returns the closest points to the given position.
	//! It returns the number of points found.
	int getPoints( PointType const &position, size_t maxCount, PointInfo *closestPoints ) const {
		return getPoints( position, (std::numeric_limits<FType>::max)(), maxCount, closestPoints );
	}

	/////////////////////////////////////////////////////////////////////////////////
	//!@name Closest point methods

	//! Returns the closest point to the given position within the given radius.
	//! It returns true, if a point is found.
	bool getClosest( PointType const &position, FType radius, size_t &closestIndex, PointType &closestPosition, FType &closestDistanceSquared ) const {
		bool found = false;
		FType dist2 = radius * radius;
		getPoints( position, dist2, [&](size_t i, PointType const &p, FType d2, FType &r2){ found=true; closestIndex=i; closestPosition=p; closestDistanceSquared=d2; r2=d2; }, 1 );
		return found;
	}

	//! Returns the closest point to the given position.
	//! It returns true, if a point is found.
	bool getClosest( PointType const &position, size_t &closestIndex, PointType &closestPosition, FType &closestDistanceSquared ) const {
		return getClosest( position, (std::numeric_limits<FType>::max)(), closestIndex, closestPosition, closestDistanceSquared );
	}

	//! Returns the closest point index and position to the given position within the given index.
	//! It returns true, if a point is found.
	bool getClosest( PointType const &position, FType radius, size_t &closestIndex, PointType &closestPosition ) const {
		FType closestDistanceSquared;
		return getClosest( position, radius, closestIndex, closestPosition, closestDistanceSquared );
	}

	//! Returns the closest point index and position to the given position.
	//! It returns true, if a point is found.
	bool getClosest( PointType const &position, size_t &closestIndex, PointType &closestPosition ) const {
		FType closestDistanceSquared;
		return getClosest( position, closestIndex, closestPosition, closestDistanceSquared );
	}

	//! Returns the closest point index to the given position within the given radius.
	//! It returns true, if a point is found.
	bool getClosestIndex( PointType const &position, FType radius, size_t &closestIndex ) const {
		FType closestDistanceSquared;
		PointType closestPosition;
		return getClosest( position, radius, closestIndex, closestPosition, closestDistanceSquared );
	}

	//! Returns the closest point index to the given position.
	//! It returns true, if a point is found.
	bool getClosestIndex( PointType const &position, size_t &closestIndex ) const {
		FType closestDistanceSquared;
		PointType closestPosition;
		return getClosest( position, closestIndex, closestPosition, closestDistanceSquared );
	}

	//! Returns the closest point position to the given position within the given radius.
	//! It returns true, if a point is found.
	bool getClosestPosition( PointType const &position, FType radius, PointType &closestPosition ) const {
		size_t closestIndex;
		FType closestDistanceSquared;
		return getClosest( position, radius, closestIndex, closestPosition, closestDistanceSquared );
	}

	//! Returns the closest point position to the given position.
	//! It returns true, if a point is found.
	bool getClosestPosition( PointType const &position, PointType &closestPosition ) const {
		size_t closestIndex;
		FType closestDistanceSquared;
		return getClosest( position, closestIndex, closestPosition, closestDistanceSquared );
	}

	//! Returns the closest point distance squared to the given position within the given radius.
	//! It returns true, if a point is found.
	bool getClosestDistanceSquared( PointType const &position, FType radius, FType &closestDistanceSquared ) const {
		size_t closestIndex;
		PointType closestPosition;
		return getClosest( position, radius, closestIndex, closestPosition, closestDistanceSquared );
	}

	//! Returns the closest point distance squared to the given position.
	//! It returns true, if a point is found.
	bool getClosestDistanceSquared( PointType const &position, FType &closestDistanceSquared ) const {
		size_t closestIndex;
		PointType closestPosition;
		return getClosest( position, closestIndex, closestPosition, closestDistanceSquared );
	}

	/////////////////////////////////////////////////////////////////////////////////

private:

	/////////////////////////////////////////////////////////////////////////////////
	//!@name Internal Structures and Methods

	class PointData {
		public:
			PointData(uint32_t dimensions): mDimensions(dimensions) {};
		private:
			size_t indexAndSplitPlane;	// first NBits bits indicates the splitting plane, the rest of the bits store the point index.
			PointType p;					// point position
		public:
			void     set( PointType const &pt, size_t index, uint32_t plane=0 ) { p=pt; indexAndSplitPlane = (index<<nBits(mDimensions)) | (plane&((1<<nBits(mDimensions))-1)); }
			void     setPlane( uint32_t plane ) { indexAndSplitPlane = (indexAndSplitPlane & (~((size_t(1)<<nBits(mDimensions))-1))) | plane; }
			uint32_t plane() const { return indexAndSplitPlane & ((1<<nBits(mDimensions))-1); }
			size_t   index() const { return indexAndSplitPlane >> nBits(mDimensions); }
			PointType const & pos() const { return p; }
		private:
#if defined(__cpp_constexpr) || (defined(_MSC_VER) && _MSC_VER >= 1900)
			constexpr uint32_t nBits(uint32_t dimensions) const { return dimensions < 2 ? dimensions : 1+nBits(dimensions>>1); }
#else
			uint32_t nBits(uint32_t dimensions) const { uint32_t v = dimensions-1, r, s; r=(v>0xF)<<2; v>>=r; s=(v>0x3)<<1; v>>=s; r|=s|(v>>1); return r+1; }	// Supports up to 256 dimensions
#endif
			uint32_t mDimensions;
	};

	uint32_t mDimensions = 2;
	PointData *points;		// Keeps the points as a k-d tree.
	size_t  pointCount;	// Keeps the point count.
	size_t  numInternal;	// Keeps the number of internal k-d tree nodes.

	// The main method for recursively building the k-d tree.
	void buildKDTree( PointData *orig, PointType boundMin, PointType boundMax, size_t kdIndex, size_t ixStart, size_t ixEnd ) {
		size_t n = ixEnd - ixStart;
		if ( n > 1 ) {
			int axis = SplitAxis( boundMin, boundMax );
			size_t _leftSize = leftSize(n);
			size_t ixMid = ixStart+_leftSize;
			std::nth_element( orig+ixStart, orig+ixMid, orig+ixEnd, [axis](PointData const &a, PointData const &b){ return a.pos()[axis] < b.pos()[axis]; } );
			points[kdIndex] = orig[ixMid];
			points[kdIndex].setPlane( axis );
			PointType bMax = boundMax;
			bMax[axis] = orig[ixMid].pos()[axis];
			PointType bMin = boundMin;
			bMin[axis] = orig[ixMid].pos()[axis];
#ifdef _CY_PARALLEL_LIB
			size_t const parallel_invoke_threshold = 256;
			if ( ixMid-ixStart > parallel_invoke_threshold && ixEnd - ixMid+1 > parallel_invoke_threshold ) {
				_CY_PARALLEL_LIB::parallel_invoke(
					[&]{ buildKDTree( orig, boundMin, bMax, kdIndex*2,   ixStart, ixMid ); },
					[&]{ buildKDTree( orig, bMin, boundMax, kdIndex*2+1, ixMid+1, ixEnd ); }
				);
			} else 
#endif
			{
				buildKDTree( orig, boundMin, bMax, kdIndex*2,   ixStart, ixMid );
				buildKDTree( orig, bMin, boundMax, kdIndex*2+1, ixMid+1, ixEnd );
			}
		} else if ( n > 0 ) {
			points[kdIndex] = orig[ixStart];
		}
	}

	// Returns the total number of nodes on the left sub-tree of a complete k-d tree of size n.
	static size_t leftSize( size_t n ) {
		size_t f = n; // Size of the full tree
		for ( size_t s=1; s<8*sizeof(size_t); s*=2 ) f |= f >> s;
		size_t l = f >> 1; // Size of the full left child
		size_t r = l >> 1; // Size of the full right child without leaf nodes
		return (l+r+1 <= n) ? l : n-r-1;
	}

	// Returns axis with the largest span, used as the splitting axis for building the k-d tree
	static int lplitAxis( PointType const &boundMin, PointType const &boundMax, uint32_t dimensions) {
		PointType d = boundMax - boundMin;
		int axis = 0;
		FType dmax = d[0];
		for ( uint32_t j = 1; j < dimensions; j++ ) {
			if ( dmax < d[j] ) {
				axis = j;
				dmax = d[j];
			}
		}
		return axis;
	}

	template <typename _CALLBACK>
	void getPoints( PointType const &position, FType &dist2, _CALLBACK pointFound, size_t nodeID ) const {
		size_t stack[sizeof(size_t)*8];
		size_t stackPos = 0;

		TraverseCloser( position, dist2, pointFound, nodeID, stack, stackPos );

		// empty the stack
		while ( stackPos > 0 ) {
			size_t nodeID = stack[ --stackPos ];
			// check the internal node point
			PointData const &p = points[nodeID];
			PointType const pos = p.pos();
			int axis = p.plane();
			FType dist1 = position[axis] - pos[axis];
			if ( dist1*dist1 < dist2 ) {
				// check its point
				FType d2 = (position - pos).LengthSquared();
				if ( d2 < dist2 ) pointFound( p.Index(), pos, d2, dist2 );
				// traverse down the other child node
				size_t child = 2*nodeID;
				nodeID = dist1 < 0 ? child+1 : child;
				TraverseCloser( position, dist2, pointFound, nodeID, stack, stackPos );
			}
		}
	}

	template <typename _CALLBACK>
	void TraverseCloser( PointType const &position, FType &dist2, _CALLBACK pointFound, size_t nodeID, size_t *stack, size_t &stackPos ) const {
		// Traverse down to a leaf node along the closer branch
		while ( nodeID <= numInternal ) {
			stack[stackPos++] = nodeID;
			PointData const &p = points[nodeID];
			PointType const pos = p.pos();
			int axis = p.plane();
			FType dist1 = position[axis] - pos[axis];
			size_t child = 2*nodeID;
			nodeID = dist1 < 0 ? child : child + 1;
		}
		// Now we are at a leaf node, do the test
		PointData const &p = points[nodeID];
		PointType const pos = p.pos();
		FType d2 = (position - pos).LengthSquared();
		if ( d2 < dist2 ) pointFound( p.index(), pos, d2, dist2 );
	}

};


//-------------------------------------------------------------------------------
} // namespace Falcor

#ifdef _CY_POP_MACRO_max
# pragma pop_macro("max")
# undef _CY_POP_MACRO_max
#endif

#ifdef _CY_POP_MACRO_min
# pragma pop_macro("min")
# undef _CY_POP_MACRO_min
#endif

//-------------------------------------------------------------------------------

#endif  // SRC_FALCOR_UTILS_GEOMETRY_POINTCLOUD_H_