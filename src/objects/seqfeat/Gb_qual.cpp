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
 *   using the following specifications:
 *   'seqfeat.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/seqfeat/Gb_qual.hpp>

// other includes
#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

// generated classes


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CGb_qual::~CGb_qual(void)
{
}

static const char * const valid_inf_prefixes [] = {
    "ab initio prediction",
    "nucleotide motif",
    "profile",
    "protein motif",
    "similar to AA sequence",
    "similar to DNA sequence",
    "similar to RNA sequence",
    "similar to RNA sequence, EST",
    "similar to RNA sequence, mRNA",
    "similar to RNA sequence, other RNA",
    "similar to sequence",
    "alignment"
};


// constructor
CInferencePrefixList::CInferencePrefixList(void)
{
}

//destructor
CInferencePrefixList::~CInferencePrefixList(void)
{
}


void CInferencePrefixList::GetPrefixAndRemainder (const string& inference, string& prefix, string& remainder)
{
    prefix = "";
    remainder = "";

    for (unsigned int i = 0; i < sizeof (valid_inf_prefixes) / sizeof (char *); i++) {
        if (NStr::StartsWith (inference, valid_inf_prefixes[i])) {
            prefix = valid_inf_prefixes[i];
        }
    }

    remainder = inference.substr (prefix.length());
    NStr::TruncateSpacesInPlace (remainder);
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 65, chars: 1885, CRC32: 3224da35 */
