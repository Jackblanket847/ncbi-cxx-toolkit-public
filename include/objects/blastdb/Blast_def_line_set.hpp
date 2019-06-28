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

/// @file Blast_def_line_set.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'blastdb.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Blast_def_line_set_.hpp


#ifndef OBJECTS_BLASTDB_BLAST_DEF_LINE_SET_HPP
#define OBJECTS_BLASTDB_BLAST_DEF_LINE_SET_HPP


// generated includes
#include <objects/blastdb/Blast_def_line_set_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_BLASTDB_EXPORT CBlast_def_line_set : public CBlast_def_line_set_Base
{
    typedef CBlast_def_line_set_Base Tparent;
public:
    // constructor
    CBlast_def_line_set(void);
    // destructor
    ~CBlast_def_line_set(void);

    /// Sort the deflines according to the toolkit established ranking of
    /// Seq-ids
    void SortBySeqIdRank(bool is_protein, bool useBlastRank = false);

    /// Place the CBlast_def_line object which contains the requested gi as the
    /// first in the list (if found)
    /// @param gi gi to be placed first [in]
    void PutTargetGiFirst(TGi gi);

private:
    // Prohibit copy constructor and assignment operator
    CBlast_def_line_set(const CBlast_def_line_set& value);
    CBlast_def_line_set& operator=(const CBlast_def_line_set& value);

};

/////////////////// CBlast_def_line_set inline methods

// constructor
inline
CBlast_def_line_set::CBlast_def_line_set(void)
{
}


/////////////////// end of CBlast_def_line_set inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_BLASTDB_BLAST_DEF_LINE_SET_HPP
/* Original file checksum: lines: 94, chars: 2754, CRC32: 8066bf8c */
