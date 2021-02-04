#ifndef RixLPEInline_h
#define RixLPEInline_h
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

#include "RixShadingUtils.h"

// The number of scattering types: e.g., D1, D2, ..., S1, S2, ...
static const int k_numScatterTypes = (k_RixBXMaxNumDiffuseLobes + 
                                      k_RixBXMaxNumSpecularLobes);

/// A compact deterministic finite automata class.
/// Stores the state transition tables and the per-state list of 
/// channel ids to accumulate results into.
class RixLPEAutomata
{
  public:
    
    /// Transition from the current state in the automata to a new state
    /// for the given symbol.
    PRMAN_INLINE int Move(int state, RixLPEToken symbol) const;
    
    /// Fetch an array of channel ids to accumulate results into given
    /// a state in the automata.
    PRMAN_INLINE ChanIdVec const *GetChannelIdsInState(int state) const;
    
    /// Fetch an array of output indexes to accumulate results into given
    /// a state in the automata.
    PRMAN_INLINE std::vector<int> const *GetOutputIndexesInState
                                             (int state) const;
    
    /// Fetch an array of flags (per-channel) given a state in the automata.
    PRMAN_INLINE std::vector<int> const *GetFlagsInState
                                             (int state) const;
    
    /// Fetch an array of custom ids given a state in the automata.
    PRMAN_INLINE std::vector<int> const *GetCustomIdsInState
                                             (int state) const;
    
    /// Fetch an array of thruput masks for the given state and symbol.
    PRMAN_INLINE RtColorRGB const * GetMasksInState(int state) const;

    struct Transition
    {
        RixLPEToken m_symbol;
        int         m_state;
    };
    
    struct TransitionSet
    {
        unsigned int     m_firstTrans;
        unsigned int     m_numTrans;
        int              m_wildcardTrans;
        std::vector<int> m_builtinTrans;
    };
    
    PRMAN_INLINE void AppendTransition(Transition &t);
    PRMAN_INLINE void AppendTransitionSet(TransitionSet &s);
    
    PRMAN_INLINE void ClearTransition();
    PRMAN_INLINE void ClearTransitionSet();

    PRMAN_INLINE void SetStateToChanIds(std::vector<ChanIdVec> const &v);
    PRMAN_INLINE void SetStateToOutputIndexes
                          (std::vector<std::vector<int> > const &v);
    PRMAN_INLINE void SetStateToFlags
                          (std::vector<std::vector<int> > const &v);
    PRMAN_INLINE void SetStateToCustomIds
                          (std::vector<std::vector<int> > const &v);
    PRMAN_INLINE void SetStateToMasks(std::vector<RtColorRGB> const &v);
    
    PRMAN_INLINE int  GetNumChannels() const;
    PRMAN_INLINE void SetNumChannels(int n);

  protected:
    
    std::vector<Transition>    m_trans;
    std::vector<TransitionSet> m_transSet;
    
    // For each state, store an array of channel ids to accumulate into.
    std::vector<ChanIdVec>     m_stateToChanIds;

    // For each state, store an array of output indexes to accumulate into.
    std::vector<std::vector<int> > m_stateToOutIdxs;

    // For each state, store an array of flags (per-channel).
    std::vector<std::vector<int> > m_stateToFlags;

    // For each state, store an array of flags (per-channel).
    std::vector<std::vector<int> > m_stateToCustomIds;

    // For each state, store an array of path thruput masks.
    std::vector<RtColorRGB>    m_stateToMasks;

    // The total number of LPE display channels.
    int                        m_numChannels;
};

//
// class RixLPE inline method implementation
//

// Convert an integer light group id to a 64-bit LPE token.
PRMAN_INLINE RixLPEToken RixLPE::LgtGrpIdToToken(RtInt lgtGrpId)
{
    // Offset the light group id by the number of built-in tokens.
    if (lgtGrpId >= 0)
        return lgtGrpId + k_baseLgtGrpTokenOffset;
    else
        return k_BLANK;
}

// Return true if there are any light path expression AOVs.
PRMAN_INLINE bool RixLPE::AnyLPEs() const 
{
    return m_anyLPEs; 
}

// Return true if there are any shadow light path expression AOVs.
PRMAN_INLINE bool RixLPE::AnyShadowLPEs() const 
{
    return m_anyShadowLPEs; 
}

// Return true if there are any custom light path expressions.
PRMAN_INLINE bool RixLPE::AnyCustomLPEs() const 
{
    return m_anyCustomLPEs; 
}

// 
// class RixLPEState inline method implementation
//

PRMAN_INLINE RixLPEState::RixLPEState(const RixLPEAutomata *automata,
                                      RixCustomLPE *rixCustomLPE)
{
    m_automata = automata;
    m_state = 0;

    int numChannels = m_automata->GetNumChannels();
    m_thruput = new RtColorRGB[numChannels];
    for (int i = 0; i < numChannels; i++)
        m_thruput[i].One();

    m_rixCustomLPE = rixCustomLPE;
}

PRMAN_INLINE RixLPEState::RixLPEState(const RixLPEState &that)
{
    m_automata = that.m_automata;
    m_state = that.m_state;
    
    int numChannels = m_automata->GetNumChannels();
    m_thruput = new RtColorRGB[numChannels];
    for (int i = 0; i < numChannels; i++)
        m_thruput[i] = that.m_thruput[i];

    m_rixCustomLPE = that.m_rixCustomLPE;
}
    
PRMAN_INLINE RixLPEState::~RixLPEState()
{
    delete[] m_thruput;
}

PRMAN_INLINE RixLPEState & 
RixLPEState::operator= (const RixLPEState &that)
{
    m_automata = that.m_automata;
    m_state = that.m_state;
    
    int numChannels = m_automata->GetNumChannels();
    for (int i = 0; i < numChannels; i++)
        m_thruput[i] = that.m_thruput[i];

    m_rixCustomLPE = that.m_rixCustomLPE;

    return *this;
}
        
PRMAN_INLINE int RixLPEState::GetState()
{
    return m_state;
}
        
PRMAN_INLINE void RixLPEState::SetState(int state)
{
    m_state = state;
}
        
PRMAN_INLINE void RixLPEState::Reset()
{
    m_state = 0;
    
    int numChannels = m_automata->GetNumChannels();
    for (int i = 0; i < numChannels; i++)
        m_thruput[i].One();
}

PRMAN_INLINE 
void RixLPEState::MoveCamera(RixShadingContext const *sCtx,
                             int sCtxIndex)
{
    move(RixLPE::k_CAMERA, RixLPE::k_BLANK);
}

PRMAN_INLINE 
void RixLPEState::MoveEmissiveObject(RixShadingContext const *sCtx,
                                     int sCtxIndex,
                                     RtColorRGB const &thruput, 
                                     RixLPEToken lpeGrpId)
{
    move(RixLPE::k_OBJECT, RixLPE::k_BLANK, lpeGrpId);

    if (m_rixCustomLPE)
    {
        m_rixCustomLPE->InvokeScatterCallbacks(this, sCtx, sCtxIndex, 
                                               thruput, NULL, true);
    }
}

PRMAN_INLINE 
void RixLPEState::MoveLight(RixShadingContext const *sCtx,
                            int sCtxIndex,
                            RtColorRGB const &thruput,
                            RtColorRGB const *lgtTrans, 
                            bool firstContribution, 
                            RixLPEToken lgtGrpToken)
{
    move(RixLPE::k_LIGHT, RixLPE::k_BLANK, lgtGrpToken);

    if (m_rixCustomLPE)
    {
        m_rixCustomLPE->InvokeScatterCallbacks(this, sCtx, sCtxIndex, thruput, 
                                               lgtTrans, firstContribution);
    }
}

PRMAN_INLINE 
void RixLPEState::MoveVertex(RixShadingContext const *sCtx,
                             int sCtxIndex,
                             RixBXLobeSampled lobeSampled,
                             RtColorRGB const &thruput,
                             bool firstContribution, 
                             RixLPEToken lpeGrpId)
{
    if (lobeSampled.GetValid())
    {
        RixLPEToken scatt = lobeSampled.GetDiffuse()
            ? RixLPE::k_DIFFUSE1  + lobeSampled.GetLpeId() 
            : RixLPE::k_SPECULAR1 + lobeSampled.GetLpeId();
        
        move(lobeSampled.GetReflect() ? RixLPE::k_REFLECT : RixLPE::k_TRANSMIT,
             scatt, lpeGrpId);
    }
}

PRMAN_INLINE 
void RixLPEState::MoveVertexMultiLobe
                      (RixShadingContext const *sCtx,
                       int sCtxIndex,
                       bool isReflect,
                       RixBXActiveLobeWeights const &activeLobes,
                       int weightIndex,
                       RixLPEToken lpeGrpId,
                       bool doStateTransition)
{
    if (activeLobes.GetNumDiffuseLobes() <= 0 && 
        activeLobes.GetNumSpecularLobes() <= 0)
    {
        return;
    }

    int oldState = m_state;

    RixLPEToken event = isReflect ? RixLPE::k_REFLECT : RixLPE::k_TRANSMIT;
    RixLPEToken scatt = RixLPE::k_EVERYLOBE;

    std::vector<int> const *idxs = 0;
    RtColorRGB const *masks = 0;
    
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, event);
    if (m_state >= 0)
    {
        masks = m_automata->GetMasksInState(m_state);
        m_state = m_automata->Move(m_state, scatt);
    }
    if (m_state >= 0)
    {
        idxs = m_automata->GetOutputIndexesInState(m_state);
        int n = idxs ? idxs->size() : 0;
        for (int i = 0; i < n; i++)
        {
            int idx = (*idxs)[i];
            RtColorRGB thru(0.0f);
            int ndiff = activeLobes.GetNumDiffuseLobes();
            for (int j = 0; j < ndiff; j++)
            {
                RtColorRGB const &diff = 
                    activeLobes.GetDiffuseLobe(j)[weightIndex];
                if (diff.r > 0.0f || diff.g > 0.0f || diff.b > 0.0f)
                {
                    int k = (idx * k_numScatterTypes + 
                             activeLobes.GetDiffuseLpeId(j));
                    thru += diff * masks[k];
                }
            }

            int nspec = activeLobes.GetNumSpecularLobes();
            for (int j = 0; j < nspec; j++)
            {
                RtColorRGB const &spec = 
                    activeLobes.GetSpecularLobe(j)[weightIndex];
                if (spec.r > 0.0f || spec.g > 0.0f || spec.b > 0.0f)
                {
                    int k = (idx * k_numScatterTypes + 
                             activeLobes.GetSpecularLpeId(j) + 
                             k_RixBXMaxNumDiffuseLobes);
                    thru += spec * masks[k];
                }
            }

            m_thruput[idx] *= thru;
        }
        if (lpeGrpId != RixLPE::k_BLANK)
            m_state = m_automata->Move(m_state, lpeGrpId);
    }
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, RixLPE::k_STOP);

    if (!doStateTransition)
    {
        m_state = oldState;
    }
}
        
PRMAN_INLINE ChanIdVec const * RixLPEState::GetChannelIds()
{
    if (m_state >= 0)
        return m_automata->GetChannelIdsInState(m_state);
    else
        return 0;
}
        
PRMAN_INLINE std::vector<int> const * RixLPEState::GetOutputIndexes()
{
    if (m_state >= 0)
        return m_automata->GetOutputIndexesInState(m_state);
    else
        return 0;
}
        
PRMAN_INLINE std::vector<int> const * RixLPEState::GetFlags()
{
    if (m_state >= 0)
        return m_automata->GetFlagsInState(m_state);
    else
        return 0;
}
        
PRMAN_INLINE std::vector<int> const * RixLPEState::GetCustomIds()
{
    if (m_state >= 0)
        return m_automata->GetCustomIdsInState(m_state);
    else
        return 0;
}
        
PRMAN_INLINE RtColorRGB const * RixLPEState::GetThruputs()
{
    return m_thruput;
}
        
PRMAN_INLINE RixCustomLPE * RixLPEState::GetRixCustomLPE()
{
    return m_rixCustomLPE;
}

PRMAN_INLINE 
void RixLPEState::move(RixLPEToken event)
{
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, event);
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, RixLPE::k_STOP);
}

PRMAN_INLINE 
void RixLPEState::move(RixLPEToken event, RixLPEToken scatt)
{
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, event);
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, scatt);
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, RixLPE::k_STOP);
}
        
PRMAN_INLINE 
void RixLPEState::move(RixLPEToken event, RixLPEToken scatt, RixLPEToken id)
{
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, event);
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, scatt);
    if (m_state >= 0 && id != RixLPE::k_BLANK)
        m_state = m_automata->Move(m_state, id);
    if (m_state >= 0)
        m_state = m_automata->Move(m_state, RixLPE::k_STOP);
}

//
// class RixLPE::SplatHelper inline method implementation
//

PRMAN_INLINE
RixLPE::SplatHelper::SplatHelper(RixDisplayServices *displaySvc, 
                                 RtInt integratorCtxIdx, 
                                 RixLPE &rixLpe, RixLPEState &state, 
                                 RtInt depth, 
                                 RtInt lgtGrpId, RixLPEToken lpeGrpId, 
                                 bool isReflect, 
                                 RtColorRGB const &eyeTrans, 
                                 RtColorRGB const &lgtTrans,
                                 RixShadingContext const *sCtx, 
                                 RtInt shadingCtxIdx)
    : 
    m_depth(depth), m_lgtGrpId(lgtGrpId), m_lpeGrpId(lpeGrpId), 
    m_isReflect(isReflect), 
    m_eyeTran(eyeTrans.ChannelAvg()), 
    m_lgtTrans(lgtTrans), 
    m_displaySvc(displaySvc), m_integratorCtxIdx(integratorCtxIdx), 
    m_rixLpe(rixLpe), m_state(state), m_sCtx(sCtx), 
    m_shadingCtxIdx(shadingCtxIdx)
{
}

PRMAN_INLINE RixLPE::SplatHelper::~SplatHelper() 
{
}

PRMAN_INLINE 
void RixLPE::SplatHelper::SplatPerLobe(RixBXActiveLobeWeights &activeLobes,
                                       int weightIndex, 
                                       RtColorRGB const &shadowWeight, 
                                       RtColorRGB const &thruput, 
                                       bool isFinite, RtFloat clamp)
{
    // Obtain alpha and write Ci.
    RixLPE const &rixLpe = m_rixLpe;
    
    // Beauty channel (Ci).
    if (rixLpe.m_beautyChanId != k_invalid)
    {
        if (isFinite)
        {
            RtColorRGB sum = activeLobes.SumAtIndex(weightIndex);
            SplatBeauty(sum * thruput * m_lgtTrans * clamp, m_eyeTran);
        }
        else
        {
            SplatBeauty(RixConstants::k_ZeroRGB, m_eyeTran);
        }
    }
    
    if (rixLpe.m_anyLPEs)
    {
        // Light path expressions.
        bool isReflect = m_isReflect;
        RixLPEToken lgtGrp = (m_lgtGrpId >= 0) 
            ? RixLPE::LgtGrpIdToToken(m_lgtGrpId) : k_BLANK;
        
        bool firstShadow = true;
        bool firstContribution = true;
        // Add any non-blank diffuse lobes.
        for (int i = 0; i < activeLobes.GetNumDiffuseLobes(); i++)
        {
            RtColorRGB const &diffuse = 
                activeLobes.GetDiffuseLobe(i)[weightIndex];
            if (diffuse.r > 0.0f || diffuse.g > 0.0f || diffuse.b > 0.0f ||
                (firstShadow && rixLpe.m_anyShadowLPEs))
            {
                firstContribution = false;
                int saveState = m_state.GetState();
                RixBXLobeSampled lbe(false, false, isReflect, 
                                     activeLobes.GetDiffuseLpeId(i), 0);
                m_state.MoveVertex(m_sCtx, m_shadingCtxIdx, lbe, diffuse, 
                                   firstContribution, m_lpeGrpId);
                m_state.MoveLight(m_sCtx, m_shadingCtxIdx, diffuse,
                                  &m_lgtTrans, firstContribution, lgtGrp);
                SplatLPE(diffuse, &m_lgtTrans, m_eyeTran, isFinite, clamp);

                if (firstShadow && rixLpe.m_anyShadowLPEs)
                {
                    firstShadow = false;
                    SplatShadowLPE(&m_lgtTrans, shadowWeight, m_eyeTran, 
                                   isFinite, clamp);
                }

                m_state.SetState(saveState);
            }
        }
        
        // Add any non-blank specular lobes.
        for (int i = 0; i < activeLobes.GetNumSpecularLobes(); i++)
        {
            RtColorRGB const &specular = 
                activeLobes.GetSpecularLobe(i)[weightIndex];
            if (specular.r > 0.0f || specular.g > 0.0f || specular.b > 0.0f ||
                (firstShadow && rixLpe.m_anyShadowLPEs))
            {
                firstContribution = false;
                int saveState = m_state.GetState();
                RixBXLobeSampled lbe(false, true, isReflect, 
                                     activeLobes.GetSpecularLpeId(i), 0);
                m_state.MoveVertex(m_sCtx, m_shadingCtxIdx, lbe, specular,
                                   firstContribution, m_lpeGrpId);
                m_state.MoveLight(m_sCtx, m_shadingCtxIdx, specular, 
                                  &m_lgtTrans, firstContribution, lgtGrp);
                SplatLPE(specular, &m_lgtTrans, m_eyeTran, isFinite, clamp);

                if (firstShadow && rixLpe.m_anyShadowLPEs)
                {
                    firstShadow = false;
                    SplatShadowLPE(&m_lgtTrans, shadowWeight, m_eyeTran, 
                                   isFinite, clamp);
                }

                m_state.SetState(saveState);
            }
        }
    }
}

PRMAN_INLINE 
void RixLPE::SplatHelper::SplatEmission(RtColorRGB const &emission,
                                        RtColorRGB const &thruput, 
                                        bool isFinite, RtFloat clamp)
{
    // Obtain alpha and write Ci.
    RixLPE const &rixLpe = m_rixLpe;
    
    // Beauty channel (Ci).
    if (rixLpe.m_beautyChanId != k_invalid)
    {
        SplatBeauty(isFinite 
                    ? (emission * thruput * m_lgtTrans * clamp)
                    : RixConstants::k_ZeroRGB, m_eyeTran);
    }
    
    if (rixLpe.m_anyLPEs)
    {
        // Light path expressions.
        if (emission.r > 0.0f || emission.g > 0.0f || emission.b > 0.0f)
        {
            int saveState = m_state.GetState();
            m_state.MoveEmissiveObject(m_sCtx, m_shadingCtxIdx, 
                                       emission, m_lpeGrpId);
            SplatLPE(emission, 0, m_eyeTran, isFinite, clamp);
            m_state.SetState(saveState);
        }
    }
}

PRMAN_INLINE
void RixLPE::SplatHelper::SplatValue(RtColorRGB const &color, 
                                     RtColorRGB const &shadowWeight, 
                                     bool isFinite, RtFloat clamp)
{
    // Beauty channel (Ci).
    if (m_rixLpe.m_beautyChanId != k_invalid)
    {
        SplatBeauty(isFinite ? (color * m_lgtTrans * clamp) 
                    : RixConstants::k_ZeroRGB, m_eyeTran);
    }
    if (m_rixLpe.m_anyLPEs)
    {
        SplatLPE(color, &m_lgtTrans, m_eyeTran, isFinite, clamp);

        if (m_rixLpe.m_anyShadowLPEs)
        {
            SplatShadowLPE(&m_lgtTrans, shadowWeight, m_eyeTran, 
                           isFinite, clamp);
        }
    }
}

PRMAN_INLINE 
void RixLPE::SplatHelper::SplatBeauty(RtColorRGB const &val, 
                                      RtFloat trans) const
{
    m_displaySvc->Splat(m_rixLpe.m_beautyChanId, m_integratorCtxIdx, 
                        val, trans);
}

PRMAN_INLINE 
void RixLPE::SplatHelper::SplatLPE(RtColorRGB const &val, 
                                   RtColorRGB const *lgtTrans, 
                                   RtFloat alpha,
                                   bool isFinite, 
                                   RtFloat clamp)
{
    ChanIdVec const *chanIdVec = m_state.GetChannelIds();
    int nchans = chanIdVec ? chanIdVec->size() : 0;
    if (nchans == 0)
        return;
    
    int const *flags = &(*m_state.GetFlags())[0];
    RtColorRGB acc = val;
    RtColorRGB v = isFinite ? acc : RixConstants::k_ZeroRGB;
    
    RixChannelId const *chans = &(*chanIdVec)[0];            
    int const *idxs = &(*m_state.GetOutputIndexes())[0];
    RtColorRGB const *thruput = m_state.GetThruputs();
    for (int i = 0; i < nchans; i++)
    {
        RixChannelId id = chans[i];
        int idx = idxs[i];
        int flag = flags[i];
        RtColorRGB vFlag = v;
        RtColorRGB const &thru = thruput[idx];

        if (flag & k_FlagShadowsOnly)
            continue;
        if ((flag & k_FlagWithOcclusion) && lgtTrans)
            vFlag *= (*lgtTrans);
        if (flag & k_FlagWithThruput)
            vFlag *= thru;
        if (flag & k_FlagWithClamp)
            vFlag *= clamp;
   
        m_displaySvc->Splat(id, m_integratorCtxIdx, vFlag, alpha);
    }
}

PRMAN_INLINE 
void RixLPE::SplatHelper::SplatShadowLPE(RtColorRGB const *lgtTrans, 
                                         RtColorRGB const &shadowWeight, 
                                         RtFloat alpha,
                                         bool isFinite, 
                                         RtFloat clamp)
{
    ChanIdVec const *chanIdVec = m_state.GetChannelIds();
    int nchans = chanIdVec ? chanIdVec->size() : 0;
    if (nchans == 0)
        return;
    
    int const *flags = &(*m_state.GetFlags())[0];
    
    RixChannelId const *chans = &(*chanIdVec)[0];            
    int const *idxs = &(*m_state.GetOutputIndexes())[0];
    RtColorRGB const *thruput = m_state.GetThruputs();
    for (int i = 0; i < nchans; i++)
    {
        RixChannelId id = chans[i];
        int idx = idxs[i];
        int flag = flags[i];
        RtColorRGB vFlag(0.0f);
        RtColorRGB const &thru = thruput[idx];

        if (flag & k_FlagShadowsOnly)
        {
            if (lgtTrans)
                vFlag = (RixConstants::k_OneRGB - (*lgtTrans)) * shadowWeight;
            else
                vFlag = RixConstants::k_ZeroRGB;

            if (flag & k_FlagWithThruput)
                vFlag *= thru.Luminance();
        }
 
        m_displaySvc->Splat(id, m_integratorCtxIdx, vFlag, alpha);
    }
}

//
// class RixLPEAutomata inline method implementation.
//

PRMAN_INLINE int RixLPEAutomata::Move(int state, RixLPEToken symbol) const
{
    const TransitionSet &transSet = m_transSet[state];
    if (symbol >= 0 && symbol < RixLPE::k_numBuiltinTokens)
    {
        // Use the per-state transition lookup table.
        return transSet.m_builtinTrans[symbol];
    }
    else
    {
        // Otherwise, use binary search.
        const Transition *begin = &m_trans[transSet.m_firstTrans];
        const Transition *end = begin + transSet.m_numTrans;
        while (begin < end) 
        { 
            // binary search
            const Transition *middle = begin + ((end - begin)>>1);
            if (symbol < middle->m_symbol)
                end = middle;
            else if (middle->m_symbol < symbol)
                begin = middle + 1;
            else // match
                return middle->m_state;
        }
        return transSet.m_wildcardTrans;
    }
}

PRMAN_INLINE 
ChanIdVec const * RixLPEAutomata::GetChannelIdsInState(int state) const
{
    return &m_stateToChanIds[state];
}

PRMAN_INLINE 
std::vector<int> const * RixLPEAutomata::GetOutputIndexesInState
                                             (int state) const
{
    return &m_stateToOutIdxs[state];
}

PRMAN_INLINE 
std::vector<int> const * RixLPEAutomata::GetFlagsInState
                                             (int state) const
{
    return &m_stateToFlags[state];
}

PRMAN_INLINE 
std::vector<int> const * RixLPEAutomata::GetCustomIdsInState
                                             (int state) const
{
    return &m_stateToCustomIds[state];
}

PRMAN_INLINE 
RtColorRGB const * RixLPEAutomata::GetMasksInState(int state) const
{
    int idx = (state * m_numChannels * k_numScatterTypes);
    return &m_stateToMasks[idx];
}

PRMAN_INLINE void RixLPEAutomata::AppendTransition(Transition &t) 
{
    m_trans.push_back(t); 
}

PRMAN_INLINE void RixLPEAutomata::AppendTransitionSet(TransitionSet &s) 
{
    m_transSet.push_back(s); 
}

PRMAN_INLINE void RixLPEAutomata::ClearTransition() 
{
    m_trans.clear();
}

PRMAN_INLINE void RixLPEAutomata::ClearTransitionSet() 
{
    m_transSet.clear();
}

PRMAN_INLINE 
void RixLPEAutomata::SetStateToChanIds(std::vector<ChanIdVec> const &v) 
{
    m_stateToChanIds = v; 
}

PRMAN_INLINE 
void RixLPEAutomata::SetStateToOutputIndexes
                         (std::vector<std::vector<int> > const &v) 
{
    m_stateToOutIdxs = v; 
}

PRMAN_INLINE 
void RixLPEAutomata::SetStateToFlags
                         (std::vector<std::vector<int> > const &v) 
{
    m_stateToFlags = v; 
}

PRMAN_INLINE 
void RixLPEAutomata::SetStateToCustomIds
                         (std::vector<std::vector<int> > const &v) 
{
    m_stateToCustomIds = v; 
}

PRMAN_INLINE 
void RixLPEAutomata::SetStateToMasks(std::vector<RtColorRGB> const &v)
{
    m_stateToMasks = v;
}

PRMAN_INLINE
int  RixLPEAutomata::GetNumChannels() const
{
    return m_numChannels;
}

PRMAN_INLINE 
void RixLPEAutomata::SetNumChannels(int n)
{
    m_numChannels = n;
}

#endif
