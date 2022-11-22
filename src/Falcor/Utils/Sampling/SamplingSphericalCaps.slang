// Copyright (c) 2019, Christoph Peters
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the Karlsruhe Institute of Technology nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "Utils/Math/MathConstants.slangh"

/** This structure carries intermediate results that only need to be computed 
	once per spherical cap for sampling proportional to projected solid angle. 
	Once it is available, the sampling is relatively fast. Dependent on the 
	geometric configuration, some of the entries have undefined values. Each 
	entry has a corresponding comment referring to the following cases:
	- Case 1: The spherical cap is entirely above the horizon.
	- Case 2: The spherical cap intersects the horizon but has its center above 
		the horizon.
	- Case 3: The spherical cap intersects the horizon but has its center below 
		the horizon.
*/
struct ProjectedSphericalCap{
	// These three vectors define an orthonormal, positively oriented frame 
	//	in which the spherical cap has its center at y-coordinate zero.
	//	\note Available in cases 1, 2, 3.
	
	float3 tangent;
	float3 bitangent;
	float3 normal;
	
	// The normalized vector towards the center of the spherical cap.
	//	\note Available in cases 1, 2 and 3 but only used by 
	//	computeProjectedSphericalCapSampleDensity().
	float3 normalizedCenter;

	// The boundary of the spherical cap is a circle. This vector stores half 
	//	the extent of its axis-aligned bounding box along x and y. By 
	//	convention, the x-extent is negative if and only if the center of the 
	//	spherical cap is below the horizon.
	//	\note Available in cases 1, 2 and 3.
	float2 circleHalfExtent;
	
	// The reciprocal of what would go into circleHalfExtent.z if it existed.
	//	\note Available in case 3.
	float invCircleHalfExtentZ;
	
	// The x-coordinate of the center of the circle bounding the spherical 
	//	cap. The y-coordinate is zero due to the choice of the local 
	//	coordinate-frame.
	//	\note Available in cases 1 and 2.
	float circleCenterX;
	
	// Minus the z-coordinate of the center of the circle bounding the 
	//	spherical cap, divided by half its extent along z.
	//	\note Available in case 3. Then this is positive.
	float centerOverHalfExtentZ;

	// The distance between the origin and the plane of the circle bounding 
	//	the spherical cap.
	//	\note Available in cases 1, 2 and 3 but only used by 
	//	computeProjectedSphericalCapSampleDensity().
	float planeOriginDistance;
	
	// The plane of the circle bounding the spherical cap intersects the 
	//	x-axis at this coordinate. It is <1.0f iff the cap is partially below 
	//	the horizon.
	//	\note Available in cases 1, 2 and 3.
	float diskCutX;
	
	// 1.0f-diskCutX*diskCutX. This is the square of the y-coordinate of the 
	//	location where the plane intersects the horizon.
	//	\note Available in case 3.
	float squaredDiskCutY;
	
	// The factor by which sample points need to be scaled along the z-axis to 
	//	warp them into the spherical cap.
	//	\note Available in case 3.
	float scaleZ;
	
	// In cases 1 and 2, this is the constant sampled density, i.e. the 
	//	reciprocal of the projected solid angle. In case 3, it is a constant 
	//	factor that is pulled out of the computation of the probability density 
	//	function. It is exactly 0.0f if the spherical cap is entirely below 
	//	the horizon.
	//	\note Available in cases 1, 2 and 3.
	float densityFactor;
	
	// The threshold for the random number at which we need to transition from 
	//	sampling of the cut ellipse to sampling of the cut disk. In other 
	//	words, it is the ratio of these two areas.
	//	\note Available in case 2.
	float cutEllipseAreaRatio;
	
	// The factor needed to turn a random number into a normalized area (i.e. 
	//	from 0 to pi) for the cut ellipse.
	//	\note Available in case 2.
	float randomToCutEllipseAreaFactor;
	
	// The factor needed to turn a random number into the area to the right of 
	//	the point on the cut disk that is sampled.
	//	\note Available in cases 2 and 3.
	float randomToCutDiskAreaFactor;
	
	// The projected solid angle of the spherical cap. Exactly 0.0f if it is 
	//	entirely below the horizon. Uncomment the appropriate lines in 
	//	prepareProjectedSphericalCapSampling() if you need this. It only incurs 
	//	an additional cost in case 3.
	//	\note Available in cases 1, 2 and 3 if uncommented.
	
	//float projectedSolidAngle;
};


/** This structure carries intermediate results that only need to be computed 
	once per spherical cap for sampling proportional to solid angle.
  */
struct SphericalCap{
	// These three vectors define an orthonormal, positively oriented frame 
	// in which the normal points towards the center of the spherical cap and 
	//	the other two directions are arbitrary.*/
	float3 tangent, bitangent, normal;
	
	// The minimal dot product between normal and a point inside the spherical	
	//	cap. This is an efficient way to express the opening angle of the cap.*/
	float minimalDot;
	
	// The solid angle of the spherical cap in steradians.*/
	float solidAngle;
};


/** Returns the signed area enclosed by the unit disk between x=0 and 
	x=MaximalX.
*/
float getCutDiskArea(float maximalX){
	return mad(sqrt(mad(-maximalX, maximalX, 1.0f)), maximalX, asin(maximalX));
}


/** Implements the inverse of getCutDiskArea().
   \note This implementation factors the inverse function into a part with a 
		 singularity and a quintic polynomial. The worst-case error is around 
		 4e-6.
*/
float getAreaDiskCut(float area){
	float absArea=min(0.5f * M_PI, abs(area));
	float polynomial = mad(mad(mad(mad(mad(absArea, -0.0079908617f, 0.0238255409f), absArea, -0.0283903598f), absArea,0.0198450184f), absArea, -0.0574433620f), absArea, 0.7400712465f);
	float result = 1.0f - polynomial * pow(0.5f * M_PI-absArea, 2.0f / 3.0f);
	return (area<0.0f) ? (-result) : result;
}


/** Prepares all intermediate values to sample a spherical cap proportional to 
	projected solid angle. The surface normal and sphere center are given in 
	the same space, the sphere center is relative to the surface point for 
	which samples are taken. The surface normal has to be normalized.
	\note For directional lights sphereCenter should be a normalized direction 
			vector towards the light source and sphereRadius should be 
			sin(0.5f*alpha) where alpha is the angle spanned by the light 
			source.
*/
void prepareProjectedSphericalCapSampling(out ProjectedSphericalCap cap,float3 surfaceNormal,float3 sphereCenter, float sphereRadius){
	float invCenterDistance=rsqrt(dot(sphereCenter,sphereCenter));
	
	// Construct a tangent frame where the normal is aligned with the positive 
	// z-axis and the projection of the sphere center onto the surface tangent 
	// plane is aligned with the positive x-axis.
	
	cap.normal = surfaceNormal;
	cap.normalizedCenter = invCenterDistance * sphereCenter;
	float normalizedCenterZ = dot(cap.normal, cap.normalizedCenter);
	cap.tangent = mad(-normalizedCenterZ, cap.normal, cap.normalizedCenter);
	float invTangentLength = rsqrt(dot(cap.tangent, cap.tangent));
	cap.tangent *= invTangentLength;
	float normalizedCenterX = dot(cap.tangent, cap.normalizedCenter);
	cap.bitangent = cross(cap.normal, cap.tangent);
	
	// Compute the radius of the circle that bounds the spherical cap. It 
	// agrees with half the diameter of the ellipse, which is also the extent 
	// along y.
	cap.circleHalfExtent.y = sphereRadius * invCenterDistance;
	
	// Compute the width of the ellipse (extent along x). Negative if the 
	// sphere center is below the horizon.
	cap.circleHalfExtent.x = cap.circleHalfExtent.y * normalizedCenterZ;
	
	// Compute the location of the center of the ellipse along the x-axis
	float planeOriginDistance = sqrt(mad(-cap.circleHalfExtent.y, cap.circleHalfExtent.y, 1.0f));
	cap.circleCenterX = planeOriginDistance * normalizedCenterX;
	
	// Compute where the plane of the circle bounding the spherical cap 
	// intersects the x-axis
	cap.diskCutX = planeOriginDistance * invTangentLength;
	
	// Case 1: The spherical cap is entirely above the horizon or maybe
	// case 4: The spherical cap is entirely below the horizon and thus empty
	if(cap.diskCutX >= 1.0f){
		// The projected solid angle is an ellipse
		float projectedSolidAngle = M_PI * cap.circleHalfExtent.x * cap.circleHalfExtent.y;
		
		//cap.projectedSolidAngle=projectedSolidAngle;
		cap.densityFactor = max(0.0f, 1.0f / projectedSolidAngle);
		
		// Set irrelevant variables to bogus values
		cap.invCircleHalfExtentZ = cap.centerOverHalfExtentZ = cap.scaleZ = cap.squaredDiskCutY = cap.cutEllipseAreaRatio = cap.randomToCutEllipseAreaFactor = cap.randomToCutDiskAreaFactor = 0.0f;
	}
	// Cases 2 and 3, the spherical cap intersects the horizon
	else{
		// The area of the cut disk is needed for case 2 and 3
		float cutDiskArea = 0.5f * M_PI - getCutDiskArea(cap.diskCutX);
		
		// Case 3, the spherical cap intersects the horizon but its center is 
		// below the horizon
		[branch]
		if(cap.circleHalfExtent.x <= 0.0f){
			float circleCenterZ=normalizedCenterZ*planeOriginDistance;
			float circleHalfExtentZ=cap.circleHalfExtent.y*normalizedCenterX;
			float circleMaxZ=circleCenterZ+circleHalfExtentZ;
			cap.invCircleHalfExtentZ=1.0f/circleHalfExtentZ;
			cap.centerOverHalfExtentZ=-circleCenterZ*cap.invCircleHalfExtentZ;
			cap.squaredDiskCutY=saturate(mad(-cap.diskCutX,cap.diskCutX,1.0f));
			cap.scaleZ=circleMaxZ*rsqrt(cap.squaredDiskCutY);
			cap.densityFactor=cap.squaredDiskCutY/(circleMaxZ*circleMaxZ*cutDiskArea);
			cap.randomToCutDiskAreaFactor=cutDiskArea;
			// Set irrelevant variables to bogus values
			cap.cutEllipseAreaRatio=cap.randomToCutEllipseAreaFactor=0.0f;
			// Optionally compute the projected solid angle. We do not need it for 
			// sampling but it may be useful in some contexts.
			//float ellipseCutRatioX=-saturate(-(cap.diskCutX-cap.circleCenterX)/cap.circleHalfExtent.x);
			//float normalizedEllipseArea=0.5f*M_PI+getCutDiskArea(ellipseCutRatioX);
			//cap.projectedSolidAngle=cap.circleHalfExtent.x*cap.circleHalfExtent.y*normalizedEllipseArea+cutDiskArea;
		}
		// Case 2, the spherical cap intersects the horizon but its center is 
		// above the horizon
		else{
			// Compute the area of the cut ellipse and the total projected solid 
			// angle
			float ellipseCutRatioX = saturate((cap.diskCutX-cap.circleCenterX) / cap.circleHalfExtent.x);
			float normalizedEllipseArea = 0.5f * M_PI + getCutDiskArea(ellipseCutRatioX);
			float cutEllipseArea = cap.circleHalfExtent.x * cap.circleHalfExtent.y * normalizedEllipseArea;
			float projectedSolidAngle = cutEllipseArea+cutDiskArea;
			
			//cap.projectedSolidAngle = projectedSolidAngle;
			cap.densityFactor = 1.0f / projectedSolidAngle;
			
			// Prepare the decision which cut disk will be sampled
			cap.cutEllipseAreaRatio = cutEllipseArea * cap.densityFactor;
			cap.randomToCutEllipseAreaFactor = normalizedEllipseArea / cap.cutEllipseAreaRatio;
			cap.randomToCutDiskAreaFactor = cutDiskArea / (1.0f - cap.cutEllipseAreaRatio);
			
			// Set irrelevant variables to bogus values
			cap.invCircleHalfExtentZ = cap.centerOverHalfExtentZ = cap.scaleZ = cap.squaredDiskCutY = 0.0f;
		}
	}
	
	// For points inside the light source, we treat the projected spherical cap 
	// as empty
	cap.densityFactor = (cap.circleHalfExtent.y >= 1.0f) ? 0.0f : cap.densityFactor;
	//cap.projectedSolidAngle = (cap.circleHalfExtent.y >= 1.0f) ? 0.0f : cap.projectedSolidAngle;
}


/*! \return true iff the given projected spherical cap is empty.*/
bool isProjectedSphericalCapEmpty(ProjectedSphericalCap cap){
	return cap.densityFactor<=0.0f;
}


/*! Given the output of prepareProjectedSphericalCapSampling(), this function 
	maps given random numbers in the range from 0 to 1 to a normalized 
	direction vector providing a sample of the spherical cap in the original 
	space (used for arguments of prepareSphericalCapSampling()). If the input 
	random numbers are independent and uniform, the distribution of the output 
	random variables has the probability density function outDensity with 
	respect to the planar Lebesgue measure. The density is constant (i.e. 
	independent of the random numbers) unless the sphere center is below the 
	horizon. Otherwise the ratio between maximum and minimum is bounded by 
	sqrt(2).*/
float3 sampleProjectedSphericalCap(out float outDensity,ProjectedSphericalCap cap,float2 randomNumbers){
	float area;
	bool sampleEllipse;
	
	// If the sphere center is below the horizon, we want to sample the cut 
	// unit disk
	if(cap.circleHalfExtent.x <= 0.0f){
		area = mad(randomNumbers.x, -cap.randomToCutDiskAreaFactor, cap.randomToCutDiskAreaFactor);
	}
	
	// If the sphere center is above the horizon but the sphere intersects it, 
	// we need to decide whether we want to sample the cut unit disk or the cut 
	// ellipse
	else if(cap.diskCutX < 1.0f){
		sampleEllipse = (randomNumbers.x<cap.cutEllipseAreaRatio);
		area = sampleEllipse?
			(randomNumbers.x * cap.randomToCutEllipseAreaFactor):
			mad(randomNumbers.x, -cap.randomToCutDiskAreaFactor, cap.randomToCutDiskAreaFactor);
	}
	// If the sphere is entirely above the horizon, we sample the ellipse
	else{
		area = M_PI * randomNumbers.x;
	}

	// Sample the cut disk
	float3 disk;
	disk.x = getAreaDiskCut(area-0.5f * M_PI);
	disk.y = sqrt(mad(-disk.x, disk.x, 1.0f)) * mad(randomNumbers.y, 2.0f, -1.0f);
	
	// If the sphere center is below the horizon, we need to warp the samples 
	// further and compute the density
	float3 local;
	outDensity = cap.densityFactor;

	[branch]
	if(cap.circleHalfExtent.x <= 0.0f){
		disk.x = -disk.x;
		disk.z = sqrt(saturate(mad(-disk.x, disk.x, mad(-disk.y, disk.y, 1.0f))));
		
		// Scale down along Z to get the appropriate maximal Z
		local.z=disk.z*cap.scaleZ;

		// Scale down along Y to account for the different shape of the cut disk
		float zQuotient = mad(local.z, cap.invCircleHalfExtentZ, cap.centerOverHalfExtentZ);
		float scaleY = cap.circleHalfExtent.y * sqrt(max(0.0f, mad(-zQuotient, zQuotient,1.0f) / mad(-disk.z, disk.z, cap.squaredDiskCutY)));
		local.y = disk.y * scaleY;
		
		// Turn it into a normalized vector to get X
		local.x = sqrt(saturate(mad(-local.y,local.y,mad(-local.z,local.z,1.0f))));
		
		// Compute the proper density
		outDensity *= local.x / (disk.x * scaleY);
	}
	// If the sphere center is above the horizon but the sphere intersects it, 
	// we may be sampling the cut disk
	else if(cap.diskCutX < 1.0f && !sampleEllipse){
		local.x = -disk.x;
		local.y = disk.y;
		local.z = sqrt(saturate(mad(-local.x, local.x, mad(-local.y, local.y, 1.0f))));
	}
	// Otherwise we are sampling the ellipse (either the cut ellipse or the 
	// entire ellipse, it does not make a difference here)
	else{
		local.x = mad(disk.x, cap.circleHalfExtent.x, cap.circleCenterX);
		local.y = cap.circleHalfExtent.y * disk.y;
		local.z = sqrt(saturate(mad(-local.x, local.x, mad(-local.y, local.y, 1.0f))));
	}
	// Go back to the original coordinate frame
	return local.x * cap.tangent + local.y * cap.bitangent + local.z * cap.normal;
}


/** Prepares all intermediate values to sample a spherical cap proportional to 
	solid angle. The sphere center is given relative to the surface point for 
	which samples are taken.
*/
void prepareSphericalCapSampling(out SphericalCap cap,float3 sphereCenter,float sphereRadius){
	float invCenterDistance = rsqrt(dot(sphereCenter, sphereCenter));

	// Construct a coordinate frame where z is aligned with the vector to the 
	// sphere center
	cap.normal=invCenterDistance * sphereCenter;
	cap.tangent = normalize(cross(cap.normal, float3(0.0f, 1.0f, 0.0f)));
	cap.bitangent = cross(cap.normal, cap.tangent);

	// Compute the radius of the circle that bounds the spherical cap
	float maximalRadius = sphereRadius * invCenterDistance;
	cap.minimalDot = sqrt(saturate(mad(-maximalRadius, maximalRadius, 1.0f)));
	cap.solidAngle = mad(-cap.minimalDot, M_2PI, M_2PI);
}


/** Maps independent, uniform random numbers from 0 to 1 to world space samples 
	in the given spherical cap. Samples are distributed in proportion to solid 
	angle.
	\param cap The output of prepareProjectedSphericalCapSampling().
	\return The sampled direction in world space.
*/
float3 sampleSphericalCap(SphericalCap cap,float2 randomNumbers){
	float3 local;
	local.z = lerp(cap.minimalDot, 1.0f, randomNumbers.x);

	// Complete to a point on the sphere
	float radius = sqrt(saturate(mad(-local.z, local.z, 1.0f)));
	local.x = radius * cos(M_2PI * randomNumbers.y);
	local.y = radius * sin(M_2PI * randomNumbers.y);

	// Now turn that into a world space sample
	return local.x * cap.tangent + local.y * cap.bitangent + local.z * cap.normal;
}