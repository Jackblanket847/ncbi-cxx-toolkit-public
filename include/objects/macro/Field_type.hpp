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

/// @file Field_type.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'macro.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Field_type_.hpp


#ifndef OBJECTS_MACRO_FIELD_TYPE_HPP
#define OBJECTS_MACRO_FIELD_TYPE_HPP


// generated includes
#include <objects/macro/Field_type_.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class CField_type : public CField_type_Base
{
    typedef CField_type_Base Tparent;
public:
    // constructor
    CField_type(void);
    // destructor
    ~CField_type(void);

    static CRef <CFeature_field>
        FeatureFieldFromCDSGeneProtField (ECDSGeneProt_field cgp_field);

    static string GetDBLinkFieldFromUserObject(const CUser_object& user_obj, 
                                    EDBLink_field_type dblink_tp, 
                                    const CString_constraint& str_cons);
private:
    // Prohibit copy constructor and assignment operator
    CField_type(const CField_type& value);
    CField_type& operator=(const CField_type& value);
};

/////////////////// CField_type inline methods

// constructor
inline
CField_type::CField_type(void)
{
}


/////////////////// end of CField_type inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_MACRO_FIELD_TYPE_HPP
/* Original file checksum: lines: 86, chars: 2405, CRC32: dbbe176e */
