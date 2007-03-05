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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   CVersionInfo -- a version info storage class
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/version.hpp>


BEGIN_NCBI_SCOPE

const CVersionInfo CVersionInfo::kAny(0, 0, 0);
const CVersionInfo CVersionInfo::kLatest(-1, -1, -1);


CVersionInfo::CVersionInfo(void) 
    : m_Major(-1),
      m_Minor(-1),
      m_PatchLevel(-1),
      m_Name(kEmptyStr)

{
}

CVersionInfo::CVersionInfo(int ver_major,
                           int  ver_minor,
                           int  patch_level, 
                           const string& name) 
    : m_Major(ver_major),
      m_Minor(ver_minor),
      m_PatchLevel(patch_level),
      m_Name(name)

{
}


CVersionInfo::CVersionInfo(const string& version,
                           const string& name)
{
    FromStr(version);
    if (!name.empty()) {
        m_Name = name;
    }
}

static
void s_ConvertVersionInfo(CVersionInfo* vi, const char* str)
{
    int major, minor, patch = 0;
    if (!isdigit((unsigned char)(*str))) {
        NCBI_THROW2(CStringException, eFormat, "Invalid version format", 0);
    }
    major = atoi(str);
    if (major < 0) {
        NCBI_THROW2(CStringException, eFormat, "Invalid version format", 0);
    }
    for (; *str && isdigit((unsigned char)(*str)); ++str) {}
    if (*str != '.') {
        NCBI_THROW2(CStringException, eFormat, "Invalid version format", 0);
    }
    ++str;
    if (!isdigit((unsigned char)(*str))) {
        NCBI_THROW2(CStringException, eFormat, "Invalid version format", 0);
    }

    minor = atoi(str);
    if (minor < 0) {
        NCBI_THROW2(CStringException, eFormat, "Invalid version format", 0);
    }
    for (; *str && isdigit((unsigned char)(*str)); ++str) {}

    if (*str != 0) {
        if (*str != '.') {
            NCBI_THROW2(CStringException, eFormat, "Invalid version format", 0);
        }
        ++str;
        patch = atoi(str);
        if (patch < 0) {
            NCBI_THROW2(CStringException, eFormat, "Invalid version format", 0);
        }
    }

    vi->SetVersion(major, minor, patch);

}


void CVersionInfo::FromStr(const string& version)
{
    s_ConvertVersionInfo(this, version.c_str());
/*
    vector<string> lst;
    NStr::Tokenize(version, ".", lst, NStr::eNoMergeDelims);

    if (lst.size() == 0) {
        NCBI_THROW2(CStringException, eFormat,
                            "Invalid version format", 0);
    }

    for (unsigned i = 0; i < 3; ++i) {
        string tmp;
        if (i < lst.size()) {
            tmp = lst[i];
        }
        int value = tmp.empty() ? 0 : NStr::StringToInt(tmp);
        switch (i) {
        case 0: 
            if (value == 0) {
                NCBI_THROW2(CStringException, eFormat,
                            "Invalid version format (major is 0)", 0);
            }
            m_Major = value;
            break;
        case 1:
            m_Minor = value;
            break;
        case 2:
            m_PatchLevel = value;
            break;
        } 
    } // for
*/
}


CVersionInfo::CVersionInfo(const CVersionInfo& version)
    : m_Major(version.m_Major),
      m_Minor(version.m_Minor),
      m_PatchLevel(version.m_PatchLevel),
      m_Name(version.m_Name)
{
}

CVersionInfo& CVersionInfo::operator=(const CVersionInfo& version)
{
    m_Major = version.m_Major;
    m_Minor = version.m_Minor;
    m_PatchLevel = version.m_PatchLevel;
    return *this;
}


string CVersionInfo::Print(void) const
{
    CNcbiOstrstream os;
    os << m_Major << "." << m_Minor << "." << m_PatchLevel;
    if ( !m_Name.empty() ) {
        os << " (" << m_Name << ")";
    }
    return CNcbiOstrstreamToString(os);
}


CVersionInfo::EMatch 
CVersionInfo::Match(const CVersionInfo& version_info) const
{
    if (GetMajor() != version_info.GetMajor())
        return eNonCompatible;

    if (GetMinor() < version_info.GetMinor())
        return eNonCompatible;

    if (GetMinor() > version_info.GetMinor())
        return eBackwardCompatible;

    // Minor versions are equal.
    
    if (GetPatchLevel() == version_info.GetPatchLevel()) {
        return eFullyCompatible;
    }

    if (GetPatchLevel() > version_info.GetPatchLevel()) {
        return eBackwardCompatible;
    }

    return eConditionallyCompatible;

}

void CVersionInfo::SetVersion(int  ver_major,
                              int  ver_minor,
                              int  patch_level)
{
    m_Major      = ver_major;
    m_Minor      = ver_minor;
    m_PatchLevel = patch_level;
}



bool IsBetterVersion(const CVersionInfo& info, 
                     const CVersionInfo& cinfo,
                     int&  best_major, 
                     int&  best_minor,
                     int&  best_patch_level)
{
    int major = cinfo.GetMajor();
    int minor = cinfo.GetMinor();
    int patch_level = cinfo.GetPatchLevel();

    if (info.GetMajor() == -1) {  // best major search
        if (major > best_major) { 
            best_major = major;
            best_minor = minor;
            best_patch_level = patch_level;
            return true;
        }
    } else { // searching for the specific major version
        // Do not chose between major versions.
        // If they are not equal then they are not compatible.
        if (info.GetMajor() != major) {
            return false;
        }
    }

    if (info.GetMinor() == -1) {  // best minor search
        if (minor > best_minor) {
            best_major = major;
            best_minor = minor;
            best_patch_level = patch_level;
            return true;
        }
    } else { 
        if (info.GetMinor() > minor) {
            return false;
        }
        if (info.GetMinor() < minor) {
            best_major = major;
            best_minor = minor;
            best_patch_level = patch_level;
            return true;
        }
    }

    // Major and minor versions are equal.
    // always looking for the best patch
    if (patch_level > best_patch_level) {
            best_major = major;
            best_minor = minor;
            best_patch_level = patch_level;
            return true;
    }
    return false;    
}


void ParseVersionString(const string&  vstr, 
                        string*        program_name, 
                        CVersionInfo*  ver)
{
    _ASSERT(program_name);
    _ASSERT(ver);

    if (vstr.empty()) {
        NCBI_THROW2(CStringException, eFormat, "Version string is empty", 0);
    }

    program_name->erase();


    string lo_vstr(vstr); NStr::ToLower(lo_vstr);
    string::size_type pos;

    const char* vstr_str = vstr.c_str();

    // 2.3.4 ( program)

    pos = lo_vstr.find("(");
    if (pos != string::npos) {
        string::size_type pos2 = lo_vstr.find(")", pos);
        if (pos2 == string::npos) { // not found
            NCBI_THROW2(CStringException, 
                        eFormat, "Version string format error", 0);
        }
        for (++pos; pos < pos2; ++pos) {
            program_name->push_back(vstr.at(pos));
        }
        NStr::TruncateSpacesInPlace(*program_name);

        s_ConvertVersionInfo(ver, vstr.c_str());
        return;

    }


    // all other normal formats

    const char* version_pattern = "version";

    pos = lo_vstr.find(version_pattern);
    if (pos == string::npos) {
        version_pattern = "v.";
        pos = lo_vstr.find(version_pattern);

        if (pos == string::npos) {
            version_pattern = "ver";
            pos = lo_vstr.find(version_pattern);

            if (pos == string::npos) {
                version_pattern = "";
                // find the first space-digit and assume it's version
                const char* ch = vstr_str;
                for (; *ch; ++ch) {
                    if (isdigit((unsigned char)(*ch))) {
                        if (ch == vstr_str) {
                            // check if it's version
                            const char* ch2 = ch + 1;
                            for (;*ch2; ++ch2) {
                                if (!isdigit((unsigned char)(*ch2))) {
                                    break;
                                }
                            } // for
                            if (*ch2 == '.') {
                                pos = ch - vstr_str;
                                break;
                            } else {
                                continue;
                            }
                        } else {
                            if (isspace((unsigned char) ch[-1])) {
                                pos = ch - vstr_str;
                                break;
                            }
                        }
                    } // if digit
                    
                } // for
            }
        }
    }


    if (pos != string::npos) {
        int pname_end = (int)(pos - 1);
        for (; pname_end >= 0; --pname_end) {
            char ch = vstr[pname_end];
            if (!isspace((unsigned char) ch)) 
                break;
        } // for
        if (pname_end <= 0) {
        } else {
            program_name->append(vstr.c_str(), pname_end + 1);
        }

        pos += strlen(version_pattern);
        for(; pos < vstr.length(); ++pos) {
            char ch = vstr[pos];
            if (ch == '.') 
                continue;
            if (!isspace((unsigned char) ch)) 
                break;            
        } // for

        const char* ver_str = vstr_str + pos;
        s_ConvertVersionInfo(ver, ver_str);
        return;
    } else {
        *ver = CVersionInfo::kAny;
        *program_name = vstr;
        NStr::TruncateSpacesInPlace(*program_name);
        if (program_name->empty()) {
            NCBI_THROW2(CStringException, eFormat, "Version string is empty", 0);
        }
    }


}



END_NCBI_SCOPE
