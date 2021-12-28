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
 *   using the following specifications:
 *   'biblio.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>
#include <objects/general/Person_id.hpp>

// generated includes
#include <objects/biblio/Author.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CAuthor::~CAuthor()
{
}


bool CAuthor::GetLabelV1(string* label, TLabelFlags) const
{
    // XXX - honor flags?
    GetName().GetLabel(label);
    return true;
}


bool CAuthor::GetLabelV2(string* label, TLabelFlags flags) const
{
    const CPerson_id& pid = GetName();
    switch (pid.Which()) {
    case CPerson_id::e_Name:
    {
        const CName_std& name = pid.GetName();
        if (HasText(name.GetLast())) {
            return x_GetLabelV2
                (label, flags, name.GetLast(),
                 name.CanGetInitials() ? name.GetInitials() : kEmptyStr,
                 name.CanGetSuffix() ? name.GetSuffix() : kEmptyStr);
        } else if (name.IsSetFull()  &&  HasText(name.GetFull())) {
            return x_GetLabelV2(label, flags, name.GetFull());
        } else {
            return false;
        }
    }
    case CPerson_id::e_Ml:
        return x_GetLabelV2(label, flags, pid.GetMl());
    case CPerson_id::e_Str:
        return x_GetLabelV2(label, flags, pid.GetStr());
    case CPerson_id::e_Consortium:
        return x_GetLabelV2(label, flags, pid.GetConsortium());
    default:
        return false;
    }
}


bool CAuthor::x_GetLabelV2(string* label, TLabelFlags flags, CTempString name,
                           CTempString initials, CTempString suffix)
{
    if (name.empty()) {
        return false;
    }

    if (name.size() <= 6  &&  (SWNC(name, "et al")  ||  SWNC(name, "et,al"))) {
        name = "et al.";
        if (NStr::EndsWith(*label, " and ")) {
            label->replace(label->size() - 5, NPOS, ", ");
        }
    }

    SIZE_TYPE pos = label->size();
    *label += name;
    if (HasText(initials)) {
        *label += ',';
        *label += initials;
    }
    if (HasText(suffix)) {
        *label += ' ';
        *label += suffix;
    }

    if ((flags & fLabel_FlatEMBL) != 0) {
        NStr::ReplaceInPlace(*label, ",", " ", pos);
    }

    return true;
}

static string s_NormalizeInitials(const string& raw_initials)
{
    //
    //  Note:
    //  Periods _only_ after CAPs to avoid decorating hyphens (which _are_ 
    //  legal in the "initials" part.
    //
    string normal_initials;
    for (char ch : raw_initials) {
        normal_initials += ch;
        if (isupper(ch)) {
            normal_initials += '.';
        }
    }
    return normal_initials;
}

static string s_NormalizeSuffix(const string& raw_suffix)
{
    //
    //  Note: (2008-02-13) Suffixes I..VI no longer have trailing periods.
    //
    static const map<string, string> smap = {
                                {"1d",  "I"  },
                                {"1st", "I"  },
                                {"2d",  "II" },
                                {"2nd", "II" },
                                {"3d",  "III"},
                                {"3rd", "III"},
                                {"4th", "IV" },
                                {"5th", "V"  },
                                {"6th", "VI" },
                                {"Jr",  "Jr."},
                                {"Sr",  "Sr."} };

    auto search = smap.find(raw_suffix);
    if (search != smap.end()) {
        return search->second;
    }
    return raw_suffix;
}

void s_SplitMLAuthorName(string name, string& last, string& initials, string& suffix, bool normalize_suffix)
{
    NStr::TruncateSpacesInPlace(name);
    if (name.empty()) {
        return;
    }

    vector<string> parts;
    NStr::Split(name, " ", parts, NStr::fSplit_Tokenize);
    if (parts.empty()) {
        return;
    }
    if (parts.size() == 1) {
        //
        //  Designate the only part we have as the last name.
        //
        last = parts[0];
        return;
    }

    const string& last_part = parts[parts.size() - 1];

    if (parts.size() == 2) {
        //
        //  Designate the first part as the last name and the second part as the
        //  initials.
        //
        last = parts[0];
        initials = s_NormalizeInitials(last_part);
        return;
    }

    //
    //  At least three parts.
    //
    //  If the second to last part is all CAPs then those are the initials. The 
    //  last part is the suffix, and everything up to the initials is the last 
    //  name.
    //
    const string& second_to_last_part = parts[parts.size() - 2];

    if (NStr::IsUpper(second_to_last_part)) {
        last = NStr::Join(vector<string>(parts.begin(), parts.end() - 2), " ");
        initials = s_NormalizeInitials(second_to_last_part);

        suffix = normalize_suffix ? s_NormalizeSuffix(last_part) : last_part;
        return;
    }

    //
    //  Fall through:
    //  Guess that the last part is the initials and everything leading up to it 
    //  is a (rather unusual) last name.
    //
    last = NStr::Join(vector<string>(parts.begin(), parts.end() - 1), " ");
    initials = s_NormalizeInitials(last_part);
    return;

    //  ------------------------------------------------------------------------
    //  CASE NOT HANDLED:
    //
    //  (1) Initials with a blank in them. UNFIXABLE!
    //  (2) Initials with non CAPs in them. Probably fixable through a 
    //      white list of allowable exceptions. Tedious, better let the indexers
    //      fix it.
    //  ------------------------------------------------------------------------
}

CRef<CPerson_id> CAuthor::x_ConvertMlToStandard(const string& name, bool normalize_suffix)
{
    string last, initials, suffix;
    s_SplitMLAuthorName(name, last, initials, suffix, normalize_suffix);

    CRef<CPerson_id> person_id;
    if (!last.empty()) {

        person_id.Reset(new CPerson_id());
        person_id->SetName().SetLast(last);
        if (!initials.empty()) {
            person_id->SetName().SetInitials(initials);
        }
        if (!suffix.empty()) {
            person_id->SetName().SetSuffix(suffix);
        }
    }
    return person_id;
}


CRef<CAuthor> CAuthor::ConvertMlToStandard(const string& ml_name, bool normalize_suffix)
{
    CRef<CAuthor> new_author;
    if (!NStr::IsBlank(ml_name)) {
        new_author.Reset(new CAuthor());
        CRef<CPerson_id> std_name = x_ConvertMlToStandard(ml_name, normalize_suffix);
        new_author->SetName(*std_name);
    }
    return new_author;
}


CRef<CAuthor> CAuthor::ConvertMlToStandard(const CAuthor& author, bool normalize_suffix)
{
    CRef<CAuthor> new_author(new CAuthor());
    new_author->Assign(author);

    if (new_author->IsSetName() &&
        new_author->GetName().IsMl()) {
        const string ml_name = new_author->GetName().GetMl();
        CRef<CPerson_id> std_name = x_ConvertMlToStandard(ml_name, normalize_suffix);
        new_author->ResetName();
        new_author->SetName(*std_name);
    }
    return new_author;
}



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 65, chars: 1880, CRC32: f9047a4f */
