/*
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
* File Name:  qual_parse.cpp
*
* Author: Frank Ludwig
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqfeat/Seq_feat.hpp>

#include "flatparse_report.hpp"
#include "qual_parse.hpp"
#include "ftaerr.hpp"

# include <objtools/flatfile/flat2err.h>

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
CQualParser::CQualParser(
    const string& featKey,
    const string& featLocation,
    const vector<string>& qualLines): 
//  ----------------------------------------------------------------------------
    mFeatKey(featKey), 
    mFeatLocation(featLocation),
    mCleanerUpper(featKey, featLocation),
    mData(qualLines)
{
    mCurrent = mData.begin();
}


//  ----------------------------------------------------------------------------
CQualParser::~CQualParser()
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------
bool CQualParser::GetNextQualifier(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    // Note: In general, if a qualifier spans multiple lines then it's 
    //  surrounded by quotes. The one exception I am aware of is anticodon.
    // If there are more, or the situation gets more complicated in general,
    //  then the code needs to be rewritten to extract values according to 
    //  qualifier dependent rules. So key based handler lookup and stuff like 
    //  that.
    // Let's hope we don't have to go there.

    qualKey.clear();
    qualVal.clear();
    bool thereIsMore = false;
    if (!xParseQualifierHead(qualKey, qualVal, thereIsMore)) {
        return false;
    }
    if (!xParseQualifierTail(qualKey, qualVal, thereIsMore)) {
        return false;
    }

    if (!xValidateSyntax(qualKey, qualVal)) {
        // error should have been reported at lower level, fail
        return false;
    }
    return mCleanerUpper.CleanAndValidate(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierHead(
    string& qualKey,
    string& qualVal,
    bool& thereIsMore)
    //  ----------------------------------------------------------------------------
{
    while (mCurrent != mData.end()) {
        if (xParseQualifierStart(false, qualKey, qualVal, thereIsMore)) {
            return true;
        }
        ++mCurrent;
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierStart(
    bool silent,
    string& qualKey,
    string& qualVal,
    bool& thereIsMore)
//  ----------------------------------------------------------------------------
{
    if (!mPendingKey.empty()) {
        qualKey = mPendingKey, mPendingKey.clear();
        qualVal = mPendingVal, mPendingVal.clear();
        return true;
    }

    auto cleaned = NStr::TruncateSpaces(*mCurrent);
    if (!NStr::StartsWith(cleaned, '/')) {
        if (!silent) {
            CFlatParseReport::UnexpectedData(mFeatKey, mFeatLocation);
        }
        return false;
    }
    auto idxEqual = cleaned.find('=', 1);
    qualKey = cleaned.substr(1, idxEqual);
    if (idxEqual != string::npos) {
        qualKey.pop_back();
    }
    if (!sIsLegalQual(qualKey)) {
        if (!silent) {
            CFlatParseReport::UnknownQualifierKey(mFeatKey, mFeatLocation, qualKey);
        }
        return false;
    }
    ++ mCurrent; // found what we are looking for, flush data

    if (idxEqual == string::npos) {
        qualVal = "";
        return true;
    }

    auto tail = cleaned.substr(idxEqual + 1, string::npos);
    if (tail.empty()) {
        // we can't be harsh here because the legacy flatfile parser regarded
        //  /xxx, /xxx=, and /xxx="" as the same thing.
        CFlatParseReport::NoTextAfterEqualSign(mFeatKey, mFeatLocation, qualKey);
        qualVal = "";
        thereIsMore = false;
        return true;
    }
    if (!NStr::StartsWith(tail, '\"')) {
        // sanity check tail?
        qualVal = tail;
        thereIsMore = (qualKey == "anticodon");
        return true;
    }
    // established: tail starts with quote
    if (NStr::EndsWith(tail, '\"')) {
        qualVal = tail.substr(1, tail.size() - 2);
        thereIsMore = false;
        return true;
    }
    // partial qualifier value
    qualVal = tail.substr(1, string::npos);
    thereIsMore = true;
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierTail(
    const string& qualKey,
    string& qualVal,
    bool& thereIsMore)
    //  ----------------------------------------------------------------------------
{
    while (thereIsMore) {
        if (mCurrent == mData.end()) {
            thereIsMore = false;
            if (qualKey != "anticodon") {
                CFlatParseReport::UnbalancedQuotes(qualKey);
                return false;
            }
            return true;
        }
        if (!xParseQualifierCont(qualKey, qualVal, thereIsMore)) {
            if (qualKey != "anticodon") {
                // error should have been reported at lower level, fail
                return false;
            }
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierCont(
    const string& qualKey,
    string& qualVal,
    bool& thereIsMore)
//  ----------------------------------------------------------------------------
{
    if (!mPendingKey.empty()) {
        // report error
        return false;
    }
    if (xParseQualifierStart(true, mPendingKey, mPendingVal, thereIsMore)) {
        return false;
    }
    auto cleaned = NStr::TruncateSpaces(*mCurrent);
    ++mCurrent; //we are going to accept it, so flush it out

    thereIsMore = true;
    if (NStr::EndsWith(cleaned, '\"')) {
        cleaned = cleaned.substr(0, cleaned.size() - 1);
        thereIsMore = false;
    }
    if (qualKey != "anticodon") {
        qualVal += ' ';
    }
    qualVal += cleaned;
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualParser::xValidateSyntax(
    const string& qualKey,
    const string& qualVal)
// -----------------------------------------------------------------------------
{
    // sIsLegalQual should be here, but for efficieny it was done before even
    //  extracting the qualifier value.

    if (!sHasBalancedQuotes(qualVal)) {
        CFlatParseReport::UnbalancedQuotes(qualKey);
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CQualParser::Done() const
//  ----------------------------------------------------------------------------
{
    return (mCurrent == mData.end());
}


//  ----------------------------------------------------------------------------
bool CQualParser::sIsLegalQual(
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    auto type = objects::CSeqFeatData::GetQualifierType(qualKey);
    return (type != objects::CSeqFeatData::eQual_bad);
}


//  ----------------------------------------------------------------------------
bool CQualParser::sHasBalancedQuotes(
    const string& qualVal)
//  ----------------------------------------------------------------------------
{
    int countEscapedQuotes(0);
    auto nextEscapedQuote = qualVal.find("\"\"");
    while (nextEscapedQuote != string::npos) {
        ++countEscapedQuotes;
        nextEscapedQuote = qualVal.find("\"\"", nextEscapedQuote+2);
    }
    int countAnyQuotes(0);
    auto nextAnyQuote = qualVal.find("\"");
    while (nextAnyQuote != string::npos) {
        ++countAnyQuotes;
        nextAnyQuote = qualVal.find("\"", nextAnyQuote+1);
    }
    return (0 == countEscapedQuotes % 4  &&  
        countAnyQuotes == 2*countEscapedQuotes);
}


END_NCBI_SCOPE
