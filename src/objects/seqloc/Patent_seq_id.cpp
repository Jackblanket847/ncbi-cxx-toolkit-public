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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqloc.asn'.
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/biblio/Id_pat.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


// destructor
CPatent_seq_id::~CPatent_seq_id(void)
{
    return;
}


// comparison function
bool CPatent_seq_id::Match(const CPatent_seq_id& psip2) const
{
    return GetSeqid() == psip2.GetSeqid()  &&  GetCit().Match(psip2.GetCit());
}


int CPatent_seq_id::Compare(const CPatent_seq_id& psip2) const
{
    int ret = GetSeqid() - psip2.GetSeqid();
    if ( ret == 0 ) {
        ret = GetCit().Match(psip2.GetCit())? 0: this < &psip2? -1: 1;
    }
    return ret;
}


// format a FASTA style string
ostream& CPatent_seq_id::AsFastaString(ostream& s) const
{
    const CId_pat& idp = GetCit();
	
    s << idp.GetCountry() << '|';  // no Upcase per Ostell - Karl 7/2001

    const CId_pat_Base::C_Id& id = idp.GetId();
    if ( id.IsNumber() ) {
        s << id.GetNumber();
    } else {
        s << id.GetApp_number();
        if ( idp.IsSetDoc_type() ) {
            s << idp.GetDoc_type();
        }
    }
    s << '|' << GetSeqid();
    return s;
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
