#ifndef CORELIB___PLUGIN_MANAGER__HPP
#define CORELIB___PLUGIN_MANAGER__HPP

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
 * Author:  Denis Vakatov, Anatoliy Kuznetsov
 *
 * File Description:  Plugin manager (using class factory paradigm)
 *
 */

/// @file plugin_manager.hpp
/// Plugin manager (using class factory paradigm).
///
/// Describe generic interface and provide basic functionality to advertise
/// and export a class factory. 
/// The class and class factory implementation code can be linked to
/// either statically (then, the class factory will need to be registered
/// explicitly by the user code) or dynamically (then, the DLL will be
/// searched for using plugin name, and the well-known DLL entry point
/// will be used to register the class factory, automagically).
/// 
/// - "class factory" -- An entity used to generate objects of the given class.
///                      One class factory can generate more than one version
///                      of the class.
/// 
/// - "interface"  -- Defines the implementation-independent API and expected
///                   behaviour of a class.
///                   Interface's name is provided by its class's factory,
///                   see IClassFactory::GetInterfaceName().
///                   Interfaces are versioned to track the compatibility.
/// 
/// - "driver"  -- A concrete implementation of the interface and its factory.
///                Each driver has its own name (do not confuse it with the
///                interface name!) and version.
/// 
/// - "host"    -- An entity (DLL or the EXEcutable itself) that contains
///                one or more drivers (or versions of the same driver),
///                which can implement one or more interfaces.
/// 
/// - "version" -- MAJOR (backward- and forward-incompatible changes in the
///                       interface and/or its expected behaviour);
///                MINOR (backward compatible changes in the driver code);
///                PATCH_LEVEL (100% compatible plugin or driver code changes).
/// 

#include <corelib/ncbimtx.hpp>
#include <corelib/version.hpp>
#include <corelib/ncbidll.hpp>
#include <corelib/ncbi_tree.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_paramtree.hpp>

#include <set>
#include <string>


BEGIN_NCBI_SCOPE

/** @addtogroup PluginMgr
 *
 * @{
 */

/////////////////////////////////////////////////////////////////////////////
///
/// CPluginManagerException --
///
/// Exception generated by CPluginManager.

class CPluginManagerException : public CCoreException
{
public:
    /// Error types that CTime can generate.
    enum EErrCode {
        eResolveFailure,       ///< Cannot resolve interface driver
        eParameterMissing      ///< Missing mandatory parameter
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eResolveFailure:   return "eResolveFailure";
        case eParameterMissing: return "eParameterMissing";
        default:    return CException::GetErrCodeString();
        }
    }

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CPluginManagerException, CCoreException);
};


/// CInterfaceVersion<> --
///
/// Current interface version.
///
/// It is just a boilerplate, to be hard-coded in the concrete interface header
/// @sa NCBI_PLUGIN_VERSION, CVersionInfo

template <class TClass>
class CInterfaceVersion
{
};


/// Macro to auto-setup the current interface name and version.
///
/// This macro must be "called" once per interface, usually in the
/// very header that describes that interface.
///
/// Example:
///    NCBI_INTERFACE_VERSION(IFooBar, "IFooBar", 1, 3, 8);
/// @sa CInterfaceVersion

#define NCBI_DECLARE_INTERFACE_VERSION(iface, iface_name, major, minor, patch_level) \
template<> \
class CInterfaceVersion<iface> \
{ \
public: \
    enum { \
        eMajor      = major, \
        eMinor      = minor, \
        ePatchLevel = patch_level \
    }; \
    static const char* GetName() { return iface_name; } \
}

/// Macro to contruct CVersionInfo class using interface name
/// (relies on CInterfaceVersion class)
/// @sa CVersionInfo
#define NCBI_INTERFACE_VERSION(iface) \
CVersionInfo(ncbi::CInterfaceVersion<iface>::eMajor, \
             ncbi::CInterfaceVersion<iface>::eMinor, \
             ncbi::CInterfaceVersion<iface>::ePatchLevel)


typedef TParamTree TPluginManagerParamTree;


/// IClassFactory<> --
///
/// Class factory for the given interface.
///
/// IClassFactory should be implemented for collection of drivers
/// and exported by hosts

template <class TClass, class TIfVer = CInterfaceVersion<TClass> >
class IClassFactory
{
public:
    typedef TClass TInterface;

    struct SDriverInfo
    {
        string         name;        ///< Driver name
        CVersionInfo   version;     ///< Driver version

        SDriverInfo(const string&       driver_name,
                    const CVersionInfo& driver_version)
            : name(driver_name),
              version(driver_version)
        {}
    };

    typedef list<SDriverInfo>  TDriverList;


    /// Create driver's instance
    ///
    /// Function creates driver by its name and version.
    /// The requirements is the drivers version should match the interface
    /// up to the patch level.
    ///
    /// @param driver
    ///  Requested driver's name (not the name of the supported interface)
    /// @param version
    ///  Requested interface version (as understood by the caller).
    ///  By default it will be passed the version which is current from
    ///  the calling code's point of view.
    /// @param 
    /// @return
    ///  NULL on any error (not found entry point, version mismatch, etc.)
    virtual TClass* CreateInstance
      (const string&  driver  = kEmptyStr,
       CVersionInfo   version = CVersionInfo(TIfVer::eMajor, TIfVer::eMinor,
                                             TIfVer::ePatchLevel),
       const TPluginManagerParamTree* params = 0) const = 0;

    /// Versions of the interface exported by the factory
    virtual void GetDriverVersions(TDriverList& driver_list) const = 0;

    virtual ~IClassFactory(void) {}

protected:

    /// Utility function to get an element of parameter tree
    /// Throws an exception when mandatory parameter is missing
    /// (or returns the deafult value)

    const string& GetParam(const string&                  driver_name,
                           const TPluginManagerParamTree* params,
                           const string&                  param_name, 
                           bool                           mandatory,
                           const string&                  default_value) const;
};



class CPluginManager_DllResolver;


struct CPluginManagerBase : public CObject
{
};


/// CPluginManager<> --
///
/// To register (either directly, or via an "entry point") class factories
/// for the given interface.
///
/// Then, to facilitate the process of instantiating the class given
/// the registered pool of drivers, and also taking into accont the driver name
/// and/or version as requested by the calling code.
///
/// Template class is protected by mutex and safe for use from diffrent threads

template <class TClass, class TIfVer = CInterfaceVersion<TClass> >
class CPluginManager : public CPluginManagerBase
{
public:
    typedef IClassFactory<TClass, TIfVer> TClassFactory;
    typedef TIfVer                        TInterfaceVersion;

    /// Create class instance
    /// @return
    ///  Never returns NULL -- always throw exception on error.
    /// @sa GetFactory()
    TClass* CreateInstance
    (const string&       driver  = kEmptyStr,
     const CVersionInfo& version = CVersionInfo(TIfVer::eMajor, TIfVer::eMinor,
                                                TIfVer::ePatchLevel),
     const TPluginManagerParamTree* params = 0)
    {
        TClassFactory* factory = GetFactory(driver, version);
        return factory->CreateInstance(driver, version, params);
    }



    /// Get class factory
    ///
    /// If more than one (of registered) class factory contain eligible
    /// driver candidates, then pick the class factory containing driver of
    /// the latest version.
    /// @param driver
    ///  Name of the driver. If passed empty, then -- any.
    /// @param version
    ///  Requested version. The returned driver can have a different (newer)
    ///  version (provided that the new implementation is backward-compatible
    ///  with the requested version.
    /// @return
    ///  Never return NULL -- always throw exception on error.
    TClassFactory* GetFactory
    (const string&       driver  = kEmptyStr,
     const CVersionInfo& version = CVersionInfo(TIfVer::eMajor, TIfVer::eMinor,
                                                TIfVer::ePatchLevel));

    /// Information about a driver, with maybe a pointer to an instantiated
    /// class factory that contains the driver.
    /// @sa FNCBI_EntryPoint
    struct SDriverInfo {
        string         name;        ///< Driver name
        CVersionInfo   version;     ///< Driver version
        // It's the plugin manager's (and not SDriverInfo) responsibility to
        // keep and then destroy class factories.
        TClassFactory* factory;     ///< Class factory (can be NULL)

        SDriverInfo(const string&       driver_name,
                    const CVersionInfo& driver_version)
            : name(driver_name),
              version(driver_version),
              factory(0)
        {}
    };

    /// List of driver information.
    ///
    /// It is used to communicate using the entry points mechanism.
    /// @sa FNCBI_EntryPoint
    typedef list<SDriverInfo> TDriverInfoList;

    /// Register factory in the manager.
    ///  
    /// The registered factory will be owned by the manager.
    /// @sa UnregisterFactory()
    void RegisterFactory(TClassFactory& factory);

    /// Unregister and release (un-own)
    /// @sa RegisterFactory()
    bool UnregisterFactory(TClassFactory& factory);

    /// Actions performed by the entry point
    /// @sa FNCBI_EntryPoint
    enum EEntryPointRequest {
        /// Add info about all drivers exported through the entry point
        /// to the end of list.
        ///
        /// "SFactoryInfo::factory" in the added info should be assigned NULL.
        eGetFactoryInfo,

        /// Scan the driver info list passed to the entry point for the
        /// [name,version] pairs exported by the given entry point.
        ///
        /// For each pair found, if its "SDriverInfo::factory" is NULL,
        /// instantiate its class factory and assign it to the
        /// "SDriverInfo::factory".
        eInstantiateFactory
    };

    /// Entry point to get drivers' info, and (if requested) their class
    /// factori(es).
    ///
    /// This function is usually (but not necessarily) called by
    /// RegisterWithEntryPoint().
    ///
    /// Usually, it's called twice -- the first time to get the info
    /// about the drivers exported by the entry point, and then
    /// to instantiate selected factories.
    ///
    /// Caller is responsible for the proper destruction (deallocation)
    /// of the instantiated factories.
    typedef void (*FNCBI_EntryPoint)(TDriverInfoList&   info_list,
                                     EEntryPointRequest method);

    /// Register all factories exported by the plugin entry point.
    /// @sa RegisterFactory()
    void RegisterWithEntryPoint(FNCBI_EntryPoint plugin_entry_point);

    /// Attach DLL resolver to plugin manager 
    /// 
    /// Plugin mananger uses all attached resolvers to search for DLLs
    /// exporting drivers of this interface. 
    ///
    /// @param resolver
    ///   DLL resolver. Plugin manager takes ownership of the resolver.
    ///
    /// @sa DetachResolver
    void AddResolver(CPluginManager_DllResolver* resolver);

    /// Remove resolver from the list of active resolvers.
    ///
    /// Method is used when we need to freeze some of the resolution variants
    /// Resolver is not deleted, and can be reattached again by AddResolver
    ///
    /// @param resolver
    ///   DLL resolver. Ownership is returned to the caller and resolver
    ///   should be deleted by the caller
    ///
    /// @return Pointer on the detached resolver (same as resolver parameter)
    /// or NULL if resolver not found
    CPluginManager_DllResolver* 
    DetachResolver(CPluginManager_DllResolver* resolver);

    /// Add path for the DLL lookup (for all resolvers)
    void AddDllSearchPath(const string& path);

    /// Scan DLLs for specified driver using attached resolvers
    void Resolve(const string&       driver  = kEmptyStr,
                 const CVersionInfo& version = CVersionInfo
                 (TIfVer::eMajor, TIfVer::eMinor, TIfVer::ePatchLevel));

    /// Disable/enable DLL resolution (search for class factories in DLLs)
    void FreezeResolution(bool value = true) { m_BlockResolution = value; }

    
    /// Disable/enable DLL resolution (search for class factories in DLLs)
    /// for the specified driver
    void FreezeResolution(const string& driver, bool value = true);

    // ctors
    CPluginManager(void) : m_BlockResolution(false) {}
    virtual ~CPluginManager();

protected:
    TClassFactory* FindClassFactory(const string&  driver,
                                    const CVersionInfo& version);

    /// Protective mutex to syncronize the access to the plugin manager
    /// from different threads
    CMutex m_Mutex;

    typedef vector<CDllResolver::SResolvedEntry> TResolvedEntries;

    typedef vector<CPluginManager_DllResolver*>  TDllResolvers;

    typedef set<string>                          TStringSet;
private:
    /// List of factories presently registered with (and owned by)
    /// the plugin manager.
    set<TClassFactory*>                  m_Factories;
    /// DLL resolvers
    TDllResolvers                        m_Resolvers;
    /// Paths used for DLL search
    vector<string>                       m_DllSearchPaths;
    /// DLL entries resolved and registered with dll resolver(s)
    TResolvedEntries                     m_RegisteredEntries;
    /// Flag, prohibits DLL resolution
    bool                                 m_BlockResolution;
    /// Set of drivers prohibited from DLL resolution
    TStringSet                           m_FreezeResolutionDrivers;
};




/// Service class for DLLs resolution.
/// 
/// Class is used by CPluginManager to scan directories for DLLs, 
/// load and resolve entry points.
///
class NCBI_XNCBI_EXPORT CPluginManager_DllResolver
{
public:
    //
    CPluginManager_DllResolver(void);

    /// Construction
    ///
    /// @param interface_name
    ///   Target interface name
    /// @param plugin_name
    ///   Plugin family name (dbapi, xloader, etc)
    /// @param driver_name
    ///   Name of the driver (dblib, id1, etc)
    /// @param version
    ///   Interface version
    CPluginManager_DllResolver
    (const string&       interface_name,
     const string&       driver_name = kEmptyStr,
     const CVersionInfo& version     = CVersionInfo::kAny);

    //
    virtual ~CPluginManager_DllResolver(void);

    /// Search for plugin DLLs, resolve entry points.
    ///
    /// @param paths
    ///   List of directories to scan for DLLs
    /// @param driver_name
    ///   Name of the driver (dblib, id1, etc)
    /// @param version
    ///   Interface version
    /// @return
    ///   Reference on DLL resolver holding all entry points
    CDllResolver& Resolve(const vector<string>& paths,
                          const string&         driver_name = kEmptyStr,
                          const CVersionInfo&   version = CVersionInfo::kAny);

    /// Search for plugin DLLs, resolve entry points.
    ///
    /// @param paths
    ///   Path to scan for DLLs
    /// @return
    ///   Reference on DLL resolver holding all entry points
    CDllResolver& Resolve(const string& path);

    /// Return dll file name. Name does not include path.
    ///
    /// Example:
    ///    "ncbi_plugin_dbapi_ftds_3_1_7".
    ///    "ncbi_pulgin_dbapi_ftds.so.3.1.7"
    /// In this case, the DLL will be searched for in the standard
    /// DLL search paths, with automatic addition of any platform-specific
    /// prefixes and suffixes.
    ///
    /// @param driver_name
    ///    Driver name ("id1", "lds", etc)
    /// @param version
    ///    Requested version of the driver
    virtual
    string GetDllName(const string&       interface_name,
                      const string&       driver_name  = kEmptyStr,
                      const CVersionInfo& version      = CVersionInfo::kAny)
        const;

    /// Return DLL name mask 
    ///
    /// DLL name mask is used for DLL file search.
    ///
    /// Example:
    ///    "ncbi_plugin_objmgr_*.dll"
    ///    "ncbi_plugin_dbapi_ftds.so.*"
    virtual
    string GetDllNameMask(const string&       interface_name,
                          const string&       driver_name = kEmptyStr,
                          const CVersionInfo& version     = CVersionInfo::kAny)
        const;

    /// Return DLL entry point name:
    ///
    /// Default name pattern is:
    ///  - "NCBI_EntryPoint_interface_driver"
    /// Alternative variants:
    ///  - "NCBI_EntryPoint"
    ///  - "NCBI_EntryPoint_interface"
    ///  - "NCBI_EntryPoint_driver"
    ///
    /// @sa GetEntryPointPrefix
    virtual
    string GetEntryPointName(const string& interface_name = kEmptyStr,
                             const string& driver_name    = kEmptyStr) const;


    /// Return DLL entry point prefix ("NCBI_EntryPoint")
    virtual string GetEntryPointPrefix() const;

    /// Return DLL file name prefix ("ncbi_plugin")
    virtual string GetDllNamePrefix() const;

    /// Set DLL file name prefix
    virtual void SetDllNamePrefix(const string& prefix);

    /// Return name of the driver
    const string& GetDriverName() const { return m_DriverName; }

protected:
    CDllResolver* GetCreateDllResolver();
    CDllResolver* CreateDllResolver() const;

protected:
    string          m_DllNamePrefix;
    string          m_EntryPointPrefix;
    string          m_InterfaceName;
    string          m_DriverName;
    CVersionInfo    m_Version;
    CDllResolver*   m_DllResolver;
};

/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


template <class TClass, class TIfVer>
typename CPluginManager<TClass, TIfVer>::TClassFactory* 
CPluginManager<TClass, TIfVer>::GetFactory(const string&       driver,
                                           const CVersionInfo& version)
{
    CMutexGuard guard(m_Mutex);

    TClassFactory* cf = 0;

    // Search among already registered factories
    cf = FindClassFactory(driver, version);
    if (cf) {
        return cf;
    }

    if (!m_BlockResolution) {

        typename TStringSet::const_iterator it = 
                 m_FreezeResolutionDrivers.find(driver);

        if (it == m_FreezeResolutionDrivers.end()) {
            // Trying to resolve the driver's factory
            Resolve(driver, version);

            // Re-scanning factories...
            cf = FindClassFactory(driver, version);
            if (cf) {
                return cf;
            }
        }
    }

    NCBI_THROW(CPluginManagerException, eResolveFailure, 
               "Cannot resolve class factory");
}


template <class TClass, class TIfVer>
typename CPluginManager<TClass, TIfVer>::TClassFactory* 
CPluginManager<TClass, TIfVer>::FindClassFactory(const string&  driver,
                                                 const CVersionInfo& version)
{
    TClassFactory* best_factory = 0;
    int best_major = -1;
    int best_minor = -1;
    int best_patch_level = -1;

    NON_CONST_ITERATE(typename set<TClassFactory*>, it, m_Factories) {
        TClassFactory* cf = *it;

        typename TClassFactory::TDriverList drv_list;

        if (!cf)
            continue;

        cf->GetDriverVersions(drv_list);

        NON_CONST_ITERATE(typename TClassFactory::TDriverList, it2, drv_list) {
             typename TClassFactory::SDriverInfo& drv_info = *it2;
             if (!driver.empty()) {
                if (driver != drv_info.name) {
                    continue;
                }
             }
             const CVersionInfo& vinfo = drv_info.version;
             if (IsBetterVersion(version, vinfo, 
                                 best_major, best_minor, best_patch_level))
             {
                best_factory = cf;
             }
        }
    }
    
    return best_factory;
}


template <class TClass, class TIfVer>
void CPluginManager<TClass, TIfVer>::RegisterFactory(TClassFactory& factory)
{
    CMutexGuard guard(m_Mutex);

    m_Factories.insert(&factory);
}


template <class TClass, class TIfVer>
bool CPluginManager<TClass, TIfVer>::UnregisterFactory(TClassFactory& factory)
{
    CMutexGuard guard(m_Mutex);

    typename set<TClassFactory*>::iterator it = m_Factories.find(&factory);
    if (it != m_Factories.end()) {
        delete *it;
        m_Factories.erase(it);
    }
    return true; //?
}


template <class TClass, class TIfVer>
void CPluginManager<TClass, TIfVer>::RegisterWithEntryPoint
(FNCBI_EntryPoint plugin_entry_point)
{
    TDriverInfoList drv_list;
    plugin_entry_point(drv_list, eGetFactoryInfo);

    if ( !drv_list.empty() ) {
        plugin_entry_point(drv_list, eInstantiateFactory);

        NON_CONST_ITERATE(typename TDriverInfoList, it, drv_list) {
            if (it->factory) {
                RegisterFactory(*(it->factory));
            }
        }
    }

}



template <class TClass, class TIfVer>
void CPluginManager<TClass, TIfVer>::AddResolver
(CPluginManager_DllResolver* resolver)
{
    _ASSERT(resolver);
    m_Resolvers.push_back(resolver);
}

template <class TClass, class TIfVer>
CPluginManager_DllResolver* 
CPluginManager<TClass, TIfVer>::DetachResolver(CPluginManager_DllResolver* 
                                                                    resolver)
{
    NON_CONST_ITERATE(TDllResolvers, it, m_Resolvers) {
        if (resolver == *it) {
            m_Resolvers.erase(it);
            return resolver;
        }
    }
    return 0;
}

template <class TClass, class TIfVer>
void CPluginManager<TClass, TIfVer>::AddDllSearchPath(const string& path)
{
    m_DllSearchPaths.push_back(path);
}

template <class TClass, class TIfVer>
void CPluginManager<TClass, TIfVer>::FreezeResolution(const string& driver, 
                                                      bool          value)
{
    if (value) {
        m_FreezeResolutionDrivers.insert(driver);
    } else {
        m_FreezeResolutionDrivers.erase(driver);
    }
}

template <class TClass, class TIfVer>
void CPluginManager<TClass, TIfVer>::Resolve(const string&       driver,
                                             const CVersionInfo& version)
{
    vector<CDllResolver*> resolvers;

    // Run all resolvers to search for driver
    ITERATE(vector<CPluginManager_DllResolver*>, it, m_Resolvers) {
        CDllResolver* dll_resolver =
            &(*it)->Resolve(m_DllSearchPaths, driver, version);
        if ( !version.IsAny()  &&
            dll_resolver->GetResolvedEntries().empty() ) {
            // Try to ignore version
            dll_resolver =
                &(*it)->Resolve(m_DllSearchPaths, driver, CVersionInfo::kAny);
            if ( dll_resolver->GetResolvedEntries().empty() ) {
                dll_resolver = 0;
            }
        }
        if ( dll_resolver ) {
            resolvers.push_back(dll_resolver);
        }
    }

    // Now choose the DLL entry point to register the class factory
    NON_CONST_ITERATE(vector<CDllResolver*>, it, resolvers) {
        CDllResolver::TEntries& entry_points = (*it)->GetResolvedEntries();

        NON_CONST_ITERATE(CDllResolver::TEntries, ite, entry_points) {
            CDllResolver::SResolvedEntry& entry = *ite;

            if (entry.entry_points.empty()) {
                continue;
            }
            CDllResolver::SNamedEntryPoint& epoint = entry.entry_points[0];
            // TODO:
            // check if entry point provides the required interface-driver-version
            // and do not register otherwise...
            if (epoint.entry_point.func) {
                FNCBI_EntryPoint ep = 
                   (FNCBI_EntryPoint)epoint.entry_point.func;
                RegisterWithEntryPoint(ep);
                m_RegisteredEntries.push_back(entry);
            }
        }
        entry_points.resize(0);
    }
}


template <class TClass, class TIfVer>
CPluginManager<TClass, TIfVer>::~CPluginManager()
{
    {{
        typename set<TClassFactory*>::iterator it = m_Factories.begin();
        typename set<TClassFactory*>::iterator it_end = m_Factories.end();
        for (; it != it_end; ++it) {
            TClassFactory* f = *it;
            delete f;
        }

    }}

    {{
        typename vector<CPluginManager_DllResolver*>::iterator it =
            m_Resolvers.begin();
        typename vector<CPluginManager_DllResolver*>::iterator it_end = 
            m_Resolvers.end();
        for (; it != it_end; ++it) {
            CPluginManager_DllResolver* r = *it;
            delete r;
        }

    }}

    NON_CONST_ITERATE(TResolvedEntries, it, m_RegisteredEntries) {
        delete it->dll;
    }
}




template <class TClass, class TIfVer >
const string& 
IClassFactory<TClass, TIfVer>::GetParam(
                        const string&                  driver_name,
                        const TPluginManagerParamTree* params,
                        const string&                  param_name, 
                        bool                           mandatory,
                        const string&                  default_value) const
{
    const TPluginManagerParamTree* tn = params->FindSubNode(param_name);

    if (tn == 0 || tn->GetValue().empty()) {
        if (mandatory) {
            string msg = 
                "Cannot init " + driver_name 
                                + ", missing parameter:" + param_name;
            NCBI_THROW(CPluginManagerException, eParameterMissing, msg);
        } else {
            return default_value;
        }
    }
    return tn->GetValue();        
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.33  2004/09/22 13:55:20  kuznets
 * All tree realted stuff moved to ncbi_paramtree.hpp
 *
 * Revision 1.32  2004/09/17 15:05:34  kuznets
 * +CPluginMananger::TInterfaceVersion
 *
 * Revision 1.31  2004/08/19 13:02:17  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.30  2004/08/11 14:09:33  grichenk
 * Improved DLL version matching
 *
 * Revision 1.29  2004/08/10 16:54:09  grichenk
 * Replaced CFastMutex with CMutex
 *
 * Revision 1.28  2004/08/09 15:39:06  kuznets
 * Improved support of driver name
 *
 * Revision 1.27  2004/08/02 13:51:29  kuznets
 * Derived CPluginManager from CObject
 *
 * Revision 1.26  2004/07/29 15:37:59  dicuccio
 * Rearranged export specifier - comes before return value
 *
 * Revision 1.25  2004/07/29 13:14:49  kuznets
 * + PluginManager_ConvertRegToTree
 *
 * Revision 1.24  2004/07/26 16:09:13  kuznets
 * GetName implementation moved to IClassFactory
 *
 * Revision 1.23  2004/07/26 13:44:45  kuznets
 * + parameter missing excpetion type
 *
 * Revision 1.22  2004/04/26 14:46:36  ucko
 * Fix a typo in FreezeResolution, and make sure to give
 * UnregisterFactory a return value per its prototype.
 *
 * Revision 1.21  2004/03/19 19:16:20  kuznets
 * Added group of functions to better control DLL resolution (FreezeResolution)
 *
 * Revision 1.20  2004/02/10 20:21:13  ucko
 * Make the interface version class a template parameter with the
 * appropriate default value to work around limitations in IBM's
 * VisualAge compiler.  (It otherwise effectively ignores specializations
 * of CInterfaceVersion that follow the templates that use them.)
 *
 * Revision 1.19  2004/01/13 17:21:23  kuznets
 * Class factory CreateInstance method received an additional parameter
 * TPluginManagerParamTree (to specify initialization parameters or prefrences)
 *
 * Revision 1.18  2003/12/02 12:44:39  kuznets
 * Fixed minor compilation issue.
 *
 * Revision 1.17  2003/12/01 19:53:06  kuznets
 * Reflecting changes in CDllResolver
 *
 * Revision 1.16  2003/11/19 15:46:04  kuznets
 * Removed clumsy function pointer conversion from plugin manager.
 * (it is now covered by CDll)
 *
 * Revision 1.15  2003/11/19 13:48:20  kuznets
 * Helper classes migrated into a plugin_manager_impl.hpp
 *
 * Revision 1.14  2003/11/18 17:09:25  kuznets
 * Fixing compilation warnings
 *
 * Revision 1.13  2003/11/18 15:26:29  kuznets
 * Numerous fixes here and there as a result of testing and debugging.
 *
 * Revision 1.12  2003/11/17 17:04:11  kuznets
 * Cosmetic fixes
 *
 * Revision 1.11  2003/11/12 18:56:53  kuznets
 * Implemented dll resolution.
 *
 * Revision 1.10  2003/11/07 17:02:48  kuznets
 * Drafted CPluginManager_DllResolver.
 *
 * Revision 1.9  2003/11/03 20:08:01  kuznets
 * Fixing various compiler warnings
 *
 * Revision 1.8  2003/11/03 17:52:00  kuznets
 * Added CSimpleClassFactoryImpl template.
 * Helps quickly implement basic PM compatible class factory.
 *
 * Revision 1.7  2003/11/03 16:32:58  kuznets
 * Cleaning the code to be compatible with GCC, WorkShop 53 and MSVC at the
 * same time...
 *
 * Revision 1.6  2003/10/31 19:53:52  kuznets
 * +CHostEntryPointImpl
 *
 * Revision 1.5  2003/10/30 20:03:49  kuznets
 * Work in progress. Added implementations of CPluginManager<> methods.
 *
 * Revision 1.4  2003/10/29 23:35:46  vakatov
 * Just starting with CDllResolver...
 *
 * Revision 1.3  2003/10/29 19:34:43  vakatov
 * Comment out unfinished defined APIs (using "#if 0")
 *
 * Revision 1.2  2003/10/28 22:29:04  vakatov
 * Draft-done with:
 *   general terminology
 *   CInterfaceVersion<>
 *   NCBI_PLUGIN_VERSION()
 *   IClassFactory<>
 *   CPluginManager<>
 * TODO:
 *   Host-related API
 *   DLL resolution
 *
 * Revision 1.1  2003/10/28 00:12:23  vakatov
 * Initial revision
 *
 * Work-in-progress, totally unfinished.
 * Note: DLL resolution shall be split and partially moved to the NCBIDLL.
 *
 * ===========================================================================
 */

#endif  /* CORELIB___PLUGIN_MANAGER__HPP */
