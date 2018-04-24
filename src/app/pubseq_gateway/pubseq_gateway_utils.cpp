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

#include <corelib/ncbistr.hpp>

#include "pubseq_gateway_utils.hpp"

USING_NCBI_SCOPE;


SBlobId::SBlobId() :
    sat(-1), sat_key(-1)
{}


SBlobId::SBlobId(const string &  blob_id) :
    sat(-1), sat_key(-1)
{
    list<string>    parts;
    NStr::Split(blob_id, ".", parts);

    if (parts.size() == 2) {
        try {
            sat = NStr::StringToNumeric<int>(parts.front());
            sat_key = NStr::StringToNumeric<int>(parts.back());
        } catch (...) {
        }
    }
}


SBlobId::SBlobId(int  sat_, int  sat_key_) :
    sat(sat_), sat_key(sat_key_)
{}


bool SBlobId::IsValid(void) const
{
    return sat >= 0 && sat_key >= 0;
}


string  GetBlobId(const SBlobId &  blob_id)
{
    return NStr::NumericToString(blob_id.sat) + "." +
           NStr::NumericToString(blob_id.sat_key);
}


SBlobId ParseBlobId(const string &  blob_id)
{
    return SBlobId(blob_id);
}


static string   s_ProtocolPrefix = "\n\nPSG-Reply-Chunk: ";
static string   s_ItemType = "item_type=";
static string   s_ChunkType = "chunk_type=";
static string   s_Size = "size=";
static string   s_BlobId = "blob_id=";
static string   s_Accession = "accession=";
static string   s_BlobChunk = "blob_chunk=";
static string   s_ReplyNChunks = "reply_n_chunks=";
static string   s_BlobNChunks = "blob_n_chunks=";
static string   s_BlobNDataChunks = "blob_n_data_chunks=";
static string   s_Status = "status=";
static string   s_Code = "code=";
static string   s_Severity = "severity=";
static string   s_ReplyItem = "reply";
static string   s_BlobItem = "blob";
static string   s_ResolutionItem = "resolution";
static string   s_Data = "data";
static string   s_Meta = "meta";
static string   s_Error = "error";
static string   s_Message = "message";

static string   s_ReplyBegin = s_ProtocolPrefix + s_ItemType;



static string SeverityToLowerString(EDiagSev  severity)
{
    string  severity_as_string = CNcbiDiag::SeverityName(severity);
    NStr::ToLower(severity_as_string);
    return severity_as_string;
}


string  GetResolutionHeader(size_t  resolution_size)
{
    return s_ReplyBegin + s_ResolutionItem +
           "&" + s_ChunkType + s_Data +
           "&" + s_Size + NStr::NumericToString(resolution_size) +
           "\n";
}


string  GetBlobChunkHeader(size_t  chunk_size, const SBlobId &  blob_id,
                           size_t  chunk_number)
{
    return s_ReplyBegin + s_BlobItem +
           "&" + s_ChunkType + s_Data +
           "&" + s_Size + NStr::NumericToString(chunk_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_BlobChunk + NStr::NumericToString(chunk_number) +
           "\n";
}


string  GetBlobCompletionHeader(const SBlobId &  blob_id,
                                size_t  total_blob_data_chunks,
                                size_t  total_cnunks)
{
    return s_ReplyBegin + s_BlobItem +
           "&" + s_ChunkType + s_Meta +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_BlobNDataChunks + NStr::NumericToString(total_blob_data_chunks) +
           "&" + s_BlobNChunks + NStr::NumericToString(total_cnunks) +
           "\n";
}


string  GetBlobErrorHeader(const SBlobId &  blob_id, size_t  msg_size,
                           CRequestStatus::ECode  status, int  code,
                           EDiagSev  severity)
{
    return s_ReplyBegin + s_BlobItem +
           "&" + s_ChunkType + s_Error +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetBlobMessageHeader(const SBlobId &  blob_id, size_t  msg_size,
                             CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity)
{
    return s_ReplyBegin + s_BlobItem +
           "&" + s_ChunkType + s_Message +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_BlobId + GetBlobId(blob_id) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetResolutionErrorHeader(const string &  accession, size_t  msg_size,
                                 CRequestStatus::ECode  status, int  code,
                                 EDiagSev  severity)
{
    return s_ReplyBegin + s_ResolutionItem +
           "&" + s_ChunkType + s_Error +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_Accession + accession +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetReplyErrorHeader(size_t  msg_size,
                            CRequestStatus::ECode  status, int  code,
                            EDiagSev  severity)
{
    return s_ReplyBegin + s_ReplyItem +
           "&" + s_ChunkType + s_Error +
           "&" + s_Size + NStr::NumericToString(msg_size) +
           "&" + s_Status + NStr::NumericToString(static_cast<int>(status)) +
           "&" + s_Code + NStr::NumericToString(code) +
           "&" + s_Severity + SeverityToLowerString(severity) +
           "\n";
}


string  GetReplyCompletionHeader(size_t  chunk_count)
{
    return s_ReplyBegin + s_ReplyItem +
           "&" + s_ChunkType + s_Meta +
           "&" + s_ReplyNChunks + NStr::NumericToString(chunk_count) +
           "\n";
}
