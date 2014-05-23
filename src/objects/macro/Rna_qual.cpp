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
 * Author:  J. Chen
 *
 * File Description:
 *   DoesObjectMatchRnaQualConstraint
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'macro.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/macro/Rna_qual.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CRna_qual::~CRna_qual(void)
{
}

struct s_rnafield2featquallegal {
   ERna_field rna_field;
   EFeat_qual_legal feat_qual_legal;
} RnaField2FeatQualLegal;

static const s_rnafield2featquallegal field_qual_map[] = {
 { eRna_field_product , eFeat_qual_legal_product} ,
 { eRna_field_comment , eFeat_qual_legal_note} ,
 { eRna_field_codons_recognized , eFeat_qual_legal_codons_recognized} ,
 { eRna_field_ncrna_class , eFeat_qual_legal_ncRNA_class} ,
 { eRna_field_anticodon , eFeat_qual_legal_anticodon} ,
 { eRna_field_transcript_id , eFeat_qual_legal_transcript_id} ,
 { eRna_field_gene_locus , eFeat_qual_legal_gene} ,
 { eRna_field_gene_description , eFeat_qual_legal_gene_description} ,
 { eRna_field_gene_maploc , eFeat_qual_legal_map} ,
 { eRna_field_gene_locus_tag , eFeat_qual_legal_locus_tag} ,
 { eRna_field_gene_synonym , eFeat_qual_legal_synonym} ,
 { eRna_field_gene_comment , eFeat_qual_legal_gene_comment},
 { eRna_field_tag_peptide , eFeat_qual_legal_tag_peptide}
};

int CRna_qual :: x_GetLegalQual(ERna_field rna_field) const
{
   for (unsigned i=0; i< ArraySize(field_qual_map); i++) {
       if (field_qual_map[i].rna_field == rna_field) {
          return (int)field_qual_map[i].feat_qual_legal;
       }
   }
   return 0;
};
 
string CRna_qual :: x_GetRNAQualFromFeature(const CSeq_feat& feat, const CString_constraint& str_cons, CConstRef <CScope> scope) const
{
   if (!GetType().Match(feat)) {
      return kEmptyStr;
   }
   int legal_qual = x_GetLegalQual(GetField());
   if (legal_qual) {
     CFeat_qual_choice feat_qual;
     feat_qual.SetLegal_qual((EFeat_qual_legal)legal_qual); 
     feat_qual.GetQualFromFeatureAnyType(feat, str_cons, scope);
   }
   else return kEmptyStr;
};

bool CRna_qual :: Match(const CSeq_feat& feat, const CString_constraint& str_cons, CConstRef <CScope> scope) const
{
  if (str_cons.Empty()) {
     return true;
  }
 
  CString_constraint tmp_cons;
  tmp_cons.Assign(str_cons);
  tmp_cons.SetNot_present(false);
  string str = x_GetRNAQualFromFeature(feat, tmp_cons, scope); 
  bool rval = (str.empty() ? false : true);
  if (str_cons.GetNot_present()) {
     rval = !rval;
  }
  return rval;
};

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1717, CRC32: 82b30fd4 */
