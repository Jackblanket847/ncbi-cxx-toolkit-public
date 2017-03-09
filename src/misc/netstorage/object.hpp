#ifndef MISC_NETSTORAGE___OBJECT__HPP
#define MISC_NETSTORAGE___OBJECT__HPP

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
 * Author: Rafael Sadyrov
 *
 */

#include <connect/services/impl/netstorage_impl.hpp>
#include "state.hpp"


BEGIN_NCBI_SCOPE


namespace NDirectNetStorageImpl
{

class CSelector
{
public:
    CSelector(SNetStorageObjectImpl& fsm, const TObjLoc&, SContext*, bool* cancel_relocate, TNetStorageFlags, ENetStorageObjectLocation = eNFL_Unknown);

    ILocation* First();
    ILocation* Next();
    bool InProgress() const;
    void Restart();
    TObjLoc& Locator();
    void SetLocator();
    SContext& GetContext();

private:
    void InitLocations(ENetStorageObjectLocation, TNetStorageFlags);
    CLocation* Top();

    TObjLoc m_ObjectLoc;
    CRef<SContext> m_Context;
    CLocatorHolding<CNotFound> m_NotFound;
    CLocatorHolding<CNetCache> m_NetCache;
    CLocatorHolding<CFileTrack> m_FileTrack;
    vector<CLocation*> m_Locations;
    size_t m_CurrentLocation;
};

class CObj : public INetStorageObjectState, private ILocation
{
public:
    CObj(SNetStorageObjectImpl& fsm, CObj* source, TNetStorageFlags flags);
    CObj(SNetStorageObjectImpl& fsm, SContext* context, TNetStorageFlags flags);
    CObj(SNetStorageObjectImpl& fsm, SContext* context, TNetStorageFlags flags, const string& service);
    CObj(SNetStorageObjectImpl& fsm, SContext* context, const string& object_loc);
    CObj(SNetStorageObjectImpl& fsm, SContext* context, const string& key, TNetStorageFlags flags, const string& service = kEmptyStr);

    ERW_Result Read(void*, size_t, size_t*) override;
    ERW_Result PendingCount(size_t* count) override { *count = 0; return eRW_Success; }
    bool Eof() override;

    ERW_Result Write(const void*, size_t, size_t*) override;
    ERW_Result Flush() override { return eRW_Success; }

    void Close() override;
    void Abort() override;

    string GetLoc() const;
    Uint8 GetSize();
    list<string> GetAttributeList() const;
    string GetAttribute(const string&) const;
    void SetAttribute(const string&, const string&);
    CNetStorageObjectInfo GetInfo();
    void SetExpiration(const CTimeout&);

    string FileTrack_Path();
    TUserInfo GetUserInfo();

    CNetStorageObjectLoc& Locator();
    string Relocate(TNetStorageFlags, TNetStorageProgressCb cb);
    void CancelRelocate();
    bool Exists();
    ENetStorageRemoveResult Remove();

private:
    template <class TCaller>
    auto Meta(TCaller caller)                   -> decltype(caller(nullptr));

    template <class TCaller>
    auto Meta(TCaller caller, bool restartable) -> decltype(caller(nullptr));

    template <class TCaller>
    auto MetaImpl(TCaller caller)               -> decltype(caller(nullptr));

    INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*) override;
    INetStorageObjectState* StartWrite(const void*, size_t, size_t*, ERW_Result*) override;
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    ENetStorageRemoveResult RemoveImpl();
    void SetExpirationImpl(const CTimeout&);
    string FileTrack_PathImpl();
    TUserInfo GetUserInfoImpl();

    bool IsSame(const ILocation* other) const { return To<CObj>(other); }

    void RemoveOldCopyIfExists();
    SNetStorageObjectImpl* Clone(TNetStorageFlags flags, CObj** copy);

    bool m_CancelRelocate = false;
    unique_ptr<CSelector> m_Selector;
    ILocation* m_Location;
    bool m_IsOpened;
};

}

END_NCBI_SCOPE

#endif
