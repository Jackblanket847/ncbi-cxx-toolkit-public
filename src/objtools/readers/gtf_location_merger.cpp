/*
 * $Id$
 *
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
 * Authors:  Frank Ludwig
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include "gtf_location_merger.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGtfLocationRecord::CGtfLocationRecord(
    const CGtfReadRecord& record,
    unsigned int flags,
    CGff3ReadRecord::SeqIdResolver seqIdResolve)
//  ----------------------------------------------------------------------------
{
    static map<string, size_t> typeToPart {
        {"cds",         1},
        {"start_codon", 0},
        {"stop_codon",  2},
        {"5utr",        0},
        {"3utr",        4},
        {"exon",        2},
        {"initial",     1},
        {"internal",    2},
        {"terminal",    3},
        {"single",      1},
    };

    mId.Assign(*record.GetSeqId(flags, seqIdResolve));
    mStart = static_cast<TSeqPos>(record.SeqStart());
    mStop  = static_cast<TSeqPos>(record.SeqStop());
    mStrand = (record.IsSetStrand() ? record.Strand() : eNa_strand_plus);
    mType = record.Type();
    NStr::ToLower(mType);
    mPartNum = 0;
    string recordPart = record.GtfAttributes().ValueOf("part");
    if (recordPart.empty()) {
        recordPart = record.GtfAttributes().ValueOf("exon_number");
    }
    if (recordPart.empty()) {
        auto partIt = typeToPart.find(mType);
        if (partIt != typeToPart.end()) {
            mPartNum = partIt->second;
        }
        return;
    }
    try {
        mPartNum = NStr::StringToInt(recordPart);
    }
    catch (CStringException&) {
        //mPartNum = 0;
    }
}

//  ----------------------------------------------------------------------------
CGtfLocationRecord::CGtfLocationRecord(
    const CGtfLocationRecord& other)
//  ----------------------------------------------------------------------------
{
    mId.Assign(other.mId);
    mStart = other.mStart;
    mStop = other.mStop;
    mStrand = other.mStrand;
    mType = other.mType;
    mPartNum = other.mPartNum;
}

//  ----------------------------------------------------------------------------
CGtfLocationRecord&
CGtfLocationRecord::operator=(
    const CGtfLocationRecord& other)
//  ----------------------------------------------------------------------------
{
    mId.Assign(other.mId);
    mStart = other.mStart;
    mStop = other.mStop;
    mStrand = other.mStrand;
    mType = other.mType;
    mPartNum = other.mPartNum;
    return *this;
}

//  ----------------------------------------------------------------------------
bool
CGtfLocationRecord::Contains(
    const CGtfLocationRecord& other) const
//  ----------------------------------------------------------------------------
{
    if (mStrand != other.mStrand) {
        return false;
    }
    return ((mStart <= other.mStart)  &&  (mStop >= other.mStop));
}

//  ----------------------------------------------------------------------------
bool
CGtfLocationRecord::IsContainedBy(
    const CGtfLocationRecord& other) const
//  ----------------------------------------------------------------------------
{
    return other.Contains(*this);
}

//  ----------------------------------------------------------------------------
CRef<CSeq_loc> 
CGtfLocationRecord::GetLocation()
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> pLocation(new CSeq_loc);
    CRef<CSeq_interval> pInterval(new CSeq_interval);
    pInterval->SetId().Assign(mId);
    pInterval->SetFrom(mStart);
    pInterval->SetTo(mStop);
    pInterval->SetStrand(mStrand);
    pLocation->SetInt(*pInterval);
    return pLocation;
}


//  ============================================================================
CGtfLocationMerger::CGtfLocationMerger(
    unsigned int flags,
    CGff3ReadRecord::SeqIdResolver idResolver):
//  ============================================================================
    mFlags(flags),
    mIdResolver(idResolver)
{
}

//  ============================================================================
string
CGtfLocationMerger::GetFeatureIdFor(
    const CGtfReadRecord& record,
    const string& prefixOverride)
//  ============================================================================
{
    static const list<string> cdsTypes{ "start_codon", "stop_codon", "cds" };
    static const list<string> transcriptTypes = { "5utr", "3utr", "exon", 
        "initial", "internal", "terminal", "single" };
    
    auto prefix = prefixOverride;
    if (prefixOverride.empty()) {
        prefix = record.Type();
        NStr::ToLower(prefix);
        if (std::find(cdsTypes.begin(), cdsTypes.end(), prefix) != cdsTypes.end()) {
            prefix = "cds";
        }
        else if (std::find(transcriptTypes.begin(), transcriptTypes.end(), prefix) 
                != transcriptTypes.end()) {
            prefix = "transcript";
        }
    }
    if (prefix == "gene") {
        return (prefix + ":" + record.GeneKey());
    }
    return (prefix + ":" + record.FeatureKey());
}

//  ============================================================================
void
CGtfLocationMerger::AddRecord(
    const CGtfReadRecord& record)
//  ============================================================================
{
    AddRecordForId(xGetLocationId(record), record);
}

//  ============================================================================
void
CGtfLocationMerger::AddRecordForId(
    const string& id,
    const CGtfReadRecord& record)
//  ============================================================================
{
    auto existingEntry = mMapIdToLocations.find(id);
    if (existingEntry == mMapIdToLocations.end()) {
        existingEntry = mMapIdToLocations.emplace(id, LOCATIONS()).first;
    }
    LOCATIONS& locations = existingEntry->second;
    CGtfLocationRecord location(record, mFlags, mIdResolver);
    auto& existingRecords = existingEntry->second;
    for (auto& existing: existingRecords) {
        if (existing.Contains(location)) {
            if (location.mType == "start_codon") {
                existing.mPartNum -= 1;
            }
            return;
        }
        if (existing.IsContainedBy(location)) {
            if (existing.mType == "start_codon") {
                location.mPartNum -= 1;
            }
            existing = location;
            return;
        }
    }
    existingEntry->second.push_back(location);
}

//  ============================================================================
void
CGtfLocationMerger::AddStubForId(
    const string& id)
//  ============================================================================
{
    auto existingEntry = mMapIdToLocations.find(id);
    if (existingEntry == mMapIdToLocations.end()) {
        return;
    }
    mMapIdToLocations.emplace(id, LOCATIONS());
}


//  ============================================================================
string CGtfLocationMerger::xGetLocationId(
    const CGtfReadRecord& record)
//  ============================================================================
{
    string recordType = record.Type();
    NStr::ToLower(recordType);

    if (recordType == "start_codon"  || recordType == "stop_codon") {
        recordType = "cds";
    }
    return (recordType + ":" + record.FeatureKey());
}

//  ============================================================================
CRef<CSeq_loc> 
CGtfLocationMerger::MergeLocation(
    CSeqFeatData::ESubtype subType,
    LOCATIONS& locations)
//  ============================================================================
{
    if (locations.empty()) {
        CRef<CSeq_loc> pSeqloc(new CSeq_loc);
        pSeqloc->SetNull();
        return pSeqloc;
    }

    switch(subType) {
        default: {
            return MergeLocationDefault(locations);
        }
        case CSeqFeatData::eSubtype_cdregion:
            return MergeLocationForCds(locations);
        case CSeqFeatData::eSubtype_mRNA:
            return MergeLocationForTranscript(locations);
        case CSeqFeatData::eSubtype_gene:
            return MergeLocationForGene(locations);
    }
}

//  ============================================================================
CRef<CSeq_loc> 
CGtfLocationMerger::MergeLocationDefault(
    LOCATIONS& locations)
//  ============================================================================
{
    NCBI_ASSERT(!locations.empty(), 
        "Cannot call MergeLocationDefault with empty location");
    CRef<CSeq_loc> pSeqloc(new CSeq_loc);
    if (locations.size() == 1) {
        auto& onlyOne = locations.front();
        pSeqloc = onlyOne.GetLocation(); 
        return pSeqloc;
    }
    std::stable_sort(
        locations.begin(), locations.end(), CGtfLocationRecord::ComparePartNumbers);
    //locations.sort(CGtfLocationRecord::ComparePartNumbers);
    auto& mix = pSeqloc->SetMix();
    for (auto& location: locations) {
        mix.AddSeqLoc(*location.GetLocation());
    }
    return pSeqloc;
}

//  ============================================================================
CRef<CSeq_loc> 
CGtfLocationMerger::MergeLocationForCds(
    LOCATIONS& locations)
//  ============================================================================
{
    NCBI_ASSERT(!locations.empty(), 
        "Cannot call MergeLocationForCds with empty location");

    std::stable_sort(
        locations.begin(), locations.end(), CGtfLocationRecord::ComparePartNumbers);
    CRef<CSeq_loc> pSeqloc(new CSeq_loc);
    auto& mix = pSeqloc->SetMix();
    for (auto& location: locations) {
        mix.AddSeqLoc(*location.GetLocation());
    }
    pSeqloc = pSeqloc->Merge(CSeq_loc::fMerge_All, 0);
    return pSeqloc;
}

//  ============================================================================
CRef<CSeq_loc> 
CGtfLocationMerger::MergeLocationForGene(
    LOCATIONS& locations)
//  ============================================================================
{
    NCBI_ASSERT(!locations.empty(), 
        "Cannot call MergeLocationForGene with empty location");

    CRef<CSeq_loc> pSeqloc = MergeLocationDefault(locations);
    if (pSeqloc->IsInt()) {
        return pSeqloc;
    }

    pSeqloc->ChangeToPackedInt();
    auto seqlocIntervals = pSeqloc->GetPacked_int().Get(); 

    CRef<CSeq_id> pRangeId(new CSeq_id);
    pRangeId->Assign(*pSeqloc->GetId());
    auto rangeStart = pSeqloc->GetStart(eExtreme_Biological);
    auto rangeStop = pSeqloc->GetStop(eExtreme_Biological);
    auto rangeStrand = eNa_strand_plus;
    if (pSeqloc->IsSetStrand()) {
        rangeStrand = pSeqloc->GetStrand();
    }

    if (rangeStrand == eNa_strand_minus) {
        if (rangeStart >= rangeStop) {
            CRef<CSeq_interval> pOverlap(
                new CSeq_interval(*pRangeId, rangeStop, rangeStart, rangeStrand)); 
            pSeqloc->SetInt(*pOverlap);
        }
        else {
            CRef<CSeq_loc_mix>  pMix(new CSeq_loc_mix);
            auto it = seqlocIntervals.begin();
            auto oldFrom = (*it)->GetFrom();
            auto oldTo = (*it)->GetTo();

            for (it++; it != seqlocIntervals.end(); it++) {
                auto newFrom = (*it)->GetFrom();
                auto newTo = (*it)->GetTo();
                if (newTo >= oldFrom) {
                    oldFrom = newFrom;
                }
                else {
                    pMix->AddInterval(*pRangeId, 0, oldTo, rangeStrand);
                    oldFrom = newFrom, oldTo = newTo;
                }
            }
            pMix->AddInterval(*pRangeId, oldFrom, oldTo, rangeStrand);
            pSeqloc->SetMix(*pMix);
        }
    }
    else {
        if (rangeStart <= rangeStop) {
            CRef<CSeq_interval> pOverlap(
                new CSeq_interval(*pRangeId, rangeStart, rangeStop, rangeStrand)); 
            pSeqloc->SetInt(*pOverlap);
        }
        else {
            CRef<CSeq_loc_mix>  pMix(new CSeq_loc_mix);
            auto it = seqlocIntervals.begin();
            auto oldFrom = (*it)->GetFrom();
            auto oldTo = (*it)->GetTo();
            for (it++; it != seqlocIntervals.end(); it++) {
                auto newFrom = (*it)->GetFrom();
                auto newTo = (*it)->GetTo();
                if (newFrom >= oldTo) {
                    oldTo = newTo;
                }
                else {
                    pMix->AddInterval(*pRangeId, oldFrom, oldTo, rangeStrand);
                    oldFrom = 0, oldTo = newTo;
                }
            }
            pMix->AddInterval(*pRangeId, oldFrom, oldTo, rangeStrand);
            pSeqloc->SetMix(*pMix);
        }
    }    
    return pSeqloc;
}

//  ============================================================================
CRef<CSeq_loc> 
CGtfLocationMerger::MergeLocationForTranscript(
    LOCATIONS& locations)
//  ============================================================================
{
    return MergeLocationDefault(locations);
}

END_SCOPE(objects)
END_NCBI_SCOPE
