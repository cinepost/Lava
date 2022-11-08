__exported import Scene.Scene;
__exported import Scene.Shading;
__exported import Scene.HitInfo;
import Scene.Material.ShadingUtils;
import Utils.Math.MathHelpers;

float powerHeuristic(float nf, float fPdf, float ng, float gPdf){
    float f = nf * fPdf;
    float g = ng * gPdf;
    return (f*f)/(f*f + g*g);
}

float3 getGeometricFaceNormalW(const GeometryInstanceID instanceID, const uint triangleIndex) {
    uint3 vtxIndices = gScene.getIndices(instanceID, triangleIndex);
    float3 p0 = gScene.vertices[vtxIndices[0]].position;
    float3 p1 = gScene.vertices[vtxIndices[1]].position;
    float3 p2 = gScene.vertices[vtxIndices[2]].position;
    float3 N = cross(p1 - p0, p2 - p0);
    
    if (gScene.isObjectFrontFaceCW(instanceID)) N = -N;
    
    float3x3 worldInvTransposeMat = gScene.getInverseTransposeWorldMatrix(instanceID);
    return normalize(mul(N, worldInvTransposeMat));
}

/** Evaluates the currently configured heuristic for multiple importance sampling (MIS).
    \param[in] n0 Number of samples taken from the first sampling strategy.
    \param[in] p0 Pdf for the first sampling strategy.
    \param[in] n1 Number of samples taken from the second sampling strategy.
    \param[in] p1 Pdf for the second sampling strategy.
    \return Weight for the contribution from the first strategy (p0).
*/
float evalMIS(float n0, float p0, float n1, float p1) {
    // Power two heuristic
    float q0 = (n0 * p0) * (n0 * p0);
    float q1 = (n1 * p1) * (n1 * p1);
    return q0 / (q0 + q1);
}

/** Helper for setting up the ShadingData struct based on loaded data.
    \param[in] pixel Current pixel coordinates.
    \param[in] frameDim Frame dimensions in pixel.
    \param[in] camera Current camera.
    \param[in] vbuffer VBuffer texture.
    \param[in] lod Method for computing texture level-of-detail.
    \param[out] sd ShadingData struct.
    \return True if the pixel has valid data (not a background pixel). Note sd.V is always valid.
*/
bool loadShadingData(const uint2 pixel, const uint2 frameDim, const Camera camera, Texture2D<PackedHitInfo> vbuffer, const ITextureSampler lod, out ShadingData sd, 
    out TriangleHit triangleHit)
{
    sd = {};

    const float3 rayDir = camera.computeRayPinhole(pixel, frameDim).dir;
    bool valid = false;

    const HitInfo hit = HitInfo(vbuffer[pixel]);
    if (hit.isValid() && hit.getType() == HitType::Triangle) {
        triangleHit = hit.getTriangleHit();

        if(gScene.isObjectMatteShaded(triangleHit.instanceID)) return false;

        // Evaluate Falcor's material parameters at the hit point.
        // TODO: Implement texLOD to enable texture filtering in prepareShadingData().
        const VertexData v = gScene.getVertexData(triangleHit);
        const uint materialID = gScene.getMaterialID(triangleHit.instanceID);
        sd = gScene.materials.prepareShadingData(v, materialID, -rayDir, lod);
        valid = true;
    }

    //sd.V = -rayDir;
    return valid;
}

bool calcShadingData(const float3 rayDir, const HitInfo hit, const ITextureSampler lod, out ShadingData sd, out TriangleHit triangleHit) {
    sd = {};

    bool valid = false;

    if (hit.isValid() && hit.getType() == HitType::Triangle) {
        triangleHit = hit.getTriangleHit();

        // Evaluate Falcor's material parameters at the hit point.
        // TODO: Implement texLOD to enable texture filtering in prepareShadingData().
        const VertexData v = gScene.getVertexData(triangleHit);
        const uint materialID = gScene.getMaterialID(triangleHit.instanceID);
        sd = gScene.materials.prepareShadingData(v, materialID, -rayDir, lod);
        valid = true;
    }

    return valid;
}
