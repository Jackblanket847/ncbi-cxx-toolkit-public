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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */


#include <ncbi_pch.hpp>

#include "pubseq_gateway.hpp"
#include "pending_operation.hpp"
#include "async_bioseq_query.hpp"
#include "insdc_utils.hpp"
#include "resolve_trace.hpp"

using namespace std::placeholders;


CAsyncBioseqQuery::CAsyncBioseqQuery(SBioseqResolution &&  bioseq_resolution,
                                     CPendingOperation *  pending_op,
                                     bool  need_trace) :
    m_BioseqResolution(std::move(bioseq_resolution)),
    m_PendingOp(pending_op),
    m_NeedTrace(need_trace),
    m_Fetch(nullptr),
    m_NoSeqIdTypeFetch(nullptr)
{}



void CAsyncBioseqQuery::MakeRequest(bool  with_seq_id_type)
{
    ++m_BioseqResolution.m_CassQueryCount;

    unique_ptr<CCassBioseqInfoFetch>   details;
    details.reset(new CCassBioseqInfoFetch());

    CBioseqInfoFetchRequest     bioseq_info_request;
    bioseq_info_request.SetAccession(m_BioseqResolution.m_BioseqInfo.GetAccession());

    auto    version = m_BioseqResolution.m_BioseqInfo.GetVersion();
    auto    gi = m_BioseqResolution.m_BioseqInfo.GetGI();

    if (version != -1)
        bioseq_info_request.SetVersion(version);
    if (with_seq_id_type) {
        auto    seq_id_type = m_BioseqResolution.m_BioseqInfo.GetSeqIdType();
        if (seq_id_type != -1)
            bioseq_info_request.SetSeqIdType(seq_id_type);
    }
    if (gi != -1)
        bioseq_info_request.SetGI(gi);

    CCassBioseqInfoTaskFetch *  fetch_task =
            new CCassBioseqInfoTaskFetch(
                    m_PendingOp->GetTimeout(), m_PendingOp->GetMaxRetries(),
                    m_PendingOp->GetConnection(),
                    CPubseqGatewayApp::GetInstance()->GetBioseqKeyspace(),
                    bioseq_info_request,
                    nullptr, nullptr);
    details->SetLoader(fetch_task);

    if (with_seq_id_type)
        fetch_task->SetConsumeCallback(
            std::bind(&CAsyncBioseqQuery::x_OnBioseqInfo, this, _1));
    else
        fetch_task->SetConsumeCallback(
            std::bind(&CAsyncBioseqQuery::x_OnBioseqInfoWithoutSeqIdType, this, _1));

    fetch_task->SetErrorCB(
        std::bind(&CAsyncBioseqQuery::x_OnBioseqInfoError, this, _1, _2, _3, _4));
    fetch_task->SetDataReadyCB(m_PendingOp->GetDataReadyCB());


    m_BioseqRequestStart = chrono::high_resolution_clock::now();
    if (with_seq_id_type) {
        m_Fetch = details.release();
        m_PendingOp->RegisterFetch(m_Fetch);
    } else {
        m_NoSeqIdTypeFetch = details.release();
        m_PendingOp->RegisterFetch(m_NoSeqIdTypeFetch);
    }

    if (m_NeedTrace) {
        if (with_seq_id_type)
            ResolveTrace("Cassandra request: " + bioseq_info_request.ToString());
        else
            ResolveTrace("Cassandra request without seq_id_type (INSDC type): " +
                         bioseq_info_request.ToString());
    }

    fetch_task->Wait();
}


void CAsyncBioseqQuery::x_OnBioseqInfo(vector<CBioseqInfoRecord>&&  records)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    m_Fetch->SetReadFinished();

    if (records.empty()) {
        // Nothing was found
        if (m_NeedTrace)
            ResolveTrace("0 records found");

        app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusNotFound,
                                  m_BioseqRequestStart);
        app->GetDBCounters().IncBioseqInfoNotFound();

        if (IsINSDCSeqIdType(m_BioseqResolution.m_BioseqInfo.GetSeqIdType())) {
            // Second try without seq_id_type (false => no seq_id_type)
            MakeRequest(false);
            return;
        }

        if (m_NeedTrace)
            ResolveTrace("Report not found");
        m_BioseqResolution.m_ResolutionResult = eNotResolved;
        m_PendingOp->OnBioseqDetailsRecord(std::move(m_BioseqResolution));
        return;
    }

    if (records.size() == 1) {
        // Exactly one match; no complications
        if (m_NeedTrace) {
            ResolveTrace("1 record found");
            ResolveTrace("Report 1 found");
        }

        m_BioseqResolution.m_ResolutionResult = eBioseqDB;

        app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                  m_BioseqRequestStart);
        app->GetDBCounters().IncBioseqInfoFoundOne();
        m_BioseqResolution.m_BioseqInfo = std::move(records[0]);
        m_PendingOp->OnBioseqDetailsRecord(std::move(m_BioseqResolution));
        return;
    }

    // Here: there are more than one records
    if (m_NeedTrace)
        ResolveTrace(to_string(records.size()) + " records found");

    if (m_BioseqResolution.m_BioseqInfo.GetVersion() != -1) {
        // More than one with the version provided
        if (m_NeedTrace) {
            ResolveTrace("Consider as nothing was found");
            ResolveTrace("Report 0 found");
        }

        m_BioseqResolution.m_ResolutionResult = eNotResolved;

        app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                  m_BioseqRequestStart);
        app->GetDBCounters().IncBioseqInfoFoundMany();
        m_PendingOp->OnBioseqDetailsRecord(std::move(m_BioseqResolution));
        return;
    }

    // More than one with no version provided: select the highest version
    size_t                          index = 0;
    CBioseqInfoRecord::TVersion     version = records[0].GetVersion();
    for (size_t  k = 0; k < records.size(); ++k) {
        if (records[k].GetVersion() > version) {
            index = k;
            version = records[k].GetVersion();
        }
    }

    if (m_NeedTrace) {
        ResolveTrace("Record with max version " + to_string(version) +
                     " selected");
        ResolveTrace("Report 1 found");
    }

    m_BioseqResolution.m_ResolutionResult = eBioseqDB;

    app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                              m_BioseqRequestStart);
    app->GetDBCounters().IncBioseqInfoFoundOne();
    m_BioseqResolution.m_BioseqInfo = std::move(records[index]);
    m_PendingOp->OnBioseqDetailsRecord(std::move(m_BioseqResolution));
}


void CAsyncBioseqQuery::x_OnBioseqInfoWithoutSeqIdType(
                                    vector<CBioseqInfoRecord>&&  records)
{
    m_NoSeqIdTypeFetch->SetReadFinished();

    auto                app = CPubseqGatewayApp::GetInstance();
    auto                request_version = m_BioseqResolution.m_BioseqInfo.GetVersion();
    SINSDCDecision      decision = DecideINSDC(records, request_version);

    switch (decision.status) {
        case CRequestStatus::e200_Ok:
            m_BioseqResolution.m_ResolutionResult = eBioseqDB;

            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqRequestStart);
            app->GetDBCounters().IncBioseqInfoFoundOne();
            m_BioseqResolution.m_BioseqInfo = std::move(records[decision.index]);

            // Data callback
            m_PendingOp->OnBioseqDetailsRecord(std::move(m_BioseqResolution));
            break;
        case CRequestStatus::e404_NotFound:
            m_BioseqResolution.m_ResolutionResult = eNotResolved;

            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusNotFound,
                                      m_BioseqRequestStart);
            app->GetDBCounters().IncBioseqInfoNotFound();

            // Data Callback
            m_PendingOp->OnBioseqDetailsRecord(std::move(m_BioseqResolution));
            break;
        case CRequestStatus::e500_InternalServerError:
            m_BioseqResolution.m_ResolutionResult = eNotResolved;

            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqRequestStart);
            app->GetDBCounters().IncBioseqInfoFoundMany();

            // Error callback
            m_PendingOp->OnBioseqDetailsError(
                CRequestStatus::e500_InternalServerError,
                eBioseqInfoMultipleRecords, eDiag_Error,
                decision.message,
                m_BioseqResolution.m_RequestStartTimestamp);
            break;
        default:
            // Impossible
            m_PendingOp->OnBioseqDetailsError(
                CRequestStatus::e500_InternalServerError, eServerLogicError,
                eDiag_Error, "Unexpected decision code when a secondary INSCD "
                "request results processed while retrieving bioseq info",
                m_BioseqResolution.m_RequestStartTimestamp);
    }
}


void CAsyncBioseqQuery::x_OnBioseqInfoError(
                                CRequestStatus::ECode  status, int  code,
                                EDiagSev  severity, const string &  message)
{
    if (m_NeedTrace)
        ResolveTrace("Cassandra error: " + message);

    if (m_Fetch)
        m_Fetch->SetReadFinished();
    if (m_NoSeqIdTypeFetch)
        m_NoSeqIdTypeFetch->SetReadFinished();

    CPubseqGatewayApp::GetInstance()->GetDBCounters().IncBioseqInfoError();

    m_PendingOp->OnBioseqDetailsError(
                                status, code, severity, message,
                                m_BioseqResolution.m_RequestStartTimestamp);
}

