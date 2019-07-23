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
 * Author:  Vinay Kumar
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'gencoll_client.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/genomecoll/genomic_collections_cli.hpp>
#include <objects/genomecoll/GCClient_AssemblyInfo.hpp>
#include <objects/genomecoll/GCClient_AssemblySequenceI.hpp>
#include <objects/genomecoll/GCClient_AssembliesForSequ.hpp>
#include <objects/genomecoll/GCClient_Error.hpp>
#include <objects/genomecoll/GC_Assembly.hpp>
#include <objects/genomecoll/GCClient_ValidateChrTypeLo.hpp>
#include <objects/genomecoll/GCClient_EquivalentAssembl.hpp>
#include <objects/genomecoll/GCClient_GetEquivalentAsse.hpp>
#include <objects/genomecoll/GCClient_GetAssemblyBlobRe.hpp>
#include <objects/genomecoll/cached_assembly.hpp>
#include <sstream>
#include <db/sqlite/sqlitewrapp.hpp>
#include <corelib/ncbiargs.hpp>

// generated classes

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


CGCServiceException::CGCServiceException(const CDiagCompileInfo& diag, const objects::CGCClient_Error& srv_error)
    : CGCServiceException(diag, nullptr,
                          CGCServiceException::EErrCode(srv_error.CanGetError_id() ? srv_error.GetError_id() : CException::eInvalid),
                          srv_error.GetDescription())
{}

const char* CGCServiceException::GetErrCodeString(void) const
{
    return CGCClient_Error::ENUM_METHOD_NAME(EError_id)()->FindName((int)GetErrCode(), true).c_str();
}

static const STimeout kTimeout = {600, 0};

CGenomicCollectionsService::CGenomicCollectionsService(const string& cache_file)
{
    SetTimeout(&kTimeout);
    SetRetryLimit(20);

    // it's a backward-compatibility fix for old versions of server (no much harm to leave it - only little data overhead is expected)
    // always send request and get response in ASN text format so that server can properly parse request
    // For binary ASN request: client\server versions of ASN request must be exactly the same (compiled using one ASN definition - strong typing)
    // For text ASN request: client\server versions of ASN request can be different (use different ASN definitions - duck typing)
    SetFormat(eSerial_AsnText);
    // SetFormat() - sets both Request and Response encoding, so we put "fo=text" as well (though not needed now it may be usefull in the future for the client back-compatibility)
    SetArgs("fi=text&fo=text");


    x_ConfigureCache(cache_file);
}

CGenomicCollectionsService::~CGenomicCollectionsService()
{
    if(!m_CacheFile.empty()) {
        m_CacheConn.reset(NULL);
        CSQLITE_Global::Finalize();
    }
} 


template<typename TReq>
void LogRequest(const TReq& req)
{
#ifdef _DEBUG
    ostringstream ostrstrm;
    ostrstrm << "Making request -" << MSerial_AsnText << req;
    ERR_POST(Info << ostrstrm.str());
#endif
}

static void ValidateAsmAccession(const string& acc)
{
    if(acc.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") != string::npos)
        NCBI_THROW(CException, eUnknown, "Invalid accession format: " + acc);
}

CRef<CGC_Assembly> CGenomicCollectionsService::GetAssembly(const string& acc_, const string& mode)
{
    string acc = NStr::TruncateSpaces(acc_);
    ValidateAsmAccession(acc);


    if(!m_CacheFile.empty()) {
        const string SltSql = "SELECT gc_blob FROM GetAssemblyBlob "
                              "WHERE acc_ver = ? AND mode = ?";
        CSQLITE_Statement Stmt(m_CacheConn.get(), SltSql);
        Stmt.Bind(1, acc);
        Stmt.Bind(2, mode);
        while(Stmt.Step()) {
            const string Blob = Stmt.GetString(0);
            CRef<CCachedAssembly> CAsm(new CCachedAssembly(Blob));
            return CAsm->Assembly();
        }
    }


    CGCClient_GetAssemblyBlobRequest req;
    CGCClientResponse reply;

    req.SetAccession(acc);
    req.SetMode(mode);

    LogRequest(req);

    try {
        return CCachedAssembly(AskGet_assembly_blob(req, &reply)).Assembly();
    } catch (CRPCClientException& e) {
        if (reply.IsSrvr_error()) {
            throw CGCServiceException(DIAG_COMPILE_INFO, reply.GetSrvr_error());
        }
        if (e.GetErrCode() == CRPCClientException::eFailed ) {
            ERR_POST(Error << "CGI failed to return result. Check applog for errors in gc_get_assembly_v3.cgi and refresh_cache");
        }
        throw;
    } catch (CException& ) {
        if (reply.IsSrvr_error()) {
            throw CGCServiceException(DIAG_COMPILE_INFO, reply.GetSrvr_error());
        }
        throw;
    }
}

CRef<CGC_Assembly> CGenomicCollectionsService::GetAssembly(int releaseId, const string& mode)
{
    CGCClient_GetAssemblyBlobRequest req;
    CGCClientResponse reply;
    
    if(!m_CacheFile.empty()) {
        const string SltSql = "SELECT gc_blob FROM GetAssemblyBlob "
                              "WHERE release_id = ? AND mode = ?";
        CSQLITE_Statement Stmt(m_CacheConn.get(), SltSql);
        Stmt.Bind(1, releaseId);
        Stmt.Bind(2, mode);
        while(Stmt.Step()) {
            const string Blob = Stmt.GetString(0);
            CRef<CCachedAssembly> CAsm(new CCachedAssembly(Blob));
            return CAsm->Assembly();
        }
    }
    
    req.SetRelease_id(releaseId);
    req.SetMode(mode);

    LogRequest(req);

    try {
        return CCachedAssembly(AskGet_assembly_blob(req, &reply)).Assembly();
    } catch (CRPCClientException& e) {
        if (reply.IsSrvr_error()) {
            throw CGCServiceException(DIAG_COMPILE_INFO, reply.GetSrvr_error());
        }
        if (e.GetErrCode() == CRPCClientException::eFailed ) {
            ERR_POST(Error << "CGI failed to return result. Check applog for errors in gc_get_assembly_v3.cgi and refresh_cache");
        }
        throw;
    } catch (CException& ) {
        if (reply.IsSrvr_error()) {
            throw CGCServiceException(DIAG_COMPILE_INFO, reply.GetSrvr_error());
        }
        throw;
    }
}

string CGenomicCollectionsService::ValidateChrType(const string& chrType, const string& chrLoc)
{
    CGCClient_ValidateChrTypeLocRequest req;
    CGCClientResponse reply;

    req.SetType(chrType);
    req.SetLocation(chrLoc);

    LogRequest(req);

    try {
        return AskGet_chrtype_valid(req, &reply);
    } catch (CException& ) {
        if (reply.IsSrvr_error())
            throw CGCServiceException(DIAG_COMPILE_INFO, reply.GetSrvr_error());
        throw;
    }
}

CRef<CGCClient_AssemblyInfo> CGenomicCollectionsService::FindOneAssemblyBySequences(const string& sequence_acc, int filter, CGCClient_GetAssemblyBySequenceRequest::ESort sort)
{
    CRef<CGCClient_AssemblySequenceInfo> asmseq_info(FindOneAssemblyBySequences(list<string>(1, sequence_acc), filter, sort));

    return asmseq_info ? CRef<CGCClient_AssemblyInfo>(&asmseq_info->SetAssembly()) : CRef<CGCClient_AssemblyInfo>();
}

CRef<CGCClient_AssemblySequenceInfo> CGenomicCollectionsService::FindOneAssemblyBySequences(const list<string>& sequence_acc, int filter, CGCClient_GetAssemblyBySequenceRequest::ESort sort, bool with_roles)
{
    CRef<CGCClient_AssembliesForSequences> assm(x_FindAssembliesBySequences(sequence_acc, filter, sort, true, with_roles));

    return assm->CanGetAssemblies() && !assm->GetAssemblies().empty() ?
           CRef<CGCClient_AssemblySequenceInfo>(assm->SetAssemblies().front()) :
           CRef<CGCClient_AssemblySequenceInfo>();
}

CRef<CGCClient_AssembliesForSequences> CGenomicCollectionsService::FindAssembliesBySequences(const string& sequence_acc, int filter, CGCClient_GetAssemblyBySequenceRequest::ESort sort, bool with_roles)
{
    return FindAssembliesBySequences(list<string>(1, sequence_acc), filter, sort, with_roles);
}

CRef<CGCClient_AssembliesForSequences> CGenomicCollectionsService::FindAssembliesBySequences(const list<string>& sequence_acc, int filter, CGCClient_GetAssemblyBySequenceRequest::ESort sort, bool with_roles)
{
    return x_FindAssembliesBySequences(sequence_acc, filter, sort, false, with_roles);
}
CRef<CGCClient_AssembliesForSequences> CGenomicCollectionsService::x_FindAssembliesBySequences(const list<string>& sequence_acc, int filter, CGCClient_GetAssemblyBySequenceRequest::ESort sort, bool top_only, bool with_roles)

{
    CGCClient_GetAssemblyBySequenceRequest req;
    CGCClientResponse reply;

    for(auto acc : sequence_acc)
        if(acc.length() > 30) {
            NCBI_THROW(CException, eUnknown, "Accession is longer than 30 characters: " + acc);
        }

    req.SetSequence_acc().assign(sequence_acc.begin(), sequence_acc.end());
    req.SetFilter(filter);
    req.SetSort(sort);
    req.SetTop_assembly_only(top_only ? 1 : 0);
    if (with_roles) {
        req.SetAdd_sequence_roles(true);
    }

    LogRequest(req);

    try {
        return AskGet_assembly_by_sequence(req, &reply);
    } catch (const CException& ) {
        if (reply.IsSrvr_error())
            throw CGCServiceException(DIAG_COMPILE_INFO, reply.GetSrvr_error());
        throw;
    }
}


CRef<CGCClient_EquivalentAssemblies> CGenomicCollectionsService::GetEquivalentAssemblies(const string& acc, int equivalency)
{
    CGCClient_GetEquivalentAssembliesRequest req;
    CGCClientResponse reply;

    req.SetAccession(acc);
    req.SetEquivalency(equivalency);

    LogRequest(req);

    try {
        return AskGet_equivalent_assemblies(req, &reply);
    } catch (const CException& ) {
        if (reply.IsSrvr_error())
            throw CGCServiceException(DIAG_COMPILE_INFO, reply.GetSrvr_error());
        throw;
    }
}


void CGenomicCollectionsService::AddArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Assembly cache options");
    arg_desc.AddOptionalKey(s_GcCacheParamStr, "gc_cache_file",
                            "Full path for local gencoll assembly cache", CArgDescriptions::eString);
}

const string CGenomicCollectionsService::s_GcCacheParamStr = "gc-cache";
void CGenomicCollectionsService::x_ConfigureCache(const string& cache_file_param) 
{
    string local_cache_file;
   
    // priority order
    //  local parameter
    //  argument
    //  no env 
    //  no reg

    if(CNcbiApplication::Instance()) {
        const CArgs& args = CNcbiApplication::Instance()->GetArgs();
        if (args.Exist(s_GcCacheParamStr) && 
            args[s_GcCacheParamStr].HasValue()) {
            const string arg_cache_file = args[s_GcCacheParamStr].AsString();
            if(!arg_cache_file.empty()) {
                local_cache_file = arg_cache_file;
            }
        }
    }

    if(!cache_file_param.empty()) {
        local_cache_file = cache_file_param;
    }
   
    if(!local_cache_file.empty() && CFile(local_cache_file).Exists()) {
        m_CacheFile = local_cache_file;
        cerr<<__LINE__<<" : "<<m_CacheFile<<endl;
        CSQLITE_Global::Initialize();
        m_CacheConn.reset(new CSQLITE_Connection(m_CacheFile,
                CSQLITE_Connection::fReadOnly|CSQLITE_Connection::eAllMT));
    }
}

END_objects_SCOPE
END_NCBI_SCOPE
