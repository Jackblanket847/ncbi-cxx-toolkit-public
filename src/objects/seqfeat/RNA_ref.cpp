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
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <util/sequtil/sequtil_convert.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CRNA_ref::~CRNA_ref(void)
{
}


static const SStaticPair<CRNA_ref::EType, const char*> sc_rna_type_map[] = {
    { CRNA_ref::eType_premsg, "precursor_RNA" },
    { CRNA_ref::eType_mRNA, "mRNA" },
    { CRNA_ref::eType_tRNA, "tRNA" },
    { CRNA_ref::eType_rRNA, "rRNA" },
    { CRNA_ref::eType_snRNA, "snRNA" },
    { CRNA_ref::eType_scRNA, "scRNA" },
    { CRNA_ref::eType_snoRNA, "snoRNA" },
    { CRNA_ref::eType_ncRNA, "ncRNA" },
    { CRNA_ref::eType_tmRNA, "tmRNA" },
    { CRNA_ref::eType_miscRNA, "misc_RNA" },
    { CRNA_ref::eType_other, "misc_RNA" }
};

typedef CStaticPairArrayMap<CRNA_ref::EType, const char*> TRnaTypeMap;
DEFINE_STATIC_ARRAY_MAP(TRnaTypeMap, sc_RnaTypeMap, sc_rna_type_map);

string CRNA_ref::GetRnaTypeName (const CRNA_ref::EType rna_type)
{
    const char* rna_type_name = "";
    TRnaTypeMap::const_iterator rna_type_it = sc_RnaTypeMap.find(rna_type);
    if ( rna_type_it != sc_RnaTypeMap.end() ) {
        rna_type_name = rna_type_it->second;
    }
    return rna_type_name;
}

static const string s_TrnaList[] = {
  "tRNA-Gap",
  "tRNA-Ala",
  "tRNA-Asx",
  "tRNA-Cys",
  "tRNA-Asp",
  "tRNA-Glu",
  "tRNA-Phe",
  "tRNA-Gly",
  "tRNA-His",
  "tRNA-Ile",
  "tRNA-Xle",
  "tRNA-Lys",
  "tRNA-Leu",
  "tRNA-Met",
  "tRNA-Asn",
  "tRNA-Pyl",
  "tRNA-Pro",
  "tRNA-Gln",
  "tRNA-Arg",
  "tRNA-Ser",
  "tRNA-Thr",
  "tRNA-Sec",
  "tRNA-Val",
  "tRNA-Trp",
  "tRNA-OTHER",
  "tRNA-Tyr",
  "tRNA-Glx",
  "tRNA-TERM"
};

static const string& s_AaName(int aa)
{
    int idx = 255;

    if (aa != '*') {
        idx = aa - 64;
    } else {
        idx = 28;   
    }
    if ( idx > 0 && idx < 28 ) {
        return s_TrnaList [idx];
    }
    return kEmptyStr;
}

static int s_ToIupacaa(int aa)
{
    vector<char> n(1, static_cast<char>(aa));
    vector<char> i;
    CSeqConvert::Convert(n, CSeqUtil::e_Ncbieaa, 0, 1, i, CSeqUtil::e_Iupacaa);
    return i.front();
}

static const string& s_GetTrnaProduct(const CTrna_ext& trna)
{
    int aa = 0;
    if ( trna.IsSetAa()  &&  trna.GetAa().IsNcbieaa() ) {
        aa = trna.GetAa().GetNcbieaa();
    }
    aa = s_ToIupacaa(aa);

    return s_AaName(aa);
}

const string& CRNA_ref::GetRnaProductName(void) const
{
    if (!IsSetExt()) {
        return kEmptyStr;
    }
    
    if (GetExt().IsName()) {
        return GetExt().GetName();
    } else if (GetExt().IsGen() && GetExt().GetGen().IsSetProduct()) {           
        return GetExt().GetGen().GetProduct();
    } else if (GetExt().IsTRNA()) {
        return s_GetTrnaProduct(GetExt().GetTRNA());
    }
    
    return kEmptyStr;
}

static void s_SetTrnaProduct(CTrna_ext& trna, const string& product, string& remainder)
{
    remainder = kEmptyStr;
    if (NStr::IsBlank(product)) {
        trna.ResetAa();
        return;
    }

    int num_names = sizeof(s_TrnaList) / sizeof (string);
    string test = product;
    if (!NStr::StartsWith(product, "tRNA-")) {
        test = "tRNA-" + test;
    }

    if (NStr::StartsWith(test, "tRNA-TERM", NStr::eNocase)
        || NStr::StartsWith(test, "tRNA-STOP", NStr::eNocase)) {
        trna.SetAa().SetNcbieaa(42);
        if (test.length() > 9) {
            remainder = test.substr(9);
            NStr::TruncateSpacesInPlace(remainder);
        }
    } else {
        remainder = product;
        for (int i = 0; i < num_names - 1; i++) {
            if (NStr::StartsWith(test, s_TrnaList[i], NStr::eNocase)) {
                trna.SetAa().SetNcbieaa(i + 64);
                remainder = test.substr(s_TrnaList[i].length());
                break;
            }
        }
    }
}

void CRNA_ref::SetRnaProductName(const string& product, string& remainder)
{
    remainder = kEmptyStr;
    switch (GetType()) {
    case CRNA_ref::eType_rRNA:
    case CRNA_ref::eType_mRNA:
        if (NStr::IsBlank(product)) {
            ResetExt();
        } else {
            SetExt().SetName(product);
        }
        break;
    case CRNA_ref::eType_tRNA:
        s_SetTrnaProduct(SetExt().SetTRNA(), product, remainder);
        break;
    default:
        if (NStr::IsBlank(product)) {
            ResetExt();
        } else {
            SetExt().SetGen().SetProduct(product);   
        }
        break;
    }
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 65, chars: 1885, CRC32: 6e6c3f8a */
