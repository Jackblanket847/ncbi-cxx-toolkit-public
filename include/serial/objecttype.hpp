#ifndef OBJECTTYPE__HPP
#define OBJECTTYPE__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/10/20 15:51:26  vasilche
* Fixed data error processing.
* Added interface for costructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objectinfo.hpp>

BEGIN_NCBI_SCOPE

// forward declaration of object tree iterator templates
template<class C> class CTypesIteratorBase;
template<class LevelIterator> class CTreeIteratorTmpl;
class CConstTreeLevelIterator;
class CTreeIterator;

// helper template for working with CTypesIterator and CTypesConstIterator
class CType_Base
{
protected:
    typedef CTypesIteratorBase<CTreeIterator> CTypesIterator;
    typedef CTreeIteratorTmpl<CConstTreeLevelIterator> CTreeConstIterator;
    typedef CTypesIteratorBase<CTreeConstIterator> CTypesConstIterator;

    static bool Match(const CObjectTypeInfo& type, TTypeInfo typeInfo)
        {
            return type.GetTypeInfo()->IsType(typeInfo);
        }
    static bool Match(const CTypesIterator& it, TTypeInfo typeInfo);
    static bool Match(const CTypesConstIterator& it, TTypeInfo typeInfo);
    static void AddTo(CTypesIterator& it, TTypeInfo typeInfo);
    static void AddTo(CTypesConstIterator& it,  TTypeInfo typeInfo);
    static TObjectPtr GetObjectPtr(const CTypesIterator& it);
    static TConstObjectPtr GetObjectPtr(const CTypesConstIterator& it);
};

template<class C>
class Type : public CType_Base
{
    typedef CType_Base CParent;
public:
    typedef typename CParent::CTypesIterator CTypesIterator;
    typedef typename CParent::CTypesConstIterator CTypesConstIterator;

    static TTypeInfo GetTypeInfo(void)
        {
            return C::GetTypeInfo();
        }
    operator CObjectTypeInfo(void) const
        {
            return GetTypeInfo();
        }
    operator TTypeInfo(void) const
        {
            return GetTypeInfo();
        }

    static void AddTo(CTypesIterator& it)
        {
            CParent::AddTo(it, GetTypeInfo());
        }
    static void AddTo(CTypesConstIterator& it)
        {
            CParent::AddTo(it, GetTypeInfo());
        }

    static bool Match(const CObjectTypeInfo& type)
        {
            return CParent::Match(type, GetTypeInfo());
        }
    static bool Match(const CTypesIterator& it)
        {
            return CParent::Match(it, GetTypeInfo());
        }
    static bool Match(const CTypesConstIterator& it)
        {
            return CParent::Match(it, GetTypeInfo());
        }

    static C* Get(const CTypesIterator& it)
        {
            if ( !Match(it) )
                return 0;
            return &CTypeConverter<C>::Get(GetObjectPtr(it));
        }
    static const C* Get(const CTypesConstIterator& it)
        {
            if ( !Match(it) )
                return 0;
            return &CTypeConverter<C>::Get(GetObjectPtr(it));
        }

    static C* GetUnchecked(const CObjectInfo& object)
        {
            return &CTypeConverter<C>::Get(object.GetObjectPtr());
        }
    static const C* GetUnchecked(const CConstObjectInfo& object)
        {
            return &CTypeConverter<C>::Get(object.GetObjectPtr());
        }
    static C* Get(const CObjectInfo& object)
        {
            if ( !Match(object) )
                return 0;
            return GetUnchecked(object);
        }
    static const C* Get(const CConstObjectInfo& object)
        {
            if ( !Match(object) )
                return 0;
            return GetUnchecked(object);
        }
};

// get starting point of object hierarchy
template<class C>
inline
TTypeInfo ObjectType(const C& /*obj*/)
{
    return C::GetTypeInfo();
}

template<class C>
inline
pair<TObjectPtr, TTypeInfo> ObjectInfo(C& obj)
{
    return pair<TObjectPtr, TTypeInfo>(&obj, C::GetTypeInfo());
}

// get starting point of non-modifiable object hierarchy
template<class C>
inline
pair<TConstObjectPtr, TTypeInfo> ConstObjectInfo(const C& obj)
{
    return pair<TConstObjectPtr, TTypeInfo>(&obj, C::GetTypeInfo());
}

template<class C>
inline
pair<TConstObjectPtr, TTypeInfo> ObjectInfo(const C& obj)
{
    return pair<TConstObjectPtr, TTypeInfo>(&obj, C::GetTypeInfo());
}

template<class C>
inline
pair<TObjectPtr, TTypeInfo> RefChoiceInfo(CRef<C>& obj)
{
    return pair<TObjectPtr, TTypeInfo>(&obj, C::GetRefChoiceTypeInfo());
}

template<class C>
inline
pair<TConstObjectPtr, TTypeInfo> ConstRefChoiceInfo(const CRef<C>& obj)
{
    return pair<TConstObjectPtr, TTypeInfo>(&obj, C::GetRefChoiceTypeInfo());
}

//#include <serial/objecttype.inl>

END_NCBI_SCOPE

#endif  /* OBJECTTYPE__HPP */
