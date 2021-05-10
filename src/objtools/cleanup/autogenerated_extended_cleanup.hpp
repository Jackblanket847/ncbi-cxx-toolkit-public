#ifndef AUTOGENERATEDEXTENDEDCLEANUP__HPP
#define AUTOGENERATEDEXTENDEDCLEANUP__HPP

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
/// This file was generated by application DATATOOL
///
/// ATTENTION:
///   Don't edit or commit this file into SVN as this file will
///   be overridden (by DATATOOL) without warning!

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/MultiOrgName.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/Txinit.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/submit/Seq_submit.hpp>

#include "newcleanupp.hpp"


BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(objects)

class CAutogeneratedExtendedCleanup { 
public: 
  CAutogeneratedExtendedCleanup(
    CScope & scope,
    CNewCleanup_imp & newCleanup ) : 
    m_Scope(scope), 
    m_NewCleanup(newCleanup), 
    m_LastArg_ExtendedCleanupSeqFeat(NULL),
    m_Dummy(0)
  { } 

  void ExtendedCleanupSeqEntry( CSeq_entry & arg0 );
  void ExtendedCleanupSeqSubmit( CSeq_submit & arg0 );
  void ExtendedCleanupSeqAnnot( CSeq_annot & arg0 );
  void ExtendedCleanupBioseq( CBioseq & arg0 );
  void ExtendedCleanupBioseqSet( CBioseq_set & arg0 );
  void ExtendedCleanupSeqFeat( CSeq_feat & arg0_raw );

private: 
  void x_ExtendedCleanupOrgName( COrgName & arg0 );
  void x_ExtendedCleanupOrgRef( COrg_ref & arg0 );
  void x_ExtendedCleanupBioSource( CBioSource & arg0 );
  void x_ExtendedCleanupImpFeat( CImp_feat & arg0 );
  void x_ExtendedCleanupProtRef( CProt_ref & arg0 );
  void x_ExtendedCleanupPubDesc( CPubdesc & arg0 );
  void x_ExtendedCleanupGeneRef( CGene_ref & arg0 );
  void x_ExtendedCleanupTxinit( CTxinit & arg0 );
  void x_ExtendedCleanupSeqFeatData( CSeqFeatData & arg0 );
  void x_ExtendedCleanupSeqFeatXref(CSeqFeatXref & arg0);
  void ExtendedCleanupSeqAnnotDescr( CAnnot_descr & arg0 );
    
  template<typename TSeqAnnotContainer>
  void x_ExtendedCleanupSeqAnnots(TSeqAnnotContainer& annots);
  void x_ExtendedCleanupGBBlock( CGB_block & arg0 );
  void x_ExtendedCleanupSeqdesc( CSeqdesc & arg0 );
  void x_ExtendedCleanupSeqdescr( CSeq_descr & arg0 );
  void x_ExtendedCleanupBioseq_inst( CSeq_inst & arg0 );

  CScope & m_Scope;
  CNewCleanup_imp & m_NewCleanup;

  CSeq_feat* m_LastArg_ExtendedCleanupSeqFeat;

  int m_Dummy;
}; // end of CAutogeneratedExtendedCleanup

END_SCOPE(objects)
END_SCOPE(ncbi)

#endif /* AUTOGENERATEDEXTENDEDCLEANUP__HPP */
