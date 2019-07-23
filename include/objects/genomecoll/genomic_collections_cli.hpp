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
 */

/// @file genomic_collections_cli.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'gencoll_client.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: genomic_collections_cli_.hpp


#ifndef INTERNAL_GPIPE_OBJECTS_GENOMECOLL_GENOMIC_COLLECTIONS_CLI_HPP
#define INTERNAL_GPIPE_OBJECTS_GENOMECOLL_GENOMIC_COLLECTIONS_CLI_HPP

#include <objects/genomecoll/genomic_collections_cli_.hpp>
#include <objects/genomecoll/GCClient_GetAssemblyBySequ.hpp>
#include <objects/genomecoll/GCClient_Error.hpp>
#include <objects/genomecoll/cached_assembly.hpp>

BEGIN_NCBI_SCOPE

class CSQLITE_Connection;
class CArgDescriptions;

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CGCClient_AssemblyInfo;
class CGCClient_AssemblySequenceInfo;
class CGCClient_EquivalentAssemblies;


class CGCServiceException : public CException
{
public:
    CGCServiceException(const CDiagCompileInfo& diag, const objects::CGCClient_Error& srv_error);

    //all error codes are taken from the CGCClient_Error_Base::EError_id, please refer to them
    typedef objects::CGCClient_Error::EError_id EErrCode;

    NCBI_EXCEPTION_DEFAULT(CGCServiceException, CException);
    const char* GetErrCodeString(void) const override;
};

/////////////////////////////////////////////////////////////////////////////
class CGenomicCollectionsService : public CGenomicCollectionsService_Base
{
public:
    // add the 'automatic' cache parameter, for the calling application
    static void AddArguments(CArgDescriptions& arg_desc); 
    
    CGenomicCollectionsService(const string& cache_file="");
    ~CGenomicCollectionsService();

    CRef<CGC_Assembly> GetAssembly(const string& acc, const string& mode);
    CRef<CGC_Assembly> GetAssembly(int releaseId,     const string& mode);

    //Some general modes
    //please contact NCBI if you have other needs
    struct SAssemblyMode
    {
        //as defined in GenomeColl_Master.dbo.CodeServiceModeAlias

        //minimal contents: Only metadata
        static string kAssemblyOnly() {return "AssemblyOnly";};

        // Top-level molecules
        // No statistics
        // No alignments data (which could be very large)
        // pseudo scaffolds are reported as unlocalized/unplaced, as needed for analysis
        static string kScaffolds()  {return "Scaffolds";};

        // Top-level molecules
        // all statistics on assembly, units and replicons
        // alignments data when available
        // pseudo scaffolds are reported as unlocalized/unplaced, as needed for analysis
        static string kScaffoldsWithAlignments()  {return "ScaffoldsWithAlignments";};

        // Scaffolds
        // No alignment data
        // pseudo scaffolds reported as submitted.
        static string kEntrezIndexing()  {return "EntrezIndexing";};

        // Almost maximum contents:
        // all sequences down to contigs
        // No statistics
        // No alignments data (which could be very large)
        // pseudo scaffolds are reported as unlocalized/unplaced, as needed for analysis
        static string kAllSequences()  {return "AllSequences";};

        // Maximum contents:
        // all sequences down to contigs
        // all statistics on assembly, units and replicons
        // alignments data when available
        // pseudo scaffolds are reported as unlocalized/unplaced, as needed for analysis
        static string kAllSequencesWithAlignments()  {return "AllSequencesWithAlignments";};
    };

    string ValidateChrType(const string& chrType, const string& chrLoc);

    /// Find assembly by sequence accession
    ///
    /// @param sequence_acc
    ///     Sequence accession in ACC.VER format
    /// @param filter
    ///     Bitfield, OR-ed combination of eGCClient_GetAssemblyBySequenceFilter_* values:
    ///         eGCClient_GetAssemblyBySequenceFilter_all     - do not filter out anything
    ///         eGCClient_GetAssemblyBySequenceFilter_latest  - look up for latest releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_major   - look up for major releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_genbank - look up for GenBank releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_refseq  - look up for RefSeq releases only
    ///     If none of genbank of refseq filters are specified then both GenBank and RefSeq releases will be returned
    /// @param sort
    ///     Assembly ordering:
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_default - sort by number of sequences found in assembly, then by reference/representative/other, then by modification date
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_latest  - sort by number of sequences found in assembly, then latest first, then by reference/representative/other, then by modification date
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_major   - sort by number of sequences found in assembly, then major first, then by reference/representative/other, then by modification date
    /// @return Assembly if found, NULL otherwise.
    CRef<CGCClient_AssemblyInfo> FindOneAssemblyBySequences
        (const string& sequence_acc,
         int filter,
         CGCClient_GetAssemblyBySequenceRequest::ESort sort = CGCClient_GetAssemblyBySequenceRequest::eSort_default);

    /// Find assembly by sequence accessions
    ///
    /// @param sequence_acc
    ///     Sequence accessions in ACC.VER format
    /// @param filter
    ///     Bitfield, OR-ed combination of eGCClient_GetAssemblyBySequenceFilter_* values:
    ///         eGCClient_GetAssemblyBySequenceFilter_all     - do not filter out anything
    ///         eGCClient_GetAssemblyBySequenceFilter_latest  - look up for latest releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_major   - look up for major releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_genbank - look up for GenBank releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_refseq  - look up for RefSeq releases only
    ///     If none of genbank of refseq filters are specified then both GenBank and RefSeq releases will be returned
    /// @param sort
    ///     Assembly ordering:
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_default - sort by number of sequences found in assembly, then by reference/representative/other, then by modification date
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_latest  - sort by number of sequences found in assembly, then latest first, then by reference/representative/other, then by modification date
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_major   - sort by number of sequences found in assembly, then major first, then by reference/representative/other, then by modification date
    /// @return If assembly is found then return it with list of related sequences. NULL otherwise.
    CRef<CGCClient_AssemblySequenceInfo> FindOneAssemblyBySequences
        (const list<string>& sequence_acc,
         int filter,
         CGCClient_GetAssemblyBySequenceRequest::ESort sort = CGCClient_GetAssemblyBySequenceRequest::eSort_default,
         bool with_roles = false);

    /// Find assemblies by sequence accession
    ///
    /// @param sequence_acc
    ///     Sequence accession in ACC.VER format
    /// @param filter
    ///     Bitfield, OR-ed combination of eGCClient_GetAssemblyBySequenceFilter_* values:
    ///         eGCClient_GetAssemblyBySequenceFilter_all     - do not filter out anything
    ///         eGCClient_GetAssemblyBySequenceFilter_latest  - look up for latest releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_major   - look up for major releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_genbank - look up for GenBank releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_refseq  - look up for RefSeq releases only
    ///     If none of genbank of refseq filters are specified then both GenBank and RefSeq releases will be returned
    /// @param sort
    ///     Assembly ordering:
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_default - sort by number of sequences found in assembly, then by reference/representative/other, then by modification date
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_latest  - sort by number of sequences found in assembly, then latest first, then by reference/representative/other, then by modification date
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_major   - sort by number of sequences found in assembly, then major first, then by reference/representative/other, then by modification date
    /// @return List of found assemblies with related sequences.
    CRef<CGCClient_AssembliesForSequences> FindAssembliesBySequences
        (const string& sequence_acc,
         int filter,
         CGCClient_GetAssemblyBySequenceRequest::ESort sort = CGCClient_GetAssemblyBySequenceRequest::eSort_default,
         bool with_roles = false);

    /// Find assemblies by sequence accessions
    ///
    /// @param sequence_acc
    ///     Sequence accessions in ACC.VER format
    /// @param filter
    ///     Bitfield, OR-ed combination of eGCClient_GetAssemblyBySequenceFilter_* values:
    ///         eGCClient_GetAssemblyBySequenceFilter_all     - do not filter out anything
    ///         eGCClient_GetAssemblyBySequenceFilter_latest  - look up for latest releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_major   - look up for major releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_genbank - look up for GenBank releases only
    ///         eGCClient_GetAssemblyBySequenceFilter_refseq  - look up for RefSeq releases only
    ///     If none of genbank of refseq filters are specified then both GenBank and RefSeq releases will be returned
    /// @param sort
    ///     Assembly ordering:
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_default - sort by number of sequences found in assembly, then by reference/representative/other, then by modification date
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_latest  - sort by number of sequences found in assembly, then latest first, then by reference/representative/other, then by modification date
    ///         CGCClient_GetAssemblyBySequenceRequest::eSort_major   - sort by number of sequences found in assembly, then major first, then by reference/representative/other, then by modification date
    /// @return List of found assemblies with related sequences.
    CRef<CGCClient_AssembliesForSequences> FindAssembliesBySequences
        (const list<string>& sequence_acc,
         int filter,
         CGCClient_GetAssemblyBySequenceRequest::ESort sort = CGCClient_GetAssemblyBySequenceRequest::eSort_default,
         bool with_roles = false);



    CRef<CGCClient_EquivalentAssemblies> GetEquivalentAssemblies(const string& acc,
                                                                 int equivalency); // see CGCClient_GetEquivalentAssembliesRequest_::EEquivalency

private:
    // Prohibit copy constructor and assignment operator
    CGenomicCollectionsService(const CGenomicCollectionsService& value);
    CGenomicCollectionsService& operator=(const CGenomicCollectionsService& value);

    CRef<CGCClient_AssembliesForSequences> x_FindAssembliesBySequences(
            const list<string>& sequence_acc, 
            int filter, 
            CGCClient_GetAssemblyBySequenceRequest::ESort sort, 
            bool top_only, 
            bool with_roles);

    static const string s_GcCacheParamStr; // "gc-cache" 
    void x_ConfigureCache(const string& cache_file_param);
    string m_CacheFile;
    auto_ptr<CSQLITE_Connection> m_CacheConn;
};

END_objects_SCOPE
END_NCBI_SCOPE
#endif
