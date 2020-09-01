#ifndef RixLPE_h
#define RixLPE_h
/* $Revision: #2 $ $Date: 2014/12/10 $
Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
** This file may contain modifications from the original source code and 
** any changes are proprietary confidential information and trade secrets 
** of PIXAR, Copyright (c) 2014 PIXAR, all rights reserved. Reverse 
** engineering of object code is prohibited. Use of copyright notice 
** is precautionary and does not imply publication.
**
** Pixar
** 1200 Park Ave.
** Emeryville, CA  94608
*/

#include "RixInterfaces.h"
#include "RixIntegrator.h"
#include <stack>

static const RixChannelId k_invalid = 2147483647;

typedef std::vector<RixChannelId> ChanIdVec;

class RixLPEAutomata;

/* RixLPE interface -- Rix Light Path Expression (LPE) interface
 * 
 * Use this interface to help support light path expression AOV display 
 * channels in RIS integrators.
 *
 * RIS usage: 
 *
 * See the source code for PxrDirectLighting and PxrPathTracer for 
 * example code that uses the RixLPE interface in order to support light 
 * path expression AOV display channels, and/or follow the example code
 * below.
 *
 * First, use RixIntegratorCtx::GetRixLPE() to obtain a RixLPE instance:
 *
 *   RixLPE *rixLPE = integratorCtx->GetRixLPE();
 *
 * Then allocate RixLPEState instances (typically one per light path):
 * 
 *   RixLPEState *states = rixLPE->AllocateStates(maxShadingCtxSize);
 *
 * Next, at each scattering event along the light path, call MoveXXX() on
 * the RixLPEState instance corresponding to that light path in order to
 * perform state transitions in the LPE deterministic finite automata for
 * each scattering event.
 * 
 *   states[sctxIdx].MoveCamera();
 *   states[sctxIdx].MoveVertex(scatterLobeTraits1); // primary hit
 *   states[sctxIdx].MoveVertex(scatterLobeTraits2); // secondary hit
 *
 * Finally, use the RixLPE::SplatHelper class to aid with writing results to 
 * the LPE AOV display channels:
 * 
 *   RixLPE::SplatHelper aovs(...);
 *   aovs.SplatPerLobe(diffuse, glossy, specular, emission, ...);
 *
 * Note that SplatHelper::SplatPerLobe() is a utility routine that performs 
 * the final direct lighting (or emissive object) light path transitions and 
 * will accumulate the per-lobe contributions into the beauty and LPE AOVs.
 * (See the implementation of SplatPerLobe() in RixLPEInline.h for details.)
 * 
 * Alternatively, some integrators may wish to manually perform the final 
 * state transitions that reach a light or emissive object, and then make 
 * separate per-lobe invocations of the SplatHelper::SplatValue() method to 
 * accumulate contributions for each lobe. E.g.,:
 *
 *   states[sctxIdx].MoveVertex(directLightingTraits); 
 *   states[sctxIdx].MoveLight(lgtGrpToken);
 *   RixLPE::SplatHelper aovs(...);
 *   aovs.SplatValue(lobeContribution, ...);
 * 
 * When the integrator is finished with the RixLPEState instances, then
 * use RixLPE::FreeStates() to free the memory of the RixLPEState instances:
 * 
 *   rixLPE->FreeStates(maxShadingCtxSize, states);
 * 
 * Use the RixLPE::AnyLPEs() method to determine whether there are any 
 * light path expression (LPE) AOV display channels at all.
 * 
 * Use the RixLPE::AnyShadowLPEs() method to determine whether there are any 
 * shadow light path expression (LPE) AOV display channels at all.
 * 
 * Use the RixLPE::AnyCustomLPEs() method to determine whether there are any 
 * custom (non-display channel) light path expressions (LPEs) at all.
 * 
 * Use RixLPE::LgtGrpIdToToken() to convert a lgtGrpId into a RixLPEToken
 * that can be passed into RixLPEState::MoveLight().
 *
 */

class RixLPEState;

class RixLPE : public RixInterface
{
  public:

    // The built-in LPE tokens.
    enum RixLPEBuiltInTokens
    {
        k_NULL = 0,
        k_NONE,
        k_CAMERA,
        k_LIGHT,
        k_OBJECT,
        k_TRANSMIT,
        k_REFLECT,
        k_STOP,
        k_BLANK,
        k_LAMBDA,
        k_DOT,
        k_DIFFUSE1,
        k_DIFFUSE2,
        k_DIFFUSE3,
        k_DIFFUSE4,
        k_SPECULAR1,
        k_SPECULAR2,
        k_SPECULAR3,
        k_SPECULAR4,
        k_SPECULAR5,
        k_SPECULAR6,
        k_SPECULAR7,
        k_SPECULAR8,
        k_EVERYLOBE,
        k_EVERYDIFFUSE,
        k_EVERYSPECULAR,
        k_numBuiltinTokens,
    };

    // Flags that control various aspects of what is written to the framebuffer
    // for LPE AOVs.
    enum RixLPEFlags
    {
        k_FlagWithOcclusion = 0x1,
        k_FlagWithThruput   = 0x2,
        k_FlagWithClamp     = 0x4,
        k_FlagShadowsOnly   = 0x8,
    };

    static const int k_baseLgtGrpTokenOffset = k_numBuiltinTokens;

    RixLPE()
        :
        RixInterface(1), 
        m_anyLPEs(false),
        m_anyShadowLPEs(false),
        m_anyCustomLPEs(false), 
        m_beautyChanId(k_invalid)
    {
    }

    // Allocate and initialize an array of size 'count' of RixLPEState 
    // instances.
    virtual RixLPEState * AllocateStates(int count) = 0;

    // Free an array of RixLPEState instances.
    virtual void FreeStates(int count, RixLPEState *states) = 0;

    /// Convert an integer light group id to an LPE token.
    PRMAN_INLINE static RixLPEToken LgtGrpIdToToken(RtInt lgtGrpId);

    // Return true if there are any light path expression AOVs.
    PRMAN_INLINE bool AnyLPEs() const;

    // Return true if there are any shadow light path expression AOVs.
    PRMAN_INLINE bool AnyShadowLPEs() const;

    // Return true if there are any custom light path expressions.
    PRMAN_INLINE bool AnyCustomLPEs() const;

  protected:

    // The destructor is protected: no need to delete RixLPE instances.
    virtual ~RixLPE() {}

    // True if there are any light path expression AOVs.
    bool      m_anyLPEs;
    
    // True if there are any shadow light path expression AOVs.
    bool      m_anyShadowLPEs;

    // True if there are any custom light path expressions.
    bool      m_anyCustomLPEs;
    
    // The channel id of the beauty pass.
    RixChannelId m_beautyChanId;   

  public:

    typedef RixLPEState State;

    // Represents the necessary information that we'll need to write out the
    // standard AOV channels.
    class SplatHelper
    {
      public:

        // Constructor overload that sets all fields. Pass in the current
        // RixLPEState instance up to this point along the light path 
        // (the 'state' parameter), along with the light group id of the 
        // current sample ('lgtGrpId'), the camera to primary hit transmission
        // ('eyeTrans'), and the shadowing term ('lgtTrans').
        PRMAN_INLINE
        SplatHelper(RixDisplayServices *displaySvc, RtInt integratorCtxIdx, 
                    RixLPE &rixLpe, RixLPEState &state, RtInt depth, 
                    RtInt lgtGrpId, RixLPEToken lpeGrpId, 
                    bool isReflect, 
                    RtColorRGB const &eyeTrans, RtColorRGB const &lgtTrans,
                    RixShadingContext const *sCtx, RtInt shadingCtxIdx);

        PRMAN_INLINE ~SplatHelper();

        // Call this routine to splat per-lobe illumination to the beauty and 
        // LPE AOVs when performing the direct lighting optimization. We want 
        // to perform the splat for some AOVs only if each component of the 
        // beauty RGB value is finite. Also, in the case of clamping, we want 
        // to use a consistent clamp factor across all illumination AOVs, so 
        // pass in the clamping scalar factor as an argument to this method 
        // (default is 1.0, or no clamping).
        PRMAN_INLINE void SplatPerLobe(RixBXActiveLobeWeights &activeLobes,
                                       int weightIndex, 
                                       RtColorRGB const &shadowWeight, 
                                       RtColorRGB const &thruput, 
                                       bool isFinite, RtFloat clamp = 1.0f);

        // Call this routine to splat emissive objects to the beauty and 
        // LPE AOVs when performing the direct lighting optimization. We want 
        // to perform the splat for some AOVs only if each component of the 
        // beauty RGB value is finite. Also, in the case of clamping, we want 
        // to use a consistent clamp factor across all illumination AOVs, so 
        // pass in the clamping scalar factor as an argument to this method 
        // (default is 1.0, or no clamping).
        PRMAN_INLINE void SplatEmission(RtColorRGB const &emission,
                                        RtColorRGB const &thruput, 
                                        bool isFinite, RtFloat clamp = 1.0f);

        // Call this routine to splat a single illumination contribution to the 
        // beauty and LPE AOVs. The RixLPEState instance passed by reference
        // into the SplatHelper class constructor should have already performed
        // the final state transition that corresponds to hitting a geolight or
        // emissive object just prior to calling this routine, and then this
        // method will accumulate the illumination contribution for the 
        // appropriate set of LPE AOVs (along with the beauty display channel).
        PRMAN_INLINE void SplatValue(RtColorRGB const &color, 
                                     RtColorRGB const &shadowWeight, 
                                     bool isFinite, RtFloat clamp = 1.0f);

        // Helper routine to splat a color/alpha to the beauty channel id.
        PRMAN_INLINE void SplatBeauty(RtColorRGB const &val, 
                                      RtFloat trans = 0.0f) const;

        // Helper routine to splat a color/alpha to the array of channel ids
        // associated with the current light path expression automata state.
        PRMAN_INLINE void SplatLPE(RtColorRGB const &val, 
                                   RtColorRGB const *lgtTrans, 
                                   RtFloat alpha,
                                   bool isFinite, 
                                   RtFloat clamp);

        PRMAN_INLINE void SplatShadowLPE(RtColorRGB const *lgtTrans, 
                                         RtColorRGB const &shadowWeight, 
                                         RtFloat alpha,
                                         bool isFinite, 
                                         RtFloat clamp);

        // The following fields represent the information that we'll 
        // need to write out the standard set of AOVs.
        RtInt                    m_depth;
        RtInt                    m_lgtGrpId;
        RixLPEToken              m_lpeGrpId;
        bool                     m_isReflect;
        RtFloat                  m_eyeTran;
        RtColorRGB               m_lgtTrans;
            
        // Cached references to the display services, context index, rixLpe,
        // and the state of the LPE automata.
        RixDisplayServices      *m_displaySvc;
        RtInt                    m_integratorCtxIdx;
        RixLPE                  &m_rixLpe;
        RixLPEState             &m_state;
        RixShadingContext const *m_sCtx;
        RtInt                    m_shadingCtxIdx;
    };
};

// Stores a current state in the deterministic finite automata.
// Create instances of this class using the RixLPE::AllocateStates()
// method and free them using RixLPE::FreeStates().
// Use the GetState()/SetState() methods to save and restore the 
// current state at any point along the light path, and use Reset()
// to re-initialize/reset. GetChannelIds() will routine a list of 
// matching channel ids for the current state in the automata.
class RixLPEState
{
  public:
    // Constructor (use RixLPE::AllocateStates(...) to allocate).
    PRMAN_INLINE RixLPEState(const RixLPEAutomata *automata,
                             RixCustomLPE *rixCustomLPE);

    // Copy constructor.
    PRMAN_INLINE RixLPEState(const RixLPEState &that);

    // Destructor (use RixLPE::FreeStates(...) to free).
    PRMAN_INLINE ~RixLPEState();

    // Operator =
    PRMAN_INLINE RixLPEState &operator= (const RixLPEState &that);

    // Get the current state in the automata.
    PRMAN_INLINE int GetState();

    // Set the current state in the automata.
    PRMAN_INLINE void SetState(int state);

    // Re-initialize / reset this instance.
    PRMAN_INLINE void Reset();

    // Perform a state transition in the DFA that represents leaving 
    // the camera.
    PRMAN_INLINE void MoveCamera(RixShadingContext const *sCtx,
                                 int sCtxIndex);

    // Perform a state transition in the DFA that represents hitting an
    // emissive object.
    PRMAN_INLINE void MoveEmissiveObject
                          (RixShadingContext const *sCtx,
                           int sCtxIndex,
                           RtColorRGB const &thruput, 
                           RixLPEToken lpeGrpId = RixLPE::k_BLANK);

    // Perform a state transition in the DFA that represents hitting a
    // geometric light known to RixLightingServices.
    PRMAN_INLINE void MoveLight(RixShadingContext const *sCtx,
                                int sCtxIndex,
                                RtColorRGB const &thruput, 
                                RtColorRGB const *lgtTrans, 
                                bool firstContribution, 
                                RixLPEToken lgtGrpToken);

    // Perform a state transition in the DFA that represents a light 
    // scattering event at a vertex in a light path given the sampled 
    // lobe's trait information. This method should only be called 
    // when a scattering event only includes one type of energy (diffuse,
    // glossy, or specular, but not a combination of energy from these
    // lobes); the per-display channel throughputs will not be updated
    // by this method (instead use MoveVertexMultiLobe for that purpose).
    PRMAN_INLINE void MoveVertex(RixShadingContext const *sCtx,
                                 int sCtxIndex,
                                 RixBXLobeSampled lobeSampled,
                                 RtColorRGB const &thruput, 
                                 bool firstContribution, 
                                 RixLPEToken lpeGrpId = RixLPE::k_BLANK);

    // Perform a state transition in the DFA that represents a light 
    // scattering event at a vertex in a light path given the sampled 
    // lobe's trait information. Use this method in preference to 
    // MoveVertex() when a scattering event potentially includes a 
    // combination of diffuse/glossy/specular contributions and you
    // wish to update the per-display channel path throughputs.
    PRMAN_INLINE void MoveVertexMultiLobe
                          (RixShadingContext const *sCtx,
                           int sCtxIndex,
                           bool isReflect,
                           RixBXActiveLobeWeights const &activeLobes,
                           int weightIndex,
                           RixLPEToken lpeGrpId = RixLPE::k_BLANK,
                           bool doStateTransition = true);
    
    // Fetch the array of channel ids for the current state.
    PRMAN_INLINE ChanIdVec const *GetChannelIds();
        
    // Fetch the array of output indexes for the current state.
    PRMAN_INLINE std::vector<int> const *GetOutputIndexes();
        
    // Fetch the array of flags for the current state.
    PRMAN_INLINE std::vector<int> const *GetFlags();
        
    // Fetch the array of custom ids for the current state.
    PRMAN_INLINE std::vector<int> const *GetCustomIds();
        
    // Fetch the per-display channel path thruputs.
    PRMAN_INLINE RtColorRGB const *GetThruputs();

    // Fetch the RixCustomLPE interface object.
    PRMAN_INLINE RixCustomLPE *GetRixCustomLPE();

  private:

    // These methods implement internal state transitions in the DFA.
    PRMAN_INLINE void move(RixLPEToken event);
    PRMAN_INLINE void move(RixLPEToken event, RixLPEToken scatt);
    PRMAN_INLINE void move(RixLPEToken event, RixLPEToken scatt, 
                           RixLPEToken id);

    // A reference to the stateless automata that can be shared
    // between multiple threads.
    RixLPEAutomata const *m_automata;
        
    // And the current state.
    int                   m_state;

    // The current per-display channel path thruputs.
    RtColorRGB           *m_thruput;

    // A handy pointer to the RixCustomLPE interface object.
    RixCustomLPE         *m_rixCustomLPE;
};

//
// class RixLPE inline method implementation
//

#include "RixLPEInline.h"

#endif
