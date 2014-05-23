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

/// @file Field_constraint.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'macro.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Field_constraint_.hpp


#ifndef OBJECTS_MACRO_FIELD_CONSTRAINT_HPP
#define OBJECTS_MACRO_FIELD_CONSTRAINT_HPP


// generated includes
#include <objects/macro/Field_constraint_.hpp>
#include <objects/macro/Field_type.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/scope.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class CField_constraint : public CField_constraint_Base
{
    typedef CField_constraint_Base Tparent;
public:
    // constructor
    CField_constraint(void);
    // destructor
    ~CField_constraint(void);

    bool Match(const CSeq_feat& feat, CConstRef <CScope> scope) const;

private:
    // Prohibit copy constructor and assignment operator
    CField_constraint(const CField_constraint& value);
    CField_constraint& operator=(const CField_constraint& value);

};

/////////////////// CField_constraint inline methods

// constructor
inline
CField_constraint::CField_constraint(void)
{
}


/////////////////// end of CField_constraint inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_MACRO_FIELD_CONSTRAINT_HPP
/* Original file checksum: lines: 86, chars: 2519, CRC32: 8371bbc9 */
