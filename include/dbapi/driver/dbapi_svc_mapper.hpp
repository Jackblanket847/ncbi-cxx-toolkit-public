#ifndef DBAPI_SVC_MAPPER_HPP
#define DBAPI_SVC_MAPPER_HPP

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
* Author: Sergey Sikorskiy
*
* File Description:
*
*============================================================================
*/

#include <dbapi/driver/dbapi_conn_factory.hpp>
#include <corelib/ncbimtx.hpp>

#ifdef HAVE_LIBCONNEXT
#  include <connect/ext/ncbi_dblb_svcmapper.hpp>
#endif

#include <vector>
#include <set>
#include <map>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
/// CDBDefaultServiceMapper
///

class NCBI_DBAPIDRIVER_EXPORT CDBDefaultServiceMapper : public IDBServiceMapper
{
public:
    CDBDefaultServiceMapper(void);
    virtual ~CDBDefaultServiceMapper(void);
    
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100);
    
private:
    typedef set<string> TSrvSet;
    
    CFastMutex  m_Mtx;
    TSrvSet     m_SrvSet;
};

///////////////////////////////////////////////////////////////////////////////
/// CDBServiceMapperCoR
///
/// IDBServiceMapper adaptor which implements the chain of responsibility
/// pattern
///

class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperCoR : public IDBServiceMapper
{
public:
    CDBServiceMapperCoR(void);
    virtual ~CDBServiceMapperCoR(void);
    
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100);
    
    void Push(const CRef<IDBServiceMapper>& mapper);
    void Pop(void);
    CRef<IDBServiceMapper> Top(void) const;
    bool Empty(void) const;
    
protected:
    void ConfigureFromRegistry(const IRegistry* registry = NULL);
    
    typedef vector<CRef<IDBServiceMapper> > TDelegates;
    
    mutable CFastMutex m_Mtx;
    TDelegates         m_Delegates;
};



///////////////////////////////////////////////////////////////////////////////
/// CDBUDRandomMapper
///

class NCBI_DBAPIDRIVER_EXPORT CDBUDRandomMapper : public IDBServiceMapper
{
public:
    CDBUDRandomMapper(const IRegistry* registry = NULL);
    virtual ~CDBUDRandomMapper(void);
    
public:
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100);
    
protected:
    void ConfigureFromRegistry(const IRegistry* registry = NULL);
    void ScalePreference(const string& service, double coeff);
    void SetPreference(const string& service, double coeff);
    void SetServerPreference(const string& service, 
                             double preference, 
                             const TSvrRef& server);
    
private:
    typedef map<string, bool>                       TLBNameMap;
    typedef map<TSvrRef, double, SDereferenceLess>  TSvrMap;
    typedef map<string, TSvrMap>                    TServiceMap;

    CFastMutex  m_Mtx;
    TLBNameMap  m_LBNameMap;
    TServiceMap m_ServerMap;
    TServiceMap m_PreferenceMap;
};



///////////////////////////////////////////////////////////////////////////////
/// CDBUDPriorityMapper
///

class NCBI_DBAPIDRIVER_EXPORT CDBUDPriorityMapper : public IDBServiceMapper
{
public:
    CDBUDPriorityMapper(const IRegistry* registry = NULL);
    virtual ~CDBUDPriorityMapper(void);
    
public:
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100);
    
protected:
    void ConfigureFromRegistry(const IRegistry* registry = NULL);
    
private:
    typedef map<string, bool>                       TLBNameMap;
    typedef map<TSvrRef, double, SDereferenceLess>  TSvrMap;
    typedef map<string, TSvrMap>                    TServiceMap;
    typedef multimap<double, TSvrRef>               TServerUsageMap;
    typedef map<string, TServerUsageMap>            TServiceUsageMap;
    
    CFastMutex          m_Mtx;
    TLBNameMap          m_LBNameMap;
    TServiceMap         m_ServerMap;
    TServiceUsageMap    m_ServiceUsageMap;
};

class NCBI_DBAPIDRIVER_EXPORT CDBUniversalMapper : public CDBServiceMapperCoR
{
public:
    typedef IDBServiceMapper* (*TMapperFactory)(const IRegistry* registry);
    typedef pair<string, TMapperFactory> TMapperConf;
    
    CDBUniversalMapper(const TMapperConf& ext_mapper 
                       = TMapperConf(kEmptyStr, NULL), 
                       const IRegistry* registry = NULL);
    virtual ~CDBUniversalMapper(void);
    
    virtual void Configure(const IRegistry* registry = NULL);
    
protected:
    void ConfigureFromRegistry(const IRegistry* registry = NULL);
    
private:
    TMapperConf m_ExtMapperConf;
};


///////////////////////////////////////////////////////////////////////////////
// Type traits
//

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBDefaultServiceMapper>
{
public:
    static string GetName(void);
};

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBServiceMapperCoR>
{
public:
    static string GetName(void);
};

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBUDRandomMapper>
{
public:
    static string GetName(void);
};

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBUDPriorityMapper>
{
public:
    static string GetName(void);
};

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBUniversalMapper>
{
public:
    static string GetName(void);
};



/// Easy-to-use macro to install the default DBAPI service mapper
#ifdef HAVE_LIBCONNEXT
#   define DBLB_INSTALL_DEFAULT()                                              \
        CDbapiConnMgr::Instance().SetConnectionFactory                         \
            (new CDBConnectionFactory(                                         \
                new CDBUniversalMapper(                                        \
                    CDBUniversalMapper::TMapperConf(                           \
                        CDBServiceMapperTraits<CDBLB_ServiceMapper>::GetName(), \
                        &CDBLB_ServiceMapper::Factory                          \
                        ))))
#else
#   define DBLB_INSTALL_DEFAULT()                                              \
        CDbapiConnMgr::Instance().SetConnectionFactory                         \
            (new CDBConnectionFactory(                                         \
                new CDBUniversalMapper()))
#endif



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/04/06 22:25:46  ssikorsk
 * - #include <string>
 *
 * Revision 1.3  2006/04/06 21:26:50  ssikorsk
 * + #include <string>
 * + #include <vector>
 * + #include <map>
 *
 * Revision 1.2  2006/01/26 12:04:10  ssikorsk
 * Added classes CDBUDRandomMapper, CDBUDPriorityMapper, CDBUniversalMapper
 *   and macro DBLB_INSTALL_DEFAULT;
 *
 * Revision 1.1  2006/01/03 19:36:14  ssikorsk
 * Added CDBDefaultServiceMapper and CDBServiceMapperCoR which
 * implement the IDBServiceMapper interface.
 *
 * ===========================================================================
 */

#endif // DBAPI_SVC_MAPPER_HPP
