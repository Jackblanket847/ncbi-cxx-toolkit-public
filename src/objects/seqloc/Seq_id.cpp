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

// standard includes

// generated includes
#include <objects/seq/Bioseq.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>

// generated classes

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


// constructor
CSeq_id::CSeq_id(void)
{
    return;
}

// destructor
CSeq_id::~CSeq_id(void)
{
    return;
}


const CTextseq_id* CSeq_id::GetTextseq_Id(void) const
{
    switch ( Which() ) {
    case e_Genbank:
        return &GetGenbank();
    case e_Embl:
        return &GetEmbl();
    case e_Ddbj:
        return &GetDdbj();
    case e_Pir:
        return &GetPir();
    case e_Swissprot:
        return &GetSwissprot();
    case e_Other:
        return &GetOther();
    case e_Prf:
        return &GetPrf();
    case e_Tpg:
        return &GetTpg();
    case e_Tpe:
        return &GetTpe();
    case e_Tpd:
        return &GetTpd();
    default:
        return 0;
    }
}


// Compare() - are SeqIds equivalent?
CSeq_id::E_SIC CSeq_id::Compare(const CSeq_id& sid2) const
{
    if ( Which() != sid2.Which() ) { // Only one case where this will work
        const CTextseq_id *tsip1 = GetTextseq_Id();
        if ( !tsip1 )
            return e_DIFF;

        const CTextseq_id *tsip2 = sid2.GetTextseq_Id();
        if ( !tsip2 )
            return e_DIFF;

        if ( tsip1->Match(*tsip2) ) // id Textseq_id match
            return e_YES;
        else
            return e_NO;
    }

    switch ( Which() ) { // Now we only need to know one
    case e_Local:
        return GetLocal().Match(sid2.GetLocal()) ? e_YES : e_NO;
    case e_Gibbsq:
        return GetGibbsq() == sid2.GetGibbsq() ? e_YES : e_NO;
    case e_Gibbmt:
        return GetGibbmt() == sid2.GetGibbmt() ? e_YES : e_NO;
    case e_Giim:
        return GetGiim().GetId() == sid2.GetGiim().GetId() ? e_YES : e_NO;
    case e_Pir:
        return GetPir().Match(sid2.GetPir()) ? e_YES : e_NO;
    case e_Swissprot:
        return GetSwissprot().Match(sid2.GetSwissprot()) ? e_YES : e_NO;
    case e_Patent:
        return GetPatent().Match(sid2.GetPatent()) ? e_YES : e_NO;
    case e_Other:
        return GetOther().Match(sid2.GetOther()) ? e_YES : e_NO;
    case e_General:
        return GetGeneral().Match(sid2.GetGeneral()) ? e_YES : e_NO;
    case e_Gi:
        return GetGi() == sid2.GetGi() ? e_YES : e_NO;
    case e_Prf:
        return GetPrf().Match(sid2.GetPrf()) ? e_YES : e_NO;
    case e_Pdb:
        return GetPdb().Match(sid2.GetPdb()) ? e_YES : e_NO;
    case e_Genbank:
        return GetGenbank().Match(sid2.GetGenbank()) ? e_YES : e_NO;
    case e_Embl:
        return GetEmbl().Match(sid2.GetEmbl()) ? e_YES : e_NO;
    case e_Ddbj:
        return GetDdbj().Match(sid2.GetDdbj()) ? e_YES : e_NO;
    case e_Tpg:
        return GetTpg().Match(sid2.GetTpg()) ? e_YES : e_NO;
    case e_Tpe:
        return GetTpe().Match(sid2.GetTpe()) ? e_YES : e_NO;
    case e_Tpd:
        return GetTpd().Match(sid2.GetTpd()) ? e_YES : e_NO;
    default:
        return e_error;
    }
}


static const char* const s_TextId[20] = {   // FASTA_LONG formats
    "???" , // not-set = ??? 
    "lcl",  // local = lcl|integer or string 
    "bbs",  // gibbsq = bbs|integer 
    "bbm",  // gibbmt = bbm|integer 
    "gim",  // giim = gim|integer 
    "gb", // genbank = gb|accession|locus
    "emb",  // embl = emb|accession|locus 
    "pir",  // pir = pir|accession|name
    "sp", // swissprot = sp|accession|name 
    "pat",  // patent = pat|country|patent number (string)|seq number (integer)
    "ref",  // other = ref|accession|name|release - changed from oth to ref
    "gnl",  // general = gnl|database(string)|id (string or number)
    "gi", // gi = gi|integer
    "dbj",  // ddbj = dbj|accession|locus
    "prf",  // prf = prf|accession|name
    "pdb",  // pdb = pdb|entry name (string)|chain id (char)
    "tpg",  // tpg = tpg|accession|name
    "tpe",  // tpe = tpe|accession|name
    "tpd",  // tpd = tpd|accession|name
    ""  // Placeholder for end of list
};

CSeq_id::E_Choice CSeq_id::WhichInverseSeqId(const char* SeqIdCode)
{
    int retval = 0;
    int dex;

    // Last item in list has null byte for first character, so
    // *s_TextId[dex] will be zero at end.
    for (dex = 0;  *s_TextId[dex];  dex++) {
        if ( PNocase().Equals(s_TextId[dex],SeqIdCode) ) {
            break;
        }
    }
    if ( !*s_TextId[dex] ) {
        retval = 0;
    } else {
        retval = dex;
    }

    return static_cast<CSeq_id_Base::E_Choice> (retval);
}


static CSeq_id::EAccessionInfo s_IdentifyNAcc(const string& acc)
{
    _ASSERT(acc[0] == 'N');
    int n = NStr::StringToInt(acc.substr(1), 10, NStr::eCheck_Skip);
    if (n >= 20000) {
        return CSeq_id::eAcc_gb_est;
    } else { // big mess; fortunately, these are all secondary
        switch (n) {
        case 1: case 2: case 11: case 57:
            return CSeq_id::eAcc_gb_embl;

        case 3: case 4: case 6: case 7: case 10: case 14: case 15:
        case 16: case 17: case 21: case 23: case 24: case 26: case 29:
        case 30: case 31: case 32: case 33: case 34: case 36: case 38:
        case 39: case 40: case 42: case 43: case 44: case 45: case 47:
        case 49: case 50: case 51: case 55: case 56: case 59:
            return CSeq_id::eAcc_gb_ddbj;

        case 5: case 9: case 12: case 20: case 22: case 25: case 58:
            return CSeq_id::eAcc_gb_embl_ddbj;

        case 8: case 13: case 18: case 19: case 27: case 41: case 46:
        case 48: case 52: case 54: case 18624:
            return CSeq_id::eAcc_gb_other_nuc;

        case 28: case 35: case 37: case 53: case 61: case 62: case 63:
        case 65: case 66: case 67: case 68: case 69: case 78: case 79:
        case 83: case 88: case 90: case 91: case 92: case 93: case 94:
            return CSeq_id::eAcc_ddbj_other_nuc;

        case 60: case 64:
            return CSeq_id::eAcc_embl_other_nuc;

        case 70:
            return CSeq_id::eAcc_embl_ddbj;

        default: // unassigned or ambiguous
            return CSeq_id::eAcc_unknown;
        }
    }
}


CSeq_id::EAccessionInfo CSeq_id::IdentifyAccession(const string& acc)
{
    SIZE_TYPE digit_pos = acc.find_first_of("0123456789");
    SIZE_TYPE main_size = acc.find('.');
    if (main_size == NPOS) {
        main_size = acc.size();
    }
    string pfx = acc.substr(0, digit_pos);
    NStr::ToUpper(pfx);
    switch (pfx.size()) {
    case 0:
        if (main_size == 4  ||  (main_size > 4  &&  acc[4] == '|')) {
            return eAcc_pdb;
        } else {
            return eAcc_unknown;
        }

    case 1:
        switch (pfx[0]) {
        case 'A':                                return eAcc_embl_patent;
        case 'B':                                return eAcc_gb_gss;
        case 'C':                                return eAcc_ddbj_est;
        case 'D':                                return eAcc_ddbj_dirsub;
        case 'E':                                return eAcc_ddbj_patent;
        case 'F':                                return eAcc_embl_est;
        case 'G':                                return eAcc_gb_sts;
        case 'H': case 'R': case 'T': case 'W':  return eAcc_gb_est;
        case 'I':                                return eAcc_gb_patent;
        case 'J': case 'K': case 'L': case 'M':  return eAcc_gsdb_dirsub;
        case 'N':                                return s_IdentifyNAcc(acc);
        case 'O': case 'P': case 'Q':            return eAcc_swissprot;
        case 'S':                                return eAcc_gb_backbone;
        case 'U':                                return eAcc_gb_dirsub;
        case 'V': case 'X': case 'Y': case 'Z':  return eAcc_embl_dirsub;
        default:                                 return eAcc_unreserved_nuc;
        }

    case 2:
        switch (pfx[0]) {
        case 'A':
            switch (pfx[1]) {
            case 'A': case 'I': case 'W': return eAcc_gb_est;
            case 'B':                     return eAcc_ddbj_dirsub;
            case 'C':                     return eAcc_gb_htgs;
            case 'D':                     return eAcc_gb_gsdb;
            case 'E':                     return eAcc_gb_genome;
            case 'F': case 'Y':           return eAcc_gb_dirsub;
            case 'G': case 'P':           return eAcc_ddbj_genome;
            case 'H':                     return eAcc_gb_segset;
            case 'J': case 'M':           return eAcc_embl_dirsub;
            case 'N':                     return eAcc_embl_con;
            case 'Q': case 'Z':           return eAcc_gb_gss;
            case 'R':                     return eAcc_gb_patent;
            case 'S':                     return eAcc_gb_other_nuc;
            case 'T': case 'U': case 'V': return eAcc_ddbj_est;
            case 'X':                     return eAcc_embl_patent;
            default:                      return eAcc_unreserved_nuc;
            }

        case 'B':
            switch (pfx[1]) {
            case 'A':                     return eAcc_ddbj_con;
            case 'B': case 'J': case 'P': return eAcc_ddbj_est;
            case 'C': case 'T':           return eAcc_gb_cdna;
            case 'D':                     return eAcc_ddbj_patent;
            case 'E': case 'F': case 'G':
            case 'I': case 'M': case 'Q': return eAcc_gb_est;
            case 'H':                     return eAcc_gb_gss;
            case 'K': case 'L':           return eAcc_gb_tpa_nuc;
            case 'N':                     return eAcc_embl_tpa_nuc;
            case 'R':                     return eAcc_ddbj_tpa_nuc;
            case 'S':                     return eAcc_ddbj_genome; // chimp
            default:                      return eAcc_unreserved_nuc;
            }

        default:                          return eAcc_unreserved_nuc;
        }

    case 3:
        if (pfx[2] == '_') { // refseq-style
            if      (pfx == "NC_") { return eAcc_refseq_chromosome;      }
            else if (pfx == "NG_") { return eAcc_refseq_genomic;         }
            else if (pfx == "NM_") { return eAcc_refseq_mrna;            }
            else if (pfx == "NP_") { return eAcc_refseq_prot;            }
            else if (pfx == "NT_") { return eAcc_refseq_contig;          }
            else if (pfx == "XM_") { return eAcc_refseq_mrna_predicted;  }
            else if (pfx == "XP_") { return eAcc_refseq_prot_predicted;  }
            else if (pfx == "XR_") { return eAcc_refseq_ncrna_predicted; }
            else                   { return eAcc_refseq_unreserved;      }
        } else { // protein
            switch (pfx[0]) {
            case 'A': return (pfx == "AAE") ? eAcc_gb_patent_prot
                          : eAcc_gb_prot;
            case 'B': return eAcc_ddbj_prot;
            case 'C': return eAcc_embl_prot;
            case 'D': return eAcc_gb_tpa_prot;
            case 'E': return eAcc_gb_wgs_prot;
            case 'F': return eAcc_ddbj_tpa_prot;
            case 'G': return eAcc_ddbj_wgs_prot;
            default:  return eAcc_unreserved_prot;
            }
        }

    case 4:
        switch (pfx[0]) {
        case 'A': return eAcc_gb_wgs_nuc;
        case 'B': return eAcc_ddbj_wgs_nuc;
        case 'C': return eAcc_embl_wgs_nuc;
        default:  return eAcc_unknown;
        }

    default:
        return eAcc_unknown;
    }
}


void CSeq_id::WriteAsFasta(ostream& out)
    const
{
    E_Choice the_type = Which();
    if (the_type > e_Tpd)  // New SeqId type
        the_type = e_not_set;

    out << s_TextId[the_type] << '|';

    switch (the_type) {
    case e_not_set:
        break;
    case e_Local:
        GetLocal().AsString(out);
        break;
    case e_Gibbsq:
        out << GetGibbsq();
        break;
    case e_Gibbmt:
        out << GetGibbmt();
        break;
    case e_Giim:
        out << (GetGiim().GetId());
        break;
    case e_Genbank:
        GetGenbank().AsFastaString(out);
        break;
    case e_Embl:
        GetEmbl().AsFastaString(out);
        break;
    case e_Pir:
        GetPir().AsFastaString(out);
        break;
    case e_Swissprot:
        GetSwissprot().AsFastaString(out);
        break;
    case e_Patent:
        GetPatent().AsFastaString(out);
        break;
    case e_Other:
        GetOther().AsFastaString(out);
        break;
    case e_General:
        {
            const CDbtag& dbt = GetGeneral();
            out << (dbt.GetDb()) << '|';  // no Upcase per Ostell - Karl 7/2001
            dbt.GetTag().AsString(out);
        }
        break;
    case e_Gi:
        out << GetGi();
        break;
    case e_Ddbj:
        GetDdbj().AsFastaString(out);
        break;
    case e_Prf:
        GetPrf().AsFastaString(out);
        break;
    case e_Pdb:
        GetPdb().AsFastaString(out);
        break;
    case e_Tpg:
        GetTpg().AsFastaString(out);
        break;
    case e_Tpe:
        GetTpe().AsFastaString(out);
        break;
    case e_Tpd:
        GetTpd().AsFastaString(out);
        break;
    default:
        out << "[UnknownSeqIdType]";
        break;
    }
}


//SeqIdFastAConstructors
CSeq_id::CSeq_id( const string& the_id )
{
    // Create an istrstream on string the_id
    std::istrstream  myin(the_id.c_str() );

    string the_type_in, acc_in, name_in, version_in, release_in;

    // Read the part of the_id up to the vertical bar ( "|" )
    // If no vertical bar, tries to interpret the string as a pure
    // accession, inferring the type from the initial letter(s).
    if ( !getline(myin, the_type_in, '|') ) {
        SIZE_TYPE dot = the_id.find('.');
        acc_in = the_id.substr(0, dot);
        EAccessionInfo info = IdentifyAccession(acc_in);
        int ver = 0;
        if (dot != NPOS) {
            ver = NStr::StringToNumeric(the_id.substr(dot + 1));
        }
        if (GetAccType(info) != e_not_set) {
            x_Init(GetAccType(info), acc_in, kEmptyStr, ver);
        }
        return;
    }

    // Remove spaces from front and back of the_type_in
    string the_type_use = NStr::TruncateSpaces(the_type_in, NStr::eTrunc_Both);
   
    // Determine the type from the string
    CSeq_id_Base::E_Choice the_type = WhichInverseSeqId(the_type_use.c_str());
   
    // Construct according to type
    if ( the_type == CSeq_id::e_Local ) {
        getline( myin, acc_in ); // take rest
        x_Init( the_type, acc_in );
        return;
    }

    if ( !getline( myin, acc_in,'|' ) )
        return;

    if ( the_type == CSeq_id::e_General  ||  the_type == CSeq_id::e_Pdb ) {
        //Take the rest of the line
        getline( myin, name_in ); 
        x_Init( the_type, acc_in, name_in );
        return;
    }

    if ( getline(myin, name_in, '|') ) {
        if ( getline(myin, version_in, '|') ) {
            getline(myin, release_in, '|');
        }
    }
  
    string version = NStr::TruncateSpaces( version_in, NStr::eTrunc_Both );
    int ver = 0;

    if ( ! version.empty() ) {
        if ( (ver = NStr::StringToNumeric(version) ) < 0) {
            THROW1_TRACE(invalid_argument, 
                         "Unexpected non-numeric version: " +
                         version +
                         "\nthe_id: " + the_id);
        }
    }
    x_Init(the_type, acc_in, name_in, ver, release_in);
}


// acc_in is just first string, as in text seqid, for
// wierd cases (patents, pdb) not really an acc
CSeq_id::CSeq_id
(CSeq_id_Base::E_Choice the_type,
 const string&          acc_in,
 const string&          name_in,
 const string&          version_in,
 const string&          release_in )
{
    string version = NStr::TruncateSpaces(version_in, NStr::eTrunc_Both);
    int ver = 0;

    if ( !version.empty() ) {
        if ( (ver = NStr::StringToNumeric(version)) < 0 ) {
            THROW1_TRACE(invalid_argument, 
                         "Unexpected non-numeric version. "
                         "\nthe_type = " + string(s_TextId[the_type]) + 
                         "\nacc_in = " + acc_in +
                         "\nname_in = " + name_in +
                         "\version_in = " + version_in +
                         "\nrelease_in = " + release_in);  
        }
    }

    x_Init(the_type, acc_in, name_in, ver, release_in);
}


static void s_InitThrow
(const string& message,
 const string& type,
 const string& acc,
 const string& name,
 const string& version,
 const string& release)
{
    THROW1_TRACE(invalid_argument,
                 "CSeq_id:: " + message +
                 "\ntype      = " + type + 
                 "\naccession = " + acc +
                 "\nname      = " + name +
                 "\nversion   = " + version +
                 "\nrelease   = " + release);
}


CSeq_id::CSeq_id
(const string& the_type_in,
 const string& acc_in,
 const string& name_in,
 const string& version_in,
 const string& release_in)
{
    string the_type_use = NStr::TruncateSpaces(the_type_in, NStr::eTrunc_Both);
    string version      = NStr::TruncateSpaces(version_in,  NStr::eTrunc_Both);
    int ver = 0;

    CSeq_id_Base::E_Choice the_type = WhichInverseSeqId(the_type_use.c_str());

    if ( !version.empty() ) {
        if ( (ver = NStr::StringToNumeric(version)) < 0) {
            s_InitThrow("Unexpected non-numeric version.",
                        the_type_in, acc_in, name_in, version_in, release_in);
        }
    }

    x_Init(the_type, acc_in, name_in, ver, release_in);
}


CSeq_id::CSeq_id
(const string& the_type_in,
 const string& acc_in,
 const string& name_in,
 int           version,
 const string& release_in )
{
    string the_type_use = NStr::TruncateSpaces(the_type_in, NStr::eTrunc_Both);
    CSeq_id_Base::E_Choice the_type = WhichInverseSeqId (the_type_use.c_str());

    x_Init(the_type, acc_in, name_in, version, release_in);
}


CSeq_id::CSeq_id
( CSeq_id_Base::E_Choice the_type,
  const string&          acc_in,
  const string&          name_in,
  int                    version,
  const string&          release_in)
{
    x_Init(the_type, acc_in, name_in, version, release_in);
}


// Karl Sirotkin 7/2001

void
CSeq_id::x_Init
( CSeq_id_Base::E_Choice the_type,
  const string&          acc_in,
  const string&          name_in,
  int                    version ,
  const string&          release_in)
{
    int the_id;
    string acc     = NStr::TruncateSpaces(acc_in,     NStr::eTrunc_Both);
    string name    = NStr::TruncateSpaces(name_in,    NStr::eTrunc_Both);
    string release = NStr::TruncateSpaces(release_in, NStr::eTrunc_Both);

    switch (the_type) {
    case CSeq_id::e_not_set: // Will cause unspecified SeqId to be returned.
        break;
    case CSeq_id::e_Local:
        {
            CSeq_id::TLocal & loc = SetLocal();
            string::const_iterator it = acc.begin();
  
            if ( (the_id = NStr::StringToNumeric(acc)) >= 0 && *it != '0' ) {
                loc.SetId(the_id);
            } else { // to cover case where embedded vertical bar in 
                // string, could add code here, to concat a
                // '|' and name string, if not null/empty
                loc.SetStr(acc);
            }
            break;
        }
    case CSeq_id::e_Gibbsq:
        if ( (the_id = NStr::StringToNumeric (acc)) >= 0 ) {
            SetGibbsq(the_id);
        } else {
             s_InitThrow("Unexpected non-numeric accession.",
                         string(s_TextId[the_type]), acc_in, name_in,
                         NStr::IntToString(version), release_in);
        }
        break;
    case CSeq_id::e_Gibbmt:
        if ( (the_id =NStr::StringToNumeric (acc)) >= 0 ) {
            SetGibbmt(the_id);
        } else {
             s_InitThrow("Unexpected non-numeric accession.",
                         string(s_TextId[the_type]), acc_in, name_in,
                         NStr::IntToString(version), release_in);
        }
        break;
    case CSeq_id::e_Giim:
        { 
            CGiimport_id &  giim = SetGiim();
            if ( (the_id =NStr::StringToNumeric (acc)) >= 0 ) {
                giim.SetId(the_id);
            } else {
                s_InitThrow("Unexpected non-numeric accession.",
                            string(s_TextId[the_type]), acc_in, name_in,
                            NStr::IntToString(version), release_in);
            }
            break;
        }
    case CSeq_id::e_Genbank:
        {
            CTextseq_id* text
                = new CTextseq_id(acc, name, version, release);
            SetGenbank(*text );
            break;
        }
    case CSeq_id::e_Embl:
        {
            CTextseq_id* text
               = new CTextseq_id(acc, name, version, release);
            SetEmbl(*text);
            break;
        }
    case CSeq_id::e_Pir:
        {
            CTextseq_id* text
                = new CTextseq_id(acc, name, version, release, false);
            SetPir(*text);
            break;
        }
    case CSeq_id::e_Swissprot:
        {
            CTextseq_id* text
                = new CTextseq_id(acc, name, version, release, false);
            SetSwissprot(*text);
            break;
        }
    case CSeq_id::e_Tpg:
        {
            CTextseq_id* text
                = new CTextseq_id(acc, name, version, release);
            SetTpg(*text);
            break;
        }
    case CSeq_id::e_Tpe:
        {
            CTextseq_id* text
                = new CTextseq_id(acc, name, version, release);
            SetTpe(*text);
            break;
        }
    case CSeq_id::e_Tpd:
        {
            CTextseq_id* text
                = new CTextseq_id(acc, name, version, release);
            SetTpd(*text);
            break;
        }
    case CSeq_id::e_Patent:
        {
            CPatent_seq_id&  pat    = SetPatent();
            CId_pat&         id_pat = pat.SetCit();
            CId_pat::C_Id&   id_pat_id = id_pat.SetId();
            id_pat.SetCountry(acc);

            const char      app_str[] = "App=";
            const SIZE_TYPE app_str_len = sizeof(app_str) - 1;
            if (name.substr(0, app_str_len) == app_str) {
                id_pat_id.SetApp_number(name.substr(app_str_len));
            } else {
                id_pat_id.SetNumber(name);
            }
            pat.SetSeqid(version);
            break;
        }
    case CSeq_id::e_Other: // RefSeq, allow dot version
        {
            CTextseq_id* text
                = new CTextseq_id(acc,name,version,release);
            SetOther(*text);
            break;
        }
    case CSeq_id::e_General:
        {
            CDbtag& dbt = SetGeneral();
            dbt.SetDb(acc);
            CObject_id& oid = dbt.SetTag();
            the_id = NStr::StringToNumeric(acc);
            if (the_id >= 0  &&  name[0] != '0') {
                oid.SetId(the_id);
            }else{
                oid.SetStr(name);
            }
            break;
        }
    case CSeq_id::e_Gi:
        the_id = NStr::StringToNumeric(acc);
        if (the_id >= 0 ) {
            SetGi(the_id);
        } else {
             s_InitThrow("Unexpected non-numeric accession.",
                         string(s_TextId[the_type]), acc_in, name_in,
                         NStr::IntToString(version), release_in);
        }
        break;
    case CSeq_id::e_Ddbj:
        {
            CTextseq_id* text
                = new CTextseq_id(acc,name,version,release);
            SetDdbj(*text);
            break;
        }
    case CSeq_id::e_Prf:
        {
            CTextseq_id* text
                = new CTextseq_id(acc,name,version,release,false);
            SetPrf(*text);
            break;
        }
    case CSeq_id::e_Pdb:
        {
            CPDB_seq_id& pdb     = SetPdb();
            CPDB_mol_id& pdb_mol = pdb.SetMol();
            pdb_mol.Set(acc);

            if (name.size() == 1) {
                pdb.SetChain(static_cast<unsigned char> (name.c_str()[0]));
            } else if ( name.compare("VB") == 0) {
                pdb.SetChain('|');
            } else if (name.size() == 2  && 
                       name.c_str()[0] ==  name.c_str()[1]) {
                pdb.SetChain( Locase(static_cast<unsigned char>
                                     (name.c_str()[0])) );
            } else {
                s_InitThrow("Unexpected PDB chain id.",
                            string(s_TextId[the_type]), acc_in, name_in,
                            NStr::IntToString(version), release_in);
            }
            break;
        }
    default:
        THROW1_TRACE(invalid_argument, "Specified Seq-id type not supported");
    }
}

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 6.27  2002/08/14 15:52:27  ucko
 * Add BT and XR_.
 *
 * Revision 6.26  2002/08/06 18:22:19  ucko
 * Properly handle versioned PDB accessions.
 *
 * Revision 6.25  2002/08/01 20:33:10  ucko
 * s_IdentifyAccession -> IdentifyAccession; s_ is only for module-static names.
 *
 * Revision 6.24  2002/07/30 19:42:44  ucko
 * Add s_IdentifyAccession, and use it in the string-based constructor if
 * the input isn't FASTA-format.
 * Move CVS log to end.
 *
 * Revision 6.23  2002/06/06 20:31:33  clausen
 * Moved methods using object manager to objects/util
 *
 * Revision 6.22  2002/05/22 14:03:40  grichenk
 * CSerialUserOp -- added prefix UserOp_ to Assign() and Equals()
 *
 * Revision 6.21  2002/05/06 03:39:12  vakatov
 * OM/OM1 renaming
 *
 * Revision 6.20  2002/05/03 21:28:17  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 6.19  2002/01/16 18:56:32  grichenk
 * Removed CRef<> argument from choice variant setter, updated sources to
 * use references instead of CRef<>s
 *
 * Revision 6.18  2002/01/10 19:00:04  clausen
 * Added GetLength
 *
 * Revision 6.17  2002/01/09 15:59:30  grichenk
 * Fixed includes
 *
 * Revision 6.16  2001/10/15 23:00:00  vakatov
 * CSeq_id::x_Init() -- get rid of unreachable "break;"
 *
 * Revision 6.15  2001/08/31 20:05:44  ucko
 * Fix ICC build.
 *
 * Revision 6.14  2001/08/31 16:02:10  clausen
 * Added new constructors for Fasta and added new id types, tpd, tpe, tpg
 *
 * Revision 6.13  2001/07/16 16:22:48  grichenk
 * Added CSerialUserOp class to create Assign() and Equals() methods for
 * user-defind classes.
 * Added SerialAssign<>() and SerialEquals<>() functions.
 *
 * Revision 6.12  2001/05/24 20:24:27  grichenk
 * Renamed seq/objmgrstub.hpp -> obgmgr/objmgr_base.hpp
 * Added Genbank, Embl and Ddbj support in CSeq_id::Compare()
 * Fixed General output by CSeq_id::WriteAsFasta()
 *
 * Revision 6.11  2001/04/17 04:14:49  vakatov
 * CSeq_id::AsFastaString() --> CSeq_id::WriteAsFasta()
 *
 * Revision 6.10  2001/01/03 16:39:05  vasilche
 * Added CAbstractObjectManager - stub for object manager.
 * CRange extracted to separate file.
 *
 * Revision 6.9  2000/12/26 17:28:55  vasilche
 * Simplified and formatted code.
 *
 * Revision 6.8  2000/12/15 19:30:31  ostell
 * Used Upcase() in AsFastaString() and changed to PNocase().Equals() style
 *
 * Revision 6.7  2000/12/08 22:19:45  ostell
 * changed MakeFastString to AsFastaString and to use ostream instead of string
 *
 * Revision 6.6  2000/12/08 20:45:14  ostell
 * added MakeFastaString()
 *
 * Revision 6.5  2000/12/04 15:09:41  vasilche
 * Added missing include.
 *
 * Revision 6.4  2000/11/30 22:08:18  ostell
 * finished Match()
 *
 * Revision 6.3  2000/11/30 16:13:12  ostell
 * added support for Textseq_id to Seq_id.Match()
 *
 * Revision 6.2  2000/11/28 12:47:41  ostell
 * fixed first switch statement to break properly
 *
 * Revision 6.1  2000/11/21 18:58:29  vasilche
 * Added Match() methods for CSeq_id, CObject_id and CDbtag.
 *
 * ===========================================================================
 */
