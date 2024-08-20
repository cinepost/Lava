#include "Utils/Math/MathConstants.slangh"

__exported import Scene.SceneTypes;


/** This struct represents the GPU-side visibility samples container system.
*/
struct VisibilitySamplesContainer {
    uint opaqueSamplesCount;            ///< Total number of opaque surface samples.
    uint transparentSamplesCount;       ///< Total number of transparent surface samples.                      
    
    //RWBuffer<uint> 
    StructuredBuffer<MaterialDataBlob> materialData;                            ///< Material parameters. The format of the data blob depends on the material type.
};
