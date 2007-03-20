/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author:  Christiam Camacho
*
*/

/// @file blast_aux.cpp
/// Implements C++ wrapper classes for structures in algo/blast/core as well as
/// some auxiliary functions to convert CSeq_loc to/from BlastMask structures.

#include <ncbi_pch.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>

#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/seqport_util.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/core/blast_seqsrc_impl.h>
#include "blast_setup.hpp"
#include "blast_aux_priv.hpp"

#include <algorithm>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

#ifndef SKIP_DOXYGEN_PROCESSING

void
CQuerySetUpOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/)
    const
{
    ddc.SetFrame("CQuerySetUpOptions");
    if (!m_Ptr)
        return;

    if (m_Ptr->filtering_options)
    {
       ddc.Log("mask_at_hash", m_Ptr->filtering_options->mask_at_hash);
       if (m_Ptr->filtering_options->dustOptions)
       {
          SDustOptions* dustOptions = m_Ptr->filtering_options->dustOptions;
          ddc.Log("dust_level", dustOptions->level);
          ddc.Log("dust_window", dustOptions->window);
          ddc.Log("dust_linker", dustOptions->linker);
       }
       else if (m_Ptr->filtering_options->segOptions)
       {
          SSegOptions* segOptions = m_Ptr->filtering_options->segOptions;
          ddc.Log("seg_window", segOptions->window);
          ddc.Log("seg_locut", segOptions->locut);
          ddc.Log("seg_hicut", segOptions->hicut);
       }
       else if (m_Ptr->filtering_options->repeatFilterOptions)
       {
          ddc.Log("repeat_database", m_Ptr->filtering_options->repeatFilterOptions->database);
       }
    }
    else if (m_Ptr->filter_string)
       ddc.Log("filter_string", m_Ptr->filter_string);

    ddc.Log("strand_option", m_Ptr->strand_option);
    ddc.Log("genetic_code", m_Ptr->genetic_code);
}

void
CBLAST_SequenceBlk::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBLAST_SequenceBlk");
    if (!m_Ptr)
        return;

    ddc.Log("sequence", m_Ptr->sequence);
    ddc.Log("sequence_start", m_Ptr->sequence_start);
    ddc.Log("sequence_allocated", m_Ptr->sequence_allocated);
    ddc.Log("sequence_start_allocated", m_Ptr->sequence_start_allocated);
    ddc.Log("length", m_Ptr->length);

}

void
CBlastQueryInfo::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastQueryInfo");
    if (!m_Ptr)
        return;

    ddc.Log("first_context", m_Ptr->first_context);
    ddc.Log("last_context", m_Ptr->last_context);
    ddc.Log("num_queries", m_Ptr->num_queries);
    ddc.Log("max_length", m_Ptr->max_length);

    for (Int4 i = m_Ptr->first_context; i <= m_Ptr->last_context; i++) {
        const string prefix = string("context[") + NStr::IntToString(i) + 
            string("].");
        ddc.Log(prefix+string("query_offset"), m_Ptr->contexts[i].query_offset);
        ddc.Log(prefix+string("query_length"), m_Ptr->contexts[i].query_length);
        ddc.Log(prefix+string("eff_searchsp"), m_Ptr->contexts[i].eff_searchsp);
        ddc.Log(prefix+string("length_adjustment"), 
                m_Ptr->contexts[i].length_adjustment);
        ddc.Log(prefix+string("query_index"), m_Ptr->contexts[i].query_index);
        ddc.Log(prefix+string("frame"), m_Ptr->contexts[i].frame);
        ddc.Log(prefix+string("is_valid"), m_Ptr->contexts[i].is_valid);
    }
}

void
CLookupTableOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CLookupTableOptions");
    if (!m_Ptr)
        return;

    ddc.Log("threshold", m_Ptr->threshold);
    ddc.Log("lut_type", m_Ptr->lut_type);
    ddc.Log("word_size", m_Ptr->word_size);
    ddc.Log("mb_template_length", m_Ptr->mb_template_length);
    ddc.Log("mb_template_type", m_Ptr->mb_template_type);
}

void
CLookupTableWrap::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CLookupTableWrap");
    if (!m_Ptr)
        return;

}
void
CBlastInitialWordOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("BlastInitialWordOptions");
    if (!m_Ptr)
        return;

    ddc.Log("window_size", m_Ptr->window_size);
    ddc.Log("ungapped_extension", m_Ptr->ungapped_extension);
    ddc.Log("x_dropoff", m_Ptr->x_dropoff);
}
void
CBlastInitialWordParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastInitialWordParameters");
    if (!m_Ptr)
        return;

}
void
CBlast_ExtendWord::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlast_ExtendWord");
    if (!m_Ptr)
        return;

}

void
CBlastExtensionOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastExtensionOptions");
    if (!m_Ptr)
        return;

    ddc.Log("gap_x_dropoff", m_Ptr->gap_x_dropoff);
    ddc.Log("gap_x_dropoff_final", m_Ptr->gap_x_dropoff_final);
    ddc.Log("ePrelimGapExt", m_Ptr->ePrelimGapExt);
    ddc.Log("eTbackExt", m_Ptr->eTbackExt);
}

void
CBlastExtensionParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastExtensionParameters");
    if (!m_Ptr)
        return;

    ddc.Log("gap_x_dropoff", m_Ptr->gap_x_dropoff);
    ddc.Log("gap_x_dropoff_final", m_Ptr->gap_x_dropoff_final);
}

void
CBlastHitSavingOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastHitSavingOptions");
    if (!m_Ptr)
        return;

    ddc.Log("hitlist_size", m_Ptr->hitlist_size);
    ddc.Log("hsp_num_max", m_Ptr->hsp_num_max);
    ddc.Log("total_hsp_limit", m_Ptr->total_hsp_limit);
    ddc.Log("hsp_range_max", m_Ptr->hsp_range_max);
    ddc.Log("culling_limit", m_Ptr->culling_limit);
    ddc.Log("expect_value", m_Ptr->expect_value);
    ddc.Log("cutoff_score", m_Ptr->cutoff_score);
    ddc.Log("percent_identity", m_Ptr->percent_identity);
    ddc.Log("do_sum_stats", m_Ptr->do_sum_stats);
    ddc.Log("longest_intron", m_Ptr->longest_intron);
}
void
CBlastHitSavingParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastHitSavingParameters");
    if (!m_Ptr)
        return;

}
void
CPSIBlastOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CPSIBlastOptions");
    if (!m_Ptr)
        return;

    ddc.Log("pseudo_count", m_Ptr->pseudo_count);
}

void
CBlastGapAlignStruct::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastGapAlignStruct");
    if (!m_Ptr)
        return;

}

void
CBlastEffectiveLengthsOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastEffectiveLengthsOptions");
    if (!m_Ptr)
        return;

    ddc.Log("db_length", (unsigned long)m_Ptr->db_length); // Int8
    ddc.Log("dbseq_num", m_Ptr->dbseq_num);
    for (Int4 i = 0; i < m_Ptr->num_searchspaces; i++) {
        const string prefix = string("searchsp[") + NStr::IntToString(i) + 
            string("]");
        ddc.Log(prefix, m_Ptr->searchsp_eff[i]);
    }
}

void
CBlastEffectiveLengthsParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastEffectiveLengthsParameters");
    if (!m_Ptr)
        return;

    ddc.Log("real_db_length", (unsigned long)m_Ptr->real_db_length); // Int8
    ddc.Log("real_num_seqs", m_Ptr->real_num_seqs);
}

void
CBlastScoreBlk::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastScoreBlk");
    if (!m_Ptr)
        return;

    ddc.Log("protein_alphabet", m_Ptr->protein_alphabet);
    ddc.Log("alphabet_size", m_Ptr->alphabet_size);
    ddc.Log("alphabet_start", m_Ptr->alphabet_start);
    ddc.Log("loscore", m_Ptr->loscore);
    ddc.Log("hiscore", m_Ptr->hiscore);
    ddc.Log("penalty", m_Ptr->penalty);
    ddc.Log("reward", m_Ptr->reward);
    ddc.Log("scale_factor", m_Ptr->scale_factor);
    ddc.Log("read_in_matrix", m_Ptr->read_in_matrix);
    ddc.Log("number_of_contexts", m_Ptr->number_of_contexts);
    ddc.Log("name", m_Ptr->name);
    ddc.Log("ambig_size", m_Ptr->ambig_size);
    ddc.Log("ambig_occupy", m_Ptr->ambig_occupy);
}

void
CBlastScoringOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastScoringOptions");
    if (!m_Ptr)
        return;

    ddc.Log("matrix", m_Ptr->matrix);
    ddc.Log("reward", m_Ptr->reward);
    ddc.Log("penalty", m_Ptr->penalty);
    ddc.Log("gapped_calculation", m_Ptr->gapped_calculation);
    ddc.Log("gap_open", m_Ptr->gap_open);
    ddc.Log("gap_extend", m_Ptr->gap_extend);
    ddc.Log("shift_pen", m_Ptr->shift_pen);
    ddc.Log("decline_align", m_Ptr->decline_align);
    ddc.Log("is_ooframe", m_Ptr->is_ooframe);
}

void
CBlastScoringParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*d*/)
    const
{
	ddc.SetFrame("CBlastScoringParameters");
    if (!m_Ptr)
        return;

    ddc.Log("reward", m_Ptr->reward);
    ddc.Log("penalty", m_Ptr->penalty);
    ddc.Log("gap_open", m_Ptr->gap_open);
    ddc.Log("gap_extend", m_Ptr->gap_extend);
    ddc.Log("decline_align", m_Ptr->decline_align);
    ddc.Log("shift_pen", m_Ptr->shift_pen);
    ddc.Log("scale_factor", m_Ptr->scale_factor);
}

void
CBlastDatabaseOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastDatabaseOptions");
    if (!m_Ptr)
        return;

}

void
CPSIMatrix::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CPSIMatrix");
    if (!m_Ptr)
        return;

    ddc.Log("ncols", m_Ptr->ncols);
    ddc.Log("nrows", m_Ptr->nrows);
    ddc.Log("lambda", m_Ptr->lambda);
    ddc.Log("kappa", m_Ptr->kappa);
    ddc.Log("h", m_Ptr->h);
    // pssm omitted because it might be too large!
}

void
CPSIDiagnosticsRequest::DebugDump(CDebugDumpContext ddc, 
                                   unsigned int /*depth*/) const
{
    ddc.SetFrame("CPSIDiagnosticsRequest");
    if (!m_Ptr)
        return;

    ddc.Log("information_content", m_Ptr->information_content);
    ddc.Log("residue_frequencies", m_Ptr->residue_frequencies);
    ddc.Log("weighted_residue_frequencies", 
            m_Ptr->weighted_residue_frequencies);
    ddc.Log("frequency_ratios", m_Ptr->frequency_ratios);
    ddc.Log("gapless_column_weights", m_Ptr->gapless_column_weights);
}

void
CPSIDiagnosticsResponse::DebugDump(CDebugDumpContext ddc, 
                                   unsigned int /*depth*/) const
{
    ddc.SetFrame("CPSIDiagnosticsResponse");
    if (!m_Ptr)
        return;

    ddc.Log("alphabet_size", m_Ptr->alphabet_size);
}

void
CBlastSeqSrc::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastSeqSrc");
    if (!m_Ptr)
        return;

    /** @todo should the BlastSeqSrc API support names for types of
     * BlastSeqSrc? Might be useful for debugging */
}

void
CBlastSeqSrcIterator::DebugDump(CDebugDumpContext ddc, 
                                unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastSeqSrcIterator");
    if (!m_Ptr)
        return;

    string iterator_type;
    switch (m_Ptr->itr_type) {
    case eOidList: iterator_type = "oid_list"; break;
    case eOidRange: iterator_type = "oid_range"; break;
    default: abort();
    }

    ddc.Log("itr_type", iterator_type);
    ddc.Log("current_pos", m_Ptr->current_pos);
    ddc.Log("chunk_sz", m_Ptr->chunk_sz);
}

void
CBlast_Message::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlast_Message");
    if (!m_Ptr)
        return;

    ddc.Log("severity", m_Ptr->severity);
    ddc.Log("message", m_Ptr->message);
    // code and subcode are unused
}

void
CBlastHSPResults::DebugDump(CDebugDumpContext ddc, 
                            unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastHSPResults");
    if (!m_Ptr)
        return;

    ddc.Log("num_queries", m_Ptr->num_queries);
    // hitlist itself is not printed
}

void 
CBlastMaskLoc::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastMaskLoc");
    if (!m_Ptr)
        return;
   
    ddc.Log("total_size", m_Ptr->total_size);
    for (int index = 0; index < m_Ptr->total_size; ++index) {
        ddc.Log("context", index);
        for (BlastSeqLoc* seqloc = m_Ptr->seqloc_array[index];
             seqloc; seqloc = seqloc->next) {
            ddc.Log("left", seqloc->ssr->left);
            ddc.Log("right", seqloc->ssr->right);
        }
    }
}

void 
CBlastSeqLoc::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastSeqLoc");
    if (!m_Ptr)
        return;
   
    for (BlastSeqLoc* tmp = m_Ptr; tmp; tmp = tmp->next) {
        ddc.Log("left", tmp->ssr->left);
        ddc.Log("right", tmp->ssr->right);
    }
}

void
CSBlastProgress::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("SBlastProgress");
    if (!m_Ptr)
        return;

    ddc.Log("stage", m_Ptr->stage);
    ddc.Log("user_data", m_Ptr->user_data);
}

#endif /* SKIP_DOXYGEN_PROCESSING */

BlastSeqLoc*
CSeqLoc2BlastSeqLoc(const objects::CSeq_loc* slp)
{
    if (!slp || 
        slp->Which() == CSeq_loc::e_not_set || 
        slp->IsEmpty() || 
        slp->IsNull() ) {
        return NULL;
    }

    _ASSERT(slp->IsInt() || slp->IsPacked_int() || slp->IsMix());

    CBlastSeqLoc retval;

    if (slp->IsInt()) {
        BlastSeqLocNew(&retval, slp->GetInt().GetFrom(), slp->GetInt().GetTo());
    } else if (slp->IsPacked_int()) {
        ITERATE(CPacked_seqint::Tdata, itr, slp->GetPacked_int().Get()) {
            BlastSeqLocNew(&retval, (*itr)->GetFrom(), (*itr)->GetTo());
        }
    } else if (slp->IsMix()) {
        ITERATE(CSeq_loc_mix::Tdata, itr, slp->GetMix().Get()) {
            if ((*itr)->IsInt()) {
                BlastSeqLocNew(&retval, (*itr)->GetInt().GetFrom(), 
                                        (*itr)->GetInt().GetTo());
            } else if ((*itr)->IsPnt()) {
                BlastSeqLocNew(&retval, (*itr)->GetPnt().GetPoint(), 
                                        (*itr)->GetPnt().GetPoint());
            }
        }
    } else {
        NCBI_THROW(CBlastException, eNotSupported, 
                   "Unsupported CSeq_loc type");
    }

    return retval.Release();
}

TAutoUint1ArrayPtr
FindGeneticCode(int genetic_code)
{
    Uint1* retval = NULL;

    // handle the sentinel value which indicates that the genetic code is not
    // applicable
    if (static_cast<Uint4>(genetic_code) == numeric_limits<Uint4>::max()) {
        return retval;
    }

    CSeq_data gc_ncbieaa(CGen_code_table::GetNcbieaa(genetic_code),
            CSeq_data::e_Ncbieaa);
    CSeq_data gc_ncbistdaa;

    TSeqPos nconv = CSeqportUtil::Convert(gc_ncbieaa, &gc_ncbistdaa,
            CSeq_data::e_Ncbistdaa);
    if (nconv == 0) {
        return retval;
    }

    _ASSERT(gc_ncbistdaa.IsNcbistdaa());
    _ASSERT(nconv == gc_ncbistdaa.GetNcbistdaa().Get().size());

    try {
        retval = new Uint1[nconv];
    } catch (const bad_alloc&) {
        return NULL;
    }

    for (TSeqPos i = 0; i < nconv; i++)
        retval[i] = gc_ncbistdaa.GetNcbistdaa().Get()[i];

    return retval;
}

EBlastProgramType
EProgramToEBlastProgramType(EProgram p)
{
    switch (p) {
    case eBlastn:
    case eMegablast:
    case eDiscMegablast:
        return eBlastTypeBlastn;
        
    case eBlastp:
        return eBlastTypeBlastp;
        
    case eBlastx:
        return eBlastTypeBlastx;
        
    case eTblastn:
        return eBlastTypeTblastn;
        
    case eTblastx:
        return eBlastTypeTblastx;
        
    case eRPSBlast:
        return eBlastTypeRpsBlast;
        
    case eRPSTblastn:
        return eBlastTypeRpsTblastn;
        
    case ePSIBlast:
        return eBlastTypePsiBlast;
        
    case ePHIBlastp:
        return eBlastTypePhiBlastp;
        
    case ePHIBlastn:
        return eBlastTypePhiBlastn;
        
    default:
        return eBlastTypeUndefined;
    }
}

EProgram ProgramNameToEnum(const std::string& program_name)
{
    _ASSERT( !program_name.empty() );

    string lowercase_program_name(program_name);
    lowercase_program_name = NStr::ToLower(lowercase_program_name);

    if (lowercase_program_name == "blastn") {
        return eBlastn;
    } else if (lowercase_program_name == "blastp") {
        return eBlastp;
    } else if (lowercase_program_name == "blastx") {
        return eBlastx;
    } else if (lowercase_program_name == "tblastn") {
        return eTblastn;
    } else if (lowercase_program_name == "tblastx") {
        return eTblastx;
    } else if (lowercase_program_name == "rpsblast") {
        return eRPSBlast;
    } else if (lowercase_program_name == "rpstblastn") {
        return eRPSTblastn;
    } else if (lowercase_program_name == "megablast") {
        return eMegablast; 
    } else if (lowercase_program_name == "psiblast") {
        return ePSIBlast;
    } else {
        // Handle discontiguous megablast (no established convention AFAIK)
        string::size_type idx_mb = lowercase_program_name.find("megablast");
        string::size_type idx_disco = lowercase_program_name.find("disc");
        if (idx_mb != string::npos && idx_disco != string::npos) {
            return eDiscMegablast;
        }
        
        // Handle others ...
    }
    NCBI_THROW(CBlastException, eNotSupported, 
               "Program type '" + program_name + "' not supported");
}

string Blast_ProgramNameFromType(EBlastProgramType program)
{
    char* program_string(0);
    if (BlastNumber2Program(program, &program_string) == 0) {
        string retval(program_string);
        sfree(program_string);
        return retval;
    } else {
        return NcbiEmptyString;
    }
}

template <class Position>
CRange<Position> Map(const CRange<Position>& target,
                     const CRange<Position>& range)
{
    if (target.Empty()) {
        throw std::runtime_error("Target range is empty");
    } 
    
    if (range.Empty() || 
        (range.GetFrom() > target.GetTo()) ||
        ((range.GetFrom() + target.GetFrom()) > target.GetTo())) {
        return target;
    }

    CRange<Position> retval;
    retval.SetFrom(max(target.GetFrom() + range.GetFrom(), target.GetFrom()));
    retval.SetTo(min(target.GetFrom() + range.GetTo(), target.GetTo()));
    return retval;
}

/// Return the masked locations for a given query as well as whether the
/// linked list's elements should be reverted or not (true in the case of
/// negative only strand)
/// The first element of the returned pair is the linked list of masked
/// locations
/// The second element of the returned pair is true if the linked list needs to
/// be reversed
static pair<BlastSeqLoc*, bool>
s_GetBlastnMask(const BlastMaskLoc* mask, unsigned int query_index)
{
    const unsigned int kNumContexts = GetNumberOfContexts(eBlastTypeBlastn);
    _ASSERT(query_index*kNumContexts < (unsigned int)mask->total_size);

    unsigned int context_index(query_index * kNumContexts);

    BlastSeqLoc* core_seqloc(0);
    bool needs_reversing(false);
    // N.B.: The elements of the seqloc_array corresponding to reverse
    // strands are all NULL except in a reverse-strand-only search
    if ( !(core_seqloc = mask->seqloc_array[context_index++])) {
        core_seqloc = mask->seqloc_array[context_index];
        needs_reversing = true;
    }
    return make_pair(core_seqloc, needs_reversing);
}

/// Convert EBlastTypeBlastn CORE masks into TSeqLocInfoVector
static void s_ConvertBlastnMasks(const CPacked_seqint::Tdata& query_intervals,
                                 const BlastMaskLoc* mask,
                                 TSeqLocInfoVector& retval)
{
    unsigned int i(0);
    ITERATE(CPacked_seqint::Tdata, query_interval, query_intervals) {
        const TSeqRange kTarget((*query_interval)->GetFrom(), 
                                (*query_interval)->GetTo());

        TMaskedQueryRegions query_masks;
        pair<BlastSeqLoc*, bool> loc_aux = s_GetBlastnMask(mask, i++);
        for (BlastSeqLoc* loc = loc_aux.first; loc; loc = loc->next) {
            TSeqRange masked_range(loc->ssr->left, loc->ssr->right);
            TSeqRange range(Map(kTarget, masked_range));
            if (range.NotEmpty() && range != kTarget) {
                CRef<CSeq_interval> seqint(new CSeq_interval);
                seqint->SetId().Assign((*query_interval)->GetId());
                seqint->SetFrom(range.GetFrom());
                seqint->SetTo(range.GetTo());
                CRef<CSeqLocInfo> seqlocinfo
                    (new CSeqLocInfo(seqint, CSeqLocInfo::eFrameNotSet));
                query_masks.push_back(seqlocinfo);
            }
        }
        if (loc_aux.second) {
            reverse(query_masks.begin(), query_masks.end());
        }
        retval.push_back(query_masks);
    }
}

void 
Blast_GetSeqLocInfoVector(EBlastProgramType program, 
                          const CPacked_seqint& queries,
                          const BlastMaskLoc* mask, 
                          TSeqLocInfoVector& mask_v)
{
    _ASSERT(mask);
    const unsigned int kNumContexts = GetNumberOfContexts(program);
    const CPacked_seqint::Tdata& query_intervals = queries.Get();

    if (query_intervals.size() != mask->total_size/kNumContexts) {
        string msg = "Blast_GetSeqLocInfoVector: number of query ids " +
            NStr::IntToString(query_intervals.size()) + 
            " not equal to number of queries in mask " + 
            NStr::IntToString(mask->total_size/kNumContexts);
        NCBI_THROW(CBlastException, eInvalidArgument, msg);
    }

    if (program == eBlastTypeBlastn) {
        s_ConvertBlastnMasks(query_intervals, mask, mask_v);
        return;
    }

    unsigned int qindex(0);
    ITERATE(CPacked_seqint::Tdata, query_interval, query_intervals) {

        const TSeqRange kTarget((*query_interval)->GetFrom(),
                                (*query_interval)->GetTo());
        TMaskedQueryRegions query_masks;
        for (unsigned int index = 0; index < kNumContexts; index++) {

            BlastSeqLoc* loc = mask->seqloc_array[qindex*kNumContexts+index];
            for ( ; loc; loc = loc->next) {
                TSeqRange masked_range(loc->ssr->left, loc->ssr->right);
                TSeqRange range(Map(kTarget, masked_range));
                if (range.NotEmpty() && range != kTarget) {
                    int frame = BLAST_ContextToFrame(program, index);
                    if (frame == INT1_MAX) {
                        string msg("Conversion from context to frame failed ");
                        msg += "for '" + Blast_ProgramNameFromType(program) 
                            + "'";
                        NCBI_THROW(CBlastException, eCoreBlastError, msg);
                    }
                    CRef<CSeq_interval> seqint(new CSeq_interval);
                    seqint->SetId().Assign((*query_interval)->GetId());
                    seqint->SetFrom(range.GetFrom());
                    seqint->SetTo(range.GetTo());
                    CRef<CSeqLocInfo> seqloc_info
                        (new CSeqLocInfo(seqint, frame));
                    query_masks.push_back(seqloc_info);
                }
            }
        }
        mask_v.push_back(query_masks);
        qindex++;
    }
}

//
// TSearchMessages
//

void
TQueryMessages::SetQueryId(const string& id)
{
    m_IdString = id;
}

string
TQueryMessages::GetQueryId() const
{
    return m_IdString;
}

void
TQueryMessages::Combine(const TQueryMessages& other)
{
    // Combine the Seq-id's
    if (m_IdString.empty()) {
        m_IdString = other.m_IdString;
    } else {
        if ( !other.m_IdString.empty() ) {
            _ASSERT(m_IdString == other.m_IdString);
        }
    }

    if ((*this).empty()) {
        *this = other;
        return;
    }

    copy(other.begin(), other.end(), back_inserter(*this));
}

//
// TSearchMessages
//

bool
TSearchMessages::HasMessages() const
{
    ITERATE(vector<TQueryMessages>, qm, *this) {
        if ( !qm->empty() ) {
            return true;
        }
    }
    return false;
}

string
TSearchMessages::ToString() const
{
    string retval;
    ITERATE(vector<TQueryMessages>, qm, *this) {
        if (qm->empty()) {
            continue;
        }
        ITERATE(TQueryMessages, msg, *qm) {
            retval += (*msg)->GetMessage() + " ";
        }
    }
    return retval;
}

void
TSearchMessages::Combine(const TSearchMessages& other)
{
    if (empty()) {
        *this = other;
        return;
    }

    for (size_t i = 0; i < other.size(); i++) {
        (*this)[i].Combine(other[i]);
    }

    RemoveDuplicates();
}

void
TSearchMessages::RemoveDuplicates()
{
    NON_CONST_ITERATE(TSearchMessages, sm, (*this)) {
        if (sm->empty()) {
            continue;
        }
        sort(sm->begin(), sm->end(), TQueryMessagesLessComparator());
        TQueryMessages::iterator new_end = 
            unique(sm->begin(), sm->end(), TQueryMessagesEqualComparator());
        sm->erase(new_end, sm->end());
    }
}

//
// TMaskedQueryRegions
//

TMaskedQueryRegions
TMaskedQueryRegions::RestrictToSeqInt(const CSeq_interval& location) const
{
    TMaskedQueryRegions retval;

    TSeqRange loc(location.GetFrom(), 0);
    loc.SetToOpen(location.GetTo());

    ITERATE(TMaskedQueryRegions, maskinfo, *this) {
        TSeqRange range = loc.IntersectionWith(**maskinfo);
        if (range.NotEmpty()) {
            const CSeq_interval& intv = (*maskinfo)->GetInterval();
            const ENa_strand kStrand = intv.CanGetStrand() 
                ? intv.GetStrand() : eNa_strand_unknown;
            CRef<CSeq_interval> si
                (new CSeq_interval(const_cast<CSeq_id&>(intv.GetId()), 
                                   range.GetFrom(), 
                                   range.GetToOpen(), 
                                   kStrand));
            CRef<CSeqLocInfo> sli(new CSeqLocInfo(si, 
                                                  (*maskinfo)->GetFrame()));
            retval.push_back(sli);
        }
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
