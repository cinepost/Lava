struct AOVData {
    float  depth;
    float3 position;
    float3 albedo;
    float3 emission;
    float  roughness;
    float3 shadow;
    float3 normal;
    float3 tangent_normal;
    float  fresnel;
    uint   primID;
    uint   opID;
    float2 uv;

    /** Create AOVData with default parameters.
    */
    static AOVData create()
    {
        AOVData d;
        d.depth = 0.f;
        d.position = float3(0.f);
        d.albedo = float3(0.f);
        d.emission = float3(0.f);
        d.shadow = float3(0.f);
        d.normal = float3(0.f);
        d.tangent_normal = float3(0.f);
        d.fresnel = d.roughness = 0.f;
        d.primID = d.opID = 0;
        d.uv = float2(0.f);

        return d;
    }
};