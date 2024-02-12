// Taken from here: https://github.com/ConfettiFX/The-Forge/blob/master/Common_3/Renderer/VisibilityBuffer/Shaders/FSL/vb_shading_utilities.h.fsl
/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

struct DerivativesOutput {
	float3 db_dx;
	float3 db_dy;
};

// 2D interpolation results for texture gradient values
struct GradientInterpolationResults {
	float2 interp;
	float2 dx;
	float2 dy;
};

// Barycentric coordinates and gradients, struct needed to interpolate values.
struct BarycentricDeriv {
	float3 m_lambda;
	float3 m_ddx;
	float3 m_ddy;
};


// Computes the partial derivatives of a triangle from the homogeneous clip space vertices
BarycentricDeriv calcFullBary(float4 pt0, float4 pt1, float4 pt2, float2 pixelNdc, float2 two_over_windowsize) {
	BarycentricDeriv ret;
	float3 invW =  rcp(float3(pt0.w, pt1.w, pt2.w));
	
	//Project points on screen to calculate post projection positions in 2D
	float2 ndc0 = pt0.xy * invW.x;
	float2 ndc1 = pt1.xy * invW.y;
	float2 ndc2 = pt2.xy * invW.z;

	// Computing partial derivatives and prospective correct attribute interpolation with barycentric coordinates
	// Equation for calculation taken from Appendix A of DAIS paper:
	// https://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf

	// Calculating inverse of determinant(rcp of area of triangle).
	float invDet = rcp(determinant(float2x2(ndc2 - ndc1, ndc0 - ndc1)));

	//determining the partial derivatives
	// ddx[i] = (y[i+1] - y[i-1])/Determinant
	ret.m_ddx = float3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * invDet * invW;
	ret.m_ddy = float3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * invDet * invW;
	
	// sum of partial derivatives.
	float ddxSum = dot(ret.m_ddx, float3(1,1,1));
	float ddySum = dot(ret.m_ddy, float3(1,1,1));
	
	// Delta vector from pixel's screen position to vertex 0 of the triangle.
	float2 deltaVec = pixelNdc - ndc0;

	// Calculating interpolated W at point.
	float interpInvW = invW.x + deltaVec.x*ddxSum + deltaVec.y*ddySum;
	float interpW = rcp(interpInvW);
	
	// The barycentric co-ordinate (m_lambda) is determined by perspective-correct interpolation. 
	// Equation taken from DAIS paper.
	ret.m_lambda.x = interpW * (invW[0] + deltaVec.x*ret.m_ddx.x + deltaVec.y*ret.m_ddy.x);
	ret.m_lambda.y = interpW * (0.0f    + deltaVec.x*ret.m_ddx.y + deltaVec.y*ret.m_ddy.y);
	ret.m_lambda.z = interpW * (0.0f    + deltaVec.x*ret.m_ddx.z + deltaVec.y*ret.m_ddy.z);

	//Scaling from NDC to pixel units
	ret.m_ddx *= two_over_windowsize.x;
	ret.m_ddy *= two_over_windowsize.y;
	ddxSum    *= two_over_windowsize.x;
	ddySum    *= two_over_windowsize.y;

	ret.m_ddy *= -1.0f;
	ddySum *= -1.0f;

	// This part fixes the derivatives error happening for the projected triangles.
	// Instead of calculating the derivatives constantly across the 2D triangle we use a projected version
	// of the gradients, this is more accurate and closely matches GPU raster behavior.
	// Final gradient equation: ddx = (((lambda/w) + ddx) / (w+|ddx|)) - lambda

	// Calculating interpW at partial derivatives position sum.
	float interpW_ddx = 1.0f / (interpInvW + ddxSum);
	float interpW_ddy = 1.0f / (interpInvW + ddySum);

	// Calculating perspective projected derivatives.
	ret.m_ddx = interpW_ddx*(ret.m_lambda*interpInvW + ret.m_ddx) - ret.m_lambda;
	ret.m_ddy = interpW_ddy*(ret.m_lambda*interpInvW + ret.m_ddy) - ret.m_lambda;  

	return ret;
}


// Helper functions to interpolate vertex attributes using derivatives.

// Interpolate a float3 vector.
float interpolateWithDeriv_float3(BarycentricDeriv deriv, float3 v) {
	return dot(v, deriv.m_lambda);
}


// Interpolate single values over triangle vertices.
float interpolateWithDeriv(BarycentricDeriv deriv, float v0, float v1, float v2) {
	return interpolateWithDeriv_float3(deriv, float3(v0, v1, v2));
}


// Interpolate a float3 attribute for each vertex of the triangle.
float3 interpolateWithDeriv_float3x3(BarycentricDeriv deriv, float3 attr0, float3 attr1, float3 attr2) {
	return float3(dot(attr0, deriv.m_lambda),dot(attr1, deriv.m_lambda),dot(attr2, deriv.m_lambda));
}


// Interpolate 2D attributes using the partial derivatives and generates dx and dy for texture sampling.
// Attribute paramters: a 3x2 matrix of float2 attributes (Column denotes attribuets per vertex).
GradientInterpolationResults interpolate2DWithDeriv(BarycentricDeriv deriv, float2 attr0, float2 attr1, float2 attr2){
	float3 attrX = float3(attr0.x, attr1.x, attr2.x);
	float3 attrY = float3(attr0.y, attr1.y, attr2.y);
	
	GradientInterpolationResults result;

	// independently interpolate x and y attributes.
	result.interp.x = interpolateWithDeriv_float3(deriv, attrX);
	result.interp.y = interpolateWithDeriv_float3(deriv, attrY);

	// Calculate attributes' dx and dy (for texture sampling).
	result.dx.x = dot(attrX, deriv.m_ddx);
	result.dx.y = dot(attrY, deriv.m_ddx);
	result.dy.x = dot(attrX, deriv.m_ddy);
	result.dy.y = dot(attrY, deriv.m_ddy);
	return result;
}

// 2D point - triangle intersection
float2 triIntersect(float2 p, float2 a, float2 b, float2 c) {
  float2  ca = c - a, // vector from c to a
          ba = b - a, // vector from b to a
          pa = p - a; // vector from p to a

  float u = (a.x*ca.y + pa.y*ca.x - p.x*ca.y) / (ba.y*ca.x - ba.x*ca.y);
  float v = (pa.y - u*ba.y) / ca.y;
    
  if (u<0. || v<0. || u+v>1.)  return float2(-1.0); // no intersection
  return float2(u, v); // return u and v
}
