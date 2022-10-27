// cyCodeBase by Cem Yuksel
// [www.cemyuksel.com]
//-------------------------------------------------------------------------------
//! \file   cySampleElim.h 
//! \author Cem Yuksel
//!
//! \brief  Implementation of the weighted sample elimination method.
//!
//! This file includes an implementation of the weighted sample elimination 
//! method for generating Poisson disk sample sets.
//!
//! Blue noise (Poisson disk) sample sets produce high-quality sampling. They
//! often lead to lower noise and better convergence with Monte Carlo sampling.
//! They provide a uniform sample distribution over a sampling domain. Unlike
//! regular random sampling, Poisson disk sample sets avoid placing any two
//! samples too close together (determined by a Poisson disk radius).
//! 
//! The weighted sample elimination method implemented in this file generates a 
//! subset of samples with blue noise (Poisson disk) characteristics from a given 
//! input sample set. The weighted sample elimination method is simple, 
//! computationally efficient, and suitable for any sampling domain. It produces 
//! high-quality blue noise sample sets with a relatively large average Poisson 
//! disk radius without the need for specifying a Poisson disk radius. It also 
//! allows progressive (adaptive) sampling and it is efficient for high-
//! dimensional sampling. However, it does not guarantee maximal coverage.
//!
//! More details can be found in the original publication:
//!
//! Cem Yuksel. 2015. Sample Elimination for Generating Poisson Disk Sample Sets. 
//! Computer Graphics Forum 34, 2 (May 2015), 25-32. 
//! http://www.cemyuksel.com/research/sampleelimination/
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

#ifndef SRC_FALCOR_UTILS_SAMPLING_POISSINSAMPLING_H_
#define SRC_FALCOR_UTILS_SAMPLING_POISSINSAMPLING_H_

//-------------------------------------------------------------------------------

//#include "cyCore.h"
//#include "cyHeap.h"
//#include "cyPointCloud.h"
#include <vector>
#include <cmath>
#include <cstring>

#include "Falcor/Utils/Geometry/PointCloud.h"

//-------------------------------------------------------------------------------
namespace Falcor {
//-------------------------------------------------------------------------------

//! An implementation of the weighted sample elimination method.
//!
//! Cem Yuksel. 2015. Sample Elimination for Generating Poisson Disk Sample Sets. 
//! Computer Graphics Forum 34, 2 (May 2015), 25-32. 
//! http://www.cemyuksel.com/research/sampleelimination/
//!
//! This class keeps a number of parameters for the weighted sample elimination algorithm.
//! The main algorithm is implemented in the Eliminate method.

template <typename PointType, typename FType>
class WeightedSampleElimination {
	public:
		//! The constructor sets the default parameters.
		WeightedSampleElimination(size_t dimensions) {
			mDimensions = std::max(2ul, std::min(3ul, dimensions));

			for ( int d = 0; d < mDimensions; d++ ) {
				boundsMin[d] = FType(0);
				boundsMax[d] = FType(1);
			}
			alpha = FType(8);
			beta  = FType(0.65);
			gamma = FType(1.5);
			tiling = false;
			weightLimiting = true;
		}

		//! Tiling determines whether the generated samples are tile-able. 
		//! Tiling is off by default, but it is a good idea to turn it on for box-shaped sampling domains.
		//! Note that when tiling is off, weighted sample elimination is less likely to eliminate samples
		//! near the boundaries of the sampling domain. If you turn on tiling, make sure to set the
		//! correct boundaries for the sampling domain.
		void setTiling( bool on=true ) { tiling = on; }

		//! Returns true if the tiling parameter is turned on.
		bool isTiling() const { return tiling; }

		//! Weight limiting is used by the default weight function and it is on by default.
		//! Using weight limiting typically leads to more pronounced blue noise characteristics;
		//! therefore, it is recommended. The beta parameter determines the amount of weight limiting.
		//! Setting the beta parameter to zero effectively turns off weight limiting.
		void setWeightLimiting( bool on=true ) { weightLimiting = on; }

		//! Returns true if weight limiting is turned on.
		bool isWeightLimiting() const { return weightLimiting; }

		//! Returns the minimum bounds of the sampling domain.
		//! The sampling domain boundaries are used for tiling and computing the maximum possible
		//! Poisson disk radius for the sampling domain. The default boundaries are between 0 and 1.
		PointType const & getBoundsMin() const { return boundsMin; }

		//! Returns the maximum bounds of the sampling domain.
		//! The sampling domain boundaries are used for tiling and computing the maximum possible
		//! Poisson disk radius for the sampling domain. The default boundaries are between 0 and 1.
		PointType const & getBoundsMax() const { return boundsMax; }

		//! Sets the minimum bounds of the sampling domain.
		//! The sampling domain boundaries are used for tiling and computing the maximum possible
		//! Poisson disk radius for the sampling domain. The default boundaries are between 0 and 1.
		void setBoundsMin( PointType const &bmin ) { boundsMin = bmin; }

		//! Sets the maximum bounds of the sampling domain.
		//! The sampling domain boundaries are used for tiling and computing the maximum possible
		//! Poisson disk radius for the sampling domain. The default boundaries are between 0 and 1.
		void setBoundsMax( PointType const &bmax ) { boundsMax = bmax; }

		//! Sets the alpha parameter that is used by the default weight function. 
		void setParamAlpha( FType a ) { alpha = a; }

		//! Returns the alpha parameter that is used by the default weight function. 
		FType getParamAlpha() const { return alpha; }

		//! Sets the beta parameter that is used by weight limiting for the default weight function.
		//! Setting the beta parameter to zero effectively turns off weight limiting.
		//! If weight limiting is off, this parameter has no effect.
		void setParamBeta ( FType b ) { beta  = b; }

		//! Returns the beta parameter that is used by weight limiting for the default weight function.
		FType getParamBeta () const { return beta;  }

		//! Sets the gamma parameter that is used by weight limiting for the default weight function. 
		//! The gamma parameter adjusts weight limiting based on the ratio of the input and output counts.
		//! If weight limiting is off, this parameter has no effect.
		void setParamGamma( FType c ) { gamma = c; }

		//! Returns the gamma parameter that is used by weight limiting for the default weight function. 
		FType getParamGamma() const { return gamma; }


		//! This is the main method that uses weighted sample elimination for selecting a subset of samples
		//! with blue noise (Poisson disk) characteristics from a given input sample set (inputPoints). 
		//! The selected samples are copied to outputPoints. The output size must be smaller than the input size.
		//! 
		//! If the progressive parameter is true, the output sample points are ordered for progressive sampling,
		//! such that when the samples are introduced one by one in this order, each subset in the sequence
		//! exhibits blue noise characteristics.
		//! 
		//! The d_max parameter defines radius within which the weight function is non-zero.
		//! 
		//! The dimensions parameter specifies the dimensionality of the sampling domain. This parameter
		//! would typically be equal to the dimensionality of the sampling domain (specified by DIMENSIONS).
		//! However, smaller values can be used when sampling a low-dimensional manifold in a high-dimensional
		//! space, such as a surface in 3D.
		//! 
		//! The weight function is the crucial component of weighted sample elimination. It computes the weight
		//! of a sample point based on the placement of its neighbors within d_max radius. The weight function
		//! must have the following form:
		//!
		//! FType weightFunction( PointType const &p0, PointType const &p1, FType dist2, FType d_max )
		//!
		//! The arguments p0 and p1 are the two neighboring points, dist2 is the square of the Euclidean distance 
		//! between these two points, and d_max is the current radius for the weight function.
		//! Note that if the progressive parameter is on, the d_max value sent to the weight function can be
		//! different than the d_max value passed to this method.
		template <typename WeightFunction>
		void eliminate ( 
			PointType const *inputPoints, 
			size_t        inputSize, 
			PointType       *outputPoints, 
			size_t        outputSize, 
			bool             progressive,
			FType            d_max,
			WeightFunction   weightFunction
			) const
		{
			assert( outputSize < inputSize );
			if ( d_max <= FType(0) ) d_max = 2 * getMaxPoissonDiskRadius( mDimensions, outputSize );
			doEliminate( inputPoints, inputSize, outputPoints, outputSize, d_max, weightFunction, false );
			if ( progressive ) {
				std::vector<PointType> tmpPoints( outputSize );
				PointType *inPts  = outputPoints;
				PointType *outPts = tmpPoints.data();
				size_t inSize  = outputSize;
				size_t outSize = 0;
				while ( inSize >= 3 ) {
					outSize = inSize / 2;
					d_max *= progressiveRadiusMultiplier( mDimensions );
					doEliminate( inPts, inSize, outPts, outSize, d_max, weightFunction, true );
					if ( outPts != outputPoints ) std::memcpy( outputPoints+outSize, outPts+outSize, inSize-outSize );
					PointType *tmpPts = inPts; inPts = outPts; outPts = tmpPts;
					inSize = outSize;
				}
				if ( inPts != outputPoints ) std::memcpy( outputPoints, inPts, outSize );
			}
		}

		//! This is the main method that uses weighted sample elimination for selecting a subset of samples
		//! with blue noise (Poisson disk) characteristics from a given input sample set (inputPoints). 
		//! The selected samples are copied to outputPoints. The output size must be smaller than the input size.
		//! This method uses the default weight function.
		//! 
		//! If the progressive parameter is true, the output sample points are ordered for progressive sampling,
		//! such that when the samples are introduced one by one in this order, each subset in the sequence
		//! exhibits blue noise characteristics.
		//! 
		//! The d_max parameter defines radius within which the weight function is non-zero. If this parameter
		//! is zero (or negative), it is automatically computed using the sampling dimensions and the size of
		//! the output set.
		//! 
		//! The dimensions parameter specifies the dimensionality of the sampling domain. This parameter
		//! would typically be equal to the dimensionality of the sampling domain (specified by DIMENSIONS).
		//! However, smaller values can be used when sampling a low-dimensional manifold in a high-dimensional
		//! space, such as a surface in 3D.
		void eliminate ( 
			PointType const *inputPoints, 
			size_t        	inputSize, 
			PointType     	*outputPoints, 
			size_t        	outputSize, 
			bool          	progressive = false,
			FType         	d_max = FType(0)
			) const
		{
			if ( d_max <= FType(0) ) d_max = 2 * getMaxPoissonDiskRadius( mDimensions, outputSize );
			FType alpha = this->alpha;
			if ( weightLimiting ) {
				FType d_min = d_max * getWeightLimitFraction( inputSize, outputSize );
				eliminate( inputPoints, inputSize, outputPoints, outputSize, progressive, d_max, mDimensions,
					[d_min, alpha] (PointType const &, PointType const &, FType d2, FType d_max)
					{
						FType d = Sqrt(d2);
						if ( d < d_min ) d = d_min;
						return std::pow( FType(1) - d/d_max, alpha );
					}
				);
			} else {
				eliminate( inputPoints, inputSize, outputPoints, outputSize, progressive, d_max, mDimensions,
					[alpha] (PointType const &, PointType const &, FType d2, FType d_max)
					{
						FType d = Sqrt(d2);
						return std::pow( FType(1) - d/d_max, alpha );
					}
				);
			}
		}

		//! Returns the maximum possible Poisson disk radius in the given dimensions for the given sampleCount
		//! to spread over the given domainSize. If the domainSize argument is zero or negative, it is computed
		//! as the area or N-dimensional volume of the box defined by the minimum and maximum bounds.
		//! This method is used for the default weight function.
		FType getMaxPoissonDiskRadius(size_t sampleCount, FType domainSize = 0 ) const {
			if ( domainSize <= FType(0) ) {
				domainSize = boundsMax[0] - boundsMin[0];
				for ( int d=1; d< mDimensions; d++ ) domainSize *= boundsMax[d] - boundsMin[d];
			}
			FType sampleArea = domainSize / (FType) sampleCount;
			FType r_max;
			switch ( mDimensions ) {
			case 2: r_max = Sqrt( sampleArea / ( FType(2) * Sqrt(FType(3)) ) ); break;
			case 3: r_max = std::pow ( sampleArea / ( FType(4) * Sqrt(FType(2)) ), FType(1)/FType(3) ); break;
			default:
				FType c;
				int d_start;
				if ( (mDimensions & 1) ) { c = FType(2);      d_start = 3; }
				else                    { c = Pi<FType>(); d_start = 4; }
				for ( int d=d_start; d<=mDimensions; d+=2 ) c *= FType(2) * Pi<FType>() / FType(d);
				r_max = std::pow( sampleArea / c, FType(1)/FType(mDimensions) );
				break;
			}
			return r_max;
		}


	private:
		PointType boundsMin;			// The minimum bounds of the sampling domain.
		PointType boundsMax;			// The maximum bounds of the sampling domain.
		FType     alpha, beta, gamma;	// Parameters of the default weight function.
		bool      weightLimiting;		// Specifies whether weight limiting is used with the default weight function.
		bool      tiling;				// Specifies whether the sampling domain is tiled.

		// Reflects a point near the bounds of the sampling domain off of all domain bounds for tiling.
		template <typename OPERATION>
		void tilePoint( size_t index, PointType const &point, FType d_max, OPERATION operation, int dim=0 ) const {
			for ( int d = dim; d < mDimensions; d++ ) {
				if ( boundsMax[d] - point[d] < d_max ) {
					PointType p = point;
					p[d] -= boundsMax[d] - boundsMin[d];
					operation( index, p );
					tilePoint( index, p, d_max, operation, d+1 );
				}
				if ( point[d] - boundsMin[d] < d_max ) {
					PointType p = point;
					p[d] += boundsMax[d] - boundsMin[d];
					operation( index, p );
					tilePoint( index, p, d_max, operation, d+1 );
				}
			}
		}

		// This is the method that performs weighted sample elimination.
		template <typename WeightFunction>
		void doEliminate( 
			PointType const *inputPoints, 
			size_t        inputSize, 
			PointType       *outputPoints, 
			size_t        outputSize, 
			FType            d_max,
			WeightFunction   weightFunction,
			bool             copyEliminated
			) const
		{
			// Build a k-d tree for samples
			PointCloud<PointType, FType> kdtree(mDimensions);
			if ( tiling ) {
				std::vector<PointType> point(inputPoints, inputPoints + inputSize);
				std::vector<size_t> index(inputSize);
				for ( size_t i=0; i<inputSize; i++ ) index[i] = i;
				auto AppendPoint = [&]( size_t ix, PointType const &pt ) {
					point.push_back(pt);
					index.push_back(ix);
				};
				for ( size_t i=0; i<inputSize; i++ ) TilePoint( i, inputPoints[i], d_max, AppendPoint );
				kdtree.Build( point.size(), point.data(), index.data() );
			} else {
				kdtree.Build( inputSize, inputPoints );
			}

			// Assign weights to each sample
			std::vector<FType> w( inputSize, FType(0) );
			auto AddWeights = [&]( size_t index, PointType const &point ) {
				kdtree.GetPoints( point, d_max, [&weightFunction,d_max,&w,index,&point,&inputSize]( size_t i, PointType const &p, FType d2, FType & ){
					if ( i >= inputSize ) return;
					if ( i != index ) w[index] += weightFunction(point,p,d2,d_max);
				} );
			};
			for ( size_t i=0; i<inputSize; i++ ) AddWeights( i, inputPoints[i] );

			// Build a heap for the samples using their weights
			MaxHeap<FType,size_t> heap;
			heap.SetDataPointer( w.data(), inputSize );
			heap.Build();

			// While the number of samples is greater than desired
			auto RemoveWeights = [&]( size_t index, PointType const &point ) {
				kdtree.GetPoints( point, d_max, [&weightFunction,d_max,&w,index,&point,&heap,&inputSize]( size_t i, PointType const &p, FType d2, FType & ){
					if ( i >= inputSize ) return;
					if ( i != index ) {
						w[i] -= weightFunction(point,p,d2,d_max);
						heap.MoveItemDown(i);
					}
				} );
			};
			size_t sampleSize = inputSize;
			while ( sampleSize > outputSize ) {
				// Pull the top sample from heap
				size_t i = heap.GetTopItemID();
				heap.Pop();
				// For each sample around it, remove its weight contribution and update the heap
				RemoveWeights( i, inputPoints[i] );
				sampleSize--;
			}

			// Copy the samples to the output array
			size_t targetSize = copyEliminated ? inputSize : outputSize;
			for ( size_t i=0; i<targetSize; i++ ) {
				outputPoints[i] = inputPoints[ heap.GetIDFromHeap(i) ];
			}
		}

		// Returns the change in weight function radius using half of the number of samples. It is used for progressive sampling.
		FType progressiveRadiusMultiplier() const { return mDimensions==2 ? Sqrt(FType(2)) : std::pow(FType(2), FType(1)/FType(mDimensions)); }

		// Returns the minimum radius fraction used by the default weight function.
		FType getWeightLimitFraction( size_t inputSize, size_t outputSize ) const {
			FType ratio = FType(outputSize) / FType(inputSize);
			return ( 1 - std::pow( ratio, gamma ) ) * beta;
		}

	private:
		uint32_t mDimensions = 2;
};

} // namespace Falcor

#endif  // SRC_FALCOR_UTILS_SAMPLING_POISSINSAMPLING_H_