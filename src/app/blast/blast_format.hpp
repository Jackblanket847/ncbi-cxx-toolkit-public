/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

Author: Jason Papadopoulos

******************************************************************************/

/** @file blast_format.hpp
 * Produce formatted blast output
*/

#ifndef APP___BLAST_FORMAT__HPP
#define APP___BLAST_FORMAT__HPP

#include <corelib/ncbi_limits.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/api/psiblast_iteration.hpp>
#include <algo/blast/core/blast_seqsrc.h>
#include <objtools/blast_format/tabular.hpp>
#include <objtools/blast_format/showalign.hpp>
#include <objtools/blast_format/showdefline.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_args.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>

BEGIN_NCBI_SCOPE

// Wrapper class for formatting BLAST results
class CBlastFormat
{
public:
    /// The line length of pairwise blast output
    static const int kFormatLineLength = 68;
    /// The string containing the message that no hits were found
    static const string kNoHitsFound;

    /// Constructor
    /// @param opts BLAST options used in the search [in]
    /// @param db_adapter Adapter object representing a BLAST database or
    /// subject sequences [in]
    /// @param format_type Integer indication the type of output [in]
    /// @param believe_query true if sequence ID's of query sequences
    ///                are to be parsed. If multiple queries are provieded,
    ///                their sequence ID's must be distinct [in]
    /// @param outfile Stream that will receive formatted output
    /// @param num_summary The number of 1-line summaries at the top of
    ///                   the blast report (for output types that have
    ///                   1-line summaries) [in]
    /// @param num_alignments The number of alignments to display in the BLAST
    ///                 report [in] 
    /// @param scope The scope to use for retrieving sequence data
    ///              (must contain query and database sequences) [in]
    /// @param matrix_name Name of protein score matrix (BLOSUM62 if
    ///                    empty, ignored for nucleotide formatting) [in]
    /// @param show_gi When printing database sequence identifiers, 
    ///                include the GI number if set to true; otherwise
    ///                database sequences only have an accession [in]
    /// @param is_html true if the output is to be in HTML format [in]
    /// @param qgencode Genetic code used to translate query sequences
    ///                 (if applicable) [in]
    /// @param dbgencode Genetic code used to translate database sequences
    ///                 (if applicable) [in]
    /// @param use_sum_statistics Were sum statistics used in this search? [in]
    /// @param is_remote_search is this formatting the results of a remote
    /// search [in]
    /// @param dbfilt_algorithms BLAST database filtering algorithm IDs (if
    /// applicable) [in]
    /// @param is_megablast true if megablast [in]
    /// @param is_indexed true if indexed search [in]
    /// @param custom_output_format custom output format specification for
    /// tabular/comma-separated value output format. An empty string implies to
    /// use the default value when applicable. [in]
    CBlastFormat(const blast::CBlastOptions& opts, 
                 blast::CLocalDbAdapter& db_adapter,
                 blast::CFormattingArgs::EOutputFormat format_type, 
                 bool believe_query, CNcbiOstream& outfile,
                 int num_summary, 
                 int num_alignments,
                 CScope & scope,
                 const char *matrix_name = BLAST_DEFAULT_MATRIX,
                 bool show_gi = false, 
                 bool is_html = false, 
                 int qgencode = BLAST_GENETIC_CODE,
                 int dbgencode = BLAST_GENETIC_CODE,
                 bool use_sum_statistics = false,
                 bool is_remote_search = false,
                 const vector<int>& dbfilt_algorithms = vector<int>(),
                 const string& custom_output_format = kEmptyStr,
                 bool is_megablast = false,
                 bool is_indexed = false);

    /// Print the header of the blast report
    void PrintProlog();

    /// Print all alignment information for a single query sequence along with
    /// any errors or warnings (errors are deemed fatal)
    /// @param results Object containing alignments, mask regions, and
    ///                ancillary data to be output [in]
    /// @param queries Query sequences (cached for XML formatting) [in]
    /// @param itr_num iteration number, if applicable, otherwise it should be
    /// numeric_limits<unsigned int>::max() [in]
    /// @param prev_seqids list of previously found Seq-ids, if applicable,
    /// otherwise it should be an empty list [in]
    void PrintOneResultSet(const blast::CSearchResults& results,
                           CConstRef<blast::CBlastQueryVector> queries,
                           unsigned int itr_num =
                           numeric_limits<unsigned int>::max(),
                           blast::CPsiBlastIterationState::TSeqIds prev_seqids =
                           blast::CPsiBlastIterationState::TSeqIds());

    /// Print all alignment information for aa PHI-BLAST run.
    /// any errors or warnings (errors are deemed fatal)
    /// @param result_set Object containing alignments, mask regions, and
    ///                ancillary data to be output [in]
    /// @param queries Query sequences (cached for XML formatting) [in]
    /// @param itr_num iteration number, if applicable, otherwise it should be
    /// numeric_limits<unsigned int>::max() [in]
    /// @param prev_seqids list of previously found Seq-ids, if applicable,
    /// otherwise it should be an empty list [in]
    void PrintPhiResult(const blast::CSearchResultSet& result_set,
                           CConstRef<blast::CBlastQueryVector> queries,
                           unsigned int itr_num =
                           numeric_limits<unsigned int>::max(),
                           blast::CPsiBlastIterationState::TSeqIds prev_seqids =
                           blast::CPsiBlastIterationState::TSeqIds());

    /// Print the footer of the blast report
    /// @param options Options used for performing the blast search [in]
    ///
    void PrintEpilog(const blast::CBlastOptions& options);
    
    /// Resets the scope history for some output formats.
    ///
    /// This method tries to release memory no longer needed after
    /// each query sequence is processed.  For XML formatting, all
    /// data must be kept until the Epilog stage, so for XML this
    /// method has no effect.  Searches with multiple queries will
    /// therefore require more memory to produce XML format output
    /// than when using other output formats.
    void ResetScopeHistory();
    
private:
    /// Format type
    blast::CFormattingArgs::EOutputFormat m_FormatType;
    bool m_IsHTML;              ///< true if HTML output desired
    bool m_DbIsAA;              ///< true if database has protein sequences
    bool m_BelieveQuery;        ///< true if query sequence IDs are parsed
    CNcbiOstream& m_Outfile;    ///< stream to receive output
    int m_NumSummary;        ///< number of 1-line summaries
    int m_NumAlignments;        ///< number of database sequences to present alignments for.
    string m_Program;           ///< blast program
    string m_DbName;            ///< name of blast database
    int m_QueryGenCode;         ///< query genetic code
    int m_DbGenCode;            ///< database genetic code
    bool m_ShowGi;              ///< add GI number of database sequence IDs
    /// If the output format supports 1-line summaries, the search is ungapped
    /// and the alignments have had HSP linking performed, append the number of
    /// hits in the linked set of the best alignment to each database sequence
    bool m_ShowLinkedSetSize;
    bool m_IsUngappedSearch;    ///< true if the search was ungapped
    const char* m_MatrixName;   ///< name of scoring matrix
    /** Scope containing query and subject sequences */
    CRef<CScope> m_Scope;       
    /** True if we are formatting for BLAST2Sequences */
    bool m_IsBl2Seq;            
    /// True if this object is formatting the results of a remote search
    bool m_IsRemoteSearch;
    /// Used to count number of searches formatted. 
    int m_QueriesFormatted;
    
    bool m_Megablast;           ///< true if megablast was used.
    bool m_IndexedMegablast;    ///< true if indexed megablast was used.
    /// Used to retrieve subject sequence information
    CRef<blast::IBlastSeqInfoSrc> m_SeqInfoSrc;

    /// internal representation of database information
    vector<CBlastFormatUtil::SDbInfo> m_DbInfo;

    /// Queries are required for XML format only
    CRef<blast::CBlastQueryVector> m_AccumulatedQueries;
    /// Accumulated results to display in XML format 
    blast::CSearchResultSet m_AccumulatedResults;
    /// The custom output format specification
    string m_CustomOutputFormatSpec;

    /// Output the ancillary data for one query that was searched
    /// @param summary The ancillary data to report [in]
    void x_PrintOneQueryFooter(const blast::CBlastAncillaryData& summary);

    /// Initialize database information
    /// @param dbfilt_algorithms filtering algorithm IDs used for this search
    /// [in]
    void x_FillDbInfo(const vector<int>& dbfilt_algorithms);

    /// Initialize database statistics with data from BLAST servers
    /// @param dbname name of a single BLAST database [in]
    /// @param info structure to fill [in|out]
    /// @return true if successfully filled, false otherwise (and a warning is
    /// printed out)
    bool x_FillDbInfoRemotely(const string& dbname, 
                              CBlastFormatUtil::SDbInfo& info) const;

    /// Initialize database statistics with data obtained from local BLAST
    /// databases
    /// @param dbname name of a single BLAST database [in]
    /// @param info structure to fill [in|out]
    /// @param dbfilt_algorithms filtering algorithm IDs used for this search
    /// [in]
    /// @return true if successfully filled, false otherwise (and a warning is
    /// printed out)
    bool x_FillDbInfoLocally(const string& dbname, 
                             CBlastFormatUtil::SDbInfo& info,
                             const vector<int>& dbfilt_algorithms) const;

    /// Display the BLAST deflines in the traditional BLAST report
    /// @param aln_set alignments to display [in]
    /// @param itr_num iteration number, if applicable, otherwise it should be
    /// numeric_limits<unsigned int>::max() [in]
    /// @param prev_seqids list of previously found Seq-ids, if applicable,
    /// otherwise it should be an empty list [in]
    void x_DisplayDeflines(CConstRef<CSeq_align_set> aln_set,
                   unsigned int itr_num,
                   blast::CPsiBlastIterationState::TSeqIds& prev_seqids);

    /// Split the full alignment into two sets of alignments: one for those
    /// seen in the previous iteration and used to build the PSSM and the other
    /// for newly found sequences. Only applicable to PSI-BLAST.
    /// @param full_alignment results of the PSI-BLAST search [in]
    /// @param repeated_seqs object where the repeated alignments will be
    /// copied to [in|out]
    /// @param new_seqs object where the newly found sequence alignments will
    /// be copied to [in|out]
    /// @param prev_seqids list of previosly found seqids [in]
    void x_SplitSeqAlign(CConstRef<CSeq_align_set> full_alignment,
                 CSeq_align_set& repeated_seqs,
                 CSeq_align_set& new_seqs,
                 blast::CPsiBlastIterationState::TSeqIds& prev_seqids);

    /// Configure the CShowBlastDefline instance passed to it
    /// @param showdef CShowBlastDefline object to configure [in|out]
    void x_ConfigCShowBlastDefline(CShowBlastDefline& showdef);

    /// Prints XML and both species of ASN.1
    /// @param results Results for one query or Phi-blast iteration [in]
    /// @param queries Bioseqs to be formatted (for XML) [in]
    void x_PrintStructuredReport(const blast::CSearchResults& results,
                                 CConstRef<blast::CBlastQueryVector> queries);

   /// Prints Tabular report for one query
   /// @param results Results for one query or Phi-blast iteration [in]
   /// @param itr_num Iteration number for PSI-BLAST [in]
   void x_PrintTabularReport(const blast::CSearchResults& results,
                             unsigned int itr_num);

   /// Creates a bioseq to be able to print the acknowledgement for the
   /// subject bioseq when formatting bl2seq results
   /// @note this method contains a static variable that assumes the order of
   /// the calls to it to retrieve the proper subject sequence
   CConstRef<objects::CBioseq> x_CreateSubjectBioseq();

   /// Wrap the Seq-align-set to be printed in a Seq-annot (as the C toolkit
   /// binaries)
   /// @param alnset Alignment to wrap in a Seq-annot, must be non-NULL [in]
   CRef<objects::CSeq_annot> 
   x_WrapAlignmentInSeqAnnot(CConstRef<objects::CSeq_align_set> alnset) const;

   /// Computes the 'Blast Type' portion of the Seq-annot created in
   /// x_WrapAlignmentInSeqAnnot
   pair<string, int> x_ComputeBlastTypePair() const;
};

END_NCBI_SCOPE

#endif /* !APP___BLAST_FORMAT__HPP */

