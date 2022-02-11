/** Forward renderer core functions.
    
    To use it import PathTracer.slang and instantiate ParameterBlock<PathTracerData>,
    which is passed in as the first argument to all functions that need it.
    Any resources placed in PathTracerData are bound once and shared between all shaders/instances.
*/

#include "Utils/Math/MathConstants.slangh"

__exported import Scene.Scene;


/** Shared path tracer data.
    The ray tracing program instantiates this and passes it to all functions.
*/
struct ForwardLightingData
{
    PathTracerParams        params;             ///< PathTracer shared parameters.
    EnvProbe                envProbe;           ///< Environment map sampling functions.
    EmissiveLightSampler    emissiveSampler;    ///< Emissive light sampler.
};


/** Samples the BSDF to evaluate incident illumination.
    This is done differently depending on the configuration.
    \param[in] sd Shading point data.
    \param[in] sg Sample generator.
    \param[out] s Generated sample. Only valid if true is returned.
    \return True if a sample was generated, false otherwise.
*/
bool generateBSDFSample(const ShadingData sd, inout SampleGenerator sg, out BSDFSample s)
{
    if (gParams.useDirectionalLight)
    {
        // With directional light, disable BSDF sampling and just return a sample in the light's direction.
        s.wi = -normalize(gParams.lightDir);
        s.weight = evalBSDFCosine(sd, s.wi);
        s.pdf = 1.f;
        return dot(sd.N, s.wi) > 0.f;
    }
    else
    {
        if (gParams.useBrdfSampling) return sampleBSDF(sd, sg, s);
        else return sampleBSDF_Reference(sd, sg, s);
    }
}