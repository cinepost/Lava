__exported import Scene.SceneTypes;


/** This struct represents the GPU-side cryptomatte system.
*/
struct CryptomatteSystem {
    uint materialCount;                            ///< Total number of materials. Just for out of bound index checking.
    uint instanceCount;                            ///< Total number of objects. Just for out of bound index checking.
    TypedBuffer<uint32_t> materialHashData;        ///< Material hashes.
    TypedBuffer<uint32_t> instanceHashData;        ///< Instance hashes.

    /** Get the material hash for the given material.
    */
    uitn32_t getMaterialHash(const uint materialID) {
        // TODO: check index bound
        return instanceHashData[materialID];
    }

    /** Get the insance hash for the given object.
    */
    uitn32_t getInstanceHash(const GeometryInstanceID instanceID) {
        // TODO: check index bound
        return instanceHashData[instanceID.index];
    }
};

#ifdef CRYPTOMATTE_SYSTEM_PARAMETER_BLOCK
// Declare parameter block used for shader reflection.
ParameterBlock<MaterialSystem> gCryptomatteBlock;
void main() {};
#endif
