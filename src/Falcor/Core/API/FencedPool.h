/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#pragma once

#include <queue>
#include <memory>

#include "GpuFence.h"
#include "Falcor/Core/Framework.h"

namespace Falcor {

class Device;

template<typename ObjectType>
class dlldecl FencedPool : public std::enable_shared_from_this<FencedPool<ObjectType>> {
 public:
    using SharedPtr = std::shared_ptr<FencedPool<ObjectType>>;
    using SharedConstPtr = std::shared_ptr<const FencedPool<ObjectType>>;
    using NewObjectFuncType = ObjectType(*)(std::shared_ptr<Device>, void*);

    /** Create a new fenced pool.
        \param[in] pFence GPU fence to use for synchronization.
        \param[in] newFunc Ptr to function called to create new objects.
        \param[in] pUserData Optional ptr to user data passed to the object creation function.
        \return A new object, or throws an exception if creation failed.
    */
    static SharedPtr create(std::shared_ptr<Device> device, GpuFence::SharedConstPtr pFence, NewObjectFuncType newFunc, void* pUserData = nullptr) {
        return SharedPtr(new FencedPool(device, pFence, newFunc, pUserData));
    }

    /** Return an object.
        \return An object, or throws an exception on failure.
    */
    ObjectType newObject() {
        // Retire the active object
        Data data;
        data.alloc = mActiveObject;
        data.timestamp = mpFence->getCpuValue();
        mQueue.push(data);

        // The queue is sorted based on time. Check if the first object is free
        data = mQueue.front();
        if (data.timestamp <= mpFence->getGpuValue()) {
            mQueue.pop();
        } else {
            data.alloc = createObject();
        }

        mActiveObject = data.alloc;
        return mActiveObject;
    }

private:
    FencedPool(std::shared_ptr<Device> device, GpuFence::SharedConstPtr pFence, NewObjectFuncType newFunc, void* pUserData)
        : mpUserData(pUserData)
        , mpFence(pFence)
        , mNewObjFunc(newFunc)
        , mpDevice(device)
    {
        assert(device);
        assert(pFence && newFunc);
        mActiveObject = createObject();
    }

    ObjectType createObject() {
        ObjectType pObj = mNewObjFunc(mpDevice, mpUserData);
        if (pObj == nullptr) {
            throw std::runtime_error("Failed to create new object in fenced pool");
        }
        return pObj;
    }

    struct Data {
        ObjectType alloc;
        uint64_t timestamp;
    };

    ObjectType mActiveObject;
    NewObjectFuncType mNewObjFunc = nullptr;
    std::queue<Data> mQueue;
    GpuFence::SharedConstPtr mpFence;
    void* mpUserData;
    std::shared_ptr<Device> mpDevice;
};

}  // namespace Falcor
