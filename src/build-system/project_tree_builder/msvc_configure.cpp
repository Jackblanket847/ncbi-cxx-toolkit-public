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
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <ncbi_pch.hpp>
#include "msvc_configure.hpp"
#include "proj_builder_app.hpp"
#include "util/random_gen.hpp"

#include "ptb_err_codes.hpp"
#ifdef NCBI_XCODE_BUILD
  #include <sys/utsname.h>
  #include <unistd.h>
#endif

#include <corelib/ncbiexec.hpp>
#include <util/xregexp/regexp.hpp>

BEGIN_NCBI_SCOPE


CMsvcConfigure::CMsvcConfigure(void)
    : m_HaveBuildVer(false)
{
}


CMsvcConfigure::~CMsvcConfigure(void)
{
}


void s_ResetLibInstallKey(const string& dir, 
                          const string& lib)
{
    string key_file_name(lib);
    NStr::ToLower(key_file_name);
    key_file_name += ".installed";
    string key_file_path = CDirEntry::ConcatPath(dir, key_file_name);
    if ( CDirEntry(key_file_path).Exists() ) {
        CDirEntry(key_file_path).Remove();
    }
}


static void s_CreateThirdPartyLibsInstallMakefile
                                            (CMsvcSite&   site, 
                                             const list<string> libs_to_install,
                                             const SConfigInfo& config,
                                             const CBuildType&  build_type)
{
    // Create makefile path
    string makefile_path = GetApp().GetProjectTreeInfo().m_Compilers;
    makefile_path = 
        CDirEntry::ConcatPath(makefile_path, 
                              GetApp().GetRegSettings().m_CompilersSubdir);

    makefile_path = CDirEntry::ConcatPath(makefile_path, build_type.GetTypeStr());
    makefile_path = CDirEntry::ConcatPath(makefile_path, config.GetConfigFullName());
    makefile_path = CDirEntry::ConcatPath(makefile_path, 
                                          "Makefile.third_party.mk");

    // Create dir if no such dir...
    string dir;
    CDirEntry::SplitPath(makefile_path, &dir);
    CDir makefile_dir(dir);
    if ( !makefile_dir.Exists() ) {
        CDir(dir).CreatePath();
    }

    CNcbiOfstream ofs(makefile_path.c_str(), 
                      IOS_BASE::out | IOS_BASE::trunc );
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, makefile_path);

    GetApp().RegisterGeneratedFile( makefile_path );
    ITERATE(list<string>, n, libs_to_install) {
        const string& lib = *n;
        SLibInfo lib_info;
        site.GetLibInfo(lib, config, &lib_info);
        if ( !lib_info.m_LibPath.empty() ) {
            string bin_dir = lib_info.m_LibPath;
            string bin_path = lib_info.m_BinPath;
            if (bin_path.empty()) {
                bin_path = site.GetThirdPartyLibsBinSubDir();
            }
            bin_dir = 
                CDirEntry::ConcatPath(bin_dir, bin_path);
            bin_dir = CDirEntry::NormalizePath(bin_dir);
            if ( CDirEntry(bin_dir).Exists() ) {
                //
                string key(lib);
                NStr::ToUpper(key);
                key += site.GetThirdPartyLibsBinPathSuffix();

                ofs << key << " = " << bin_dir << "\n";

                s_ResetLibInstallKey(dir, lib);
                site.SetThirdPartyLibBin(lib, bin_dir);
            } else {
                PTB_WARNING_EX(bin_dir, ePTB_PathNotFound,
                               lib << "|" << config.GetConfigFullName()
                               << " disabled, path not found");
            }
        } else {
            PTB_WARNING_EX(kEmptyStr, ePTB_PathNotFound,
                           lib << "|" << config.GetConfigFullName()
                           << ": no LIBPATH specified");
        }
    }
}


void CMsvcConfigure::Configure(CMsvcSite&         site, 
                               const list<SConfigInfo>& configs,
                               const string&            root_dir)
{
    _TRACE("*** Analyzing 3rd party libraries availability ***");
    
    site.InitializeLibChoices();
    InitializeFrom(site);
    site.ProcessMacros(configs);

    if (CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
        return;
    }
    _TRACE("*** Creating Makefile.third_party.mk files ***");
    // Write makefile uses to install 3-rd party dlls
    list<string> third_party_to_install;
    site.GetThirdPartyLibsToInstall(&third_party_to_install);
    // For static buid
    ITERATE(list<SConfigInfo>, p, configs) {
        const SConfigInfo& config = *p;
        s_CreateThirdPartyLibsInstallMakefile(site, 
                                              third_party_to_install, 
                                              config, 
                                              GetApp().GetBuildType());
    }
}

void CMsvcConfigure::CreateConfH(
    CMsvcSite& site,
    const list<SConfigInfo>& configs,
    const string& root_dir)
{
    CMsvc7RegSettings::EMsvcPlatform platform = CMsvc7RegSettings::GetMsvcPlatform();

    ITERATE(list<SConfigInfo>, p, configs) {
        WriteExtraDefines(site, root_dir, *p);
        // Windows and XCode only. On Unix build version header file is generated by configure, not PTB.
        if (platform != CMsvc7RegSettings::eUnix) {
            WriteBuildVer(site, root_dir, *p);
        }
    }
    if (platform == CMsvc7RegSettings::eUnix) {
        return;
    }
    _TRACE("*** Creating local ncbiconf headers ***");
    const CBuildType& build_type(GetApp().GetBuildType());
    ITERATE(list<SConfigInfo>, p, configs) {
        /*if (!p->m_VTuneAddon && !p->m_Unicode)*/ {
            AnalyzeDefines( site, root_dir, *p, build_type);
        }
    }
}


void CMsvcConfigure::InitializeFrom(const CMsvcSite& site)
{
    m_ConfigSite.clear();

    m_ConfigureDefinesPath = site.GetConfigureDefinesPath();
    if ( m_ConfigureDefinesPath.empty() ) {
        NCBI_THROW(CProjBulderAppException, 
           eConfigureDefinesPath,
           "Configure defines file name is not specified");
    }

    site.GetConfigureDefines(&m_ConfigureDefines);
    if( m_ConfigureDefines.empty() ) {
        PTB_ERROR(m_ConfigureDefinesPath,
                  "No configurable macro definitions specified.");
    } else {
        _TRACE("Configurable macro definitions: ");
        ITERATE(list<string>, p, m_ConfigureDefines) {
            _TRACE(*p);
        }
    }
}


bool CMsvcConfigure::ProcessDefine(const string& define, 
                                   const CMsvcSite& site, 
                                   const SConfigInfo& config) const
{
    if ( !site.IsDescribed(define) ) {
        PTB_ERROR_EX(kEmptyStr, ePTB_MacroUndefined,
                     define << ": Macro not defined");
        return false;
    }
    list<string> components;
    site.GetComponents(define, &components);
    ITERATE(list<string>, p, components) {
        const string& component = *p;
        if (site.IsBanned(component)) {
            PTB_WARNING_EX("", ePTB_ConfigurationError,
                            component << "|" << config.GetConfigFullName()
                            << ": " << define << " not provided, disabled");
            return false;
        }
        if (site.IsProvided( component, false)) {
            continue;
        }
        SLibInfo lib_info;
        site.GetLibInfo(component, config, &lib_info);
        if ( !site.IsLibOk(lib_info)  ||
             !site.IsLibEnabledInConfig(component, config)) {
            if (!lib_info.IsEmpty()) {
                PTB_WARNING_EX("", ePTB_ConfigurationError,
                               component << "|" << config.GetConfigFullName()
                               << ": " << define << " not satisfied, disabled");
            }
            return false;
        }
    }
    return true;
}


void CMsvcConfigure::WriteExtraDefines(CMsvcSite& site, const string& root_dir, const SConfigInfo& config)
{
    string cfg = CMsvc7RegSettings::GetConfigNameKeyword();
    string cfg_root_inc(root_dir);
    if (!cfg.empty()) {
        NStr::ReplaceInPlace(cfg_root_inc,cfg,config.GetConfigFullName());
    }
    string extra = site.GetConfigureEntry("ExtraDefines");
    string filename = CDirEntry::ConcatPath(cfg_root_inc, extra);
    int random_count = NStr::StringToInt(site.GetConfigureEntry("RandomValueCount"), NStr::fConvErr_NoThrow);

    if (extra.empty() || random_count <= 0 || CFile(filename).Exists()) {
        return;
    }

    string dir;
    CDirEntry::SplitPath(filename, &dir);
    CDir(dir).CreatePath();

    CNcbiOfstream ofs(filename.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs ) {
	    NCBI_THROW(CProjBulderAppException, eFileCreation, filename);
    }
    WriteNcbiconfHeader(ofs);

    CRandom rnd(CRandom::eGetRand_Sys);
    string prefix("#define NCBI_RANDOM_VALUE_");
    ofs << prefix << "TYPE   Uint4" << endl;
    ofs << prefix << "MIN    0" << endl;
    ofs << prefix << "MAX    "  << std::hex 
        << std::showbase << rnd.GetMax() << endl << endl;
    for (int i = 0; i < random_count; ++i) {
        ofs << prefix  << std::noshowbase << i 
            << setw(16) << std::showbase << rnd.GetRand() << endl;
    }
    ofs << endl;
}


#if defined(NCBI_OS_MSWIN)
#  if defined(_UNICODE)
#    define NcbiSys_strdup   _wcsdup
#  else
#    define NcbiSys_strdup   strdup
#  endif
#else
#  define NcbiSys_strdup     strdup
#endif

void CMsvcConfigure::WriteBuildVer(CMsvcSite& site, const string& root_dir, const SConfigInfo& config)
{
    static TXChar* filename_cache = NULL;
    
    string cfg = CMsvc7RegSettings::GetConfigNameKeyword();
    string cfg_root_inc(root_dir);
    if (!cfg.empty()) {
        NStr::ReplaceInPlace(cfg_root_inc, cfg, config.GetConfigFullName());
    }
    string extra = site.GetConfigureEntry("BuildVerPath");
    string filename = CDirEntry::ConcatPath(cfg_root_inc, extra);

    if (extra.empty()) {
        return;
    }

    string dir;
    CDirEntry::SplitPath(filename, &dir);
    CDir(dir).CreatePath();

    // Each file with build version information is the same for each configuration,
    // so create it once and copy to each configuration on next calls.

    if (filename_cache) {
        CFile(_T_STDSTRING(filename_cache)).Copy(filename, CFile::fCF_Overwrite);
        return;
    }

    // Create file

    CNcbiOfstream ofs(filename.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs ) {
	    NCBI_THROW(CProjBulderAppException, eFileCreation, filename);
    }
    WriteNcbiconfHeader(ofs);

    string tc_ver   = GetApp().GetEnvironment().Get("TEAMCITY_VERSION");
    string tc_build = GetApp().GetEnvironment().Get("BUILD_NUMBER");
    string tc_prj   = GetApp().GetEnvironment().Get("TEAMCITY_PROJECT_NAME");
    string tc_conf  = GetApp().GetEnvironment().Get("TEAMCITY_BUILDCONF_NAME");
    string sc_ver;
    string sc_rev;

    string prefix("#define NCBI_");

    if (!tc_ver.empty()  &&  !tc_build.empty()) {
        ofs << prefix << "TEAMCITY_BUILD_NUMBER    " << tc_build << endl;
        ofs << prefix << "TEAMCITY_PROJECT_NAME    " << "\"" << tc_prj  << "\"" << endl;
        ofs << prefix << "TEAMCITY_BUILDCONF_NAME  " << "\"" << tc_conf << "\"" << endl;
    }

    string tree_root = GetApp().GetEnvironment().Get("TREE_ROOT");

    if (!tree_root.empty()) {
        // Get SVN info
        string tmp_file = CFile::GetTmpName();
        string cmd = "svn info " + CDir::ConcatPath(tree_root, "compilers") + " >> " + tmp_file;

        TExitCode ret = CExec::System(cmd.c_str());
        if (ret == 0) {
            // SVN client present, parse results
            string info;
            CNcbiIfstream is(tmp_file.c_str(), IOS_BASE::in);
            NcbiStreamToString(&info, is);
            CFile(tmp_file).Remove();
            if (!info.empty()) {
                CRegexp re("Revision: (\\d+)");
                sc_rev = re.GetMatch(info, 0, 1);
                re.Set("/production/components/[^/]*/(\\d+)");
                sc_ver = re.GetMatch(info, 0, 1);
            }
        }
        else {
            // Fallback
            string sc_ver  = GetApp().GetEnvironment().Get("NCBI_SC_VERSION");
            // try to get revision bumber from teamcity property file
            string prop_file  = GetApp().GetEnvironment().Get("TEAMCITY_BUILD_PROPERTIES_FILE");
            if (!prop_file.empty() && CFile(prop_file).Exists()) {
                string info;
                CNcbiIfstream is(prop_file.c_str(), IOS_BASE::in);
                NcbiStreamToString(&info, is);
                if (!info.empty()) {
                    CRegexp re("build.vcs.number=(\\d+)");
                    sc_rev = re.GetMatch(info, 0, 1);
                }
            }
        }
        if (!sc_rev.empty()) {
            ofs << prefix << "SUBVERSION_REVISION      " << sc_rev << endl;
        }
        if (!sc_ver.empty()) {
            ofs << prefix << "SC_VERSION               " << sc_ver << endl;
        }
    }
    ofs << endl;

    // Cache file name for the generated file
    filename_cache = NcbiSys_strdup(_T_XCSTRING(filename));

    m_HaveBuildVer = true;
}


void CMsvcConfigure::AnalyzeDefines(
    CMsvcSite& site, const string& root_dir,
    const SConfigInfo& config, const CBuildType&  build_type)
{
    string cfg_root_inc = NStr::Replace(root_dir,
        CMsvc7RegSettings::GetConfigNameKeyword(),config.GetConfigFullName());
    string filename =
        CDirEntry::ConcatPath(cfg_root_inc, m_ConfigureDefinesPath);
    string dir;
    CDirEntry::SplitPath(filename, &dir);

    _TRACE("Configuration " << config.m_Name << ":");

    m_ConfigSite.clear();

    ITERATE(list<string>, p, m_ConfigureDefines) {
        const string& define = *p;
        if( ProcessDefine(define, site, config) ) {
            _TRACE("Macro definition Ok  " << define);
            m_ConfigSite[define] = '1';
        } else {
            PTB_WARNING_EX(kEmptyStr, ePTB_MacroUndefined,
                           "Macro definition not satisfied: " << define);
            m_ConfigSite[define] = '0';
        }
    }
    if (m_HaveBuildVer) {
        // Add define that we have build version info file
        m_ConfigSite["HAVE_COMMON_NCBI_BUILD_VER_H"] = '1';
    }

    string signature;
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        signature = "MSVC";
    } else if (CMsvc7RegSettings::GetMsvcPlatform() > CMsvc7RegSettings::eUnix) {
        signature = "XCODE";
    } else {
        signature = CMsvc7RegSettings::GetMsvcPlatformName();
    }
    signature += "_";
    signature += CMsvc7RegSettings::GetMsvcVersionName();
    signature += "-" + config.GetConfigFullName();
    if (config.m_rtType == SConfigInfo::rtMultiThreadedDLL ||
        config.m_rtType == SConfigInfo::rtMultiThreadedDebugDLL) {
        signature += "MT";
    }
#ifdef NCBI_XCODE_BUILD
    string tmp = CMsvc7RegSettings::GetRequestedArchs();
    NStr::ReplaceInPlace(tmp, " ", "_");
    signature += "_" + tmp;
    signature += "--";
    struct utsname u;
    if (uname(&u) == 0) {
//        signature += string(u.machine) + string("-apple-") + string(u.sysname) + string(u.release);
        signature +=
            GetApp().GetSite().GetPlatformInfo( u.sysname, "arch", u.machine) +
            string("-apple-") +
            GetApp().GetSite().GetPlatformInfo( u.sysname, "os", u.sysname) +
            string(u.release);
    } else {
        signature += HOST;
    }
    signature += "-";
    {
        char hostname[255];
        string tmp1, tmp2;
        if (0 == gethostname(hostname, 255))
            NStr::SplitInTwo(hostname,".", tmp1, tmp2);
            signature += tmp1;
    }
#else
    signature += "--";
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        signature += "i386-pc-";
        signature += CMsvc7RegSettings::GetRequestedArchs();
    } else {
        signature += HOST;
    }
    signature += "-";
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        signature += GetApp().GetEnvironment().Get("COMPUTERNAME");
    }
#endif

    string candidate_path = filename + ".candidate";
    CDirEntry::SplitPath(filename, &dir);
    CDir(dir).CreatePath();
    WriteNcbiconfMsvcSite(candidate_path, signature);
    if (PromoteIfDifferent(filename, candidate_path)) {
        PTB_WARNING_EX(filename, ePTB_FileModified,
                       "Configuration file modified");
    } else {
        PTB_INFO_EX(filename, ePTB_NoError,
                    "Configuration file unchanged");
    }
}

CNcbiOfstream& CMsvcConfigure::WriteNcbiconfHeader(CNcbiOfstream& ofs) const
{
    ofs <<"/* $" << "Id" << "$" << endl;
    ofs <<"* ===========================================================================" << endl;
    ofs <<"*" << endl;
    ofs <<"*                            PUBLIC DOMAIN NOTICE" << endl;
    ofs <<"*               National Center for Biotechnology Information" << endl;
    ofs <<"*" << endl;
    ofs <<"*  This software/database is a \"United States Government Work\" under the" << endl;
    ofs <<"*  terms of the United States Copyright Act.  It was written as part of" << endl;
    ofs <<"*  the author's official duties as a United States Government employee and" << endl;
    ofs <<"*  thus cannot be copyrighted.  This software/database is freely available" << endl;
    ofs <<"*  to the public for use. The National Library of Medicine and the U.S." << endl;
    ofs <<"*  Government have not placed any restriction on its use or reproduction." << endl;
    ofs <<"*" << endl;
    ofs <<"*  Although all reasonable efforts have been taken to ensure the accuracy" << endl;
    ofs <<"*  and reliability of the software and data, the NLM and the U.S." << endl;
    ofs <<"*  Government do not and cannot warrant the performance or results that" << endl;
    ofs <<"*  may be obtained by using this software or data. The NLM and the U.S." << endl;
    ofs <<"*  Government disclaim all warranties, express or implied, including" << endl;
    ofs <<"*  warranties of performance, merchantability or fitness for any particular" << endl;
    ofs <<"*  purpose." << endl;
    ofs <<"*" << endl;
    ofs <<"*  Please cite the author in any work or product based on this material." << endl;
    ofs <<"*" << endl;
    ofs <<"* ===========================================================================" << endl;
    ofs <<"*" << endl;
    ofs <<"* Author:  ......." << endl;
    ofs <<"*" << endl;
    ofs <<"* File Description:" << endl;
    ofs <<"*   ......." << endl;
    ofs <<"*" << endl;
    ofs <<"* ATTENTION:" << endl;
    ofs <<"*   Do not edit or commit this file into SVN as this file will" << endl;
    ofs <<"*   be overwritten (by PROJECT_TREE_BUILDER) without warning!" << endl;
    ofs <<"*/" << endl;
    ofs << endl;
    ofs << endl;
    return ofs;
}

void CMsvcConfigure::WriteNcbiconfMsvcSite(
    const string& full_path, const string& signature) const
{
    CNcbiOfstream  ofs(full_path.c_str(), 
                       IOS_BASE::out | IOS_BASE::trunc );
    if ( !ofs )
	    NCBI_THROW(CProjBulderAppException, eFileCreation, full_path);

    WriteNcbiconfHeader(ofs);

    ITERATE(TConfigSite, p, m_ConfigSite) {
        if (p->second == '1') {
            ofs << "#define " << p->first << " " << p->second << endl;
        } else {
            ofs << "/* #undef " << p->first << " */" << endl;
        }
    }
    ofs << endl;
    ofs << "#define NCBI_SIGNATURE \\" << endl << "  \"" << signature << "\"" << endl;

    list<string> customH;
    GetApp().GetCustomConfH(&customH);
    ITERATE(list<string>, c, customH) {
        string file (CDirEntry::CreateRelativePath(GetApp().m_Root, *c));
        NStr::ReplaceInPlace(file, "\\", "/");
        ofs << endl << "/*"
            << endl << "* ==========================================================================="
            << endl << "* Included contents of " << file
            << endl << "*/"
            << endl;

            CNcbiIfstream is(c->c_str(), IOS_BASE::in | IOS_BASE::binary);
            if ( !is ) {
                continue;
            }
            char   buf[1024];
            while ( is ) {
                is.read(buf, sizeof(buf));
                ofs.write(buf, is.gcount());
            }
    }
}


END_NCBI_SCOPE
