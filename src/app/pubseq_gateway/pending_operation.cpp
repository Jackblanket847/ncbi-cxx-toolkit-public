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
#include <ncbi_pch.hpp>

#include "pending_operation.hpp"
#include "pubseq_gateway_exception.hpp"


CPendingOperation::CPendingOperation(string &&  sat_name, int  sat_key,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout) :
    m_Reply(nullptr),
    m_Cancelled(false),
    m_FinishedRead(false),
    m_SatKey(sat_key)
{
    m_Loader.reset(new CCassBlobLoader(&m_Op, timeout, conn,
                                       std::move(sat_name), sat_key,
                                       true, true, nullptr,  nullptr));
}


CPendingOperation::~CPendingOperation()
{
    LOG5(("CPendingOperation::CPendingOperation: this: %p, m_Loader: %p",
          this, m_Loader.get()));
}


void CPendingOperation::Clear()
{
    LOG5(("CPendingOperation::Clear: this: %p, m_Loader: %p",
          this, m_Loader.get()));
    if (m_Loader) {
        m_Loader->SetDataReadyCB(nullptr, nullptr);
        m_Loader->SetErrorCB(nullptr);
        m_Loader->SetDataChunkCB(nullptr);
        m_Loader = nullptr;
    }
    m_Chunks.clear();
    m_Reply = nullptr;
    m_Cancelled = false;
    m_FinishedRead = false;
}


void CPendingOperation::Start(HST::CHttpReply<CPendingOperation>& resp)
{
    m_Reply = &resp;

    m_Loader->SetDataReadyCB(HST::CHttpReply<CPendingOperation>::s_DataReady, &resp);
    m_Loader->SetErrorCB(
        [&resp](const char* Text, CCassBlobWaiter* Waiter) {
            ERRLOG1(("%s", Text));
        }
    );
    m_Loader->SetDataChunkCB(
        [this, &resp](const unsigned char* Data, unsigned int Size, int ChunkNo, bool IsLast) {
            LOG3(("Chunk: [%d]: %u", ChunkNo, Size));
            assert(!m_FinishedRead);
            if (m_Cancelled) {
                m_Loader->Cancel();
                m_FinishedRead = true;
                return;
            }
            if (resp.IsFinished()) {
                ERRLOG1(("Unexpected data received while the output has finished, ignoring"));
                return;
            }
            if (resp.GetState() == HST::CHttpReply<CPendingOperation>::eReplyInitialized) {
                resp.SetContentLength(m_Loader->GetBlobSize());
            }
            if (Data && Size > 0)
                m_Chunks.push_back(resp.PrepadeChunk(Data, Size));
            if (IsLast)
                m_FinishedRead = true;
            if (resp.IsOutputReady())
                Peek(resp);
        }
    );

    m_Loader->Wait();
}


void CPendingOperation::Peek(HST::CHttpReply<CPendingOperation>& resp)
{
    if (m_Cancelled) {
        if (resp.IsOutputReady() && !resp.IsFinished()) {
            resp.Send(nullptr, 0, true, true);
        }
        return;
    }
    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call resp->  to send what we have if it is ready
    if (m_Loader) {
        m_Loader->Wait();
        if (m_Loader->HasError() && resp.IsOutputReady() && !resp.IsFinished()) {
            resp.Send503("error", m_Loader->LastError().c_str());
            return;
        }
    }

    if (resp.IsOutputReady() && (!m_Chunks.empty() || m_FinishedRead)) {
        resp.Send(m_Chunks, m_FinishedRead);
        m_Chunks.clear();
    }

/*
            void *dest = m_buf.Reserve(Size);
            if (resp.IsReady()) {

                size_t sz = m_len;
                if (sz > sizeof(m_buf))
                    sz = sizeof(m_buf);
                bool is_final = (m_len == sz);
                resp.SetOnReady(nullptr);
                resp.Send(m_buf, sz, true, is_final);
                m_len -= sz;
                if (!is_final) {
                    if (!m_wake)
                        NCBI_THROW(CPubseqGatewayException, eNoWakeCallback,
                                   "WakeCB is not assigned");
                    m_wake();
                }
            }
            else {
                if (!m_wake)
                    NCBI_THROW(CPubseqGatewayException, eNoWakeCallback,
                               "WakeCB is not assigned");
                resp.SetOnReady(m_wake);
            }
*/

}
