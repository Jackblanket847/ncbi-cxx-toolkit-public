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
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 * Cassandra delete blob task.
 *
 */

#include <ncbi_pch.hpp>

#include "delete_expired.hpp"

#include <memory>
#include <string>
#include <utility>

#include <inttypes.h>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassBlobTaskDeleteExpired::CCassBlobTaskDeleteExpired(
    unsigned int op_timeout_ms,
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    int32_t key,
    CBlobRecord::TTimestamp last_modified,
    CBlobRecord::TTimestamp expiration,
    unsigned int max_retries,
    TDataErrorCallback error_cb
)
    : CCassBlobWaiter(op_timeout_ms, conn, keyspace, key, true, max_retries, move(error_cb))
    , m_LastModified(last_modified)
    , m_Expiration(expiration)
{}

void CCassBlobTaskDeleteExpired::Wait1()
{
    bool b_need_repeat;
    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                CloseAll();
                m_QueryArr.clear();
                m_QueryArr.emplace_back(m_Conn->NewQuery());
                string sql = "SELECT writetime(n_chunks) FROM " + GetKeySpace()
                    + ".blob_prop WHERE sat_key = ? and last_modified = ?";
                m_QueryArr[0]->SetSQL(sql, 2);
                m_QueryArr[0]->BindInt32(0, m_Key);
                m_QueryArr[0]->BindInt64(0, m_LastModified);
                m_QueryArr[0]->Query(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                UpdateLastActivity();
                m_State = eReadingWriteTime;
                break;
            }

            case eReadingWriteTime: {
                string sql;
                auto & qry = m_QueryArr[0];
                if (!CheckReady(qry, eInit, &b_need_repeat)) {
                    if (b_need_repeat) {
                        ERR_POST(Info << "Restart eReadingWriteTime key="
                             << m_Keyspace << "." << m_Key
                             << " last_updated = " << m_LastModified);
                    }
                    break;
                }
                if (!qry->IsEOF()) {
                    UpdateLastActivity();
                    async_rslt_t wr = qry->NextRow();
                    if (wr == ar_dataready) {
                        CBlobRecord::TTimestamp write_timestamp = qry->FieldGetInt64Value(0);
                        ERR_POST(Trace << "BlobDeleteExpired write_time fetch key=" << m_Keyspace << "."
                                 << m_Key << ", last_modified = " << m_LastModified
                                 << ", write_time = " << write_timestamp
                        );

                        CloseAll();
                        CTime expiration(CTime::eEmpty, CTime::eUTC);
                        expiration.SetSecond(write_timestamp / 1000);
                        expiration.SetMilliSecond(write_timestamp % 1000);
                        expiration.AddSecond(m_Expiration);
                        if (CTime(CTime::eCurrent, CTime::eUTC) > expiration) {
                            m_State = eDeleteData;
                            b_need_repeat = true;
                        } else {
                            ERR_POST(Trace << "BlobDeleteExpired key=" << m_Keyspace << "." << m_Key
                                     << ", last_modified = " << m_LastModified << " is not expired yet"
                            );
                            m_State = eDone;
                        }
                    }
                } else {
                    CloseAll();
                    ERR_POST(Warning << "BlobDeleteExpired key=" << m_Keyspace << "." << m_Key
                             << ", last_modified = " << m_LastModified << " is not found"
                    );
                    m_State = eDone;
                }
                break;
            }

            case eDeleteData: {
                UpdateLastActivity();
                auto& qry = m_QueryArr[0];
                qry->NewBatch();
                string sql = "DELETE FROM " + GetKeySpace() + ".blob_chunk WHERE sat_key = ? and last_modified = ?";
                qry->SetSQL(sql, 2);
                qry->BindInt32(0, m_Key);
                qry->BindInt64(1, m_LastModified);
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                sql = "DELETE FROM " + GetKeySpace() + ".blob_prop WHERE sat_key = ? and last_modified = ?";
                qry->SetSQL(sql, 2);
                qry->BindInt32(0, m_Key);
                qry->BindInt64(1, m_LastModified);
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                qry->RunBatch();
                m_State = eWaitDeleteData;
                break;
            }

            case eWaitDeleteData: {
                if (!CheckReady(m_QueryArr[0], eDeleteData, &b_need_repeat)) {
                    if (b_need_repeat) {
                        ERR_POST(Info << "BlobDeleteExpired restarted eDeleteData key="
                                 << m_Keyspace << "." << m_Key << ", last_modified = " << m_LastModified
                        );
                    }
                    break;
                } else {
                    ERR_POST(Trace << "BlobDeleteExpired blob key=" << m_Keyspace << "." << m_Key
                             << ", last_modified = " << m_LastModified << " has been deleted successfully"
                    );
                }

                CloseAll();
                m_State = eDone;
                break;
            }

            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg),
                         "Failed to delete expired version of blob (key=%s.%d, v= %" PRId64 ") unexpected state (%d)",
                         m_Keyspace.c_str(), m_Key, m_LastModified, static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
