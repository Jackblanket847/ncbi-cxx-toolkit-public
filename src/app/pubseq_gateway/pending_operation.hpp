#ifndef PENDING_OPERATION__HPP
#define PENDING_OPERATION__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <string>
#include <memory>
using namespace std;

#include <corelib/request_ctx.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot/record.hpp>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

#include "http_server_transport.hpp"
#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_types.hpp"
#include "pending_requests.hpp"


class CBlobChunkCallback;
class CBlobPropCallback;
class CGetBlobErrorCallback;


class CPendingOperation
{
public:
    CPendingOperation(const SBlobRequest &  blob_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);
    CPendingOperation(const SResolveRequest &  resolve_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);
    CPendingOperation(const SAnnotRequest &  annot_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);

    ~CPendingOperation();

    void Clear();
    void Start(HST::CHttpReply<CPendingOperation>& resp);
    void Peek(HST::CHttpReply<CPendingOperation>& resp, bool  need_wait);

    void Cancel(void)
    {
        m_Cancelled = true;
    }

    size_t  GetItemId(void)
    {
        return ++m_NextItemId;
    }

    void UpdateOverallStatus(CRequestStatus::ECode  status)
    {
        m_OverallStatus = max(status, m_OverallStatus);
    }

public:
    CPendingOperation(const CPendingOperation&) = delete;
    CPendingOperation& operator=(const CPendingOperation&) = delete;
    CPendingOperation(CPendingOperation&&) = default;
    CPendingOperation& operator=(CPendingOperation&&) = default;

private:
    // Internal structure to keep track of the retrieving:
    // - a blob
    // - a named annotation
    struct SFetchDetails
    {
        SFetchDetails(const SBlobRequest &  blob_request) :
            m_RequestType(eBlobRequest),
            m_BlobId(blob_request.m_BlobId),
            m_TSEOption(blob_request.m_TSEOption),
            m_BlobPropSent(false),
            m_BlobIdType(blob_request.GetBlobIdentificationType()),
            m_TotalSentBlobChunks(0), m_FinishedRead(false),
            m_BlobPropItemId(0),
            m_BlobChunkItemId(0)
        {}

        SFetchDetails(const SAnnotRequest &  annot_request) :
            m_RequestType(eAnnotationRequest),
            m_TSEOption(eUnknownTSE),
            m_BlobPropSent(false),
            m_BlobIdType(eBySatAndSatKey),
            m_TotalSentBlobChunks(0),
            m_FinishedRead(false),
            m_BlobPropItemId(0),
            m_BlobChunkItemId(0)
        {}

        SFetchDetails() :
            m_RequestType(eUnknownRequest),
            m_TSEOption(eUnknownTSE),
            m_BlobPropSent(false),
            m_BlobIdType(eBySatAndSatKey),
            m_TotalSentBlobChunks(0),
            m_FinishedRead(false),
            m_BlobPropItemId(0),
            m_BlobChunkItemId(0)
        {}

        // Applicable only for the blob requests
        bool IsBlobPropStage(void) const
        {
            // At the time of an error report it needs to be known to what the
            // error message is associated - to blob properties or to blob
            // chunks
            return !m_BlobPropSent;
        }

        // Applicable only for the blob requests
        size_t GetBlobPropItemId(CPendingOperation *  pending_op)
        {
            if (m_BlobPropItemId == 0)
                m_BlobPropItemId = pending_op->GetItemId();
            return m_BlobPropItemId;
        }

        // Applicable only for the blob requests
        size_t GetBlobChunkItemId(CPendingOperation *  pending_op)
        {
            if (m_BlobChunkItemId == 0)
                m_BlobChunkItemId = pending_op->GetItemId();
            return m_BlobChunkItemId;
        }

        void ResetCallbacks(void)
        {
            if (m_Loader) {
                if (m_RequestType == eBlobRequest) {
                    CCassBlobTaskLoadBlob *     load_task =
                            static_cast<CCassBlobTaskLoadBlob*>(m_Loader.get());
                    load_task->SetDataReadyCB(nullptr, nullptr);
                    load_task->SetErrorCB(nullptr);
                    load_task->SetChunkCallback(nullptr);
                } else if (m_RequestType == eAnnotationRequest) {
                    CCassNAnnotTaskFetch *      fetch_task =
                            static_cast<CCassNAnnotTaskFetch*>(m_Loader.get());
                    fetch_task->SetConsumeCallback(nullptr);
                    fetch_task->SetErrorCB(nullptr);
                }
            }
        }

        EPendingRequestType                 m_RequestType;

        SBlobId                             m_BlobId;
        ETSEOption                          m_TSEOption;

        bool                                m_BlobPropSent;

        EBlobIdentificationType             m_BlobIdType;
        int32_t                             m_TotalSentBlobChunks;
        bool                                m_FinishedRead;

        // This is a base class. There are multiple types of the loaders stored
        // here:
        // - CCassBlobTaskLoadBlob to load a blob
        // - CCassNAnnotTaskFetch to load an annotation
        unique_ptr<CCassBlobWaiter>         m_Loader;

        private:
            size_t                          m_BlobPropItemId;
            size_t                          m_BlobChunkItemId;
    };

public:
    void PrepareBioseqMessage(size_t  item_id, const string &  msg,
                              CRequestStatus::ECode  status,
                              int  err_code, EDiagSev  severity);
    void PrepareBioseqData(size_t  item_id, const string &  content,
                           EOutputFormat  output_format);
    void PrepareBioseqCompletion(size_t  item_id, size_t  chunk_count);
    void PrepareBlobPropMessage(size_t  item_id, const string &  msg,
                                CRequestStatus::ECode  status,
                                int  err_code, EDiagSev  severity);
    void PrepareBlobPropMessage(
                            SFetchDetails *  fetch_details,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity);
    void PrepareBlobPropData(SFetchDetails *  fetch_details,
                             const string &  content);
    void PrepareBlobData(SFetchDetails *  fetch_details,
                         const unsigned char *  chunk_data,
                         unsigned int  data_size, int  chunk_no);
    void PrepareBlobPropCompletion(size_t  item_id, size_t  chunk_count);
    void PrepareBlobPropCompletion(
                            SFetchDetails *  fetch_details);
    void PrepareBlobMessage(size_t  item_id, const SBlobId &  blob_id,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity);
    void PrepareBlobMessage(SFetchDetails *  fetch_details,
                            const string &  msg,
                            CRequestStatus::ECode  status, int  err_code,
                            EDiagSev  severity);
    void PrepareBlobCompletion(size_t  item_id, const SBlobId &  blob_id,
                               size_t  chunk_count);
    void PrepareBlobCompletion(SFetchDetails *  fetch_details);
    void PrepareReplyMessage(const string &  msg,
                             CRequestStatus::ECode  status, int  err_code,
                             EDiagSev  severity);
    void PrepareNamedAnnotationData(const string &  accession,
                                    int16_t  version, int16_t  seq_id_type,
                                    const string &  annot_name,
                                    const string &  content);
    void PrepareReplyCompletion(size_t  chunk_count);

    ECacheAndCassandraUse GetUrlUseCache(void) const
    {
        return m_UrlUseCache;
    }

private:
    void x_ProcessResolveRequest(void);
    void x_ProcessGetRequest(void);
    void x_ProcessAnnotRequest(void);
    void x_StartMainBlobRequest(void);
    bool x_AllFinishedRead(void) const;
    void x_SendReplyCompletion(bool  forced = false);
    void x_SetRequestContext(void);
    void x_PrintRequestStop(int  status);
    bool x_SatToSatName(const SBlobRequest &  blob_request,
                        SBlobId &  blob_id);
    void x_SendUnknownServerSatelliteError(size_t  item_id,
                                           const string &  message,
                                           int  error_code);
    void x_SendBioseqInfo(const string &  protobuf_bioseq_info,
                          SBioseqInfo &  bioseq_info,
                          EOutputFormat  output_format);
    void x_Peek(HST::CHttpReply<CPendingOperation>& resp, bool  need_wait,
                unique_ptr<SFetchDetails> &  fetch_details);

    bool x_UsePsgProtocol(void) const;
    void x_InitUrlIndentification(void);

private:
    SBioseqResolution x_ResolveInputSeqId(string &  err_msg);
    bool x_ResolvePrimaryOSLT(const string &  primary_id,
                              int16_t  effective_version,
                              int16_t  effective_seq_id_type,
                              bool  need_to_try_bioseq_info,
                              SBioseqResolution &  bioseq_resolution);
    bool x_ResolveSecondaryOSLT(const string &  secondary_id,
                                int16_t  effective_seq_id_type,
                                SBioseqResolution &  bioseq_resolution);
    bool x_ResolvePrimaryOSLTviaDB(const string &  primary_id,
                                   int16_t  effective_seq_id_type,
                                   int16_t  effective_version,
                                   bool  need_to_try_bioseq_info,
                                   SBioseqResolution &  bioseq_resolution);
    bool x_ResolveSecondaryOSLTviaDB(const string &  secondary_id,
                                     int16_t  effective_seq_id_type,
                                     SBioseqResolution &  bioseq_resolution);
    bool x_ResolveViaComposeOSLT(CSeq_id &  parsed_seq_id,
                                 string &  err_msg,
                                 SBioseqResolution &  bioseq_resolution);
    bool x_ResolveAsIs(SBioseqResolution &  bioseq_resolution);
    bool x_ResolveAsIsInDB(SBioseqResolution &  bioseq_resolution);
    bool x_GetEffectiveSeqIdType(const CSeq_id &  parsed_seq_id,
                                 int16_t &  eff_seq_id_type);
    int16_t x_GetEffectiveVersion(const CTextseq_id *  text_seq_id);
    bool x_LookupCachedBioseqInfo(const string &  accession,
                                  int16_t &  version,
                                  int16_t &  seq_id_type,
                                  string &  bioseq_info_cache_data);
    bool x_LookupCachedCsi(const string &  seq_id,
                           int16_t &  seq_id_type,
                           string &  csi_cache_data);
    bool x_LookupBlobPropCache(int  sat, int  sat_key,
                               int64_t &  last_modified,
                               CBlobRecord &  blob_record);
    ESeqIdParsingResult x_ParseInputSeqId(CSeq_id &  seq_id, string &  err_msg);

private:
    HST::CHttpReply<CPendingOperation> *    m_Reply;
    shared_ptr<CCassConnection>             m_Conn;
    unsigned int                            m_Timeout;
    unsigned int                            m_MaxRetries;
    CRequestStatus::ECode                   m_OverallStatus;

    // Incoming request. It could be by sat/sat_key or by seq_id/id_type
    EPendingRequestType                     m_RequestType;
    SBlobRequest                            m_BlobRequest;
    SResolveRequest                         m_ResolveRequest;
    SAnnotRequest                           m_AnnotRequest;
    CTempString                             m_UrlSeqId;
    int16_t                                 m_UrlSeqIdType;
    ECacheAndCassandraUse                   m_UrlUseCache;

    int32_t                                 m_TotalSentReplyChunks;
    bool                                    m_Cancelled;
    vector<h2o_iovec_t>                     m_Chunks;
    CRef<CRequestContext>                   m_RequestContext;

    unique_ptr<SFetchDetails>               m_MainObjectFetchDetails;
    unique_ptr<SFetchDetails>               m_Id2InfoFetchDetails;
    unique_ptr<SFetchDetails>               m_OriginalBlobChunkFetch;

    // Multiple requests like ID2 chanks or named annotations in
    // many keyspaces
    vector<unique_ptr<SFetchDetails>>       m_FetchDetails;

    size_t                                  m_NextItemId;

    friend class CBlobChunkCallback;
    friend class CBlobPropCallback;
    friend class CGetBlobErrorCallback;
    friend class CNamedAnnotationCallback;
    friend class CNamedAnnotationErrorCallback;
};



class CBlobChunkCallback
{
    public:
        CBlobChunkCallback(
                CPendingOperation *  pending_op,
                HST::CHttpReply<CPendingOperation> *  reply,
                CPendingOperation::SFetchDetails *  fetch_details) :
            m_PendingOp(pending_op), m_Reply(reply),
            m_FetchDetails(fetch_details)
        {}

        void operator()(CBlobRecord const & blob, const unsigned char *  data,
                        unsigned int  size, int  chunk_no);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SFetchDetails *      m_FetchDetails;
};


class CBlobPropCallback
{
    public:
        CBlobPropCallback(
                CPendingOperation *  pending_op,
                HST::CHttpReply<CPendingOperation> *  reply,
                CPendingOperation::SFetchDetails *  fetch_details) :
            m_PendingOp(pending_op), m_Reply(reply),
            m_FetchDetails(fetch_details),
            m_Id2InfoParsed(false), m_Id2InfoValid(false),
            m_Id2InfoSat(-1),
            m_Id2InfoInfo(-1), m_Id2InfoChunks(-1)
        {}

         void operator()(CBlobRecord const &  blob, bool is_found);

    private:
        void x_RequestOriginalBlobChunks(CBlobRecord const &  blob);
        void x_RequestID2BlobChunks(CBlobRecord const &  blob,
                                    bool  info_blob_only);
        void x_ParseId2Info(CBlobRecord const &  blob);
        void x_RequestId2SplitBlobs(const string &  sat_name);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SFetchDetails *      m_FetchDetails;

        // id2_info parsing support
        bool            m_Id2InfoParsed;
        bool            m_Id2InfoValid;
        int             m_Id2InfoSat;
        int             m_Id2InfoInfo;
        int             m_Id2InfoChunks;
};


class CGetBlobErrorCallback
{
    public:
        CGetBlobErrorCallback(
                CPendingOperation *  pending_op,
                HST::CHttpReply<CPendingOperation> *  reply,
                CPendingOperation::SFetchDetails *  fetch_details) :
            m_PendingOp(pending_op), m_Reply(reply),
            m_FetchDetails(fetch_details)
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SFetchDetails *      m_FetchDetails;
};


class CNamedAnnotationCallback
{
    public:
        CNamedAnnotationCallback(
                CPendingOperation *  pending_op,
                HST::CHttpReply<CPendingOperation> *  reply,
                CPendingOperation::SFetchDetails *  fetch_details,
                int32_t  sat) :
            m_PendingOp(pending_op), m_Reply(reply),
            m_FetchDetails(fetch_details),
            m_Sat(sat)
        {}

        bool operator()(CNAnnotRecord &&  annot_record, bool  last);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SFetchDetails *      m_FetchDetails;
        int32_t                                 m_Sat;
};


class CNamedAnnotationErrorCallback
{
    public:
        CNamedAnnotationErrorCallback(
                CPendingOperation *  pending_op,
                HST::CHttpReply<CPendingOperation> *  reply,
                CPendingOperation::SFetchDetails *  fetch_details) :
            m_PendingOp(pending_op), m_Reply(reply),
            m_FetchDetails(fetch_details)
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message);

    private:
        CPendingOperation *                     m_PendingOp;
        HST::CHttpReply<CPendingOperation> *    m_Reply;
        CPendingOperation::SFetchDetails *      m_FetchDetails;
};

#endif
