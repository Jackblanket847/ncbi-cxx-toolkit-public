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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>

#include <objmgr/feat_ci.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gff2_data.hpp>
#include <objtools/error_codes.hpp>

#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>


#include <algorithm>

//#include "gff3_data.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
const string* CGff2Reader::s_GetAnnotId(
    const CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if ( ! annot.CanGetId() || annot.GetId().size() != 1 ) {
        // internal error
        return 0;
    }
    
    CRef< CAnnot_id > pId = *( annot.GetId().begin() );
    if ( ! pId->IsLocal() ) {
        // internal error
        return 0;
    }
    return &pId->GetLocal().GetStr();
}

//  ----------------------------------------------------------------------------
CGff2Reader::CGff2Reader(
    int iFlags,
    const string& name,
    const string& title,
    SeqIdResolver seqidresolve):
//  ----------------------------------------------------------------------------
    CReaderBase(iFlags, name, title, seqidresolve),
    m_pErrors(0),
    mCurrentFeatureCount(0),
    mParsingAlignment(false)
{
}

//  ----------------------------------------------------------------------------
CGff2Reader::~CGff2Reader()
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------                
CRef<CSeq_annot>
CGff2Reader::ReadSeqAnnot(
    ILineReader& lr,
    ILineErrorListener* pEC ) 
//  ----------------------------------------------------------------------------                
{
    CRef<CSeq_annot> pAnnot;
    pAnnot.Reset(new CSeq_annot);

    mCurrentFeatureCount = 0;
    mParsingAlignment = false;

    map<string, list<CRef<CSeq_align>>> alignments;
    list<string> id_list;
   

    string line;
    while (xGetLine(lr, line)) {

        if (IsCanceled()) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Info,
                0,
                "Reader stopped by user.",
                ILineError::eProblem_ProgressInfo));
            ProcessError(*pErr, pEC);
            return pAnnot;
        }
        xReportProgress(pEC);
        if ( xParseStructuredComment(line)  
                &&  !NStr::StartsWith(line, "##sequence-region") ) {
            continue;
        }

        try {
            if (xIsTrackLine(line)) {
                if (!mCurrentFeatureCount) {
                    xParseTrackLine(line, pEC);
                    continue;
                }
                m_PendingLine = line;
                break;
            }
            if (xIsTrackTerminator(line)) {
                if (!mCurrentFeatureCount) {
                    xParseTrackLine("track", pEC);
                    continue;
                }
                break;
            }

            if (xNeedsNewSeqAnnot(line)) {
                break;
            }
           
            if (xParseBrowserLine(line, pAnnot, pEC)) {
                continue;
            }

            if (!xIsCurrentDataType(line)) {
                xUngetLine(lr);
                break;
            }

            if ( CGff2Reader::IsAlignmentData(line)) {
                if ((m_iFlags & fGenbankMode)  ||  
                        x_ParseAlignmentGff(line, id_list, alignments)) {
                    continue;
                }
            }

            if (xParseFeature(line, pAnnot, pEC)) {
                continue;
            }
        }
        catch (CObjReaderLineException& err) {
            ProcessError(err, pEC);
        }
    }

    if (!mCurrentFeatureCount) {
        return CRef<CSeq_annot>();
    }

    if (!alignments.empty()) {
        x_ProcessAlignmentsGff(id_list, alignments, pAnnot);
    }
    xPostProcessAnnot(pAnnot, pEC);
    return pAnnot;
}


//  --------------------------------------------------------------------------- 
void
CGff2Reader::ReadSeqAnnots(
    TAnnots& annots,
    CNcbiIstream& istr,
    ILineErrorListener* pMessageListener )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pMessageListener );
}
 
//  ---------------------------------------------------------------------------                       
void
CGff2Reader::ReadSeqAnnots(
    TAnnots& annots,
    ILineReader& lr,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    xProgressInit(lr);
    while (!lr.AtEOF()) {
        CRef<CSeq_annot> pNext = this->ReadSeqAnnot(lr, pEC);
        if (pNext) {
            annots.push_back(pNext);
        }
    }
    return;

    string line;
    while (xGetLine(lr, line)) {
        if (IsCanceled()) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Info,
                0,
                "Reader stopped by user.",
                ILineError::eProblem_ProgressInfo));
            ProcessError(*pErr, pEC);
            annots.clear();
            return;
        }
        xReportProgress(pEC);
        if ( xParseStructuredComment(line) ) {
            continue;
        }

        try {
            if (xIsTrackTerminator(line)) {
                continue;
            }
            if (x_ParseBrowserLineGff(line, m_CurrentBrowserInfo)) {
                continue;
            }
            if (xIsTrackLine(line)) {
                //completely ignore in Genbank mode
                if (m_iFlags & CGff2Reader::fGenbankMode) {
                    continue;
                }
                if (!annots.empty()) {
                    xPostProcessAnnot(annots.back(), pEC);
                }
                xParseTrackLine(line, pEC);
                CRef< CSeq_annot > pAnnot( new CSeq_annot );
                annots.push_back(pAnnot);
                continue;
            }
            if (xNeedsNewSeqAnnot(line)) {
                if (!annots.empty()) {
                    xPostProcessAnnot(annots.back(), pEC);
                }
                mCurrentFeatureCount = 0;
                mParsingAlignment = false;
                CRef< CSeq_annot > pAnnot( new CSeq_annot );
                annots.push_back(pAnnot);
                continue;
            }                
            if (x_ParseFeatureGff(line, annots, pEC)) {
                continue;
            }
        }
        catch(CObjReaderLineException& err) {
            err.SetLineNumber(m_uLineNumber);
        }
    }
    if (!annots.empty()) {
        xPostProcessAnnot(annots.back(), pEC);
    }
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_entry >
CGff2Reader::ReadSeqEntry(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    xProgressInit(lr);

    TAnnots annots;
    //ReadSeqAnnotsNew( annots, lr, pMessageListener );
    ReadSeqAnnots( annots, lr, pMessageListener );
    
    CRef<CSeq_entry> pSeqEntry(new CSeq_entry());
    pSeqEntry->SetSet();

    for (TAnnots::iterator it = annots.begin();
            it != annots.end(); ++it) {
        CRef<CBioseq> pSeq( new CBioseq() );
        pSeq->SetAnnot().push_back(*it);
        pSeq->SetId().push_back( CRef<CSeq_id>( 
            new CSeq_id(CSeq_id::e_Local, "gff-import") ) );
        pSeq->SetInst().SetRepr(CSeq_inst::eRepr_not_set);
        pSeq->SetInst().SetMol(CSeq_inst::eMol_not_set);

        CRef<CSeq_entry> pEntry(new CSeq_entry());
        pEntry->SetSeq(*pSeq);
        pSeqEntry->SetSet().SetSeq_set().push_back( pEntry );
    }
    return pSeqEntry;
}

//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CGff2Reader::ReadObject(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqEntry( lr, pMessageListener ).ReleaseOrNull() );
    return object;
}
 
//  ----------------------------------------------------------------------------
void CGff2Reader::xPostProcessAnnot(
    CRef<CSeq_annot>& pAnnot,
    ILineErrorListener *pEC)
//  ----------------------------------------------------------------------------
{
    xAssignAnnotId(pAnnot);
    if (!(m_iFlags & fGenbankMode)) {
        //xAssignTrackData(pAnnot);
        xAddConversionInfo(pAnnot, pEC);
        xGenerateParentChildXrefs(pAnnot);
    }
}

//  ----------------------------------------------------------------------------
void CGff2Reader::xAssignAnnotId(
    CRef<CSeq_annot>& pAnnot,
    const string& givenId)
//  ----------------------------------------------------------------------------
{
    if (givenId.empty() && pAnnot->GetData().IsAlign()) {
        return;
    }

    string annotId(givenId);
    if (annotId.empty() && 
        (!(m_iFlags & fGenbankMode)) &&
        m_pTrackDefaults) {
        annotId = m_pTrackDefaults->Name();
    }
    if (annotId.empty()) {
        return;
    }    
    CRef< CAnnot_id > pAnnotId(new CAnnot_id);
    pAnnotId->SetLocal().SetStr(annotId);
    pAnnot->SetId().push_back(pAnnotId);   
}


//  ----------------------------------------------------------------------------
void CGff2Reader::x_SetTrackDataToSeqEntry(
    CRef<CSeq_entry>& entry,
    CRef<CUser_object>& trackdata,
    const string& strKey,
    const string& strValue )
//  ----------------------------------------------------------------------------
{
    CSeq_descr& descr = entry->SetDescr();

    if ( strKey == "name" ) {
        CRef<CSeqdesc> name( new CSeqdesc() );
        name->SetName( strValue );
        descr.Set().push_back( name );
        return;
    }
    if ( strKey == "description" ) {
        CRef<CSeqdesc> title( new CSeqdesc() );
        title->SetTitle( strValue );
        descr.Set().push_back( title );
        return;
    }
    trackdata->AddField( strKey, strValue );
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xParseStructuredComment(
    const string& strLine)
//  ----------------------------------------------------------------------------
{
    if (NStr::StartsWith(strLine, "###")) {
        return false;
    }
    if (!NStr::StartsWith( strLine, "##")) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CGff2Reader::xParseFeature(
    const string& line,
    CRef<CSeq_annot>& pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    if (CGff2Reader::IsAlignmentData(line)) {
        return false;
    }

    //parse record:
    shared_ptr<CGff2Record> pRecord(x_CreateRecord());
    try {
        if (!pRecord->AssignFromGff(line)) {
			return false;
		}
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pEC);
        return false;
    }

    //make sure we are interested:
    if (xIsIgnoredFeatureType(pRecord->Type())) {
        return true;
    }
    if (xIsIgnoredFeatureId(pRecord->Id())) {
        return true;
    }

    //append feature to annot:
    if (!x_UpdateAnnotFeature(*pRecord, pAnnot, pEC)) {
        return false;
    }

    ++mCurrentFeatureCount;
    mParsingAlignment = false;
    return true;
}


//  ----------------------------------------------------------------------------
void CGff2Reader::x_GetAlignmentScores(const CSeq_align& alignment, 
                                       TScoreValueMap& score_values) const
//  ----------------------------------------------------------------------------
{
    // Start with empty scores
    score_values.clear();

    if (!alignment.IsSetScore()) {
        return;
    }

    for (const CRef<CScore>& score : alignment.GetScore()) {

        if (!score->IsSetId() ||
            !score->GetId().IsStr() ||
            !score->IsSetValue()) {
            continue;
        }
        const string name = score->GetId().GetStr();
        const CScore::TValue& value = score->GetValue();
        score_values[name] = Ref(new CScore::TValue());
        score_values[name]->Assign(value);
    }
}


//  ----------------------------------------------------------------------------
bool s_CompareValues(const CScore::TValue& score_val1, 
                     const CScore::TValue& score_val2)
//  ----------------------------------------------------------------------------
{

    if (score_val1.IsInt() && 
        score_val2.IsInt() &&
        score_val1.GetInt() == score_val2.GetInt()) {
        return true;
    }

    if (score_val1.IsReal() &&
        score_val2.IsReal() &&
        score_val1.GetReal() == score_val2.GetReal()) {
        return true;
    }

    return false;
}

// Result is a set of matching scores
//  ----------------------------------------------------------------------------
void CGff2Reader::x_FindMatchingScores(const TScoreValueMap& scores_1,
                                       const TScoreValueMap& scores_2,
                                       set<string>& matching_scores) const
//  ----------------------------------------------------------------------------
{
    matching_scores.clear();

    for (const auto& score1 : scores_1) {
        const string& name = score1.first;
        const CScore::TValue& value = *(score1.second);

        const auto& it = scores_2.find(name);
        if (it != scores_2.end() &&
            s_CompareValues(value, *(it->second))) {
            matching_scores.insert(name);
        }
    }
}


//  ----------------------------------------------------------------------------
void CGff2Reader::x_ProcessAlignmentsGff(const list<string>& id_list,
                            const map<string, list<CRef<CSeq_align>>>& alignments,
                            CRef<CSeq_annot> pAnnot) 
//  ----------------------------------------------------------------------------
{
    if (pAnnot.IsNull()) {
        pAnnot = Ref(new CSeq_annot());
    }

    for (const auto id : id_list) {
        CRef<CSeq_align> pAlign = Ref(new CSeq_align());
        if (x_MergeAlignments(alignments.at(id), pAlign)) {
            // if available, add current browser information
            if ( m_CurrentBrowserInfo ) {
                pAnnot->SetDesc().Set().push_back( m_CurrentBrowserInfo );
            }

            pAnnot->SetNameDesc("alignments");

            if ( !m_AnnotTitle.empty() ) {
                pAnnot->SetTitleDesc(m_AnnotTitle);
            }
            // Add alignment
            pAnnot->SetData().SetAlign().push_back(pAlign);
        }
    }
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseAlignmentGff(
    const string& strLine, 
    list<string>& id_list, // Add id to alignment
    map<string, list<CRef<CSeq_align>>>& alignments)
//  ----------------------------------------------------------------------------
{
    unique_ptr<CGff2Record> pRecord(x_CreateRecord());

    if ( !pRecord->AssignFromGff(strLine) ) {
        return false;
    }
    
    string id;
    if ( !pRecord->GetAttribute("ID", id) ) {
        id = pRecord->Id();
    }

    if (alignments.find(id) == alignments.end()) {
       id_list.push_back(id);
    }

    CRef<CSeq_align> alignment;
    if (!x_CreateAlignment(*pRecord, alignment)) {
        return false;
    }

    alignments[id].push_back(alignment);

    ++mCurrentFeatureCount;
    mParsingAlignment = true;
    return true;
}



//  ----------------------------------------------------------------------------
void CGff2Reader::x_InitializeScoreSums(const TScoreValueMap score_values, 
        map<string, TSeqPos>& summed_scores) const
//  ----------------------------------------------------------------------------
{
    const list<string> score_names {"num_ident", "num_mismatch"};
    
    for (const string& score_name : score_names) {
        if (score_values.find(score_name) != score_values.end()) {
            summed_scores[score_name] = score_values.at(score_name)->GetInt();
        }    
    }
}


//  ----------------------------------------------------------------------------
void CGff2Reader::x_ProcessAlignmentScores(const CSeq_align& alignment, 
    map<string, TSeqPos>& summed_scores, 
    TScoreValueMap& common_scores) const
//  ----------------------------------------------------------------------------
{
    const list<string> summed_score_names {"num_ident", "num_mismatch"};

    TScoreValueMap new_scores;
    x_GetAlignmentScores(alignment, new_scores);

    for (const string& score_name : summed_score_names) {
        if (new_scores.find(score_name) == new_scores.end()) {
            summed_scores.erase(score_name);
        } else if (summed_scores.find(score_name) != summed_scores.end()) {
            summed_scores[score_name] += new_scores[score_name]->GetInt();
            new_scores.erase(score_name);
        }
    }

    set<string> matching_score_names;
    x_FindMatchingScores(common_scores, 
        new_scores, 
        matching_score_names);

    common_scores.clear(); 
    for (string score_name : matching_score_names) {
        common_scores[score_name] = Ref(new CScore::TValue());
        common_scores[score_name]->Assign(*new_scores[score_name]);
    }
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::x_MergeAlignments(
        const list<CRef<CSeq_align>>& alignment_list,
        CRef<CSeq_align>& processed)
//  ----------------------------------------------------------------------------
{
    if (alignment_list.empty()) {
        return false;
    }

    if (alignment_list.size() == 1) {
        processed = alignment_list.front();
        return true;
    }

    map<string, TSeqPos> summed_scores;
    const list<string> summed_score_names {"num_ident", "num_mismatch"};

    // Factor out identical scores
    list<CRef<CSeq_align>>::const_iterator align_it = alignment_list.begin();
    TScoreValueMap score_values;
    x_GetAlignmentScores(**align_it, score_values);

    x_InitializeScoreSums(score_values, 
        summed_scores);
    ++align_it;

    while (align_it != alignment_list.end() &&
           !score_values.empty()) {

        x_ProcessAlignmentScores(**align_it, summed_scores, score_values);
        ++align_it;
    }
    // At this point, the score_values map should contain the scores that 
    // do not change over the rows

    const auto first_alignment = alignment_list.front();
    if (first_alignment->IsSetSegs() && 
        first_alignment->GetSegs().IsSpliced()) {
        
        processed->SetType(CSeq_align::eType_global);

        if (first_alignment->IsSetDim()) {
            processed->SetDim(first_alignment->GetDim());
        }

        for (auto& kv : summed_scores) {
            auto score = Ref(new CScore());
            score->SetId().SetStr(kv.first);
            score->SetValue().SetInt(kv.second);
            processed->SetScore().push_back(score);
        }

        for (auto& kv : score_values) {
            auto score = Ref(new CScore());
            score->SetId().SetStr(kv.first);
            score->SetValue().Assign(*(kv.second));
            processed->SetScore().push_back(score);
        }

        CRef<CSpliced_seg> spliced = Ref(new CSpliced_seg());
        spliced->Assign(first_alignment->GetSegs().GetSpliced());
        processed->SetSegs().SetSpliced(*spliced);

        auto align_it = alignment_list.cbegin();
        ++align_it;

        while(align_it != alignment_list.end()) {
            const auto& spliced_seg = (*align_it)->GetSegs().GetSpliced();
            if (spliced_seg.IsSetExons()) {
                for (auto exon : spliced_seg.GetExons()) {
                    processed->SetSegs().SetSpliced().SetExons().push_back(exon);
                }
            }
            ++align_it;
        }
        return true;
    }


    processed->SetType(CSeq_align::eType_disc);

    for (auto& kv : summed_scores) {
        auto score = Ref(new CScore());
        score->SetId().SetStr(kv.first);
        score->SetValue().SetInt(kv.second);
        processed->SetScore().push_back(score);
    }

    for (auto& kv : score_values) {
        auto score = Ref(new CScore());
        score->SetId().SetStr(kv.first);
        score->SetValue().Assign(*(kv.second));
        processed->SetScore().push_back(score);
    }

    for (auto current : alignment_list) {
        auto new_align = Ref(new CSeq_align());
        new_align->Assign(*current);
        new_align->ResetScore();

        for (CRef<CScore> score : current->GetScore()) {
            const string& score_name = score->GetId().GetStr();
            if (score_values.find(score_name) == score_values.end()) {
                new_align->SetScore().push_back(score);
            }
        }
        processed->SetSegs().SetDisc().Set().push_back(new_align);
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool
CGff2Reader::xParseAlignment(
    const string& line,
    CRef<CSeq_annot>& pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    if (!CGff2Reader::IsAlignmentData(line)) {
        return false;
    }

    //parse record:
    auto_ptr<CGff2Record> pRecord(x_CreateRecord());
    try {
        if ( ! pRecord->AssignFromGff(line) ) {
            return false;
        }
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pEC);
        return false;
    }

    if (!x_UpdateAnnotAlignment(*pRecord, pAnnot, pEC)) {
        return false;
    }

    ++mCurrentFeatureCount;
    mParsingAlignment = true;
    return true;
}

//  ----------------------------------------------------------------------------
bool
CGff2Reader::xIsCurrentDataType(
    const string& line)
//  ----------------------------------------------------------------------------
{
    if (CGff2Reader::IsAlignmentData(line)) {
        return (mParsingAlignment  ||  !mCurrentFeatureCount);
    }
    return (!mParsingAlignment  ||  !mCurrentFeatureCount);
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseFeatureGff(
    const string& strLine,
    TAnnots& annots,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    auto_ptr<CGff2Record> pRecord(x_CreateRecord());
    try {
        if (!pRecord->AssignFromGff(strLine)) {
			return false;
		}
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pEC);
        return false;
    }
    string ftype = pRecord->Type();
    if (xIsIgnoredFeatureType(ftype)) {
        return true;
    }

    if (annots.empty()) {
        CRef< CSeq_annot > pAnnot( new CSeq_annot );
        if ( ! x_InitAnnot( *pRecord, pAnnot, pEC ) ) {
            return false;
        }
        annots.push_back(pAnnot);
        return true;
    }
    return x_UpdateAnnotFeature(*pRecord, annots.back(), pEC); 
};



//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseAlignmentGff(
    const string& strLine,
    TAnnots& annots )
//  ----------------------------------------------------------------------------
{
    //
    //  Parse the record and determine which ID the given feature will pertain 
    //  to:
    //
    auto_ptr<CGff2Record> pRecord(x_CreateRecord());
    if ( ! pRecord->AssignFromGff( strLine ) ) {
        return false;
    }

    //
    //  Search annots for a pre-existing annot pertaining to the same ID:
    //
    TAnnotIt it = annots.begin();
    for ( /*NOOP*/; it != annots.end(); ++it ) {
        if (!(**it).IsAlign()) continue;
        const string* strAnnotId = s_GetAnnotId(**it);
        if (!strAnnotId) {
            return false;
        }
        if ( pRecord->Id() == *strAnnotId ) {
            break;
        }
    }

    //
    //  If a preexisting annot was found, update it with the new feature
    //  information:
    //
    if ( it != annots.end() ) {
        if ( ! x_UpdateAnnotAlignment( *pRecord, *it ) ) {
            return false;
        }
    }

    //
    //  Otherwise, create a new annot pertaining to the new ID and initialize it
    //  with the given feature information:
    //
    else {
        CRef< CSeq_annot > pAnnot( new CSeq_annot );
        if ( ! x_InitAnnot( *pRecord, pAnnot ) ) {
            return false;
        }
        annots.insert(annots.begin(), pAnnot );      
    }

    return true; 
};

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseBrowserLineGff(
    const string& strRawInput,
    CRef< CAnnotdesc >& pAnnotDesc )
//  ----------------------------------------------------------------------------
{ 
    if ( ! NStr::StartsWith( strRawInput, "browser" ) ) {
        return false;
    }
    vector< string > columns;
    NStr::Split( strRawInput, " \t", columns, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate );

    if ( columns.size() <= 1 || 1 != ( columns.size() % 2 ) ) {
        // don't know how to unwrap this
        pAnnotDesc.Reset();
        return true;
    }    
    pAnnotDesc.Reset( new CAnnotdesc );
    CUser_object& user = pAnnotDesc->SetUser();
    user.SetType().SetStr( "browser" );

    for ( size_t u = 1 /* skip "browser" */; u < columns.size(); u += 2 ) {
        user.AddField( columns[ u ], columns[ u+1 ] );
    }
    return true; 
};

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ParseTrackLineGff(
    const string& strRawInput,
    CRef< CAnnotdesc >& pAnnotDesc )
//  ----------------------------------------------------------------------------
{ 
    const char cBlankReplace( '+' );

    if ( ! NStr::StartsWith( strRawInput, "track" ) ) {
        return false;
    }

    string strCookedInput( strRawInput );
    bool bInString = false;
    for ( size_t u=0; u < strCookedInput.length(); ++u ) {
        if ( strCookedInput[u] == ' ' && bInString ) {
            strCookedInput[u] = cBlankReplace;
        }
        if ( strCookedInput[u] == '\"' ) {
            bInString = !bInString;
        }
    }
    vector< string > columns;
    NStr::Split( strCookedInput, " \t", columns, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate );

    if ( columns.size() <= 1 ) {
        pAnnotDesc.Reset();
        return true;
    } 
    pAnnotDesc.Reset( new CAnnotdesc );
    CUser_object& user = pAnnotDesc->SetUser();
    user.SetType().SetStr( "track" );

    for ( size_t u = 1 /* skip "track" */; u < columns.size(); ++u ) {
        string strKey;
        string strValue;
        NStr::SplitInTwo( columns[u], "=", strKey, strValue );
        NStr::TruncateSpacesInPlace( strKey, NStr::eTrunc_End );
        if ( NStr::StartsWith( strValue, "\"" ) && NStr::EndsWith( strValue, "\"" ) ) {
            strValue = strValue.substr( 1, strValue.length() - 2 );
        }
        for ( unsigned u = 0; u < strValue.length(); ++u ) {
            if ( strValue[u] == cBlankReplace ) {
                strValue[u] = ' ';
            }
        } 
        NStr::TruncateSpacesInPlace( strValue, NStr::eTrunc_Begin );
        user.AddField( strKey, strValue );
    }
       
    return true; 
};
 
//  ----------------------------------------------------------------------------
bool CGff2Reader::x_InitAnnot(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot,
    ILineErrorListener* pEC )
//  ----------------------------------------------------------------------------
{
    //CRef< CAnnot_id > pAnnotId( new CAnnot_id );
    //pAnnotId->SetLocal().SetStr( gff.Id() );
    //pAnnot->SetId().push_back( pAnnotId );
    //pAnnot->SetData().SetFtable();

    // if available, add current browser information
    if ( m_CurrentBrowserInfo ) {
        pAnnot->SetDesc().Set().push_back( m_CurrentBrowserInfo );
    }

    // if available, add current track information
    if (m_pTrackDefaults->ContainsData() ) {
        m_pTrackDefaults->WriteToAnnot(*pAnnot);
    }

    if ( !m_AnnotName.empty() ) {
        pAnnot->SetNameDesc(m_AnnotName);
    }
    if ( !m_AnnotTitle.empty() ) {
        pAnnot->SetTitleDesc(m_AnnotTitle);
    }

    if (gff.IsAlignmentRecord()) {
        pAnnot->SetData().SetAlign();
        return x_UpdateAnnotAlignment( gff, pAnnot );
    }
    else {
        pAnnot->SetData().SetFtable();
        return x_UpdateAnnotFeature( gff, pAnnot, pEC );
    }
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_UpdateAnnotFeature(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    return false;
}


bool CGff2Reader::x_CreateAlignment(
        const CGff2Record& gff, 
        CRef<CSeq_align>& pAlign ) 
{
    pAlign = Ref(new CSeq_align());
    pAlign->SetType(CSeq_align::eType_partial);
    pAlign->SetDim(2);

    //score
    if (!xAlignmentSetScore(gff, pAlign)) {
        return false;
    }

    if (!xAlignmentSetSegment(gff, pAlign)) {
        return false;
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::x_UpdateAnnotAlignment(
    const CGff2Record& gff,
    CRef< CSeq_annot > pAnnot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_align> pAlign( new CSeq_align );
    pAlign->SetType(CSeq_align::eType_partial);
    pAlign->SetDim(2);

    //score
    if (!xAlignmentSetScore(gff, pAlign)) {
        return false;
    }
    if (!xAlignmentSetSegment(gff, pAlign)) {
        return false;
    }
    pAnnot->SetData().SetAlign().push_back( pAlign ) ;
    return true;
}



bool CGff2Reader::xUpdateSplicedAlignment(const CGff2Record& gff,
                                          CRef<CSeq_align> pAlign) const
{
    if (!pAlign->IsSetType()) {
        pAlign->SetType(CSeq_align::eType_partial);
    }
    // Need to set a whole bunch of things

    if (!xUpdateSplicedSegment(gff, pAlign->SetSegs().SetSpliced())) {
        return false;
    }

    return true;
}



bool CGff2Reader::xUpdateSplicedSegment(
        const CGff2Record& gff, 
        CSpliced_seg& segment) const
{
    if (segment.IsSetProduct_type()) {
        segment.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    }
    

    CRef<CSpliced_exon> pExon = Ref(new CSpliced_exon());
    if (!xSetSplicedExon(gff, pExon)) {
        return false;
    }

   segment.SetExons().push_back(pExon);

    return true;
}



//  ----------------------------------------------------------------------------
bool CGff2Reader::xSetSplicedExon(
        const CGff2Record& gff, 
        CRef<CSpliced_exon> pExon) const
//  ----------------------------------------------------------------------------
{
    vector<string> targetParts;
    if (!xGetTargetParts(gff, targetParts)) {
        return false;
    }


    pExon->SetGenomic_start(gff.SeqStart()-1);
    pExon->SetGenomic_end(gff.SeqStop()-1);
    if (gff.IsSetStrand()) {
        pExon->SetGenomic_strand(gff.Strand());
    }


    const int product_start = NStr::StringToInt(targetParts[1])-1;
    const int product_end = NStr::StringToInt(targetParts[2])-1;

    // Check to see that product start and product end are
    // non-negative and that product_end >= product_start

    pExon->SetProduct_start().SetNucpos(product_start);
    pExon->SetProduct_end().SetNucpos(product_end);

    ENa_strand targetStrand = eNa_strand_plus;
    if (targetParts[3] == "-") {
        targetStrand = eNa_strand_minus;
    }
    pExon->SetProduct_strand(targetStrand);

    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xGetTargetParts(const CGff2Record& gff, vector<string>& targetParts) const
//  ----------------------------------------------------------------------------
{
    string targetInfo;
    if (!gff.GetAttribute("Target", targetInfo)) {
        return false;
    }

    NStr::Split(targetInfo, " ", targetParts);
    if (targetParts.size() != 4) {
        return false;
    }
   
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xGetStartsOnMinusStrand(TSeqPos offset, 
        const vector<string>& gapParts,
        const bool isTarget,
        vector<int>& starts) const
//  ----------------------------------------------------------------------------
{
    starts.clear();
    const size_t gapCount = gapParts.size();

    for (size_t i=0; i<gapCount; ++i) {
        char changeType = gapParts[i][0];
        int changeSize = NStr::StringToInt(gapParts[i].substr(1));
        switch (changeType) {
        default:
            return false;

        case 'M': 
            starts.push_back(offset+1-changeSize);
            offset -= changeSize;
            break;

        case 'I':
            if (isTarget) {
                starts.push_back(offset+1-changeSize);
                offset -= changeSize;
            } else {
                starts.push_back(-1);
            }
            break;

        case 'D':
            if (isTarget) {
                starts.push_back(-1);
            } else {
                starts.push_back(offset+1-changeSize);
                offset -= changeSize;
            }
            break;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xGetStartsOnPlusStrand(TSeqPos offset, 
        const vector<string>& gapParts,
        const bool isTarget,
        vector<int>& starts) const
//  ----------------------------------------------------------------------------
{
    starts.clear();
    const auto gapCount = gapParts.size();

    for (auto i=0; i<gapCount; ++i) {
        char changeType = gapParts[i][0];
        int changeSize = NStr::StringToInt(gapParts[i].substr(1));
        switch (changeType) {
        default:
            return false;

        case 'M': 
            starts.push_back(offset);
            offset += changeSize;
            break;

        case 'I':
            if (isTarget) {
                starts.push_back(offset);
                offset += changeSize;
            } else {
                starts.push_back(-1);
            }
            break;

        case 'D':
            if (isTarget) {
                starts.push_back(-1);
            } else {
                starts.push_back(offset);
                offset += changeSize;
            }
            break;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xSetDensegStarts(const vector<string>& gapParts, 
                                   const ENa_strand identStrand,
                                   const ENa_strand targetStrand,
                                   const TSeqPos targetStart,
                                   const TSeqPos targetEnd,
                                   const CGff2Record& gff,
                                   CSeq_align::C_Segs::TDenseg& denseg)
//  ----------------------------------------------------------------------------
{
    const size_t gapCount = gapParts.size();

    const bool isTarget = true;
    vector<int> targetStarts;
    if (targetStrand == eNa_strand_minus) {
        if( !xGetStartsOnMinusStrand(targetEnd,
            gapParts,
            isTarget,
            targetStarts)) {
            return false;
        }
    }
    else {
        if (!xGetStartsOnPlusStrand(targetStart, 
            gapParts,
            isTarget,
            targetStarts)) {
            return false;
        }
    }

    vector<int> identStarts;
    const bool isIdent = !isTarget;

    if (identStrand == eNa_strand_minus) {

        if ( !xGetStartsOnMinusStrand(gff.SeqStop(),
            gapParts,
            isIdent,
            identStarts)) {
            return false;
        }
    }
    else {
        if ( !xGetStartsOnPlusStrand(gff.SeqStart(),
            gapParts,
            isIdent,
            identStarts)) {
            return false;
        }
    }

    for (auto i=0; i<gapCount; ++i) {
        denseg.SetStarts().push_back(targetStarts[i]);
        denseg.SetStarts().push_back(identStarts[i]);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetSegment(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    const string& type = gff.Type();

    if (type == "cDNA_match" || 
        type == "EST_match"  || 
        type == "translated_nucleotide_match") {
        return xAlignmentSetSpliced_seg(gff, pAlign);
    }

    return xAlignmentSetDenseg(gff, pAlign);
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetSpliced_seg(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    vector<string> targetParts;
    if (!xGetTargetParts(gff, targetParts)) {
        return false;
    }

    CSeq_align::TSegs& segs = pAlign->SetSegs();
    
    auto& spliced_seg = segs.SetSpliced();

    const string& type = gff.Type();
    if (type == "translated_nucleotide_match") {
        spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    } 
    else {
        spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    }
    CRef<CSeq_id> product_id = mSeqIdResolve(targetParts[0], 0, true);
    spliced_seg.SetProduct_id(*product_id);

    CRef<CSeq_id> genomic_id = mSeqIdResolve(gff.Id(), 0, true);
    spliced_seg.SetGenomic_id(*genomic_id);

    if (targetParts[3] == "+") {
        spliced_seg.SetProduct_strand(eNa_strand_plus);
    }
    else
    if (targetParts[3] == "-") {
        spliced_seg.SetProduct_strand(eNa_strand_minus);
    }

    if (gff.IsSetStrand()) {
        ENa_strand ident_strand = gff.Strand();
        spliced_seg.SetGenomic_strand(ident_strand);
    }

    CRef<CSpliced_exon> exon(new CSpliced_exon());
    exon->SetProduct_start().SetNucpos(NStr::StringToInt(targetParts[1])-1);
    exon->SetProduct_end().SetNucpos(NStr::StringToInt(targetParts[2])-1);

    const auto genomic_start = gff.SeqStart();
    const auto genomic_end = gff.SeqStop();
    exon->SetGenomic_start(genomic_start);
    exon->SetGenomic_end(genomic_end);

    string gapInfo;
    vector<string> gapParts;
    if (gff.GetAttribute("Gap", gapInfo)) {
        NStr::Split(gapInfo, " ", gapParts);
    }
    else {
        gapParts.push_back(string("M") + NStr::NumericToString(gff.SeqStop()-gff.SeqStart()+1));
    }

    const auto gapCount = gapParts.size();

    for (auto i=0; i<gapCount; ++i) {
        CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk());
        char changeType = gapParts[i][0];
        int changeSize = NStr::StringToInt(gapParts[i].substr(1));
        switch (changeType) {
        default:
            return false;

        case 'M': 
            chunk->SetMatch(changeSize);
            break;

        case 'I':
            chunk->SetProduct_ins(changeSize);
            break;

        case 'D':
            chunk->SetGenomic_ins(changeSize);
            break;

        }
        exon->SetParts().push_back(chunk);
    }

    spliced_seg.SetExons().push_back(exon);

    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetDenseg(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    vector<string> targetParts;
    if (!xGetTargetParts(gff, targetParts)) {
        return false;
    }

    //strands
    ENa_strand targetStrand = eNa_strand_plus;
    if (targetParts[3] == "-") {
        targetStrand = eNa_strand_minus;
    }
    ENa_strand identStrand = eNa_strand_plus;
    if (gff.IsSetStrand()) {
        identStrand = gff.Strand();
    }


    string gapInfo;
    vector<string> gapParts;
    if (gff.GetAttribute("Gap", gapInfo)) {
        NStr::Split(gapInfo, " ", gapParts);
    }
    else {
        gapParts.push_back(string("M") + NStr::NumericToString(gff.SeqStop()-gff.SeqStart()+1));
    }

    int gapCount = gapParts.size();

    //meta
    CSeq_align::TSegs& segs = pAlign->SetSegs();
    CSeq_align::C_Segs::TDenseg& denseg = segs.SetDenseg();
    denseg.SetDim(2);
    denseg.SetNumseg(gapCount);

    //ids
    denseg.SetIds().push_back(
        mSeqIdResolve(targetParts[0], 0, true));
    denseg.SetIds().push_back(
        mSeqIdResolve(gff.Id(), 0, true));

    const TSeqPos targetStart = NStr::StringToInt(targetParts[1])-1;
    const TSeqPos targetEnd   = NStr::StringToInt(targetParts[2])-1;

    if (!xSetDensegStarts(gapParts, 
                          identStrand,
                          targetStrand,
                          targetStart,
                          targetEnd,
                          gff,
                          denseg)) {
        return false;
    }

    //lengths
    for (int i=0; i < gapCount; ++i) {
        denseg.SetLens().push_back(NStr::StringToInt(CTempString(gapParts[i],1,string::npos)));
    }

    for (int i=0; i < gapCount; ++i) {
        denseg.SetStrands().push_back(targetStrand);
        denseg.SetStrands().push_back(identStrand);
    }
    return true;
}




//  ----------------------------------------------------------------------------
bool CGff2Reader::xAlignmentSetScore(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
//  ----------------------------------------------------------------------------
{
    if (gff.IsSetScore()) {
        pAlign->SetNamedScore(CSeq_align::eScore_Score, 
            int(gff.Score()));
    }

    string extraScore;

    const string intScores[] = {
        //official
        "score",
        "align_length",
        "num_ident",
        "num_positives",
        "num_negatives",
        "num_mismatch",
        "num_gap",

        //picked up from real data files
        "common_component",
        "filter_score",
        "for_remapping",
        "merge_aligner",
        "rank",
        "reciprocity",
        "batch_id",
        "align_id",
    };

    const size_t intCount(sizeof(intScores)/sizeof(string));
    for (size_t i=0; i < intCount; ++i) {
        if (gff.GetAttribute(intScores[i], extraScore)) {
            pAlign->SetNamedScore(
                intScores[i], int(NStr::StringToDouble(extraScore)));
        }
    }

    const string realScores[] = {
        //official
        "bit_score",
        "e_value",
        "pct_identity_gap",
        "pct_identity_ungap",
        "pct_identity_gapopen_only",
        "pct_coverage",
        "sum_e",
        "comp_adjustment_method",
        "pct_coverage_hiqual",

        //picked up from real data files
        "inversion_merge_alignmer",
        "expansion",
    };

    const size_t realCount(sizeof(realScores)/sizeof(string));
    for (size_t i=0; i < realCount; ++i) {
        if (gff.GetAttribute(realScores[i], extraScore)) {
            pAlign->SetNamedScore(
                realScores[i], NStr::StringToDouble(extraScore));
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetLocation(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_id > pId = mSeqIdResolve(record.Id(), m_iFlags, true);
    CRef< CSeq_loc > pLocation( new CSeq_loc );
    pLocation->SetInt().SetId( *pId );
    pLocation->SetInt().SetFrom( record.SeqStart() );
    pLocation->SetInt().SetTo( record.SeqStop() );
    if ( record.IsSetStrand() ) {
        pLocation->SetInt().SetStrand( record.Strand() );
    }
    pFeature->SetLocation( *pLocation );

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_ProcessQualifierSpecialCase(
    CGff2Record::TAttrCit it,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    return false;
}  

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureTrimQualifiers(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    typedef CSeq_feat::TQual TQual;
    //task:
    // for each attribute of the new piece check if we already got a feature 
    //  qualifier
    // if so, and with the same value, then the qualifier is allowed to live
    // otherwise it is subfeature specific and hence removed from the feature
    TQual& quals = pFeature->SetQual();
    for (TQual::iterator it = quals.begin(); it != quals.end(); /**/) {
        const string& qualKey = (*it)->GetQual();
        if (NStr::StartsWith(qualKey, "gff_")) {
            it++;
            continue;
        }
        if (qualKey == "locus_tag") {
            it++;
            continue;
        }
        if (qualKey == "old_locus_tag") {
            it++;
            continue;
        }
        if (qualKey == "product") {
            it++;
            continue;
        }
        if (qualKey == "protein_id") {
            it++;
            continue;
        }
        const string& qualVal = (*it)->GetVal();
        string attrVal;
        if (!record.GetAttribute(qualKey, attrVal)) {
            //superfluous qualifier- squish
            it = quals.erase(it);
            continue;
        }
        if (qualVal != attrVal) {
            //ambiguous qualifier- squish
            it = quals.erase(it);
            continue;
        }
        it++;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_FeatureSetQualifiers(
    const CGff2Record& record,
    CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    //
    //  Create GB qualifiers for the record attributes:
    //
    CRef< CGb_qual > pQual(0);
    const CGff2Record::TAttributes& attrs = record.Attributes();
    CGff2Record::TAttrCit it = attrs.begin();
    for (/*NOOP*/; it != attrs.end(); ++it) {
        // special case some well-known attributes
        if (x_ProcessQualifierSpecialCase(it, pFeature)) {
            continue;
        }

        // turn everything else into a qualifier
        pQual.Reset(new CGb_qual);
        pQual->SetQual(it->first);
        pQual->SetVal(it->second);
        pFeature->SetQual().push_back(pQual);
    } 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xFeatureSetQualifier(
    const string& key,
    const string& value,
    CRef<CSeq_feat> pTargetFeature)
//  ----------------------------------------------------------------------------
{
    if (!pTargetFeature) {
        return false;
    }
    pTargetFeature->AddOrReplaceQualifier(key, value);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_GetFeatureById(
    const string & strId, 
    ncbi::CRef<CSeq_feat>& pFeature )
//  ----------------------------------------------------------------------------
{
    map< string, CRef< CSeq_feat > >::iterator it;
    it = m_MapIdToFeature.find(strId);
	if(it != m_MapIdToFeature.end()) {
        pFeature = it->second;
		return true;
	}
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::x_HasTemporaryLocation(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( ! feature.CanGetExts() ) {
        return false;
    }
    list< CRef< CUser_object > > pExts = feature.GetExts();
    list< CRef< CUser_object > >::iterator it;
    for ( it = pExts.begin(); it != pExts.end(); ++it ) {
        if ( ! (*it)->CanGetType() || ! (*it)->GetType().IsStr() ) {
            continue;
        }
        if ( (*it)->GetType().GetStr() != "gff-info" ) {
            continue;
        }
        if ( ! (*it)->HasField( "gff-cooked" ) ) {
            return false;
        }
        return ( (*it)->GetField( "gff-cooked" ).GetData().GetStr() == "false" );
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::IsExon(
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    if (!pFeature->CanGetData() || !pFeature->GetData().IsImp()) {
        return false;
    }
    return (pFeature->GetData().GetImp().GetKey() == "exon" );
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::IsCds(
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    if (!pFeature->CanGetData()) {
        return false;
    }
    return (pFeature->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion);
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xAddFeatureToAnnot(
    CRef< CSeq_feat > pFeature,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    if (IsExon(pFeature)) {
        CRef< CSeq_feat > pParent;    
        if (!xGetParentFeature(*pFeature, pParent) ) {
            pAnnot->SetData().SetFtable().push_back(pFeature) ;
            return true;
        }
        return xFeatureMergeExon( pFeature, pParent );
    }
    if (IsCds(pFeature)) {
        CRef<CSeq_feat> pExisting;
        if (!xGetExistingFeature(*pFeature, pAnnot, pExisting)) {
            pAnnot->SetData().SetFtable().push_back(pFeature) ;
            return true;
        }
        return xFeatureMergeCds(pFeature, pExisting);
    }
    pAnnot->SetData().SetFtable().push_back( pFeature ) ;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xGetExistingFeature(
    const CSeq_feat& feature,
    CRef<CSeq_annot> pAnnot,
    CRef< CSeq_feat >& pExisting )
//  ----------------------------------------------------------------------------
{
    if (!feature.CanGetQual()) {
        return false;
    }

    string strExistingId = feature.GetNamedQual("ID");
    if (strExistingId.empty()) {
        return false;
    }
    if (!x_GetFeatureById( strExistingId, pExisting)) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Reader::xGetParentFeature(
    const CSeq_feat& feature,
    CRef< CSeq_feat >& pParent )
//  ----------------------------------------------------------------------------
{
    if ( ! feature.CanGetQual() ) {
        return false;
    }

    string strParentId = feature.GetNamedQual("Parent");
    if (strParentId.empty()) {
        return false;
    }
    if ( ! x_GetFeatureById( strParentId, pParent ) ) {
        return false;
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CGff2Reader::xFeatureMergeExon(
    CRef< CSeq_feat > pExon,
    CRef< CSeq_feat > pMrna )
//  ---------------------------------------------------------------------------
{
    if ( x_HasTemporaryLocation( *pMrna ) ) {
        // start rebuilding parent location from scratch
        pMrna->SetLocation().Assign( pExon->GetLocation() );
        list< CRef< CUser_object > > pExts = pMrna->SetExts();
        list< CRef< CUser_object > >::iterator it;
        for ( it = pExts.begin(); it != pExts.end(); ++it ) {
            if ( ! (*it)->CanGetType() || ! (*it)->GetType().IsStr() ) {
                continue;
            }
            if ( (*it)->GetType().GetStr() != "gff-info" ) {
                continue;
            }
            (*it)->SetField( "gff-cooked" ).SetData().SetStr( "true" );
        }
    }
    else {
        // add exon location to current parent location
        pMrna->SetLocation().Add(  pExon->GetLocation() );
    }

    return true;
}
                                
//  ---------------------------------------------------------------------------
bool CGff2Reader::xFeatureMergeCds(
    CRef< CSeq_feat > pNewPiece,
    CRef< CSeq_feat > pExisting )
//  ---------------------------------------------------------------------------
{
    pExisting->SetLocation().Add(pNewPiece->GetLocation());
    return true;
}
                                
//  ============================================================================
CRef< CDbtag >
CGff2Reader::x_ParseDbtag(
    const string& str )
//  ============================================================================
{
    CRef< CDbtag > pDbtag( new CDbtag() );
    static const char* digits = "0123456789";
    string strDb, strTag;
    NStr::SplitInTwo( str, ":", strDb, strTag );

    // dbtag names for Gff2 do not always match the names for genbank.
    // special case known fixups here:
    if ( strDb == "NCBI_gi" ) {
        strDb = "GI";
    }
    // todo: all the other ones


    if ( ! strTag.empty() ) {
        pDbtag->SetDb( strDb );
        if (strTag.find_first_not_of(digits, 0) == string::npos)
            pDbtag->SetTag().SetId( NStr::StringToUInt( strTag ) );
        else
            pDbtag->SetTag().SetStr( strTag );

    }
    else {
        pDbtag->SetDb( "unknown" );
        pDbtag->SetTag().SetStr( str );
    }
    return pDbtag;
}

//  ============================================================================
bool CGff2Reader::xAnnotPostProcess(
    CRef<CSeq_annot> pAnnot)
//  ============================================================================
{
    if (!xGenerateParentChildXrefs(pAnnot)) {
        return false;
    }
    return true;
}

//  ============================================================================
bool CGff2Reader::xGenerateParentChildXrefs(
    CRef<CSeq_annot> pAnnot)
//  ============================================================================
{
    typedef list<CRef<CSeq_feat> > FTABLE;
    typedef list<string> PARENTS;

    if (!pAnnot->IsFtable()) {
        return true;
    }
    FTABLE& ftable = pAnnot->SetData().SetFtable();
    for (FTABLE::iterator featIt = ftable.begin(); featIt != ftable.end(); ++featIt) {
        CSeq_feat& feat = **featIt;
        const string& parentStr = feat.GetNamedQual("Parent");
        PARENTS parents;
        NStr::Split(parentStr, ",", parents, 0);
        for (PARENTS::iterator parentIt = parents.begin(); parentIt != parents.end(); ++parentIt) {
            const string& parent = *parentIt; 
            xSetAncestryLine(feat, parent);
        }
    }
    return true;
}

//  ============================================================================
void CGff2Reader::xSetAncestryLine(
    CSeq_feat& feat,
    const string& directParentStr)
//  ============================================================================
{
    typedef list<string> PARENTS;

    string ancestorStr(directParentStr);
    CRef<CSeq_feat> pAncestor;
    while (!ancestorStr.empty()) {
        if (!x_GetFeatureById(ancestorStr, pAncestor)) {
            return;
        }
        xSetAncestorXrefs(feat, *pAncestor);
        ancestorStr = pAncestor->GetNamedQual("Parent");
        PARENTS ancestors;
        NStr::Split(ancestorStr, ",", ancestors, 0);
        for (PARENTS::iterator it = ancestors.begin(); it != ancestors.end(); ++it) {
            const string& ancestorStr = *it;
            xSetAncestryLine(feat, ancestorStr);
        }
    }
}

//  ============================================================================
bool sFeatureHasXref(
    const CSeq_feat& feat,
    const CFeat_id& featId)
//  ============================================================================
{
    typedef vector<CRef<CSeqFeatXref> > XREFS;
    if (!feat.IsSetXref()) {
        return false;
    }
    int xrefId = featId.GetLocal().GetId();
    const XREFS& xrefs = feat.GetXref();
    for (XREFS::const_iterator cit = xrefs.begin(); cit != xrefs.end(); ++cit) {
        const CSeqFeatXref& ref = **cit; 
        int contentId = ref.GetId().GetLocal().GetId();
        if (contentId == xrefId) {
            return true;
        }
    }
    return false;
}

//  ============================================================================
void CGff2Reader::xSetAncestorXrefs(
    CSeq_feat& descendent,
    CSeq_feat& ancestor)
//  ============================================================================
{

    //xref descendent->ancestor
    if (!sFeatureHasXref(descendent, ancestor.GetId())) {
        CRef<CFeat_id> pAncestorId(new CFeat_id);
        pAncestorId->Assign(ancestor.GetId());
        CRef<CSeqFeatXref> pAncestorXref(new CSeqFeatXref);
        pAncestorXref->SetId(*pAncestorId);
        descendent.SetXref().push_back(pAncestorXref);
    }

    //xref ancestor->descendent
    if (!sFeatureHasXref(ancestor, descendent.GetId())) {
        CRef<CFeat_id> pDescendentId(new CFeat_id);
        pDescendentId->Assign(descendent.GetId());
        CRef<CSeqFeatXref> pDescendentXref(new CSeqFeatXref);
        pDescendentXref->SetId(*pDescendentId);
        ancestor.SetXref().push_back(pDescendentXref);
    }
}

//  ============================================================================
void CGff2Reader::xSetXrefFromTo(
    CSeq_feat& from,
    CSeq_feat& to)
//  ============================================================================
{

    //xref descendent->ancestor
    if (!sFeatureHasXref(from, to.GetId())) {
        CRef<CFeat_id> pToId(new CFeat_id);
        pToId->Assign(to.GetId());
        CRef<CSeqFeatXref> pToXref(new CSeqFeatXref);
        pToXref->SetId(*pToId);
        from.SetXref().push_back(pToXref);
    }
}

//  ============================================================================
bool CGff2Reader::IsAlignmentData(
    const string& line)
//  ============================================================================
{
    vector<CTempStringEx> columns;
    CGff2Record::TokenizeGFF(columns, line);
    if (columns.size() < 9) {
        return false;
    }
    if (NStr::StartsWith(columns[2], "match") || 
        NStr::EndsWith(columns[2], "_match")) {
        return true;
    }
    return false;
}

//  ============================================================================
bool CGff2Reader::xIsIgnoredFeatureType(
    const string& type)
//  ============================================================================
{
    return false;
}

//  ============================================================================
bool CGff2Reader::xIsIgnoredFeatureId(
    const string& type)
//  ============================================================================
{
    return false;
}

//  ---------------------------------------------------------------------------
bool
CGff2Reader::xNeedsNewSeqAnnot(
    const string& line)
//  ---------------------------------------------------------------------------
{
    if (m_iFlags & CGff2Reader::fGenbankMode) {
        vector<string> columns;
        NStr::Split(line, "\t ", columns, NStr::eMergeDelims);
        string seqId = columns[0];
        if (m_CurrentSeqId == seqId) {
            return false;
        }
        m_CurrentSeqId = seqId;
        m_PendingLine = line;
        return true;
    }
    //in normal mode, segmentation is driven by track line markers rather than 
    // Seq-ids, and that test has already been done
    return false;
}

END_objects_SCOPE
END_NCBI_SCOPE
