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
 *   using specifications from the data definition file
 *   'seqfeat.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/seqfeat/OrgName.hpp>

#include <objects/seqfeat/PartialOrgName.hpp>
#include <objects/seqfeat/TaxElement.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
COrgName::~COrgName(void)
{
}


bool COrgName::GetFlatName(string& name_out, string* lineage) const
{
    if (lineage  &&  lineage->empty()  &&  IsSetLineage()) {
        *lineage = GetLineage();
    }
    
    if ( !IsSetName() ) {
        return false;
    }

    const TName& name = GetName();
    name_out.clear();
    switch (name.Which()) {
    case C_Name::e_Namedhybrid: {
	const CBinomialOrgName& bin = name.GetNamedhybrid();
	name_out += bin.GetGenus();
	if( bin.IsSetSpecies() ) {
	    name_out += " x ";
	    name_out += bin.GetSpecies();
	}
	return true;
    }
    case C_Name::e_Binomial:  {
        const CBinomialOrgName& bin = name.GetBinomial();
        if (bin.IsSetSpecies()) {
	    string sSpecies = bin.GetSpecies();
	    if( sSpecies.find(" ") != string::npos &&
		!( NStr::StartsWith(sSpecies,"sp. ") ||
		   NStr::StartsWith(sSpecies,"species ") ) ) {
		// When species epithet is complex -- occurs when genus name doesn't coincide with beginning of species name
		name_out += sSpecies;
	    } else { // Simple case with single genus and species epithets
		if (bin.IsSetSubspecies() && !NStr::TruncateSpaces(bin.GetSubspecies()).empty()) {
		    string sSubspecies = NStr::TruncateSpaces(bin.GetSubspecies());
		    bool bSubspIndicator = NStr::StartsWith(sSubspecies, "subsp. ") ||
			NStr::StartsWith(sSubspecies, "ssp. ") ||
			NStr::StartsWith(sSubspecies, "subspecies ");
		    if( sSubspecies.find(" ") != string::npos && !bSubspIndicator ) { 
			// When subspecies epithet is complex -- occurs when species name doesn't coincide with beginning of subspecies name
			name_out += sSubspecies;
		    } else { // Simple case with single sub-species epithet
			name_out += bin.GetGenus();
			name_out += ' ' + bin.GetSpecies();
			if( !bSubspIndicator ) {
			    string sLineage = lineage ? *lineage : IsSetLineage() ? GetLineage() : NcbiEmptyString;
			    if( sLineage.find("Metazoa;") == string::npos ) {
				name_out += " subsp.";
			    }
			}
			name_out += ' ' + bin.GetSubspecies();
		    }
		} else { // No subspecies
		    name_out += bin.GetGenus();
		    name_out += ' ' + bin.GetSpecies();
		}
		if( IsSetMod() ) { // Add variety and forma to flat name -- those are valid classification categories
		    ITERATE (COrgName::TMod, it, GetMod()) {
			if( (*it)->GetSubtype() == COrgMod::eSubtype_variety ) {
			    name_out += " var. ";
			    name_out += (*it)->GetSubname();
			    break;
			}
		    }
		    ITERATE (COrgName::TMod, it, GetMod()) {
			if( (*it)->GetSubtype() == COrgMod::eSubtype_forma ) {
			    name_out += " f. ";
			    name_out += (*it)->GetSubname();
			    break;
			} else if( (*it)->GetSubtype() == COrgMod::eSubtype_forma_specialis ) {
			    name_out += " f.sp. ";
			    name_out += (*it)->GetSubname();
			    break;
			}
		    }
		}
	    }
        }
        return true;
    }

    case COrgName::C_Name::e_Virus:
        name_out = name.GetVirus();
        return true;

    case COrgName::C_Name::e_Hybrid:
    {
	string tmp;
	string delim;
	string sLineage = lineage ? *lineage : IsSetLineage() ? GetLineage() : NcbiEmptyString;
        ITERATE (CMultiOrgName::Tdata, it, name.GetHybrid().Get()) {
	    tmp.clear();
            if ((*it)->GetFlatName(tmp, &sLineage)) {
		name_out += delim;
		if( (*it)->IsSetName() && (*it)->GetName().IsHybrid() ) {
		    name_out += '(';
		    name_out += tmp;
		    name_out += ')';
		} else {
		    name_out += tmp;
		}
		delim = " x ";
            } else {
		return false;
	    }
        }
	return true;
    }

    case COrgName::C_Name::e_Partial:
    {
        string delim;
        ITERATE (CPartialOrgName::Tdata, it, name.GetPartial().Get()) {
            name_out += delim + (*it)->GetName();
            delim = " ";
        }
        return true;
    }
    
    default:
        return false;
    }
}

// The proposed format for orgname flags: flagname1[;flagname2]...[;flagnameN]
// where flagnameX consists of ascii alphanum characters only. Each value of flagnameX is unique.
// Presence of flag name in the strings means 'true' value for the flag.
const CTempString s_flagDelim = ";";

void COrgName::x_SetAttribFlag( const string& name )
{
    if( !x_GetAttribFlag( name ) ) {
	if( IsSetAttrib() && !GetAttrib().empty() ) {
	    SetAttrib().append(s_flagDelim).append(name);
	} else {
	    SetAttrib( name );
	}
    }
}

void COrgName::x_ResetAttribFlag( const string& name )
{
    if( !name.empty() && IsSetAttrib() ) {
        const string& attr = GetAttrib();
        list< CTempString > lVals;
        NStr::Split(attr, s_flagDelim, lVals, NStr::fSplit_Tokenize);
        for( list< CTempString >::iterator i = lVals.begin(), li = lVals.end(); i != li; ) {
            NStr::TruncateSpacesInPlace( *i );
            if( NStr::EqualNocase( *i, name ) ) {
                i = lVals.erase( i );
            } else {
                ++i;
            }
        }
        SetAttrib( NStr::Join( lVals, s_flagDelim ) );
        if( SetAttrib().empty() ) {
            ResetAttrib();
        }
    }
}

bool COrgName::x_GetAttribFlag( const string& name ) const
{
    if( !name.empty() && IsSetAttrib() ) {
	const string& attr = GetAttrib();
	list< CTempString > lVals;
	NStr::Split(attr, s_flagDelim, lVals, NStr::fSplit_Tokenize);
	NON_CONST_ITERATE( list< CTempString >, i, lVals ) {
	    NStr::TruncateSpacesInPlace( *i );
	    if( NStr::EqualNocase( *i, name ) ) {
		return true;
	    }
	}
    }
    return false;
}

// Flag indicating that node's scientific name is "well specified" according to the
// respective taxonomic nomenclature (e.g. Genus species subspecies). 
// Based on "specified" property from Taxonomy database
bool COrgName::IsFormalName() const
{
    return x_GetAttribFlag( "specified" );
}

void COrgName::SetFormalNameFlag( bool bFormalName )
{
    if( bFormalName ) {
	x_SetAttribFlag( "specified" );
    } else {
	x_ResetAttribFlag( "specified" );
    }
}

bool COrgName::IsUncultured() const
{
    return x_GetAttribFlag( "uncultured" );
}

void COrgName::SetUncultured( bool bUncultured )
{
    if( bUncultured ) {
        x_SetAttribFlag( "uncultured" );
    } else {
        x_ResetAttribFlag( "uncultured" );
    }
}

// Modifier forwarding flag. Used during org-ref lookup to enable/disable
// modifier forwarding. If set modifier forwarding is disabled.
// Flag is kept along with other flags in orgname.attrib field
// (see comments to x_SetAttribFlag() function)
bool COrgName::IsModifierForwardingDisabled() const
{
    return x_GetAttribFlag( "nomodforward" );
}

void COrgName::DisableModifierForwarding()
{
    x_SetAttribFlag( "nomodforward" );
}

void COrgName::EnableModifierForwarding()
{
    x_ResetAttribFlag( "nomodforward" );
}

#define MAKE_COMMON(o1,o2,o3,Field) if (o1.IsSet##Field() && o2.IsSet##Field() && NStr::Equal(o1.Get##Field(), o2.Get##Field())) o3.Set##Field(o1.Get##Field());
#define MAKE_COMMON_INT(o1,o2,o3,Field) if (o1.IsSet##Field() && o2.IsSet##Field() && o1.Get##Field() == o2.Get##Field()) o3.Set##Field(o1.Get##Field());

CRef<COrgName> COrgName::MakeCommon(const COrgName& other) const
{
    bool any = false;
    CRef<COrgName> common(new COrgName());

    // name
    if (IsSetName() && other.IsSetName() && GetName().Equals(other.GetName())) {
        common->SetName().Assign(GetName());
        any = true;
    }

    // mod
    if (IsSetMod() && other.IsSetMod()) {
        ITERATE(TMod, it1, GetMod()) {
            bool found = false;
            ITERATE(TMod, it2, other.GetMod()) {
                if ((*it1)->Equals(**it2)) {
                    found = true;
                }
            }
            if (found) {
                CRef<COrgMod> add(new COrgMod());
                add->Assign(**it1);
                common->SetMod().push_back(add);
                any = true;
            }
        }
    }

    MAKE_COMMON((*this), other, (*common), Attrib);
    MAKE_COMMON((*this), other, (*common), Lineage);
    MAKE_COMMON((*this), other, (*common), Div);
    MAKE_COMMON_INT((*this), other, (*common), Gcode);
    MAKE_COMMON_INT((*this), other, (*common), Mgcode);
    MAKE_COMMON_INT((*this), other, (*common), Pgcode);
    if (common->IsSetAttrib() || common->IsSetLineage() || common->IsSetDiv() ||
        common->IsSetGcode() || common->IsSetMgcode() || common->IsSetPgcode()) {
        any = true;
    }

    if (!any) {
        common.Reset(NULL);
    }
    return common;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 64, chars: 1877, CRC32: 38972243 */
