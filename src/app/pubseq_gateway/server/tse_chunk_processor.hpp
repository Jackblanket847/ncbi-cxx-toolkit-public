#ifndef PSGS_TSECHUNKPROCESSOR__HPP
#define PSGS_TSECHUNKPROCESSOR__HPP

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
 * File Description: get TSE chunk processor
 *
 */

#include "ipsgs_processor.hpp"
#include "cass_blob_base.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

// Forward declaration
class CCassFetch;


class CPSGS_TSEChunkProcessor : public IPSGS_Processor,
                                public CPSGS_CassBlobBase
{
public:
    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply) const;
    virtual void Process(void);
    virtual void Cancel(void);
    virtual bool IsFinished(void);
    virtual void ProcessEvent(void);

public:
    CPSGS_TSEChunkProcessor();
    CPSGS_TSEChunkProcessor(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply);
    virtual ~CPSGS_TSEChunkProcessor();

private:
    void OnGetBlobProp(CCassBlobFetch *  fetch_details,
                       CBlobRecord const &  blob, bool is_found);
    void OnGetBlobError(CCassBlobFetch *  fetch_details,
                        CRequestStatus::ECode  status, int  code,
                        EDiagSev  severity, const string &  message);
    void OnGetBlobChunk(CCassBlobFetch *  fetch_details,
                        CBlobRecord const &  blob,
                        const unsigned char *  chunk_data,
                        unsigned int  data_size, int  chunk_no);
    void OnGetSplitHistory(CCassSplitHistoryFetch *  fetch_details,
                           vector<SSplitHistoryRecord> && result);
    void OnGetSplitHistoryError(CCassSplitHistoryFetch *  fetch_details,
                                CRequestStatus::ECode  status, int  code,
                                EDiagSev  severity, const string &  message);

private:
    bool x_ParseTSEChunkId2Info(const string &  info,
                                unique_ptr<CPSGId2Info> &  id2_info,
                                const SPSGS_BlobId &  blob_id,
                                bool  need_finish);
    void x_SendReplyError(const string &  msg,
                          CRequestStatus::ECode  status,
                          int  code);
    bool x_ValidateTSEChunkNumber(int64_t  requested_chunk,
                                  CPSGId2Info::TChunks  total_chunks,
                                  bool  need_finish);
    bool x_TSEChunkSatToSatName(SPSGS_BlobId &  blob_id,
                                bool  need_finish);
    void x_RequestTSEChunk(const SSplitHistoryRecord &  split_record,
                           CCassSplitHistoryFetch *  fetch_details);

private:
    void x_Peek(bool  need_wait);
    void x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                bool  need_wait);

private:
    SPSGS_TSEChunkRequest *     m_TSEChunkRequest;
    bool                        m_Cancelled;
};

#endif  // PSGS_TSECHUNKPROCESSOR__HPP

