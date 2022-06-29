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
#include <regex>

#ifdef _WIN32
#include <filesystem>
namespace fs = std::filesystem;
#else
#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;
#endif

#include <boost/format.hpp>

#include "stdafx.h"
#include "MxNode.h"
#include "MxGeneratorsLibrary.h"
#include "Falcor/Utils/Debug/debug.h"


namespace Falcor {

std::vector<MxNode*> gMxNodes;


const std::string MxNode::path() const {
    if (mpParent) {
        return boost::str(boost::format("%1%/%2%") % mpParent->path() % mName); 
    } else {
        return boost::str(boost::format("/%1%") % mName); 
    }
}

MxNode::MxNode(const TypeCreateInfo& info, const std::string& name, MxNode::SharedPtr pParent): mInfo(info), mName(name), mpParent(pParent) {

}

MxNode::SharedPtr MxNode::create(const TypeCreateInfo& info, const std::string& name, MxNode::SharedPtr pParent) {
    MxNode* pNode = new MxNode(info, name, pParent);
    if (pNode) {
        const auto& mxLibrary = MxGeneratorsLibrary::instance();

        LLOG_DBG << "Created MxNode " << to_string(pNode->info()) << " at " <<  pNode->path();
    }

    return SharedPtr(pNode);
}

MxNode::~MxNode() = default;

MxNode::SharedPtr MxNode::createNode(const TypeCreateInfo& info, const std::string& name) {
    auto pNode = MxNode::create(info, name, shared_from_this());

    if(pNode) {
        mChildNodesMap.insert({name, pNode});
        mChildNodes.push_back(pNode);
        return pNode;
    }

    return MxNode::create(info, name, shared_from_this());
}

MxSocket::SharedPtr MxNode::addInputSocket(const std::string& name, MxSocket::DataType dataType) {
    return addDataSocket(name, dataType, MxSocket::Direction::INPUT);
}


MxSocket::SharedPtr MxNode::addOutputSocket(const std::string& name, MxSocket::DataType dataType) {
    return addDataSocket(name, dataType, MxSocket::Direction::OUTPUT);
}

MxSocket::SharedPtr MxNode::addDataSocket(const std::string& name, MxSocket::DataType dataType, MxSocket::Direction direction) {
    auto pSocket = MxSocket::create(shared_from_this(), name, dataType, direction);

    if ( direction == MxSocket::Direction::INPUT) {
        // Check there is no socket with same name already exist
        if ( mInputs.find(name) == mInputs.end() ) {
            mInputs.insert(std::pair<std::string, MxSocket::SharedPtr>(name,pSocket) );
            return pSocket;
        }
    } else {
        // Check there is no socket with same name already exist
        if ( mOutputs.find(name) == mOutputs.end() ) {
            mOutputs.insert(std::pair<std::string, MxSocket::SharedPtr>(name,pSocket) );
            return pSocket;
        }
    }

    return nullptr;
}

MxSocket::SharedPtr MxNode::socket(const std::string& name, MxSocket::Direction direction) {
    if ( direction == MxSocket::Direction::INPUT) {
        return inputSocket(name);
    }
    return outputSocket(name);
}

MxSocket::SharedPtr MxNode::inputSocket(const std::string& name) {
    if ( mInputs.find(name) != mInputs.end() ) {
        return mInputs[name];
    }
    return nullptr;
}

MxSocket::SharedPtr MxNode::outputSocket(const std::string& name) {
    if ( mOutputs.find(name) != mOutputs.end() ) {
        return mOutputs[name];
    }
    return nullptr;
}

MxNode::SharedPtr MxNode::node(const std::string& name) {
    if ( mChildNodesMap.find(name) == mChildNodesMap.end() ) {
        // node not found
        return nullptr;
    }
    return mChildNodesMap[name];
}

MxNode::SharedConstPtr MxNode::node(const std::string& name) const {
    return node(name);
}


bool MxNode::TypeCreateInfo::operator==(const MxNode::TypeCreateInfo& other) const {
    if (nameSpace != other.nameSpace) return false;
    if (typeName != other.typeName) return false;
    if (version != other.version) return false;
    return true;
}

bool MxNode::TypeCreateInfo::operator!=(const MxNode::TypeCreateInfo& other) const {
    if (nameSpace != other.nameSpace) return true;
    if (typeName != other.typeName) return true;
    if (version != other.version) return true;
    return false;
}


bool MxNode::operator==(const MxNode& other) const {
    if (mInfo != other.mInfo) return false;
    if (mpGenerator != other.mpGenerator) return false;
    return true;
}

}  // namespace Falcor
