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
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  2000/12/08 22:19:45  ostell
 * changed MakeFastString to AsFastaString and to use ostream instead of string
 *
 * Revision 6.2  2000/12/08 20:45:14  ostell
 * added MakeFastaString()
 *
 * Revision 6.1  2000/11/30 18:39:27  ostell
 * added Textseq_id.Match
 *
 *
 * ===========================================================================
 */

// standard includes

// generated includes
#include <objects/seqloc/Textseq_id.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CTextseq_id::~CTextseq_id(void)
{
}

// comparison function
bool CTextseq_id::Match(const CTextseq_id& tsip2) const
{

	// Check Accessions first
	if (IsSetAccession() && (tsip2.IsSetAccession()))
	{
		if (AStrEquiv(GetAccession(), tsip2.GetAccession(), PNocase()))
		{
			if (IsSetVersion() && tsip2.IsSetVersion())
			{
				if ((GetVersion()) == (tsip2.GetVersion()))
					return true;
				else
					return false;
			}
			else
				return true;
		}
		else
			return false;
	}

	// then try name
	if (IsSetName() && tsip2.IsSetName())
	{
                if (AStrEquiv(GetName(), tsip2.GetName(), PNocase()))
                {
                        if (IsSetVersion() && tsip2.IsSetVersion())
                        {
                                if ((GetVersion()) == (tsip2.GetVersion()))
                                        return true;
                                else
                                        return false;
                        }
                        else
                                return true;
                }
                else
                        return false;
        }

	//nothing to compare
	return false;
}

    // format the contents FASTA string style
ostream& CTextseq_id::AsFastaString(ostream& s) const
{

	if (IsSetAccession())
	{
		s << GetAccession();
		if (IsSetVersion())
		{
			int version = GetVersion();
			if (version)
			{
				s << '.' << version;
			}
		}
	}
	s << '|';
	if (IsSetName())
		s << GetName();
	return s;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 61, chars: 1889, CRC32: bd4a7f5d */
