/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'seq.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

#include <iterator>
// generated includes
#include <objects/seq/Linkage_evidence.hpp>

// generated classes

// user-added includes
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CLinkage_evidence::~CLinkage_evidence(void)
{
}

bool CLinkage_evidence::GetLinkageEvidence (
    TLinkage_evidence& output_result, 
    const string& linkage_evidence )
{
    // basically just a wrapper for the other function which does
    // the convenience of splitting the string
    vector<string> linkage_evidence_vec;
    NStr::Split(linkage_evidence, ";", linkage_evidence_vec);
    return GetLinkageEvidence( output_result, linkage_evidence_vec );
}


bool CLinkage_evidence::GetLinkageEvidence(
    TLinkage_evidence& output_result, 
    const vector<string> &linkage_evidence )
{
    static const map<string, EType> kStringToEType {
        {"paired-ends",        eType_paired_ends},
        {"align_genus",        eType_align_genus},
        {"align_xgenus",       eType_align_xgenus},
        {"align_trnscpt",      eType_align_trnscpt},
        {"within_clone",       eType_within_clone},
        {"clone_contig",       eType_clone_contig},
        {"map",                eType_map},
        {"strobe",             eType_strobe},
        {"unspecified",        eType_unspecified},
        {"pcr",                eType_pcr},
        {"proximity_ligation", eType_proximity_ligation}
    };

    TLinkage_evidence temp_vector;
    for (const auto& evidence : linkage_evidence) {
        
        auto it = kStringToEType.find(evidence);
        if (it == kStringToEType.end()) {
            return false;
        }
        CRef<CLinkage_evidence> new_evid( new CLinkage_evidence() );
        new_evid->SetType(it->second);
        temp_vector.push_back(move(new_evid));

    }

    output_result.insert(end(output_result),
                         make_move_iterator(begin(temp_vector)),
                         make_move_iterator(end(temp_vector)));
    return true;
}

bool CLinkage_evidence::VecToString( 
    string & output_result,
    const TLinkage_evidence & linkage_evidence )
{
    bool all_converted_okay = true;

    ITERATE( TLinkage_evidence, evid_iter, linkage_evidence ) {
        const char *evid_str = NULL;
        if( (*evid_iter)->IsSetType() ) {
            switch( (*evid_iter)->GetType() ) {
                case eType_paired_ends:
                    evid_str = "paired-ends";
                    break;
                case eType_align_genus:
                    evid_str = "align_genus";
                    break;
                case eType_align_xgenus:
                    evid_str = "align_xgenus";
                    break;
                case eType_align_trnscpt:
                    evid_str = "align_trnscpt";
                    break;
                case eType_within_clone:
                    evid_str = "within_clone";
                    break;
                case eType_clone_contig:
                    evid_str = "clone_contig";
                    break;
                case eType_map:
                    evid_str = "map";
                    break;
                case eType_strobe:
                    evid_str = "strobe";
                    break;
                case eType_unspecified:
                    evid_str = "unspecified";
                    break;
                case eType_pcr:
                    evid_str = "pcr";
                    break;
                case eType_proximity_ligation:
                    evid_str = "proximity_ligation";
                default:
                    break;
            }
        }
        if( evid_str == NULL ) {
            evid_str = "UNKNOWN";
            all_converted_okay = false;
        }
        if( ! output_result.empty() ) {
            output_result += ';';
        }
        output_result += evid_str;
    }

    return all_converted_okay;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1737, CRC32: da3bca7d */
