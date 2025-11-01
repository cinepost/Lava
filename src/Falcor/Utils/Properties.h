/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
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
#ifndef SRC_FALCOR_UTILS_PROPERTIES_H_
#define SRC_FALCOR_UTILS_PROPERTIES_H_


#include "Falcor/Utils/Dictionary.h"

namespace Falcor {

/**
 * A class for storing properties.
 *
 * Properties are stored as a JSON object. The JSON object is ordered, so the order of properties is preserved.
 * Using JSON as a backing storage, properties can easily be serialized to/from files.
 * This class also supports conversion to/from python dictionaries, making it easy to specify properties from python.
 *
 * For usage patterns, look at the unit tests.
 */
class FALCOR_API Properties: public Dictionary {
    public:
        /// Get a property.
        /// Returns the default value if the property does not exist.
        /// Throws if the property exists but has the wrong type.
        template<typename T>
        T get(const std::string& key, const T& default_value) const {
            return getValue<T>(key, default_value);
        }

        template<typename T>
        T get(const std::string& key) const {
            return getValue<T>(key);
        }
};

} // namespace Falcor

#endif // SRC_FALCOR_UTILS_PROPERTIES_H_