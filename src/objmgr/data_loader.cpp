/*  $Id$
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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Data loader base class for object manager
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objects/seq/seq_id_mapper.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objects/seq/Seq_annot.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CDataLoader::RegisterInObjectManager(
    CObjectManager&            om,
    CLoaderMaker_Base&         loader_maker,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    om.RegisterDataLoader(loader_maker, is_default, priority);
}


CDataLoader::CDataLoader(void)
{
    m_Name = NStr::PtrToString(this);
    return;
}


CDataLoader::CDataLoader(const string& loader_name)
    : m_Name(loader_name)
{
    if (loader_name.empty())
    {
        m_Name = NStr::PtrToString(this);
    }
}


CDataLoader::~CDataLoader(void)
{
    return;
}


void CDataLoader::SetTargetDataSource(CDataSource& data_source)
{
    m_DataSource = &data_source;
}


CDataSource* CDataLoader::GetDataSource(void)
{
    return m_DataSource;
}


void CDataLoader::SetName(const string& loader_name)
{
    m_Name = loader_name;
}


string CDataLoader::GetName(void) const
{
    return m_Name;
}


void CDataLoader::DropTSE(CRef<CTSE_Info> /*tse_info*/)
{
}


void CDataLoader::GC(void)
{
}


CDataLoader::TTSE_LockSet
CDataLoader::GetRecords(const CSeq_id_Handle& /*idh*/,
                        EChoice /*choice*/)
{
    NCBI_THROW(CObjMgrException, eNotImplemented,
               "CDataLoader::GetRecords()");
}


CDataLoader::TTSE_LockSet
CDataLoader::GetRecords(const CSeq_id_Handle& /*idh*/,
                        const SRequestDetails& /*details*/)
{
    NCBI_THROW(CObjMgrException, eNotImplemented,
               "CDataLoader::GetRecords()");
}


CDataLoader::EChoice
CDataLoader::DetailsToChoice(const SRequestDetails::TAnnotSet& annots) const
{
    EChoice ret = eCore;
    ITERATE ( SRequestDetails::TAnnotSet, i, annots ) {
        ITERATE ( SRequestDetails::TAnnotTypesSet, j, i->second ) {
            EChoice cur = eCore;
            switch ( j->GetAnnotType() ) {
            case CSeq_annot::C_Data::e_Ftable:
                cur = eFeatures;
                break;
            case CSeq_annot::C_Data::e_Graph:
                cur = eGraph;
                break;
            case CSeq_annot::C_Data::e_Align:
                cur = eAlign;
                break;
            case CSeq_annot::C_Data::e_not_set:
                return eAnnot;
            default:
                break;
            }
            if ( cur != eCore && cur != ret ) {
                if ( ret != eCore ) return eAnnot;
                ret = cur;
            }
        }
    }
    return ret;
}


CDataLoader::EChoice
CDataLoader::DetailsToChoice(const SRequestDetails& details) const
{
    EChoice ret = DetailsToChoice(details.m_NeedInternalAnnots);
    if ( !details.m_NeedSeqMap.Empty() || !details.m_NeedSeqData.Empty() ) {
        // include sequence
        if ( ret == eCore ) {
            ret = eSequence;
        }
        else {
            ret = eBlob;
        }
    }

    EChoice external = DetailsToChoice(details.m_NeedExternalAnnots);
    if ( external != eCore ) {
        // shift from internal to external annotations
        _ASSERT(external >= eFeatures && external <= eAnnot);
        external = EChoice(external + eExtFeatures - eFeatures);
        _ASSERT(external >= eExtFeatures && external <= eExtAnnot);
        if ( ret == eCore ) {
            ret = external;
        }
        else {
            ret = eAll;
        }
    }
    return ret;
}


SRequestDetails CDataLoader::ChoiceToDetails(EChoice choice) const
{
    SRequestDetails details;
    // internal annotations
    bool internal = false;
    bool external = false;
    bool sequence = false;
    CSeq_annot::C_Data::E_Choice type = CSeq_annot::C_Data::e_not_set;
    switch ( choice ) {
    case eAll:
        sequence = external = internal = true; // from all blobs
        break;
    case eBlob:
    case eBioseq:
        sequence = internal = true; // internal only
        break;
    case eSequence:
        sequence = true;
        break;
    case eAnnot:
        internal = true; // internal only
        break;
    case eGraph:
        type = CSeq_annot::C_Data::e_Graph;
        internal = true;
        break;
    case eFeatures:
        type = CSeq_annot::C_Data::e_Ftable;
        internal = true;
        break;
    case eAlign:
        type = CSeq_annot::C_Data::e_Align;
        internal = true;
        break;
    case eExtAnnot:
        type = CSeq_annot::C_Data::e_not_set; // all annotations
        external = true; // external only
        break;
    case eExtGraph:
        type = CSeq_annot::C_Data::e_Graph;
        external = true;
        break;
    case eExtFeatures:
        type = CSeq_annot::C_Data::e_Ftable;
        external = true;
        break;
    case eExtAlign:
        type = CSeq_annot::C_Data::e_Align;
        external = true;
        break;
    default:
        break;
    }
    if ( sequence ) {
        details.m_NeedSeqMap = SRequestDetails::TRange::GetWhole();
        details.m_NeedSeqData = SRequestDetails::TRange::GetWhole();
    }
    if ( internal ) {
        details.m_NeedInternalAnnots[CAnnotName()]
            .insert(SAnnotTypeSelector(type));
    }
    if ( external ) {
        details.m_NeedExternalAnnots[CAnnotName()]
            .insert(SAnnotTypeSelector(type));
    }
    return details;
}


void CDataLoader::GetChunk(TChunk /*chunk_info*/)
{
    NCBI_THROW(CObjMgrException, eNotImplemented,
               "CDataLoader::GetChunk()");
}


void CDataLoader::GetChunks(const TChunkSet& chunks)
{
    ITERATE ( TChunkSet, it, chunks ) {
        GetChunk(*it);
    }
}


TTSE_Lock CDataLoader::ResolveConflict(const CSeq_id_Handle& /*id*/,
                                       const TTSE_LockSet& /*tse_set*/)
{
    return TTSE_Lock();
}


void CDataLoader::DebugDump(CDebugDumpContext, unsigned int) const
{
    return;
}


CDataLoader::TBlobId CDataLoader::GetBlobId(const CSeq_id& id)
{
    return GetBlobId(CSeq_id_Handle::GetHandle(id));
}


CDataLoader::TBlobId CDataLoader::GetBlobId(const CSeq_id_Handle& /*sih*/)
{
    return TBlobId();
}


CDataLoader::TBlobVersion CDataLoader::GetBlobVersion(const TBlobId& /*id*/)
{
    return 0;
}


bool CDataLoader::LessBlobId(const TBlobId& id1, const TBlobId& id2) const
{
    return id1 < id2;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.17  2004/07/28 14:02:57  grichenk
* Improved MT-safety of RegisterInObjectManager(), simplified the code.
*
* Revision 1.16  2004/07/26 14:13:31  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.15  2004/07/21 15:51:25  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.14  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.13  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.12  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.11  2003/09/30 16:22:02  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.10  2003/06/02 16:06:37  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.9  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.8  2003/05/06 18:54:09  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.7  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.6  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.5  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.4  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.3  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/03/18 17:26:35  grichenk
* +CDataLoader::x_GetSeq_id(), x_GetSeq_id_Key(), x_GetSeq_id_Handle()
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
