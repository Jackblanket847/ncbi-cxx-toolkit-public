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
* Author:  Colleen Bollin, NCBI
*
* File Description:
*   Unit tests for the validator.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "unit_test_validator.hpp"

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN

#define BAD_VALIDATOR

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

// for ignoring external config files
#include <util/util_misc.hpp>

#include <objects/biblio/Id_pat.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Ref_ext.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/taxon3/taxon3.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objtools/validator/validator.hpp>
#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/validerror_format.hpp>
#include <objtools/validator/translation_problems.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <corelib/ncbiapp.hpp>
#include <common/ncbi_export.h>
#include <objtools/unit_test_util/unit_test_util.hpp>
#include <objtools/edit/struc_comm_field.hpp>
#include <objtools/edit/cds_fix.hpp>

// for writing out tmp files
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>

extern const char* sc_TestEntryCollidingLocusTags;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using namespace validator;
using namespace unit_test_util;



CExpectedError::CExpectedError(string accession, EDiagSev severity, string err_code, string err_msg) 
: m_Accession (accession), m_Severity (severity), m_ErrCode(err_code), m_ErrMsg(err_msg)
{

}


CExpectedError::~CExpectedError()
{
}


bool CExpectedError::Match(const CValidErrItem& err_item, bool ignore_severity)
{
    if (!NStr::IsBlank(m_Accession) && !NStr::IsBlank(err_item.GetAccnver()) &&
        !NStr::Equal(err_item.GetAccnver(), m_Accession)) {
        return false;
    }
    if (!NStr::Equal(err_item.GetErrCode(), m_ErrCode)) {
        return false;
    }
    string msg = err_item.GetMsg();
    size_t pos = NStr::Find(msg, " EXCEPTION: NCBI C++ Exception:");
    if (pos != string::npos) {
        msg = msg.substr(0, pos);
    }

    if (!NStr::Equal(msg, m_ErrMsg)) {
        return false;
    }
    if (!ignore_severity && m_Severity != err_item.GetSeverity()) {
        return false;
    }
    return true;
}


void CExpectedError::Test(const CValidErrItem& err_item)
{
    if (!NStr::IsBlank (m_Accession) && !NStr::IsBlank (err_item.GetAccnver())) {
        BOOST_CHECK_EQUAL(err_item.GetAccnver(), m_Accession);
    }
    BOOST_CHECK_EQUAL(err_item.GetSeverity(), m_Severity);
    BOOST_CHECK_EQUAL(err_item.GetErrCode(), m_ErrCode);
    string msg = err_item.GetMsg();
    size_t pos = NStr::Find(msg, " EXCEPTION: NCBI C++ Exception:");
    if (pos != string::npos) {
        msg = msg.substr(0, pos);
    }
    BOOST_CHECK_EQUAL(msg, m_ErrMsg);
}


void CExpectedError::PrintSeenError(const CValidErrItem& err_item)
{
    string description = err_item.GetAccnver() + ":"
        + CValidErrItem::ConvertSeverity(err_item.GetSeverity()) + ":"
        + err_item.GetErrCode() + ":"
        + err_item.GetMsg();
    printf("%s\n", description.c_str());

}

static bool s_debugMode = false;

void WriteErrors(const CValidError& eval, bool debug_mode)
{
    if (debug_mode) {
        printf ("\n-\n");
    }
    for ( CValidError_CI vit(eval); vit; ++vit) {
        CExpectedError::PrintSeenError(*vit);
    }
    if (debug_mode) {
        printf ("\n\n");
    }
        printf ("\n\n");
}


void CheckErrors(const CValidError& eval,
                 vector< CExpectedError* >& expected_errors)
{
    bool   problem_found = false;

    if (s_debugMode) {
        WriteErrors (eval, true);
        return;
    }

    vector<bool> expected_found;
    for (size_t i = 0; i < expected_errors.size(); i++) {
        if (expected_errors[i]) {
            expected_found.push_back(false);
        } else {
            expected_found.push_back(true);
        }
    }

    for (CValidError_CI vit(eval); vit; ++vit) {
        bool found = false;
        for (size_t i = 0; i < expected_errors.size(); i++) {
            if (!expected_found[i] && expected_errors[i]->Match(*vit)) {
                expected_found[i] = true;
                found = true;
                break;
            }
        }
        if (!found) {
            for (size_t i = 0; i < expected_errors.size(); i++) {
                if (!expected_found[i] && expected_errors[i]->Match(*vit, true)) {
                    printf("Problem with ");
                    CExpectedError::PrintSeenError(*vit);
                    expected_errors[i]->Test(*vit);
                    expected_found[i] = true;
                    found = true;
                    problem_found = true;
                    break;
                }
            }
        }
        if (!found) {
            BOOST_CHECK_EQUAL("Unexpected error", "Error not found");
            CExpectedError::PrintSeenError(*vit);
            problem_found = true;
        }
    }

    for (size_t i = 0; i < expected_errors.size(); i++) {
        if (!expected_found[i]) {
            BOOST_CHECK_EQUAL(expected_errors[i]->GetErrMsg(), "Expected error not found");
            problem_found = true;
        }
    }

    if (problem_found) {
        WriteErrors (eval, false);
    }
}


void CheckStrings(vector<string>& seen, vector<string>& expected)
{
    vector<string>::iterator it1 = seen.begin();
    vector<string>::iterator it2 = expected.begin();
    bool any = false;
    while (it1 != seen.end() && it2 != expected.end()) {
        BOOST_CHECK_EQUAL(*it1, *it2);
        if (!NStr::Equal(*it1, *it2)) {
            any = true;
        }
        it1++;
        it2++;
    }
    while (it1 != seen.end()) {
        BOOST_CHECK_EQUAL(*it1, "Unexpected string");
        it1++;
        any = true;
    }
    while (it2 != expected.end()) {
        BOOST_CHECK_EQUAL("Missing string", *it2);
        it2++;
        any = true;
    }

    if (any) {
        printf("Seen:\n");
        vector<string>::iterator it1 = seen.begin();
        while (it1 != seen.end()) {
            printf("%s\n", (*it1).c_str());
            it1++;
        }
        printf("Expected:\n");
        vector<string>::iterator it2 = expected.begin();
        while (it2 != expected.end()) {
            printf("%s\n", (*it2).c_str());
            it2++;
        }
    }
}


// Not currently used, but I'll leave it here in case
// it's useful in the future.

//static void SetCountryOnSrc (CBioSource& src, string country) 
//{
//    if (NStr::IsBlank(country)) {
//        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, src) {
//            if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == CSubSource::eSubtype_country) {
//                ERASE_SUBSOURCE_ON_BIOSOURCE (it, src);
//            }
//        }
//    } else {
//        CRef<CSubSource> sub(new CSubSource(CSubSource::eSubtype_country, country));
//        src.SetSubtype().push_back(sub);
//    }
//}



END_SCOPE(objects)
END_NCBI_SCOPE

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_INIT_TREE()
{
    if ( !CNcbiApplication::Instance()->GetConfig().HasEntry("NCBI", "Data") ) {
        NCBITEST_DISABLE(Test_Descr_BadStructuredCommentFormat);
        NCBITEST_DISABLE(Test_Descr_MissingKeyword);
    }
}


static void SetErrorsAccessions (vector< CExpectedError *> & expected_errors, string accession)
{
    size_t i, len = expected_errors.size();
    for (i = 0; i < len; i++) {
        expected_errors[i]->SetAccession(accession);
    }
}

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddFlag( "debug_mode",
        "Debugging mode writes errors seen for each test" );
}

NCBITEST_AUTO_INIT()
{
    // initialization function body

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    if (args["debug_mode"]) {
        s_debugMode = true;
    }
    g_IgnoreDataFile("institution_codes.txt");
}


// new case test ground
BOOST_AUTO_TEST_CASE(Test_Descr_LatLonValue)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "USA");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "35 S 80 W");
    
    STANDARD_SETUP

    /*
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "LatLonValue",
                              "Latitude should be set to N (northern hemisphere)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    */

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "35 N 80 E");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "LatLonValue",
                              "Longitude should be set to W (western hemisphere)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "Madagascar");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "25 N 47 E");
    expected_errors[0]->SetErrMsg("Latitude should be set to S (southern hemisphere)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    /*
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "25 S 47 W");
    expected_errors[0]->SetErrMsg("Longitude should be set to E (eastern hemisphere)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    */

    CLEAR_ERRORS

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "15 N 47 E");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "Austria");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "LatLonValue",
                              "Latitude and longitude values appear to be exchanged"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_LatLonCountry)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "Romania");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "46.5 N 20 E");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "LatLonCountry",
                              "Lat_lon '46.5 N 20 E' maps to 'Hungary' instead of 'Romania' - claimed region 'Romania' is at distance 45 km"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "34 N 65 E");
    expected_errors[0]->SetErrCode("LatLonCountry");
    expected_errors[0]->SetErrMsg("Lat_lon '34 N 65 E' maps to 'Afghanistan' instead of 'Romania'");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "48 N 15 E");
    expected_errors[0]->SetErrMsg("Lat_lon '48 N 15 E' maps to 'Austria' instead of 'Romania'");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "48 N 15 W");
    expected_errors[0]->SetErrCode("LatLonWater");
    expected_errors[0]->SetErrMsg("Lat_lon '48 N 15 W' is in water 'Atlantic Ocean'");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "18.47 N 64.23000000000002 W");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "Puerto Rico: Rio Mameyes in Luquillo");
    expected_errors[0]->SetErrMsg("Lat_lon '18.47 N 64.23000000000002 W' is in water 'Caribbean Sea', 'Puerto Rico: Rio Mameyes in Luquillo' is 108 km away");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CValidErrorFormat format(*objmgr);
    vector<string> expected;
    expected.push_back("LatLonCountry Errors");
    expected.push_back("lcl|good:Lat_lon '18.47 N 64.23000000000002 W' is in water 'Caribbean Sea', 'Puerto Rico: Rio Mameyes in Luquillo' is 108 km away");
    expected.push_back("");
    vector<string> seen;
    vector<string> cat_list = format.FormatCompleteSubmitterReport(*eval, scope);
    ITERATE(vector<string>, it, cat_list) {
        vector<string> sublist;
        NStr::Split(*it, "\n", sublist, 0);
        ITERATE(vector<string>, sit, sublist) {
            seen.push_back(*sit);
        }
    }

    CheckStrings(seen, expected);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_ValidError_Format)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();

    // Create consensus splice problems
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(nuc->SetSeq().SetId().front()));
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[44] = 'A';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[45] = 'G';
    CRef<CSeq_feat> intron = unit_test_util::MakeIntronForMixLoc(nuc->SetSeq().SetId().front());
    unit_test_util::AddFeat(intron, nuc);

    CRef<CSeq_feat> other_intron = unit_test_util::AddMiscFeature(nuc);
    other_intron->SetData().SetImp().SetKey("intron");

    // create EC number problems
    CRef<CSeq_feat> prot = entry->GetSet().GetSeq_set().back()->GetAnnot().front()->GetData().GetFtable().front();
    prot->SetData().SetProt().SetEc().push_back("1.2.3.10");
    prot->SetData().SetProt().SetEc().push_back("1.1.3.22");
    prot->SetData().SetProt().SetEc().push_back("1.1.99.n");
    prot->SetData().SetProt().SetEc().push_back("1.1.1.17");
    prot->SetData().SetProt().SetEc().push_back("11.22.33.44");
    prot->SetData().SetProt().SetEc().push_back("11.22.n33.44");
    prot->SetData().SetProt().SetEc().push_back("11.22.33.n44");


    // create bad institution code errors
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "XXX:foo");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "YYY:foo");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "ZZZ:foo");

    // create lat-lon country error
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "30 N 30 E");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "Panama");

    STANDARD_SETUP

        eval = validator.Validate(seh, options);

    CValidErrorFormat format(*objmgr);

    vector<string> expected;
    expected.push_back("intron\tlcl|nuc\tGT at 17");
    expected.push_back("intron\tlcl|nuc\tGT at 1");
    expected.push_back("intron\tlcl|nuc\tAG at 11");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("CDS\tlcl|nuc\tGT at 16");
    expected.push_back("lcl|nuc:Lat_lon '30 N 30 E' maps to 'Egypt' instead of 'Panama'");
    expected.push_back("lcl|nuc\tXXX;YYY;ZZZ");
    expected.push_back("lcl|nuc\tXXX;YYY;ZZZ");
    expected.push_back("lcl|nuc\tXXX;YYY;ZZZ");

    vector<string> seen;
    for (CValidError_CI vit(*eval); vit; ++vit) {
        string val = format.FormatForSubmitterReport(*vit, scope);
        seen.push_back(val);
    }
    CheckStrings(seen, expected);

    expected.clear();
    seen.clear();
    for (CValidError_CI vit(*eval); vit; ++vit) {
        seen.push_back(vit->GetErrCode());
    }
    expected.push_back("NotSpliceConsensusDonor");
    expected.push_back("NotSpliceConsensusDonorTerminalIntron");
    expected.push_back("NotSpliceConsensusAcceptor");
    expected.push_back("DeletedEcNumber");
    expected.push_back("ReplacedEcNumber");
    expected.push_back("BadEcNumberValue");
    expected.push_back("BadEcNumberFormat");
    expected.push_back("BadEcNumberValue");
    expected.push_back("NotSpliceConsensusDonor");
    expected.push_back("LatLonCountry");
    expected.push_back("BadInstitutionCode");
    expected.push_back("BadInstitutionCode");
    expected.push_back("BadInstitutionCode");
    CheckStrings(seen, expected);

    seen.clear();
    expected.clear();
    vector<unsigned int> codes = format.GetListOfErrorCodes(*eval);
    ITERATE(vector<unsigned int>, it, codes) {
        string val = CValidErrItem::ConvertErrCode(*it);
        seen.push_back(val);
    }
    expected.push_back("LatLonCountry");
    expected.push_back("BadInstitutionCode");
    expected.push_back("BadEcNumberFormat");
    expected.push_back("BadEcNumberValue");
    expected.push_back("NotSpliceConsensusDonor");
    expected.push_back("NotSpliceConsensusAcceptor");
    expected.push_back("DeletedEcNumber");
    expected.push_back("ReplacedEcNumber");
    expected.push_back("NotSpliceConsensusDonorTerminalIntron");
    CheckStrings(seen, expected);

    string rval = format.FormatForSubmitterReport(*eval, scope, eErr_SEQ_FEAT_NotSpliceConsensusDonor);
    expected.clear();
    seen.clear();
    NStr::Split(rval, "\n", seen, 0);
    expected.push_back("Not Splice Consensus");
    expected.push_back("intron\tlcl|nuc\tGT at 17");
    expected.push_back("CDS\tlcl|nuc\tGT at 16");
    expected.push_back("");
    CheckStrings(seen, expected);

    rval = format.FormatCategoryForSubmitterReport(*eval, scope, eSubmitterFormatErrorGroup_ConsensusSplice);
    expected.clear();
    seen.clear();
    NStr::Split(rval, "\n", seen, 0);
    expected.push_back("Not Splice Consensus");
    expected.push_back("intron\tlcl|nuc\tGT at 17");
    expected.push_back("intron\tlcl|nuc\tGT at 1");
    expected.push_back("intron\tlcl|nuc\tAG at 11");
    expected.push_back("CDS\tlcl|nuc\tGT at 16");
    expected.push_back("");
    CheckStrings(seen, expected);

    expected.clear();
    seen.clear();
    vector<string> cat_list = format.FormatCompleteSubmitterReport(*eval, scope);
    ITERATE(vector<string>, it, cat_list) {
        vector<string> sublist;
        NStr::Split(*it, "\n", sublist, 0);
        ITERATE(vector<string>, sit, sublist) {
            seen.push_back(*sit);
        }
    }
    expected.push_back("Not Splice Consensus");
    expected.push_back("intron\tlcl|nuc\tGT at 17");
    expected.push_back("intron\tlcl|nuc\tGT at 1");
    expected.push_back("intron\tlcl|nuc\tAG at 11");
    expected.push_back("CDS\tlcl|nuc\tGT at 16");
    expected.push_back("");
    expected.push_back("EC Number Format");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("");
    expected.push_back("EC Number Value");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("lcl|prot\t1.2.3.10;1.1.3.22;1.1.99.n;1.1.1.17;11.22.33.44;11.22.n33.44;11.22.33.n44\t\tfake protein name");
    expected.push_back("");
    expected.push_back("Bad Institution Codes");
    expected.push_back("lcl|nuc\tXXX;YYY;ZZZ");
    expected.push_back("lcl|nuc\tXXX;YYY;ZZZ");
    expected.push_back("lcl|nuc\tXXX;YYY;ZZZ");
    expected.push_back("");
    expected.push_back("LatLonCountry Errors");
    expected.push_back("lcl|nuc:Lat_lon '30 N 30 E' maps to 'Egypt' instead of 'Panama'");
    expected.push_back("");
    CheckStrings(seen, expected);

}


BOOST_AUTO_TEST_CASE(Test_GB_6395)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxon(entry, 0);

    STANDARD_SETUP

    eval = validator.Validate(seh, options);

    CValidErrorFormat format(*objmgr);
    vector<string> expected;
    vector<string> seen;

    vector<string> cat_list = format.FormatCompleteSubmitterReport(*eval, scope);
    ITERATE(vector<string>, it, cat_list) {
        vector<string> sublist;
        NStr::Split(*it, "\n", sublist, 0);
        ITERATE(vector<string>, sit, sublist) {
            seen.push_back(*sit);
        }
    }
    expected.push_back("NoTaxonID");
    expected.push_back("lcl|good:Sebaea microphylla");
    expected.push_back("");

    CheckStrings(seen, expected);
}


BOOST_AUTO_TEST_CASE(Test_Descr_LatLonState)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "USA: South Carolina");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "36 N 80 W");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "LatLonState",
    "Lat_lon '36 N 80 W' maps to 'USA: North Carolina' instead of 'USA: South Carolina' - claimed region 'USA: South Carolina' is at distance 130 km"));
    options |= CValidator::eVal_latlon_check_state;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


CRef<CSeq_entry> s_BuildBadEcNumberEntry()
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet ();
    CRef<CSeq_feat> prot = entry->GetSet().GetSeq_set().back()->GetAnnot().front()->GetData().GetFtable().front();
    prot->SetData().SetProt().SetEc().push_back("1.2.3.10");
    prot->SetData().SetProt().SetEc().push_back("1.1.3.22");
    prot->SetData().SetProt().SetEc().push_back("1.1.99.n");
    prot->SetData().SetProt().SetEc().push_back("1.1.1.17");
    prot->SetData().SetProt().SetEc().push_back("11.22.33.44");
    prot->SetData().SetProt().SetEc().push_back("11.22.n33.44");
    prot->SetData().SetProt().SetEc().push_back("11.22.33.n44");
    return entry;
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadEcNumberValue)
{
    CRef<CSeq_entry> entry = s_BuildBadEcNumberEntry ();
    CRef<CSeq_feat> prot = entry->GetSet().GetSeq_set().back()->GetAnnot().front()->GetData().GetFtable().front();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "DeletedEcNumber", 
                      "EC_number 1.2.3.10 was deleted"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "ReplacedEcNumber", 
                      "EC_number 1.1.3.22 was transferred and is no longer valid"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "BadEcNumberValue", 
                      "11.22.33.44 is not a legal value for qualifier EC_number"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "BadEcNumberFormat", 
                      "11.22.n33.44 is not in proper EC_number format"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Info, "BadEcNumberValue", 
                      "11.22.33.n44 is not a legal preliminary value for qualifier EC_number"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    prot->SetData().SetProt().ResetEc();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature (entry->GetSet().GetSeq_set().front());
    misc->SetData().SetImp().SetKey("exon");
    misc->AddQualifier("EC_number", "1.2.3.10");
    misc->AddQualifier("EC_number", "1.1.3.22");
    misc->AddQualifier("EC_number", "1.1.99.n");
    misc->AddQualifier("EC_number", "1.1.1.17");
    misc->AddQualifier("EC_number", "11.22.33.44");
    misc->AddQualifier("EC_number", "11.22.n33.44");
    misc->AddQualifier("EC_number", "11.22.33.n44");
    SetErrorsAccessions(expected_errors, "lcl|nuc");
    expected_errors[1]->SetErrMsg("EC_number 1.1.3.22 was replaced");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_InvalidQualifierValue)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature (entry);
    misc->SetData().SetImp().SetKey("repeat_region");
    misc->AddQualifier("rpt_unit_seq", "ATA");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "RepeatSeqDoNotMatch", 
                      "repeat_region /rpt_unit and underlying sequence do not match"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    misc = unit_test_util::AddMiscFeature(entry);
    misc->SetData().SetImp().SetKey("repeat_region");
    misc->AddQualifier("rpt_unit_seq", "ATAGTGATAGTG");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrCode("InvalidRepeatUnitLength");
    expected_errors[0]->SetErrMsg("Length of rpt_unit_seq is greater than feature length");
    expected_errors[0]->SetSeverity(eDiag_Info);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ExtNotAllowed)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "ExtNotAllowed", "Bioseq-ext not allowed on virtual Bioseq"));

    // repr = virtual
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetExt().SetDelta();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = raw
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGG");
    expected_errors[0]->SetErrMsg("Bioseq-ext not allowed on raw Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetExt();
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetErrCode("SeqDataNotFound");
    expected_errors[0]->SetErrMsg("Missing Seq-data on raw Bioseq");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetGap();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = const
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_const);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGG");
    entry->SetSeq().SetInst().SetExt().SetDelta();
    expected_errors[0]->SetErrCode("ExtNotAllowed");
    expected_errors[0]->SetErrMsg("Bioseq-ext not allowed on constructed Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetExt();
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetErrCode("SeqDataNotFound");
    expected_errors[0]->SetErrMsg("Missing Seq-data on constructed Bioseq");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetGap();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = map
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_map);
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetErrCode("ExtBadOrMissing");
    expected_errors[0]->SetErrMsg("Missing or incorrect Bioseq-ext on map Bioseq");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().SetDelta();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().SetRef();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().SetMap();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGG");
    expected_errors[0]->SetErrCode("SeqDataNotAllowed");
    expected_errors[0]->SetErrMsg("Seq-data not allowed on map Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // repr = ref
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_ref);
    entry->SetSeq().SetInst().ResetExt();
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetErrCode("ExtBadOrMissing");
    expected_errors[0]->SetErrMsg("Missing or incorrect Bioseq-ext on reference Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = seg
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_seg);
    expected_errors[0]->SetErrMsg("Missing or incorrect Bioseq-ext on seg Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = consen
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_consen);
    expected_errors[0]->SetSeverity(eDiag_Critical);
    expected_errors[0]->SetErrCode("ReprInvalid");
    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 6");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = notset
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_not_set);
    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 0");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = other
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_other);
    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 255");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = delta
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGG");
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[0]->SetErrCode("SeqDataNotAllowed");
    expected_errors[0]->SetErrMsg("Seq-data not allowed on delta Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetExt();
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[0]->SetErrCode("ExtBadOrMissing");
    expected_errors[0]->SetErrMsg("Missing or incorrect Bioseq-ext on delta Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ReprInvalid)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "ReprInvalid", "Invalid Bioseq->repr = 0"));

    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_not_set);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 255");
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 6");
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_consen);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_CollidingLocusTags)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    {{
         CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
         istr >> MSerial_AsnText >> *entry;
     }}

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
                          | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
                          | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Warning, "TerminalNs", "N at end of sequence"));
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Warning, "GeneLocusCollidesWithLocusTag", "locus collides with locus_tag in another gene"));
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Warning, "CollidingGeneNames", "Colliding names in gene features"));
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Error, "CollidingLocusTags", "Colliding locus_tags in gene features"));
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Error, "CollidingLocusTags", "Colliding locus_tags in gene features"));
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Error, "NoMolInfoFound", "No Mol-info applies to this Bioseq"));
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Error, "LocusTagGeneLocusMatch", "Gene locus and locus_tag 'foo' match"));
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Error, "NoPubFound", "No publications anywhere on this entire record."));
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Info, "MissingPubRequirement", "No submission citation anywhere on this entire record."));
    expected_errors.push_back(new CExpectedError("lcl|LocusCollidesWithLocusTag", eDiag_Error, "NoOrgFound", "No organism name anywhere on this entire record."));

    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


const char* sc_TestEntryCollidingLocusTags ="Seq-entry ::= seq {\
    id {\
      local str \"LocusCollidesWithLocusTag\" } ,\
    inst {\
      repr raw ,\
      mol dna ,\
      length 24 ,\
      seq-data\
        iupacna \"AATTGGCCAANNAATTGGCCAANN\" } ,\
    annot {\
      {\
        data\
          ftable {\
            {\
              data\
                gene {\
                  locus \"foo\" ,\
                  locus-tag \"foo\" } ,\
              location\
                int {\
                  from 0 ,\
                  to 4 ,\
                  strand plus ,\
                  id\
                    local str \"LocusCollidesWithLocusTag\" } } ,\
            {\
              data\
                gene {\
                  locus \"bar\" ,\
                  locus-tag \"foo\" } ,\
              location\
                int {\
                  from 5 ,\
                  to 9 ,\
                  strand plus ,\
                  id\
                    local str \"LocusCollidesWithLocusTag\" } } ,\
            {\
              data\
                gene {\
                  locus \"bar\" ,\
                  locus-tag \"baz\" } ,\
              location\
                int {\
                  from 10 ,\
                  to 14 ,\
                  strand plus ,\
                  id\
                    local str \"LocusCollidesWithLocusTag\" } } ,\
            {\
              data\
                gene {\
                  locus \"quux\" ,\
                  locus-tag \"baz\" } ,\
              location\
                int {\
                  from 15 ,\
                  to 19 ,\
                  strand plus ,\
                  id\
                    local str \"LocusCollidesWithLocusTag\" } } } } } }\
";


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_CircularProtein)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodProtSeq();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "CircularProtein", "Non-linear topology set on protein"));

    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_complete);

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_tandem);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // should be no error for not set or linear
    CLEAR_ERRORS

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_not_set);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_linear);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_DSProtein)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodProtSeq();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "DSProtein", "Protein not single stranded"));

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_ds);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_mixed);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no errors expected for not set or single strand
    CLEAR_ERRORS

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_not_set);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_ss);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_MolNotSet)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MolNotSet", "Bioseq.mol is 0"));

    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_not_set);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrCode("MolOther");
    expected_errors[0]->SetErrMsg("Bioseq.mol is type other");
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrCode("MolNuclAcid");
    expected_errors[0]->SetErrMsg("Bioseq.mol is type na");
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_na);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_FuzzyLen)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "FuzzyLen", "Fuzzy length on raw Bioseq"));

    entry->SetSeq().SetInst().SetFuzz();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Fuzzy length on const Bioseq");
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_const);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // shouldn't get fuzzy length if gap
    expected_errors[0]->SetErrCode("SeqDataNotFound");
    expected_errors[0]->SetErrMsg("Missing Seq-data on constructed Bioseq");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    entry->SetSeq().SetInst().SetSeq_data().SetGap();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_InvalidAlphabet)
{
    CRef<CSeq_entry> prot_entry = unit_test_util::BuildGoodProtSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle prot_seh = scope.AddTopLevelSeqEntry(*prot_entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
                          | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
                          | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidAlphabet", "Using a nucleic acid alphabet on a protein sequence"));
    prot_entry->SetSeq().SetInst().SetSeq_data().SetIupacna();
    CConstRef<CValidError> eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    prot_entry->SetSeq().SetInst().SetSeq_data().SetNcbi2na();
    eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    prot_entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na();
    eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    prot_entry->SetSeq().SetInst().SetSeq_data().SetNcbi8na();
    eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    prot_entry->SetSeq().SetInst().SetSeq_data().SetNcbipna();
    eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CScope scope2(*objmgr);
    scope2.AddDefaults();
    CSeq_entry_Handle seh = scope2.AddTopLevelSeqEntry(*entry);

    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa();
    expected_errors[0]->SetErrMsg("Using a protein alphabet on a nucleic acid");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbi8aa();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbieaa();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbipaa();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbistdaa();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_InvalidResidue)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP

    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFB');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFB');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFB');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFC');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFC');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFC');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFD');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFD');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFD');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFE');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFE');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFF');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('\xFF');
    entry->SetSeq().SetInst().SetLength(65);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'E' at position [5]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'F' at position [6]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'I' at position [9]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'J' at position [10]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'L' at position [12]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'O' at position [15]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'P' at position [16]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'Q' at position [17]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'U' at position [21]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'X' at position [24]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'Z' at position [26]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'E' at position [31]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'F' at position [32]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'I' at position [35]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'J' at position [36]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'L' at position [38]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'O' at position [41]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'P' at position [42]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'Q' at position [43]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'U' at position [47]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'X' at position [50]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'Z' at position [52]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [53]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [54]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [55]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [252] at position [56]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [252] at position [57]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [252] at position [58]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [253] at position [59]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [253] at position [60]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [253] at position [61]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [254] at position [62]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "More than 10 invalid residues. Checking stopped"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "NonAsciiAsn", "Non-ASCII character '251' found in item"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now repeat test, but with mRNA - this time Us should not be reported
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    delete expected_errors[8];
    expected_errors[8] = NULL;
    delete expected_errors[19];
    expected_errors[19] = NULL;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now repeat test, but with protein
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
        }
    }
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFB');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFB');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFB');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFC');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFC');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFC');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFD');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFD');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFD');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFE');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFE');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFF');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back('\xFF');
    entry->SetSeq().SetInst().SetLength(65);
    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetProt().SetName().push_back("fake protein name");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(64);
    unit_test_util::AddFeat(feat, entry);
    scope.RemoveEntry (*entry);
    seh = scope.AddTopLevelSeqEntry(*entry);

    for (int j = 0; j < 22; j++) {
        if (expected_errors[j] != NULL) {
            delete expected_errors[j];
            expected_errors[j] = NULL;
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // now look for lowercase characters
    scope.RemoveEntry (*entry);
    entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("abcdefghijklmnopqrstuvwxyz");
    entry->SetSeq().SetInst().SetLength(26);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Sequence contains lower-case characters"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveEntry (*entry);
    entry = unit_test_util::BuildGoodProtSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("protein");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    CLEAR_ERRORS

    // now try delta sequence
    scope.RemoveEntry (*entry);
    entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().ResetSeq_data();
    CRef<CDelta_seq> seg(new CDelta_seq());
    seg->SetLiteral().SetSeq_data().SetIupacna().Set("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
    seg->SetLiteral().SetLength(52);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(seg);
    entry->SetSeq().SetInst().SetLength(52);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [E] at position [5]"));    
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [F] at position [6]"));    
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [I] at position [9]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [J] at position [10]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [L] at position [12]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [O] at position [15]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [P] at position [16]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [Q] at position [17]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [U] at position [21]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [X] at position [24]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [Z] at position [26]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [E] at position [31]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [F] at position [32]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [I] at position [35]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [J] at position [36]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [L] at position [38]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [O] at position [41]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [P] at position [42]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [Q] at position [43]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [U] at position [47]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [X] at position [50]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [Z] at position [52]"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // try protein delta sequence
    scope.RemoveEntry (*entry);
    entry = unit_test_util::BuildGoodProtSeq();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().ResetSeq_data();
    CRef<CDelta_seq> seg2(new CDelta_seq());
    seg2->SetLiteral().SetSeq_data().SetIupacaa().Set("1234567");
    seg2->SetLiteral().SetLength(7);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(seg2);
    entry->SetSeq().SetInst().SetLength(7);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [1] at position [1]"));    
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [2] at position [2]"));    
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [3] at position [3]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [4] at position [4]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [5] at position [5]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [6] at position [6]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue [7] at position [7]"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static void WriteOutTemp (CRef<CSeq_entry> entry)
{
    // construct a temp file name
    CNcbiOstrstream oss;
    oss << "test.asn";
    string filename = CNcbiOstrstreamToString(oss);
    string fullPath = CDirEntry::MakePath(".", filename);

    // initialize a binary output stream
    auto_ptr<CNcbiOstream> outStream;
    outStream.reset(new CNcbiOfstream(
        fullPath.c_str(),
        IOS_BASE::out));
    if (!(*outStream)) {
        return;
    }

    auto_ptr<CObjectOStream> outObject;
    // Associate ASN.1 text serialization methods with the input
    outObject.reset(new CObjectOStreamAsn(*outStream));

    // write the asn data
    try {
        *outObject << *entry;
        outStream->flush();
    } catch (exception& ) {
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_StopInProtein)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();

    STANDARD_SETUP

    entry->SetSet().SetSeq_set().back()->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MP*K*E*N");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("GTGCCCTAAAAATAAGAGTAAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetExcept(true);
    cds->SetExcept_text("unclassified translation discrepancy");

    BOOST_CHECK_EQUAL(validator::HasStopInProtein(*cds, scope), true);
    BOOST_CHECK_EQUAL(validator::HasInternalStop(*cds, scope, false), true);

    // list of expected errors
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "StopInProtein", "[3] termination symbols in protein sequence (gene? - fake protein name)"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "ExceptionProblem", "unclassified translation discrepancy is not a legal exception explanation"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "InternalStop", "3 internal stops (and illegal start codon). Genetic code [0]"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    WriteOutTemp(entry);

    CLEAR_ERRORS
    cds->ResetExcept();
    cds->ResetExcept_text();
    BOOST_CHECK_EQUAL(validator::HasStopInProtein(*cds, scope), true);
    BOOST_CHECK_EQUAL(validator::HasInternalStop(*cds, scope, false), true);
    BOOST_CHECK_EQUAL(validator::HasBadStartCodon(*cds, scope, false), true);

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "StopInProtein", "[3] termination symbols in protein sequence (gene? - fake protein name)"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "StartCodon", "Illegal start codon (and 3 internal stops). Probably wrong genetic code [0]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "InternalStop", "3 internal stops (and illegal start codon). Genetic code [0]"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    WriteOutTemp(entry);

    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCTAAAAATAAGAGTAAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");

    // write out seq-entry
    WriteOutTemp(entry);

    delete expected_errors[1];
    expected_errors[1] = NULL;
    expected_errors[2]->SetErrMsg("3 internal stops. Genetic code [0]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_PartialInconsistent)
{
#if 0
    //We don't care about segmented sets any more
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodProtSeq();

    STANDARD_SETUP

    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_seg);
    CRef<CSeq_id> id(new CSeq_id("gb|AY123456"));
    CRef<CSeq_loc> loc1(new CSeq_loc(*id, 0, 3));
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc1);
    CRef<CSeq_id> id2(new CSeq_id("gb|AY123457"));
    CRef<CSeq_loc> loc2(new CSeq_loc(*id2, 0, 2));
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc2);

    // list of expected errors
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "PartialInconsistent", "Partial segmented sequence without MolInfo partial"));

    // not-set
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // unknown
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_unknown);

    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // complete
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_complete);

    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // partial
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_partial);

    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    expected_errors[0]->SetErrMsg("Complete segmented sequence with MolInfo partial");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no-left
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_no_left);

    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    expected_errors[0]->SetErrMsg("No-left inconsistent with segmented SeqLoc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no-right
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_no_right);

    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    expected_errors[0]->SetErrMsg("No-right inconsistent with segmented SeqLoc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no-ends
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_no_ends);

    expected_errors[0]->SetErrMsg("No-ends inconsistent with segmented SeqLoc");
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
#endif
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ShortSeq)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodProtSeq();

    STANDARD_SETUP

    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPR");
    entry->SetSeq().SetInst().SetLength(3);
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetTo(2);

    // don't report if pdb
    CRef<CPDB_seq_id> pdb_id(new CPDB_seq_id());
    pdb_id->SetMol().Set("foo");
    entry->SetSeq().SetId().front()->SetPdb(*pdb_id);
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetPdb(*pdb_id);
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // new test if no coding region
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PartialsInconsistent", "Molinfo completeness and protein feature partials conflict"));

    entry->SetSeq().SetId().front()->SetLocal().SetStr("good");
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_partial);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_no_left);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_no_right);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_no_ends);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // for all other completeness, report
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ShortSeq", "Sequence only 3 residues"));
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().ResetCompleteness();
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_unknown);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_complete);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // nucleotide
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    seh = scope.AddTopLevelSeqEntry(*entry);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCTTT");
    entry->SetSeq().SetInst().SetLength(9);
    expected_errors[0]->SetErrMsg("Sequence only 9 residues");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    
    // don't report if pdb
    entry->SetSeq().SetId().front()->SetPdb(*pdb_id);
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static bool IsProteinTech (CMolInfo::TTech tech)
{
    bool rval = false;

    switch (tech) {
         case CMolInfo::eTech_concept_trans:
         case CMolInfo::eTech_seq_pept:
         case CMolInfo::eTech_both:
         case CMolInfo::eTech_seq_pept_overlap:
         case CMolInfo::eTech_seq_pept_homol:
         case CMolInfo::eTech_concept_trans_a:
             rval = true;
             break;
         default:
             break;
    }
    return rval;
}


static void AddRefGeneTrackingUserObject(CRef<CSeq_entry> entry)
{
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().SetObjectType(CUser_object::eObjectType_RefGeneTracking);
    desc->SetUser().SetRefGeneTrackingStatus(CUser_object::eRefGeneTrackingStatus_INFERRED);
    if (entry->IsSeq()) {
        entry->SetSeq().SetDescr().Set().push_back(desc);
    } else if (entry->IsSet()) {
        entry->SetSet().SetDescr().Set().push_back(desc);
    }
}


static void SetRefGeneTrackingStatus(CRef<CSeq_entry> entry, string status)
{
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsUser() && (*it)->GetUser().IsRefGeneTracking()) {
                (*it)->SetUser().SetData().front()->SetData().SetStr(status);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsUser() && (*it)->GetUser().IsRefGeneTracking()) {
                (*it)->SetUser().SetData().front()->SetData().SetStr(status);
            }
        }
    }
}


static void SetTitle(CRef<CSeq_entry> entry, string title) 
{
    bool found = false;

    EDIT_EACH_DESCRIPTOR_ON_SEQENTRY (it, *entry) {
        if ((*it)->IsTitle()) {
            if (NStr::IsBlank((*it)->GetTitle())) {
                ERASE_DESCRIPTOR_ON_SEQENTRY (it, *entry);
            } else {
                (*it)->SetTitle(title);
            }
            found = true;
        }
    }
    if (!found && !NStr::IsBlank(title)) {
        CRef<CSeqdesc> desc(new CSeqdesc());
        desc->SetTitle(title);
        entry->SetSeq().SetDescr().Set().push_back(desc);
    }
}


static void AddGenbankKeyword (CRef<CSeq_entry> entry, string keyword)
{
    bool found = false;

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsGenbank()) {
            (*it)->SetGenbank().SetKeywords().push_back(keyword);
            found = true;
        }
    }
    if (!found) {
        CRef<CSeqdesc> desc(new CSeqdesc());
        desc->SetGenbank().SetKeywords().push_back(keyword);
        entry->SetSeq().SetDescr().Set().push_back(desc);
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_BadDeltaSeq)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();

    STANDARD_SETUP

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetTech(CMolInfo::eTech_derived);
        }
    }

    // don't report if NT or NC
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NT_123456");
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // don't report if gen-prod-set

    entry->SetSeq().SetId().front()->SetLocal().SetStr("good");
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);

    // allowed tech values
    vector<CMolInfo::TTech> allowed_list;
    allowed_list.push_back(CMolInfo::eTech_htgs_0);
    allowed_list.push_back(CMolInfo::eTech_htgs_1);
    allowed_list.push_back(CMolInfo::eTech_htgs_2);
    allowed_list.push_back(CMolInfo::eTech_htgs_3);
    allowed_list.push_back(CMolInfo::eTech_wgs);
    allowed_list.push_back(CMolInfo::eTech_composite_wgs_htgs); 
    allowed_list.push_back(CMolInfo::eTech_unknown);
    allowed_list.push_back(CMolInfo::eTech_standard);
    allowed_list.push_back(CMolInfo::eTech_htc);
    allowed_list.push_back(CMolInfo::eTech_barcode);
    allowed_list.push_back(CMolInfo::eTech_tsa);

    for (CMolInfo::TTech i = CMolInfo::eTech_unknown;
         i <= CMolInfo::eTech_tsa;
         i++) {
         bool allowed = false;
         for (vector<CMolInfo::TTech>::iterator it = allowed_list.begin();
              it != allowed_list.end() && !allowed;
              ++it) {
              if (*it == i) {
                  allowed = true;
              }
         }
         if (allowed) {
             // don't report for htgs_0
             SetTech(entry, i);
             eval = validator.Validate(seh, options);
             if (i == CMolInfo::eTech_barcode) {
                 expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadKeyword", "Molinfo.tech barcode without BARCODE keyword"));
             } else if (i == CMolInfo::eTech_tsa) {
                 expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "SeqGapProblem", "TSA Seq_gap NULL"));
                 expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ConflictingBiomolTech", "TSA sequence should not be DNA"));
                 expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "WrongBiomolForTechnique", "Biomol \"genomic\" is not appropriate for sequences that use the TSA technique."));
                 expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "SeqGapProblem", "TSA submission includes wrong gap type. Gaps for TSA should be Assembly Gaps with linkage evidence."));
             } else if (i == CMolInfo::eTech_wgs) {
                 expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "SeqGapProblem", "WGS submission includes wrong gap type. Gaps for WGS genomes should be Assembly Gaps with linkage evidence."));
             }
             CheckErrors (*eval, expected_errors);
             if (i == CMolInfo::eTech_barcode || i == CMolInfo::eTech_tsa || i == CMolInfo::eTech_wgs) {
                 CLEAR_ERRORS
             }
         }
    }

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadDeltaSeq", "Delta seq technique should not be [1]"));
    for (CMolInfo::TTech i = CMolInfo::eTech_unknown;
         i <= CMolInfo::eTech_tsa;
         i++) {
         bool allowed = false;
         for (vector<CMolInfo::TTech>::iterator it = allowed_list.begin();
              it != allowed_list.end() && !allowed;
              ++it) {
              if (*it == i) {
                  allowed = true;
              }
         }
         if (!allowed) {
             // report
             SetTech(entry, i);
             if (IsProteinTech(i)) {
                 if (expected_errors.size() < 2) {
                     expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType", "Nucleic acid with protein sequence method"));
                 }
             } else {
                 if (expected_errors.size() > 1) {
                     delete expected_errors[1];
                     expected_errors.pop_back();
                 }
                 if (i == CMolInfo::eTech_est) {
                     expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ConflictingBiomolTech", "EST sequence should be mRNA"));
                 }
             }
             expected_errors[0]->SetErrMsg("Delta seq technique should not be [" + NStr::UIntToString(i) + "]");
             eval = validator.Validate(seh, options);
             CheckErrors (*eval, expected_errors);
         }
    }   

    CLEAR_ERRORS

    CRef<CDelta_seq> start_gap_seg(new CDelta_seq());
    start_gap_seg->SetLiteral().SetLength(10);
    start_gap_seg->SetLiteral().SetSeq_data().SetGap();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().insert(entry->SetSeq().SetInst().SetExt().SetDelta().Set().begin(), start_gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("AAATTTGGGC", CSeq_inst::eMol_dna);
    CRef<CDelta_seq> end_gap_seg(new CDelta_seq());
    end_gap_seg->SetLiteral().SetLength(10);
    end_gap_seg->SetLiteral().SetSeq_data().SetGap();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(end_gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral(10);
    entry->SetSeq().SetInst().SetLength(94);
    SetTech(entry, CMolInfo::eTech_wgs);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadDeltaSeq", "First delta seq component is a gap"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadDeltaSeq", "There is 1 adjacent gap in delta seq"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadDeltaSeq", "Last delta seq component is a gap"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "SeqGapProblem", "WGS submission includes wrong gap type. Gaps for WGS genomes should be Assembly Gaps with linkage evidence."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    delete expected_errors[3];
    expected_errors.pop_back();
    SetTech(entry, CMolInfo::eTech_htgs_0);
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[2]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ConflictingIdsOnBioseq)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP_NO_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (lcl|good - lcl|bad)"));

    // local IDs
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> id2(new CSeq_id());
    id2->SetLocal().SetStr("bad");
    entry->SetSeq().SetId().push_back(id2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // GIBBSQ
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> id1 = entry->SetSeq().SetId().front();
    id1->SetGibbsq(1);
    id2->SetGibbsq(2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("bbs|1");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (bbs|1 - bbs|2)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // GIBBSQ
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGibbmt(1);
    id2->SetGibbmt(2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("bbm|1");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (bbm|1 - bbm|2)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // GI
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGi(GI_CONST(1));
    id2->SetGi(GI_CONST(2));
    CRef<CSeq_id> id3(new CSeq_id("gb|AY123456.1"));
    entry->SetSeq().SetId().push_back (id3);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("gb|AY123456.1|");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gi|1 - gi|2)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    entry->SetSeq().SetId().pop_back();

    // GIIM
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGiim().SetId(1);
    id1->SetGiim().SetDb("foo");
    id2->SetGiim().SetId(2);
    id2->SetGiim().SetDb("foo");
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("gim|1", eDiag_Error, "IdOnMultipleBioseqs", "BioseqFind (gim|1) unable to find itself - possible internal error"));
    expected_errors.push_back(new CExpectedError("gim|1", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gim|1 - gim|2)"));
    expected_errors.push_back(new CExpectedError("gim|1", eDiag_Error, "IdOnMultipleBioseqs", "BioseqFind (gim|2) unable to find itself - possible internal error"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    // patent
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetPatent().SetSeqid(1);
    id1->SetPatent().SetCit().SetCountry("USA");
    id1->SetPatent().SetCit().SetId().SetNumber("1");
    id2->SetPatent().SetSeqid(2);
    id2->SetPatent().SetCit().SetCountry("USA");
    id2->SetPatent().SetCit().SetId().SetNumber("2");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("pat|USA|1|1", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (pat|USA|1|1 - pat|USA|2|2)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // pdb
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetPdb().SetMol().Set("good");
    id2->SetPdb().SetMol().Set("badd");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("pdb|good| ");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (pdb|good|  - pdb|badd| )");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // general
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGeneral().SetDb("a");
    id1->SetGeneral().SetTag().SetStr("good");
    id2->SetGeneral().SetDb("a");
    id2->SetGeneral().SetTag().SetStr("bad");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("gnl|a|good");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gnl|a|good - gnl|a|bad)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // should get no error if db values are different
    scope.RemoveTopLevelSeqEntry(seh);
    id2->SetGeneral().SetDb("b");
    seh = scope.AddTopLevelSeqEntry(*entry);
    delete expected_errors[0];
    expected_errors.clear();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // genbank
    scope.RemoveTopLevelSeqEntry(seh);
    expected_errors.push_back(new CExpectedError("gb|AY123456|", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456| - gb|AY222222|)"));
    id1->SetGenbank().SetAccession("AY123456");
    id2->SetGenbank().SetAccession("AY222222");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try genbank with accession same, versions different
    scope.RemoveTopLevelSeqEntry(seh);
    id2->SetGenbank().SetAccession("AY123456");
    id2->SetGenbank().SetVersion(2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("gb|AY123456.2|", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456| - gb|AY123456.2|)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try similar id type
    scope.RemoveTopLevelSeqEntry(seh);
    id2->SetGpipe().SetAccession("AY123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("gb|AY123456|", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456| - gpp|AY123456|)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // LRG
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGeneral().SetDb("LRG");
    id1->SetGeneral().SetTag().SetStr("good");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("gpp|AY123456|");
    expected_errors[0]->SetErrMsg("LRG sequence needs NG_ accession");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // no error if has NG
    scope.RemoveTopLevelSeqEntry(seh);
    id2->SetOther().SetAccession("NG_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
   
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_MolNuclAcid)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MolNuclAcid", "Bioseq.mol is type na"));

    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_na);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ConflictingBiomolTech)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP

    // allowed tech values
    vector<CMolInfo::TTech> genomic_list;
    genomic_list.push_back(CMolInfo::eTech_sts);
    genomic_list.push_back(CMolInfo::eTech_survey);
    genomic_list.push_back(CMolInfo::eTech_wgs);
    genomic_list.push_back(CMolInfo::eTech_htgs_0);
    genomic_list.push_back(CMolInfo::eTech_htgs_1);
    genomic_list.push_back(CMolInfo::eTech_htgs_2);
    genomic_list.push_back(CMolInfo::eTech_htgs_3);
    genomic_list.push_back(CMolInfo::eTech_composite_wgs_htgs);

    for (CMolInfo::TTech i = CMolInfo::eTech_unknown;
         i <= CMolInfo::eTech_tsa;
        i++) {
        bool genomic = false;
        for (vector<CMolInfo::TTech>::iterator it = genomic_list.begin();
              it != genomic_list.end() && !genomic;
              ++it) {
            if (*it == i) {
                genomic = true;
            }
        }
        entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
        SetTech (entry, i);
        unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_cRNA);
        expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InconsistentMolType", "Molecule type (DNA) does not match biomol (RNA)"));
        if (i == CMolInfo::eTech_est) {
            expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ConflictingBiomolTech", "EST sequence should be mRNA"));
        }
        if (i == CMolInfo::eTech_htgs_2) {
            expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadHTGSeq", "HTGS 2 raw seq has no gaps and no graphs"));
        }
        if (genomic) {
            expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ConflictingBiomolTech", "HTGS/STS/GSS/WGS sequence should be genomic"));            
            eval = validator.Validate(seh, options);
            CheckErrors (*eval, expected_errors);
            unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_genomic);
            entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
            delete expected_errors[0];
            expected_errors[0] = NULL;
            expected_errors.back()->SetErrMsg("HTGS/STS/GSS/WGS sequence should not be RNA");
            eval = validator.Validate(seh, options);
            CheckErrors (*eval, expected_errors);
        } else {
            if (IsProteinTech(i)) {
                expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType", "Nucleic acid with protein sequence method"));
            }
            if (i == CMolInfo::eTech_barcode) {
                expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadKeyword", "Molinfo.tech barcode without BARCODE keyword"));
            } else if (i == CMolInfo::eTech_tsa) {
                expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ConflictingBiomolTech", "TSA sequence should not be DNA"));            
                expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "WrongBiomolForTechnique", "Biomol \"cRNA\" is not appropriate for sequences that use the TSA technique."));
            }
            eval = validator.Validate(seh, options);
            CheckErrors (*eval, expected_errors);
        }
        CLEAR_ERRORS
    }

    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
    SetTech (entry, CMolInfo::eTech_tsa);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InconsistentMolType", "Molecule type (DNA) does not match biomol (RNA)"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ConflictingBiomolTech", "TSA sequence should not be DNA"));            
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "WrongBiomolForTechnique", "Biomol \"cRNA\" is not appropriate for sequences that use the TSA technique."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ConflictingBiomolTech", "TSA sequence should not be DNA"));  
    eval = validator.GetTSAConflictingBiomolTechErrors(seh);
    CheckErrors (*eval, expected_errors);
    eval = validator.GetTSAConflictingBiomolTechErrors(*(seh.GetSeq().GetCompleteBioseq()));
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_SeqIdNameHasSpace)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    entry->SetSeq().SetId().front()->SetOther().SetName("good one");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("ref|NC_123456|good one", eDiag_Critical, "SeqIdNameHasSpace", "Seq-id.name 'good one' should be a single word without any spaces"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_DuplicateSegmentReferences)
{
#if 0
    // removed per VR-779
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_seg);
    CRef<CSeq_loc> seg1 (new CSeq_loc());
    seg1->SetWhole().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(seg1);
    CRef<CSeq_loc> seg2 (new CSeq_loc());
    seg2->SetWhole().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(seg2);
    entry->SetSeq().SetInst().SetLength(970);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    // need to call this statement before calling AddDefaults 
    // to make sure that we can fetch the sequence referenced by the
    // delta sequence so that we can detect that the loc in the
    // delta sequence is longer than the referenced sequence
    CGBDataLoader::RegisterInObjectManager(*objmgr);
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
                          | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
                          | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "SeqLocOrder", "Segmented BioseqIntervals out of order in SeqLoc [[gb|AY123456|, gb|AY123456|]]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "DuplicateSegmentReferences", "Segmented sequence has multiple references to gb|AY123456"));
    CConstRef<CValidError> eval;

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    seg2->SetInt().SetId().SetGenbank().SetAccession("AY123456");
    seg2->SetInt().SetFrom(0);
    seg2->SetInt().SetTo(484);
    expected_errors[0]->SetErrMsg("Segmented BioseqIntervals out of order in SeqLoc [[gb|AY123456|, 1-485]]");
    expected_errors[1]->SetSeverity(eDiag_Warning);
    expected_errors[1]->SetErrMsg("Segmented sequence has multiple references to gb|AY123456 that are not SEQLOC_WHOLE");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
#endif
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_TrailingX)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> prot = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat> prot_feat = prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    CRef<CSeq_feat> cds_feat = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATANNNNNN");
    nuc->SetSeq().SetInst().SetLength(27);
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEIXX");
    prot->SetSeq().SetInst().SetLength(9);
    unit_test_util::SetCompleteness (prot, CMolInfo::eCompleteness_no_right);
    prot_feat->SetLocation().SetInt().SetTo(8);
    prot_feat->SetLocation().SetPartialStop(true, eExtreme_Biological);
    prot_feat->SetPartial(true);
    cds_feat->SetLocation().SetPartialStop(true, eExtreme_Biological);
    cds_feat->SetPartial(true);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "TerminalNs", "N at end of sequence"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "TrailingX", "Sequence ends in 2 trailing Xs"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_BadSeqIdFormat)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc_entry = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> prot_entry = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat> prot_feat = prot_entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("",eDiag_Error, "BadSeqIdFormat", "Bad accession"));

    vector<string> bad_ids;
    bad_ids.push_back("AY123456ABC");  // can't have letters after digits
    bad_ids.push_back("A1234");        // for a single letter, only acceptable number of digits is 5
    bad_ids.push_back("A123456");
    bad_ids.push_back("AY12345");      // for two letters, only acceptable number of digits is 6
    bad_ids.push_back("AY1234567");
    bad_ids.push_back("ABC1234");      // three letters bad unless prot and 5 digits
    bad_ids.push_back("ABC123456");
    bad_ids.push_back("ABCD1234567");  // four letters
    bad_ids.push_back("ABCDE123456");  // five letters
    bad_ids.push_back("ABCDE12345678");  


    vector<string> bad_nuc_ids;
    bad_nuc_ids.push_back("ABC12345");

    vector<string> bad_prot_ids;
    bad_prot_ids.push_back("AY123456");
    bad_prot_ids.push_back("A12345");

    vector<string> good_ids;

    vector<string> good_nuc_ids;
    good_nuc_ids.push_back("AY123456");
    good_nuc_ids.push_back("A12345");
    good_nuc_ids.push_back("ABCD123456789");
    good_nuc_ids.push_back("ABCD1234567890");

    vector<string> good_prot_ids;
    good_prot_ids.push_back("ABC12345");


    CRef<CSeq_id> good_nuc_id(new CSeq_id());
    good_nuc_id->SetLocal().SetStr("nuc");
    CRef<CSeq_id> good_prot_id(new CSeq_id());
    good_prot_id->SetLocal().SetStr("prot");

    CRef<CSeq_id> bad_id(new CSeq_id());

    // bad for both
    for (vector<string>::iterator id_it = bad_ids.begin();
        id_it != bad_ids.end();
        ++id_it) {
        string id_str = *id_it;
        expected_errors[0]->SetAccession("gb|" + id_str + "|");
        expected_errors[0]->SetErrMsg("Bad accession " + id_str);

        if (id_str.length() == 12 || id_str.length() == 13 || id_str.length() == 14 || id_str.length() == 15) {
            expected_errors.push_back(new CExpectedError("gb|" + id_str + "|", eDiag_Error, "InconsistentMolInfoTechnique", "WGS accession should have Mol-info.tech of wgs"));
        }

        //GenBank
        scope.RemoveTopLevelSeqEntry(seh);
        scope.ResetDataAndHistory();
        bad_id->SetGenbank().SetAccession(id_str);
        unit_test_util::ChangeNucId(entry, bad_id);
        unit_test_util::ChangeProtId(entry, good_prot_id);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors(*eval, expected_errors);
        scope.RemoveTopLevelSeqEntry(seh);
        scope.ResetDataAndHistory();
        unit_test_util::ChangeNucId(entry, good_nuc_id);
        unit_test_util::ChangeProtId(entry, bad_id);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors(*eval, expected_errors);

        if (expected_errors.size() > 1) {
            delete expected_errors[1];
            expected_errors.pop_back();
        }

    }

    for (vector<string>::iterator id_it = bad_ids.begin();
        id_it != bad_ids.end();
        ++id_it) {
        string id_str = *id_it;
        id_str = "B" + id_str.substr(1);
        expected_errors[0]->SetAccession("embl|" + id_str + "|");
        expected_errors[0]->SetErrMsg("Bad accession " + id_str);

        if (id_str.length() == 12 || id_str.length() == 13 || id_str.length() == 14 || id_str.length() == 15) {
            expected_errors.push_back(new CExpectedError("embl|" + id_str + "|", eDiag_Error, "InconsistentMolInfoTechnique", "WGS accession should have Mol-info.tech of wgs"));
        }

        // EMBL
        scope.RemoveTopLevelSeqEntry(seh);
        scope.ResetDataAndHistory();
        bad_id->SetEmbl().SetAccession(id_str);
        unit_test_util::ChangeNucId(entry, bad_id);
        unit_test_util::ChangeProtId(entry, good_prot_id);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        expected_errors[0]->SetAccession("emb|" + id_str + "|");
        if (expected_errors.size() > 1) {
            expected_errors[1]->SetAccession("emb|" + id_str + "|");
        }
        CheckErrors (*eval, expected_errors);
        scope.RemoveTopLevelSeqEntry(seh);
        scope.ResetDataAndHistory();
        unit_test_util::ChangeNucId(entry, good_nuc_id);
        unit_test_util::ChangeProtId(entry, bad_id);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        if (expected_errors.size() > 1) {
            delete expected_errors[1];
            expected_errors.pop_back();
        }

    }

    for (vector<string>::iterator id_it = bad_ids.begin();
        id_it != bad_ids.end();
        ++id_it) {
        string id_str = *id_it;
        id_str = "C" + id_str.substr(1);
        expected_errors[0]->SetAccession("dbj|" + id_str + "|");
        expected_errors[0]->SetErrMsg("Bad accession " + id_str);

        if (id_str.length() == 12 || id_str.length() == 13 || id_str.length() == 14 || id_str.length() == 15) {
            expected_errors.push_back(new CExpectedError("dbj|" + id_str + "|", eDiag_Error, "InconsistentMolInfoTechnique", "WGS accession should have Mol-info.tech of wgs"));
        }

        // DDBJ
        scope.RemoveTopLevelSeqEntry(seh);
        scope.ResetDataAndHistory();
        bad_id->SetDdbj().SetAccession(id_str);
        unit_test_util::ChangeNucId(entry, bad_id);
        unit_test_util::ChangeProtId(entry, good_prot_id);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        expected_errors[0]->SetAccession("dbj|" + id_str + "|");
        if (expected_errors.size() > 1) {
            expected_errors[1]->SetAccession("dbj|" + id_str + "|");
        }
        CheckErrors (*eval, expected_errors);
        scope.RemoveTopLevelSeqEntry(seh);
        scope.ResetDataAndHistory();
        unit_test_util::ChangeNucId(entry, good_nuc_id);
        unit_test_util::ChangeProtId(entry, bad_id);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        if (expected_errors.size() > 1) {
            delete expected_errors[1];
            expected_errors.pop_back();
        }

    }

    // bad for just nucs
    for (vector<string>::iterator id_it = bad_nuc_ids.begin();
         id_it != bad_nuc_ids.end();
         ++id_it) {
        string id_str = *id_it;
        bad_id->SetGenbank().SetAccession(id_str);
        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeNucId(entry, bad_id);
        unit_test_util::ChangeProtId(entry, good_prot_id);
        expected_errors[0]->SetAccession("gb|"+id_str+"|");
        expected_errors[0]->SetErrMsg("Bad accession " + id_str);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    // bad for just prots
    for (vector<string>::iterator id_it = bad_prot_ids.begin();
         id_it != bad_prot_ids.end();
         ++id_it) {
        string id_str = *id_it;
        bad_id->SetGenbank().SetAccession(id_str);
        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeNucId(entry, good_nuc_id);
        unit_test_util::ChangeProtId(entry, bad_id);
        expected_errors[0]->SetAccession("gb|"+id_str+"|");
        expected_errors[0]->SetErrMsg("Bad accession " + id_str);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    CLEAR_ERRORS

    // good for both
    for (vector<string>::iterator id_it = good_ids.begin();
         id_it != good_ids.end();
         ++id_it) {
        string id_str = *id_it;
        bad_id->SetGenbank().SetAccession(id_str);
        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeNucId(entry, bad_id);
        unit_test_util::ChangeProtId(entry, good_prot_id);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeNucId(entry, good_nuc_id);
        unit_test_util::ChangeProtId(entry, bad_id);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

    }

    // good for nucs
    for (vector<string>::iterator id_it = good_nuc_ids.begin();
         id_it != good_nuc_ids.end();
         ++id_it) {
        string id_str = *id_it;
        bad_id->SetGenbank().SetAccession(id_str);
        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeNucId(entry, bad_id);
        unit_test_util::ChangeProtId(entry, good_prot_id);
        if (id_str.length() == 12 ||id_str.length() == 13 ||id_str.length() == 14 ||id_str.length() == 15) {
            SetTech (entry->SetSet().SetSeq_set().front(), CMolInfo::eTech_wgs);
        }
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        SetTech(entry->SetSet().SetSeq_set().front(), CMolInfo::eTech_unknown);
    }

    // good for just prots
    for (vector<string>::iterator id_it = good_prot_ids.begin();
         id_it != good_prot_ids.end();
         ++id_it) {
        string id_str = *id_it;
        bad_id->SetGenbank().SetAccession(id_str);
        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeNucId(entry, good_nuc_id);
        unit_test_util::ChangeProtId(entry, bad_id);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    // if GI, needs version
    scope.RemoveTopLevelSeqEntry(seh);
    bad_id->SetGenbank().SetAccession("AY123456");
    bad_id->SetGenbank().SetVersion(0);
    unit_test_util::ChangeNucId(entry, bad_id);
    unit_test_util::ChangeProtId(entry, good_prot_id);
    CRef<CSeq_id> gi_id(new CSeq_id("gi|21914627"));
    nuc_entry->SetSeq().SetId().push_back(gi_id);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors.push_back (new CExpectedError ("gb|AY123456|", eDiag_Critical, "BadSeqIdFormat", 
                                                   "Accession AY123456 has 0 version"));
    expected_errors.push_back (new CExpectedError ("gb|AY123456|", eDiag_Warning, "UnexpectedIdentifierChange", "New accession (gb|AY123456|) does not match one in NCBI sequence repository (gb|AY123456.1|) on gi (21914627)"));
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    nuc_entry->SetSeq().SetId().pop_back();

    // id that is too long
    scope.RemoveTopLevelSeqEntry(seh);
    bad_id->SetLocal().SetStr("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345678901234");
    unit_test_util::ChangeNucId(entry, bad_id);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // shouldn't report if ncbifile ID
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> ncbifile(new CSeq_id("gnl|NCBIFILE|ABCDEFGHIJKLMNOPQRSTUVWXYZ012345678901234"));
    unit_test_util::ChangeNucId(entry, good_nuc_id);
    nuc_entry->SetSeq().SetId().push_back(ncbifile);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    nuc_entry->SetSeq().SetId().pop_back();

    // report if database name len too long
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_id> general(new CSeq_id());
    general->SetGeneral().SetDb("thisdatabasevalueislong");
    general->SetGeneral().SetTag().SetStr("b");
    entry->SetSeq().ResetId();
    entry->SetSeq().SetId().push_back(general);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back (new CExpectedError ("gnl|thisdatabasevalueislong|b", eDiag_Critical, "BadSeqIdFormat",
                                                   "General database longer than 20 characters"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // do not report forward slash
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetLocal().SetStr("a/b");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


#if 0
// Commenting out this unit test - we do not plan on supporting segmented sets in the future
BOOST_AUTO_TEST_CASE(Test_SEQ_INST_PartsOutOfOrder)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSegSet();
    CRef<CSeq_entry> master_seg = entry->SetSet().SetSeq_set().front();

    STANDARD_SETUP_WITH_DATABASE

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_loc> loc4(new CSeq_loc());
    loc4->SetWhole().SetLocal().SetStr("part1");
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc4);
    master_seg->SetSeq().SetInst().SetLength(240);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "SeqLocOrder", "Segmented BioseqIntervals out of order in SeqLoc [[lcl|part1, lcl|part2, lcl|part3, lcl|part1]]"));
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "DuplicateSegmentReferences", "Segmented sequence has multiple references to lcl|part1"));
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "PartsOutOfOrder", "Parts set does not contain enough Bioseqs"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    
    scope.RemoveTopLevelSeqEntry(seh);
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().pop_back();
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().pop_back();
    master_seg->SetSeq().SetInst().SetLength(120);
    seh = scope.AddTopLevelSeqEntry(*entry);
    /*
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "SeqLocOrder", "Segmented BioseqIntervals out of order in SeqLoc [[lcl|part1, lcl|part2]]"));
    */
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "PartsOutOfOrder", "Parts set contains too many Bioseqs"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    master_seg->SetSeq().SetInst().ResetExt();
    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetWhole().SetLocal().SetStr("part1");
    CRef<CSeq_loc> loc3(new CSeq_loc());
    loc3->SetWhole().SetLocal().SetStr("part3");
    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetWhole().SetLocal().SetStr("part2");
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc1);
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc3);
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc2);
    master_seg->SetSeq().SetInst().SetLength(180);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "PartsOutOfOrder", "Segmented bioseq seq_ext does not correspond to parts packaging order"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().pop_back();
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().pop_back();
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc2);
    loc3->SetWhole().SetLocal().SetStr("part4");
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc3);
    master_seg->SetSeq().SetInst().SetLength(120);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().SetSeq_set().back()->SetSet().SetSeq_set().front()->SetSet().SetClass(CBioseq_set::eClass_parts);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.clear();
    expected_errors.push_back(new CExpectedError("master", eDiag_Critical, "SeqDataLenWrong", "Bioseq.seq_data too short [60] for given length [120]"));
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "PartsOutOfOrder", "Parts set component is not Bioseq"));
    expected_errors.push_back(new CExpectedError("", eDiag_Critical, "PartsSetHasSets", "Parts set contains unwanted Bioseq-set, its class is \"parts\"."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}
#endif


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_BadSecondaryAccn)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");

    STANDARD_SETUP

    CRef<CSeqdesc> gbdesc (new CSeqdesc());
    gbdesc->SetGenbank().SetExtra_accessions().push_back("AY123456");
    entry->SetSeq().SetDescr().Set().push_back(gbdesc);

    expected_errors.push_back(new CExpectedError("gb|AY123456|", eDiag_Error, "BadSecondaryAccn", "AY123456 used for both primary and secondary accession"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gbdesc->SetEmbl().SetExtra_acc().push_back("AY123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ZeroGiNumber)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGi(ZERO_GI);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gi|0", eDiag_Critical, "ZeroGiNumber", "Invalid GI number"));
    expected_errors.push_back(new CExpectedError("gi|0", eDiag_Error, "GiWithoutAccession", "No accession on sequence with gi number"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_HistoryGiCollision)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetId().front()->SetGenbank().SetVersion(1);
    CRef<CSeq_id> gi_id(new CSeq_id());
    gi_id->SetGi(GI_CONST(21914627));
    entry->SetSeq().SetId().push_back(gi_id);

    STANDARD_SETUP

    CRef<CSeq_id> hist_id(new CSeq_id());
    hist_id->SetGi(GI_CONST(21914627));
    entry->SetSeq().SetInst().SetHist().SetReplaced_by().SetIds().push_back(hist_id);
    entry->SetSeq().SetInst().SetHist().SetReplaced_by().SetDate().SetStd().SetYear(2008);

    expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error, "HistoryGiCollision", "Replaced by gi (21914627) is same as current Bioseq"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetHist().ResetReplaced_by();
    entry->SetSeq().SetInst().SetHist().SetReplaces().SetIds().push_back(hist_id);
    entry->SetSeq().SetInst().SetHist().SetReplaces().SetDate().SetStd().SetYear(2008);
    expected_errors[0]->SetErrMsg("Replaces gi (21914627) is same as current Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // should not generate errors if date has not been set
    entry->SetSeq().SetInst().SetHist().ResetReplaces();
    entry->SetSeq().SetInst().SetHist().SetReplaced_by().SetIds().push_back(hist_id);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetHist().ResetReplaced_by();
    entry->SetSeq().SetInst().SetHist().SetReplaces().SetIds().push_back(hist_id);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    
}


BOOST_AUTO_TEST_CASE(Test_GiWithoutAccession)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGi(GI_CONST(123456));

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gi|123456", eDiag_Error, "GiWithoutAccession", "No accession on sequence with gi number"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_MultipleAccessions)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetId().front()->SetGenbank().SetVersion(1);
    CRef<CSeq_id> gi_id(new CSeq_id());
    gi_id->SetGi(GI_CONST(21914627));
    entry->SetSeq().SetId().push_back(gi_id);
    CRef<CSeq_id> other_acc(new CSeq_id());
    other_acc->SetGenbank().SetAccession("AY123457");
    other_acc->SetGenbank().SetVersion(1);
    entry->SetSeq().SetId().push_back(other_acc);

    STANDARD_SETUP

    // genbank, ddbj, embl, tpg, tpe, tpd, other, pir, swissprot, and prf all count as accessionts
    // genbank
    expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456.1| - gb|AY123457.1|)"));
    expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error, "MultipleAccessions", "Multiple accessions on sequence with gi number"));
    expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Warning, "UnexpectedIdentifierChange", "New accession (gb|AY123457.1|) does not match one in NCBI sequence repository (gb|AY123456.1|) on gi (21914627)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    delete expected_errors[2];
    expected_errors.pop_back();

    // ddbj
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetDdbj().SetAccession("AY123457");
    other_acc->SetDdbj().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - dbj|AY123457.1|)");
    CheckErrors (*eval, expected_errors);

    // embl
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetEmbl().SetAccession("AY123457");
    other_acc->SetEmbl().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - emb|AY123457.1|)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // pir
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetPir().SetAccession("AY123457");
    other_acc->SetPir().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    delete expected_errors[0];
    expected_errors[0] = NULL;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // swissprot
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetSwissprot().SetAccession("AY123457");
    other_acc->SetSwissprot().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // prf
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetPrf().SetAccession("AY123457");
    other_acc->SetPrf().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tpg
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetTpg().SetAccession("AY123457");
    other_acc->SetTpg().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456.1| - tpg|AY123457.1|)"));
    expected_errors.push_back (new CExpectedError("gb|AY123456.1|", eDiag_Info, "HistAssemblyMissing", "TPA record gb|AY123456.1| should have Seq-hist.assembly for PRIMARY block"));
    expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error, "MultipleAccessions", "Multiple accessions on sequence with gi number"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tpe
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetTpe().SetAccession("AY123457");
    other_acc->SetTpe().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - tpe|AY123457.1|)");
    expected_errors[1]->SetErrMsg("TPA record gb|AY123456.1| should have Seq-hist.assembly for PRIMARY block");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tpd
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetTpd().SetAccession("AY123457");
    other_acc->SetTpd().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - tpd|AY123457.1|)");
    expected_errors[1]->SetErrMsg("TPA record gb|AY123456.1| should have Seq-hist.assembly for PRIMARY block");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // other
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetOther().SetAccession("NC_123457");
    other_acc->SetOther().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error, "INSDRefSeqPackaging", "INSD and RefSeq records should not be present in the same set"));
    expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error, "MultipleAccessions", "Multiple accessions on sequence with gi number"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_HistAssemblyMissing)
{
    CRef<CSeq_entry> tpg_entry = unit_test_util::BuildGoodSeq();
    tpg_entry->SetSeq().SetId().front()->SetTpg().SetAccession("AY123456");
    tpg_entry->SetSeq().SetId().front()->SetTpg().SetVersion(1);

    CRef<CSeq_entry> tpe_entry = unit_test_util::BuildGoodSeq();
    tpe_entry->SetSeq().SetId().front()->SetTpe().SetAccession("AY123456");
    tpe_entry->SetSeq().SetId().front()->SetTpe().SetVersion(1);

    CRef<CSeq_entry> tpd_entry = unit_test_util::BuildGoodSeq();
    tpd_entry->SetSeq().SetId().front()->SetTpd().SetAccession("AY123456");
    tpd_entry->SetSeq().SetId().front()->SetTpd().SetVersion(1);

    STANDARD_SETUP_NAME(tpg_entry)

    // tpg
    expected_errors.push_back(new CExpectedError("tpg|AY123456.1|", eDiag_Info, "HistAssemblyMissing", "TPA record tpg|AY123456.1| should have Seq-hist.assembly for PRIMARY block"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tpe
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*tpe_entry);
    expected_errors[0]->SetAccession("tpe|AY123456.1|");
    expected_errors[0]->SetErrMsg("TPA record tpe|AY123456.1| should have Seq-hist.assembly for PRIMARY block");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // tpd
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*tpd_entry);
    expected_errors[0]->SetAccession("tpd|AY123456.1|");
    expected_errors[0]->SetErrMsg("TPA record tpd|AY123456.1| should have Seq-hist.assembly for PRIMARY block");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // error suppressed if keyword present
    CRef<CSeqdesc> block(new CSeqdesc());
    block->SetGenbank().SetKeywords().push_back("TPA:reassembly");
    tpg_entry->SetSeq().SetDescr().Set().push_back(block);
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*tpg_entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    block->SetEmbl().SetKeywords().push_back("TPA:reassembly");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_TerminalNs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("NNNNNNNNNNAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCCAANNNNNNNNNN");
    entry->SetSeq().SetInst().SetLength(62);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TerminalNs", "N at beginning of sequence"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TerminalNs", "N at end of sequence"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning level changes if not local only    
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("gb|AY123456|");
    expected_errors[1]->SetAccession("gb|AY123456|");
    expected_errors[2]->SetAccession("gb|AY123456|");
    expected_errors[3]->SetAccession("gb|AY123456|");
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[1]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // also try delta sequence
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodDeltaSeq ();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLiteral().SetSeq_data().SetIupacna().Set("NNNNNNNNNCCC");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetSeq_data().SetIupacna().Set("CCCNNNNNNNNN");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("lcl|good");
    expected_errors[1]->SetAccession("lcl|good");
    expected_errors[2]->SetAccession("lcl|good");
    expected_errors[3]->SetAccession("lcl|good");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[1]->SetSeverity(eDiag_Warning);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent", "Sequence contains 52 percent Ns"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // 10 Ns but just local stays at warning
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodDeltaSeq ();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLiteral().SetSeq_data().SetIupacna().Set("NNNNNNNNNNCC");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetSeq_data().SetIupacna().Set("CCNNNNNNNNNN");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[4]->SetErrMsg ("Sequence contains 58 percent Ns");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // 10 Ns but now has non-local ID, error
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("gb|AY123456|");
    expected_errors[1]->SetAccession("gb|AY123456|");
    expected_errors[2]->SetAccession("gb|AY123456|");
    expected_errors[3]->SetAccession("gb|AY123456|");
    expected_errors[4]->SetAccession("gb|AY123456|");
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[1]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // NC and patent IDs back to warning
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("ref|NC_123456|");
    expected_errors[1]->SetAccession("ref|NC_123456|");
    expected_errors[2]->SetAccession("ref|NC_123456|");
    expected_errors[3]->SetAccession("ref|NC_123456|");
    expected_errors[4]->SetAccession("ref|NC_123456|");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[1]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetPatent().SetSeqid(1);
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetCountry("USA");
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetId().SetNumber("1");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("pat|USA|1|1");
    expected_errors[1]->SetAccession("pat|USA|1|1");
    expected_errors[2]->SetAccession("pat|USA|1|1");
    expected_errors[3]->SetAccession("pat|USA|1|1");
    delete expected_errors[4];
    expected_errors.pop_back();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // no more TerminalNs warnings if circular
    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    unit_test_util::SetCompleteness(entry, CMolInfo::eCompleteness_complete);
    expected_errors.push_back(new CExpectedError("pat|USA|1|1", eDiag_Warning, "UnwantedCompleteFlag",
                              "Suspicious use of complete"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_UnexpectedIdentifierChange)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123457");
    entry->SetSeq().SetId().front()->SetGenbank().SetVersion(1);
    CRef<CSeq_id> gi_id(new CSeq_id());
    gi_id->SetGi(GI_CONST(21914627));
    entry->SetSeq().SetId().push_back(gi_id);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gb|AY123457.1|", eDiag_Warning, "UnexpectedIdentifierChange", "New accession (gb|AY123457.1|) does not match one in NCBI sequence repository (gb|AY123456.1|) on gi (21914627)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetTpg().SetAccession("AY123456");
    entry->SetSeq().SetId().front()->SetTpg().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    delete expected_errors[0];
    expected_errors[0] = new CExpectedError("tpg|AY123456.1|", eDiag_Info, "HistAssemblyMissing", "TPA record tpg|AY123456.1| should have Seq-hist.assembly for PRIMARY block");
    expected_errors.push_back(new CExpectedError("tpg|AY123456.1|", eDiag_Warning, "UnexpectedIdentifierChange", "Loss of accession (gb|AY123456.1|) on gi (21914627) compared to the NCBI sequence repository"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // TODO - try to instigate other errors

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_InternalNsInSeqLit)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    unit_test_util::AddToDeltaSeq(entry, "AANNNNNNNNNNNNNNNNNNNNGG");
    SetTech(entry, CMolInfo::eTech_wgs);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "InternalNsInSeqLit", "Run of 20 Ns in delta component 5 that starts at base 45"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "SeqGapProblem", "WGS submission includes wrong gap type. Gaps for WGS genomes should be Assembly Gaps with linkage evidence."));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    unit_test_util::AddToDeltaSeq(entry, "AANNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNGG");
    SetTech(entry, CMolInfo::eTech_htgs_1);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "InternalNsInSeqLit",
        "Run of 81 Ns in delta component 7 that starts at base 79"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_2);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_composite_wgs_htgs);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::AddToDeltaSeq(entry, "AANNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNGG");
    SetTech(entry, CMolInfo::eTech_unknown);
    expected_errors[0]->SetErrMsg("Run of 101 Ns in delta component 9 that starts at base 174");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SeqLitGapLength0)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    CRef<CDelta_seq> delta_seq(new CDelta_seq());
    delta_seq->SetLiteral().SetLength(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(delta_seq);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "SeqLitGapLength0", "Gap of length 0 in delta chain"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadDeltaSeq", "Last delta seq component is a gap"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // some kinds of fuzz don't trigger other kind of error
    delta_seq->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_gt);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delta_seq->SetLiteral().SetFuzz().Reset();
    delta_seq->SetLiteral().SetFuzz().SetP_m(10);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // others will
    delta_seq->SetLiteral().SetFuzz().Reset();
    delta_seq->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    expected_errors[0]->SetErrMsg("Gap of length 0 with unknown fuzz in delta chain");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try again with swissprot, error goes to warning
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetSwissprot().SetAccession("AY123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetAccession("sp|AY123456|");
    expected_errors[1]->SetAccession("sp|AY123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delta_seq->SetLiteral().SetFuzz().SetP_m(10);
    expected_errors[0]->SetErrMsg("Gap of length 0 in delta chain");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delta_seq->SetLiteral().SetFuzz().Reset();
    delta_seq->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_gt);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delta_seq->SetLiteral().ResetFuzz();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static void AddTpaAssemblyUserObject(CRef<CSeq_entry> entry)
{
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().SetType().SetStr("TpaAssembly");
    entry->SetSeq().SetDescr().Set().push_back(desc);

    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr("Label");
    field->SetData().SetStr("Data");
    desc->SetUser().SetData().push_back(field);
}


BOOST_AUTO_TEST_CASE(Test_TpaAssmeblyProblem)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    CRef<CSeq_entry> member1 = unit_test_util::BuildGoodSeq();
    member1->SetSeq().SetId().front()->SetLocal().SetStr("good");
    AddTpaAssemblyUserObject(member1);
    entry->SetSet().SetSeq_set().push_back(member1);
    CRef<CSeq_entry> member2 = unit_test_util::BuildGoodSeq();
    member2->SetSeq().SetId().front()->SetLocal().SetStr("good2");
    AddTpaAssemblyUserObject(member2);
    entry->SetSet().SetSeq_set().push_back(member2);

    STANDARD_SETUP_WITH_DATABASE

    // two Tpa sequences, but neither has assembly and neither has GI, so no errors expected
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now one has hist, other does not
    member1->SetSeq().SetInst().SetHist().SetAssembly().push_back(unit_test_util::BuildGoodAlign());
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "TpaAssmeblyProblem", "There are 1 TPAs with history and 1 without history in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now one has gi
    scope.RemoveTopLevelSeqEntry(seh);
    member1->SetSeq().SetId().front()->SetTpg().SetAccession("AY123456");
    member1->SetSeq().SetId().front()->SetTpg().SetVersion(1);
    CRef<CSeq_id> gi_id(new CSeq_id());
    gi_id->SetGi(GI_CONST(21914627));
    member1->SetSeq().SetId().push_back(gi_id);
    seh = scope.AddTopLevelSeqEntry(*entry);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("tpg|AY123456.1|", eDiag_Warning, "UnexpectedIdentifierChange", "Loss of accession (gb|AY123456.1|) on gi (21914627) compared to the NCBI sequence repository"));
    expected_errors.push_back(new CExpectedError("tpg|AY123456.1|", eDiag_Error, "TpaAssmeblyProblem", "There are 1 TPAs with history and 1 without history in this record."));
    expected_errors.push_back(new CExpectedError("tpg|AY123456.1|", eDiag_Warning, "TpaAssmeblyProblem", "There are 1 TPAs without history in this record, but the record has a gi number assignment."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SeqLocLength)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetId().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(9);
    entry->SetSeq().SetInst().SetLength(32);

    STANDARD_SETUP

    // expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "SeqLocLength", "Short length (10) on seq-loc (gb|AY123456|:1-10) of delta seq_ext"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    // if length 11, should not be a problem
    entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetId().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(10);
    entry->SetSeq().SetInst().SetLength(33);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_MissingGaps)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    // remove gaps
    unit_test_util::RemoveDeltaSeqGaps (entry);

    STANDARD_SETUP

    // only report errors for specific molinfo tech values
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // htgs_3 should not report
    SetTech(entry, CMolInfo::eTech_htgs_3);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    SetTech(entry, CMolInfo::eTech_htgs_0);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingGaps", "HTGS delta seq should have gaps between all sequence runs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_1);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_2);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadHTGSeq", "HTGS 2 delta seq has no gaps and no graphs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // RefGeneTracking changes severity
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    AddRefGeneTrackingUserObject(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Info);
    expected_errors[0]->SetAccession("ref|NC_123456|");
    expected_errors[1]->SetAccession("ref|NC_123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    delete expected_errors[1];
    expected_errors.pop_back();

    SetTech(entry, CMolInfo::eTech_htgs_1);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_0);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_CompleteTitleProblem)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    unit_test_util::SetLineage (entry, "Viruses; foo");
    SetTitle(entry, "Foo complete genome");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gb|AY123456|", eDiag_Warning, "CompleteTitleProblem", "Complete genome in title without complete flag set"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

        // should be no error if complete
    unit_test_util::SetCompleteness(entry, CMolInfo::eCompleteness_complete);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_CompleteCircleProblem)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, 
                              "CompleteCircleProblem", 
                              "Circular topology without complete flag set"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    SetTitle(entry, "This is just a title");
    unit_test_util::SetCompleteness(entry, CMolInfo::eCompleteness_complete);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("gb|AY123456|", eDiag_Warning, 
                              "CompleteCircleProblem", 
      "Circular topology has complete flag set, but title should say complete sequence or complete genome"));
    expected_errors.push_back(new CExpectedError("gb|AY123456|", eDiag_Warning, 
                              "UnwantedCompleteFlag",
                              "Suspicious use of complete"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_BadHTGSeq)
{
    // prepare entry
    CRef<CSeq_entry> delta_entry = unit_test_util::BuildGoodDeltaSeq();
    // remove gaps
    unit_test_util::RemoveDeltaSeqGaps (delta_entry);

    STANDARD_SETUP_NAME(delta_entry)

    SetTech(delta_entry, CMolInfo::eTech_htgs_2);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingGaps", "HTGS delta seq should have gaps between all sequence runs"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadHTGSeq", "HTGS 2 delta seq has no gaps and no graphs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[1];
    expected_errors.pop_back();

    // HTGS_ACTIVEFIN keyword disables BadHTGSeq error
    AddGenbankKeyword(delta_entry, "HTGS_ACTIVEFIN");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_entry> raw_entry = unit_test_util::BuildGoodSeq();
    SetTech(raw_entry, CMolInfo::eTech_htgs_2);
    seh = scope.AddTopLevelSeqEntry(*raw_entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadHTGSeq", "HTGS 2 raw seq has no gaps and no graphs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // HTGS_ACTIVEFIN keyword disables error
    AddGenbankKeyword(raw_entry, "HTGS_ACTIVEFIN");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // htg3 errors
    SetTech(raw_entry, CMolInfo::eTech_htgs_3);
    AddGenbankKeyword(raw_entry, "HTGS_DRAFT");
    AddGenbankKeyword(raw_entry, "HTGS_PREFIN");
    AddGenbankKeyword(raw_entry, "HTGS_FULLTOP");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadHTGSeq", "HTGS 3 sequence should not have HTGS_DRAFT keyword"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadHTGSeq", "HTGS 3 sequence should not have HTGS_PREFIN keyword"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadHTGSeq", "HTGS 3 sequence should not have HTGS_ACTIVEFIN keyword"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadHTGSeq", "HTGS 3 sequence should not have HTGS_FULLTOP keyword"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*delta_entry);
    SetTech(delta_entry, CMolInfo::eTech_htgs_3);
    AddGenbankKeyword(delta_entry, "HTGS_DRAFT");
    AddGenbankKeyword(delta_entry, "HTGS_PREFIN");
    AddGenbankKeyword(delta_entry, "HTGS_FULLTOP");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_GapInProtein_and_BadProteinStart)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodProtSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetNcbieaa().Set("PRK-EIN");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GapInProtein", "[1] internal gap symbols in protein sequence (gene? - fake protein name)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    entry->SetSeq().SetInst().SetSeq_data().SetNcbieaa().Set("-RKTEIN");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadProteinStart", "gap symbol at start of protein sequence (gene? - fake protein name)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbieaa().Set("-RK-EIN");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GapInProtein", "[1] internal gap symbols in protein sequence (gene? - fake protein name)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_TerminalGap)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    CRef<CDelta_seq> first_seg(new CDelta_seq());
    first_seg->SetLiteral().SetLength(9);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_front(first_seg);
    CRef<CDelta_seq> last_seg(new CDelta_seq());
    last_seg->SetLiteral().SetLength(9);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(last_seg);
    entry->SetSeq().SetInst().SetLength(entry->SetSeq().SetInst().GetLength() + 18);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadDeltaSeq", "First delta seq component is a gap"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadDeltaSeq", "Last delta seq component is a gap"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TerminalGap", "Gap at beginning of sequence"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TerminalGap", "Gap at end of sequence"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // if gap length is 10, severity is still warning because still all local IDS
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetLength(entry->SetSeq().SetInst().GetLength() + 2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("ref|NC_123456|");
    expected_errors[1]->SetAccession("ref|NC_123456|");
    expected_errors[2]->SetAccession("ref|NC_123456|");
    expected_errors[3]->SetAccession("ref|NC_123456|");
    expected_errors[4]->SetAccession("ref|NC_123456|");
    expected_errors[5]->SetAccession("ref|NC_123456|");
    expected_errors[2]->SetSeverity(eDiag_Warning);
    expected_errors[3]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetPatent().SetSeqid(1);
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetCountry("USA");
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetId().SetNumber("1");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("pat|USA|1|1");
    expected_errors[1]->SetAccession("pat|USA|1|1");
    expected_errors[2]->SetAccession("pat|USA|1|1");
    expected_errors[3]->SetAccession("pat|USA|1|1");
    expected_errors[4]->SetAccession("pat|USA|1|1");
    expected_errors[5]->SetAccession("pat|USA|1|1");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // no more terminal gap warnings if circular - changed to still show first/last delta component
    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    unit_test_util::SetCompleteness(entry, CMolInfo::eCompleteness_complete);
    expected_errors.push_back(new CExpectedError("pat|USA|1|1", eDiag_Warning, "UnwantedCompleteFlag",
                              "Suspicious use of complete"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_OverlappingDeltaRange)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().ResetExt();
    CRef<CSeq_id> seqid(new CSeq_id());
    seqid->SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*seqid, 0, 10);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*seqid, 5, 15);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*seqid, 20, 30);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*seqid, 25, 35);
    entry->SetSeq().SetInst().SetLength(44);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "OverlappingDeltaRange", "Overlapping delta range 6-16 and 1-11 on a Bioseq gb|AY123456|"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "OverlappingDeltaRange", "Overlapping delta range 26-36 and 21-31 on a Bioseq gb|AY123456|"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_LeadingX)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodProtSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("XROTEIN");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "LeadingX", "Sequence starts with leading X"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_InternalNsInSeqRaw)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAANNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNTTTTT");
    entry->SetSeq().SetInst().SetLength(110);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "InternalNsInSeqRaw", "Run of 100 Ns in raw sequence starting at base 6"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent", "Sequence contains 90 percent Ns"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // expect no InternalNsInSeqRaw error
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAANNNNNNNNNNNNNNNNNNNNTTTTT");
    entry->SetSeq().SetInst().SetLength(30);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent", "Sequence contains 66 percent Ns"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // WGS has lower threshold
    SetTech (entry, CMolInfo::eTech_wgs);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "InternalNsInSeqRaw", "Run of 20 Ns in raw sequence starting at base 6"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent", "Sequence contains 66 percent Ns"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_InternalNsAdjacentToGap)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLiteral().SetSeq_data().SetIupacna().Set("ATGATGATGNNN");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetSeq_data().SetIupacna().Set("NNNATGATGATG");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InternalNsAdjacentToGap", "Ambiguous residue N is adjacent to a gap around position 13"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InternalNsAdjacentToGap", "Ambiguous residue N is adjacent to a gap around position 23"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_DeltaComponentIsGi0)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(11);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetId().SetGi(ZERO_GI);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "DeltaComponentIsGi0", "Delta component is gi|0"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ServiceError", "Unable to find far delta sequence component"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_InternalGapsInSeqRaw)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGG-CAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "InvalidResidue", "Invalid residue '-' at position [27]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "InternalGapsInSeqRaw", "Raw nucleotide should not contain gap characters"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SelfReferentialSequence)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(11);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetId().SetLocal().SetStr("good");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "SelfReferentialSequence", "Self-referential delta sequence"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_WholeComponent)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetWhole().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetLength(507);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "WholeComponent", "Delta seq component should not be of type whole"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


void s_AddGeneralAndLocal(CBioseq& seq)
{
    CRef<CSeq_id> gnl(new CSeq_id());
    gnl->SetGeneral().SetDb("a");
    gnl->SetGeneral().SetTag().SetStr("b");
    seq.SetId().front()->Assign(*gnl);
    CRef<CSeq_id> lcl(new CSeq_id());
    lcl->SetLocal().SetStr("x");
    seq.SetId().push_back(lcl);
    seq.SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().Assign(*gnl);
}


BOOST_AUTO_TEST_CASE(Test_ProteinsHaveGeneralID)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodProtSeq();
    s_AddGeneralAndLocal(entry->SetSeq());

    STANDARD_SETUP

    // no error unless part of nuc-prot set
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(entry);
    s_AddGeneralAndLocal(prot->SetSeq());

    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetProduct().SetWhole().SetGeneral().SetDb("a");
    cds->SetProduct().SetWhole().SetGeneral().SetTag().SetStr("b");
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Info, "ProteinsHaveGeneralID", "INDEXER_ONLY - Protein bioseqs have general seq-id."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_HighNContentPercent_and_HighNContentStretch)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAATTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCCCCNNNNNNNNNNNAAAATTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCCCCAAAAATTTTT");
    entry->SetSeq().SetInst().SetLength(100);
    SetTech (entry, CMolInfo::eTech_tsa);
    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_mRNA);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent", "Sequence contains 11 percent Ns"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAATTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCCCCNNNNNNNNNNNNNNNNTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCCCCAAAAATTTTT");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Sequence contains 16 percent Ns");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentStretch", "Sequence has a stretch of 16 Ns"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentStretch", "Sequence has a stretch of 16 Ns"));
    eval = validator.GetTSANStretchErrors(seh);
    CheckErrors (*eval, expected_errors);
    eval = validator.GetTSANStretchErrors(entry->GetSeq());
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AANNNNNNNNNNGGGCCCCCAAAAATTTTTGGGGGCCCCCAAAAATTTTTGGGGGTTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCNNNNNNNNNNAAA");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent", "Sequence contains 20 percent Ns"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentStretch", "Sequence has a stretch of at least 10 Ns within the first 20 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentStretch", "Sequence has a stretch of at least 10 Ns within the last 20 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentStretch", "Sequence has a stretch of at least 10 Ns within the first 20 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentStretch", "Sequence has a stretch of at least 10 Ns within the last 20 bases"));
    eval = validator.GetTSANStretchErrors(seh);
    CheckErrors (*eval, expected_errors);
    eval = validator.GetTSANStretchErrors(entry->GetSeq());
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodDeltaSeq();
    CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCATGATGA", objects::CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetLength(entry->GetSeq().GetInst().GetLength() + 20);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_SeqLitDataLength0)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();

    CDelta_ext::Tdata::iterator seg_it = entry->SetSeq().SetInst().SetExt().SetDelta().Set().begin();
    ++seg_it;
    (*seg_it)->SetLiteral().SetSeq_data().SetIupacna().Set();
    (*seg_it)->SetLiteral().SetLength(0);

    entry->SetSeq().SetInst().SetLength(24);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "SeqLitDataLength0", "Seq-lit of length 0 in delta chain"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static CRef<CSeq_entry> BuildGapFuzz100DeltaSeq(void)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATGCCC", CSeq_inst::eMol_dna);
    CRef<CDelta_seq> gap_seg(new CDelta_seq());
    gap_seg->SetLiteral().SetLength(101);
    gap_seg->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCATGATGATG", CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetLength(125);

    return entry;
}


BOOST_AUTO_TEST_CASE(Test_SeqLitGapFuzzNot100)
{
    CRef<CSeq_entry> entry = BuildGapFuzz100DeltaSeq();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "SeqLitGapFuzzNot100", "Gap of unknown length should have length 100"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_DSmRNA)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_mRNA);
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_ds);

    STANDARD_SETUP

    // double strand
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "DSmRNA", "mRNA not single stranded"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // mixed strand
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_mixed);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // mixed strand
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // these should not produce errors

    // strand not set
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_not_set);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetStrand();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // single strand
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_ss);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_BioSourceMissing)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::RemoveDescriptorType (entry, CSeqdesc::e_Source);
    unit_test_util::AddGoodSource (entry->SetSet().SetSeq_set().front());

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BioSourceMissing", "Nuc-prot set does not contain expected BioSource descriptor"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "NoOrgFound", "No organism name has been applied to this Bioseq.  Other qualifiers may exist."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_InvalidForType)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> desc;
    desc.Reset(new CSeqdesc());
    desc->SetMol_type(eGIBB_mol_genomic);
    entry->SetDescr().Set().push_back(desc);
    desc.Reset(new CSeqdesc());
    desc->SetModif().push_back(eGIBB_mod_dna);
    entry->SetDescr().Set().push_back(desc);
    desc.Reset(new CSeqdesc());
    desc->SetMethod(eGIBB_method_other);
    entry->SetDescr().Set().push_back(desc);
    desc.Reset(new CSeqdesc());
    desc->SetOrg().SetTaxname("Sebaea microphylla");
    entry->SetDescr().Set().push_back(desc);
    AddTpaAssemblyUserObject (entry);
   

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "Nucleic acid with protein sequence method"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "MolType descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "Modif descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "Method descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "OrgRef descriptor is obsolete"));

    // won't complain about TPA assembly if only local ID
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    unit_test_util::RemoveDescriptorType (entry, CSeqdesc::e_Mol_type);
    unit_test_util::RemoveDescriptorType (entry, CSeqdesc::e_Modif);
    unit_test_util::RemoveDescriptorType (entry, CSeqdesc::e_Method);
    unit_test_util::RemoveDescriptorType (entry, CSeqdesc::e_Org);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "Non-TPA record gb|AY123456| should not have TpaAssembly object"));
    SetErrorsAccessions(expected_errors, "gb|AY123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    SetErrorsAccessions(expected_errors, "ref|NC_123456|");
    expected_errors[0]->SetErrMsg("Non-TPA record ref|NC_123456| should not have TpaAssembly object");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc.Reset(new CSeqdesc());
    desc->SetMol_type(eGIBB_mol_peptide);
    entry->SetDescr().Set().push_back(desc);
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Error, "InvalidForType",
                              "Nucleic acid with GIBB-mol = peptide"));
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Error, "InvalidForType",
                              "MolType descriptor is obsolete"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_other);
    expected_errors[1]->SetErrMsg("GIBB-mol unknown or other used");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_unknown);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodProtSeq();
    desc.Reset(new CSeqdesc());
    desc->SetMol_type(eGIBB_mol_genomic);
    entry->SetDescr().Set().push_back(desc);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "GIBB-mol [1] used on protein"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "MolType descriptor is obsolete"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_pre_mRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [2] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_mRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [3] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_rRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [4] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_tRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [5] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_snRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [6] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_scRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [7] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_other_genetic);
    expected_errors[0]->SetErrMsg("GIBB-mol [9] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_genomic_mRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [10] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
 
    // invalid modif
    desc->SetModif().push_back(eGIBB_mod_dna);
    desc->SetModif().push_back(eGIBB_mod_rna);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "Nucleic acid GIBB-mod [0] on protein"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "Nucleic acid GIBB-mod [1] on protein"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "Modif descriptor is obsolete"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsSource()) {
            (*it)->SetSource().SetOrigin(CBioSource::eOrigin_synthetic);
        }
    }
    seh = scope.AddTopLevelSeqEntry(*entry);
    // if biomol not other, should generate error
    expected_errors.push_back(new CExpectedError ("lcl|good", eDiag_Warning, "InvalidForType",
                                                  "Molinfo-biomol other should be used if Biosource-location is synthetic"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsSource()) {
            (*it)->SetSource().ResetOrigin();
        }
    }

    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_peptide);
    expected_errors.push_back(new CExpectedError ("lcl|good", eDiag_Error, "InvalidForType",
                                                  "Nucleic acid with Molinfo-biomol = peptide"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_other_genetic);
    expected_errors[0]->SetErrMsg("Molinfo-biomol = other genetic");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_unknown);
    expected_errors[0]->SetErrMsg("Molinfo-biomol unknown used");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_other);
    expected_errors[0]->SetErrMsg("Molinfo-biomol other used");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodProtSeq();
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors[0]->SetSeverity(eDiag_Error);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_genomic);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [1] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_pre_RNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [2] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_mRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [3] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_rRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [4] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_tRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [5] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_snRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [6] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_scRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [7] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_genomic_mRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [10] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_cRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [11] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_snoRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [12] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_transcribed_RNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [13] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_ncRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [14] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_tmRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [15] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    seh = scope.AddTopLevelSeqEntry(*entry);
    unit_test_util::SetSynthetic_construct(entry);
    expected_errors.push_back(new CExpectedError ("lcl|good", eDiag_Warning, "InvalidForType",
                                                  "synthetic construct should have other-genetic"));
    expected_errors.push_back(new CExpectedError ("lcl|good", eDiag_Warning, "InvalidForType",
                                                  "synthetic construct should have artificial origin"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    unit_test_util::SetSebaea_microphylla(entry);

    SetTech(entry, CMolInfo::eTech_concept_trans);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                                                 "Nucleic acid with protein sequence method"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_seq_pept);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    SetTech(entry, CMolInfo::eTech_both);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_seq_pept_overlap);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    SetTech(entry, CMolInfo::eTech_seq_pept_homol);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_concept_trans_a);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodProtSeq();
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Protein with nucleic acid sequence method");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ConflictingBiomolTech",
                                                 "EST sequence should be mRNA"));

    SetTech(entry, CMolInfo::eTech_est);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                                                 "Protein with nucleic acid sequence method"));
    SetTech(entry, CMolInfo::eTech_genemap);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_physmap);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_fli_cdna);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htc);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ConflictingBiomolTech",
                                                 "HTGS/STS/GSS/WGS sequence should be genomic"));
    SetTech(entry, CMolInfo::eTech_sts);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_1);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_3);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_0);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_wgs);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_composite_wgs_htgs);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadHTGSeq",
                                                 "HTGS 2 raw seq has no gaps and no graphs"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                                                 "Protein with nucleic acid sequence method"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ConflictingBiomolTech",
                                                 "HTGS/STS/GSS/WGS sequence should be genomic"));

    SetTech(entry, CMolInfo::eTech_htgs_2);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadKeyword",
                                                 "Molinfo.tech barcode without BARCODE keyword"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                                                 "Protein with nucleic acid sequence method"));

    SetTech(entry, CMolInfo::eTech_barcode);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_Unknown)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetModif().push_back(eGIBB_mod_other);
    entry->SetDescr().Set().push_back(desc);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "Modif descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Unknown",
                              "GIBB-mod = other used"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static CRef<CSeq_entry> MakeGps(CRef<CSeq_entry> member)
{
    CRef<CSeq_entry> set(new CSeq_entry());
    set->SetSet().SetClass(CBioseq_set::eClass_gen_prod_set);
    set->SetSet().SetSeq_set().push_back(member);
    return set;
}


BOOST_AUTO_TEST_CASE(Test_Descr_NoPubFound)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::RemoveDescriptorType (entry, CSeqdesc::e_Pub);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "NoPubFound",
                              "No publications anywhere on this entire record."));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Info, "MissingPubRequirement",
                              "No submission citation anywhere on this entire record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // make gpipe - should suppress error
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> id_suppress(new CSeq_id());
    id_suppress->SetGpipe().SetAccession("AY123456");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().push_back(id_suppress);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("gpp|AY123456|", eDiag_Info, "MissingPubRequirement",
                              "No submission citation anywhere on this entire record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // make GPS - will suppress pub errors, although introduce gps erros
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().pop_back();
    CRef<CSeq_entry> gps = MakeGps(entry);
    seh = scope.AddTopLevelSeqEntry(*gps);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, 
                              "GenomicProductPackagingProblem", 
                              "Nucleotide bioseq should be product of mRNA feature on contig, but is not"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, 
                              "GenomicProductPackagingProblem", 
                              "Protein bioseq should be product of CDS feature on contig, but is not"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Info, "MissingPubRequirement",
                              "No submission citation anywhere on this entire record."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // only one has pub
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::RemoveDescriptorType (entry, CSeqdesc::e_Pub);
    unit_test_util::AddGoodPub(entry->SetSet().SetSeq_set().front());
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "NoPubFound",
                              "No publications refer to this Bioseq."));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Info, "MissingPubRequirement",
                              "Expected submission citation is missing for this Bioseq"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // intermediate wgs should suppress NoPubFound
    scope.RemoveTopLevelSeqEntry(seh);
    id_suppress->SetOther().SetAccession("NC_123456");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().push_back(id_suppress);
    SetTech (entry->SetSet().SetSeq_set().front(), CMolInfo::eTech_wgs);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Info, "MissingPubRequirement",
        "Expected submission citation is missing for this Bioseq"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_NoOrgFound)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::RemoveDescriptorType (entry, CSeqdesc::e_Source);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BioSourceMissing",
                              "Nuc-prot set does not contain expected BioSource descriptor"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "NoOrgFound",
                              "No organism name anywhere on this entire record."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[1];
    expected_errors.pop_back();

    // suppress if patent or pdb
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> id2(new CSeq_id());
    id2->SetPatent().SetSeqid(1);
    id2->SetPatent().SetCit().SetCountry("USA");
    id2->SetPatent().SetCit().SetId().SetNumber("1");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().push_back(id2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CPDB_seq_id> pdb_id(new CPDB_seq_id());
    pdb_id->SetMol().Set("foo");
    id2->SetPdb(*pdb_id);
    seh = scope.AddTopLevelSeqEntry(*entry);
    SetErrorsAccessions(expected_errors, "pdb|foo| ");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // add one source
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().pop_back();
    unit_test_util::AddGoodSource (entry->SetSet().SetSeq_set().front());
    seh = scope.AddTopLevelSeqEntry(*entry);
    SetErrorsAccessions(expected_errors, "lcl|nuc");
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "NoOrgFound",
                              "No organism name has been applied to this Bioseq.  Other qualifiers may exist."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // if there is a source descriptor but no tax name, still produce error
    unit_test_util::AddGoodSource(entry->SetSet().SetSeq_set().back());
    unit_test_util::SetTaxname(entry->SetSet().SetSeq_set().back(), "");
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "NoOrgFound",
                              "No organism name has been applied to this Bioseq.  Other qualifiers may exist."));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BioSourceOnProtein",
                                                 "Nuc-prot set has 1 protein with a BioSource descriptor"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BioSourceMissing",
                              "Nuc-prot set does not contain expected BioSource descriptor"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_MultipleBioSources)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::AddGoodSource (entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MultipleBioSources",
                              "Undesired multiple source descriptors"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_NoMolInfoFound)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::RemoveDescriptorType (entry, CSeqdesc::e_Molinfo);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "NoMolInfoFound",
                              "No Mol-info applies to this Bioseq"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_NoTaxonID)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxon(entry, 0);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NoTaxonID",
                              "BioSource is missing taxon ID"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_InconsistentBiosources)
{
    // prepare entry
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_pop_set);
    CRef<CSeq_entry> first = unit_test_util::BuildGoodSeq();
    entry->SetSet().SetSeq_set().push_back(first);
    CRef<CSeq_entry> second = unit_test_util::BuildGoodSeq();
    second->SetSeq().SetId().front()->SetLocal().SetStr("good2");
    unit_test_util::SetTaxname(second, "");
    unit_test_util::SetTaxon(second, 0);
    unit_test_util::SetTaxname(second, "Trichechus manatus latirostris");
    unit_test_util::SetTaxon(second, 127582);
    entry->SetSet().SetSeq_set().push_back(second);

    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetTitle("popset title");
    entry->SetSet().SetDescr().Set().push_back(desc);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InconsistentTaxNameSet",
                              "Population set contains inconsistent organism names."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning instead of error if same up to ' sp. '
    unit_test_util::SetTaxname(first, "");
    unit_test_util::SetTaxon(first, 0);
    unit_test_util::SetTaxname(first, "Corynebacterium sp. 979");
    unit_test_util::SetTaxon(first, 215582);
    unit_test_util::SetTaxname(second, "");
    unit_test_util::SetTaxon(second, 0);
    unit_test_util::SetTaxname(second, "Corynebacterium sp. DJ1");
    unit_test_util::SetTaxon(second, 632939);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning instead of error if one name is subset of the other
    unit_test_util::SetTaxname(first, "");
    unit_test_util::SetTaxon(first, 0);
    unit_test_util::SetTaxname(first, "Trichechus manatus");
    unit_test_util::SetTaxon(first, 9778);
    unit_test_util::SetTaxname(second, "");
    unit_test_util::SetTaxon(second, 0);
    unit_test_util::SetTaxname(second, "Trichechus manatus latirostris");
    unit_test_util::SetTaxon(second, 127582);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // no error if not pop-set
    unit_test_util::SetTaxname(first, "");
    unit_test_util::SetTaxon(first, 0);
    unit_test_util::SetTaxname(first, "Corynebacterium sp. 979");
    unit_test_util::SetTaxon(first, 215582);
    unit_test_util::SetTaxname(second, "");
    unit_test_util::SetTaxon(second, 0);
    unit_test_util::SetTaxname(second, "Trichechus manatus latirostris");
    unit_test_util::SetTaxon(second, 127582);
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    unit_test_util::RemoveDescriptorType(entry, CSeqdesc::e_Title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_MissingLineage)
{
   // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::ResetOrgname(entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingLineage",
                              "No lineage for this BioSource."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetLineage (entry, "");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning if EMBL
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetEmbl().SetAccession("B12345");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetAccession("emb|B12345|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning if DDBJ
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetDdbj().SetAccession("C12345");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetAccession("dbj|C12345|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // critical instead of error if refseq AND has taxon
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Critical);
    expected_errors[0]->SetAccession("ref|NC_123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // back to error if no taxon but refseq
    unit_test_util::SetTaxon (entry, 0);
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Warning, "NoTaxonID",
        "BioSource is missing taxon ID"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_SerialInComment)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> comment(new CSeqdesc());
    comment->SetComment("blah blah [123456]");
    entry->SetSeq().SetDescr().Set().push_back(comment);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "SerialInComment",
                              "Comment may refer to reference by serial number - attach reference specific comments to the reference REMARK instead."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BioSourceNeedsFocus)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::AddGoodSourceFeature (entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BioSourceNeedsFocus",
                              "BioSource descriptor must have focus or transgenic when BioSource feature with different taxname is present."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // error goes away if focus is set on descriptor
    unit_test_util::SetFocus(entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // error goes away if descriptor is transgenic
    unit_test_util::ClearFocus(entry);
    unit_test_util::SetTransgenic (entry, true);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_BadOrganelle)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetGenome (entry, CBioSource::eGenome_kinetoplast);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadOrganelle",
                              "Only Kinetoplastida have kinetoplasts"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetGenome (entry, CBioSource::eGenome_nucleomorph);
    expected_errors[0]->SetErrMsg("Only Chlorarachniophyceae and Cryptophyta have nucleomorphs");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TaxonomyNucleomorphProblem",
                                                 "Taxonomy lookup does not have expected nucleomorph flag"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    unit_test_util::SetGenome (entry, CBioSource::eGenome_macronuclear);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadOrganelle",
                              "Only Ciliophora have macronuclear locations"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    unit_test_util::SetDrosophila_melanogaster(entry);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_plastid);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TaxonomyPlastidsProblem",
                              "Taxonomy lookup does not have expected plastid flag"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // no plastid error if flag is present
    unit_test_util::SetSebaea_microphylla(entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_MultipleChromosomes)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetChromosome (entry, "1");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultipleSourceQualifiers",
                              "Multiple identical chromosome qualifiers present"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetChromosome (entry, "2");
    expected_errors[0]->SetErrMsg("Multiple conflicting chromosome qualifiers present");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadSubSource)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource (entry, 0, "foo");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "BadSubSource",
                              "Unknown subsource subtype 0"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}

void ShowOrgRef(const COrg_ref& org)
{
    ESerialDataFormat outFormat = eSerial_AsnText;
    auto_ptr<CObjectOStream> os;
    os.reset(CObjectOStream::Open(outFormat, cout));
    *os << org;    
}


void ShowOrgRef(const CSeq_entry& entry)
{
    if (entry.IsSeq()) {
        if (entry.GetSeq().IsSetDescr()) {
            ITERATE(objects::CSeq_descr::Tdata, it, entry.GetSeq().GetDescr().Get()) {
                if ((*it)->IsSource() && (*it)->GetSource().IsSetOrg()) {
                    ShowOrgRef((*it)->GetSource().GetOrg());
                }
            }
        }
    } else if (entry.IsSet()) {
        if (entry.GetSet().IsSetDescr()) {
            ITERATE(objects::CSeq_descr::Tdata, it, entry.GetSet().GetDescr().Get()) {
                if ((*it)->IsSource() && (*it)->GetSource().IsSetOrg()) {
                    ShowOrgRef((*it)->GetSource().GetOrg());
                }
            }
        }
        if (entry.GetSet().IsSetSeq_set()) {
            ITERATE(objects::CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set()) {
                ShowOrgRef(**it);
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadOrgMod)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetOrgMod (entry, 0, "foo");
    unit_test_util::SetOrgMod (entry, 1, "bar");
    unit_test_util::SetOrgMod (entry, COrgMod::eSubtype_strain, "a");
    unit_test_util::SetOrgMod (entry, COrgMod::eSubtype_strain, "b");
    unit_test_util::SetOrgMod (entry, COrgMod::eSubtype_variety, "c");
    unit_test_util::SetOrgMod (entry, COrgMod::eSubtype_nat_host, "Sebaea microphylla");
    unit_test_util::SetCommon (entry, "some common name");
    unit_test_util::SetOrgMod (entry, COrgMod::eSubtype_common, "some common name");
    unit_test_util::SetOrgMod (entry, COrgMod::eSubtype_type_material, "invalid type material name");
//    ShowOrgRef(*entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, 
                              "OrganismNotFound", "Organism not found in taxonomy database (suggested:Sebaea microphylla var. c)"));

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "BadOrgMod",
                              "Unknown orgmod subtype 0"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "BadOrgMod",
                              "Unknown orgmod subtype 1"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadOrgMod",
                              "Multiple strain qualifiers on the same BioSource"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadOrgMod",
                              "Bad value for type_material"));
                               
    /*
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadOrgMod",
                              "Orgmod variety should only be in plants, fungi, or cyanobacteria"));
    */
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Variety value specified is not found in taxname"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadOrgMod",
                              "Specific host is identical to taxname"));
    /*
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadOrgMod",
                              "OrgMod common is identical to Org-ref common"));
    */

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_InconsistentProteinTitle)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetTitle("Not the correct title");
    entry->SetSet().SetSeq_set().back()->SetSeq().SetDescr().Set().push_back(desc);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "InconsistentProteinTitle",
                              "Instantiated protein title does not match automatically generated title"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_Inconsistent)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> desc1(new CSeqdesc());
    desc1->SetMol_type(eGIBB_mol_genomic);
    entry->SetSeq().SetDescr().Set().push_back(desc1);
    CRef<CSeqdesc> desc2(new CSeqdesc());
    desc2->SetMol_type(eGIBB_mol_pre_mRNA);
    entry->SetSeq().SetDescr().Set().push_back(desc2);
    CRef<CSeqdesc> desc3(new CSeqdesc());
    desc3->SetModif().push_back(eGIBB_mod_dna);
    desc3->SetModif().push_back(eGIBB_mod_rna);
    desc3->SetModif().push_back(eGIBB_mod_mitochondrial);
    desc3->SetModif().push_back(eGIBB_mod_cyanelle);
    desc3->SetModif().push_back(eGIBB_mod_complete);
    desc3->SetModif().push_back(eGIBB_mod_partial);
    desc3->SetModif().push_back(eGIBB_mod_no_left);
    desc3->SetModif().push_back(eGIBB_mod_no_right);
    entry->SetSeq().SetDescr().Set().push_back(desc3);

    CRef<CSeqdesc> desc_gb1(new CSeqdesc());
    desc_gb1->SetGenbank().SetKeywords().push_back("TPA:experimental");
    desc_gb1->SetGenbank().SetKeywords().push_back("TPA:inferential");
    entry->SetSeq().SetDescr().Set().push_back(desc_gb1);
    CRef<CSeqdesc> desc_gb2(new CSeqdesc());
    desc_gb2->SetGenbank();
    entry->SetSeq().SetDescr().Set().push_back(desc_gb2);

    CRef<CSeqdesc> desc_embl1(new CSeqdesc());
    desc_embl1->SetEmbl();
    entry->SetSeq().SetDescr().Set().push_back(desc_embl1);
    CRef<CSeqdesc> desc_embl2(new CSeqdesc());
    desc_embl2->SetEmbl();
    entry->SetSeq().SetDescr().Set().push_back(desc_embl2);

    CRef<CSeqdesc> desc_pir1(new CSeqdesc());
    desc_pir1->SetPir();
    entry->SetSeq().SetDescr().Set().push_back(desc_pir1);
    CRef<CSeqdesc> desc_pir2(new CSeqdesc());
    desc_pir2->SetPir();
    entry->SetSeq().SetDescr().Set().push_back(desc_pir2);

    CRef<CSeqdesc> desc_sp1(new CSeqdesc());
    desc_sp1->SetSp();
    entry->SetSeq().SetDescr().Set().push_back(desc_sp1);
    CRef<CSeqdesc> desc_sp2(new CSeqdesc());
    desc_sp2->SetSp();
    entry->SetSeq().SetDescr().Set().push_back(desc_sp2);

    CRef<CSeqdesc> desc_pdb1(new CSeqdesc());
    desc_pdb1->SetPdb();
    entry->SetSeq().SetDescr().Set().push_back(desc_pdb1);
    CRef<CSeqdesc> desc_pdb2(new CSeqdesc());
    desc_pdb2->SetPdb();
    entry->SetSeq().SetDescr().Set().push_back(desc_pdb2);

    CRef<CSeqdesc> desc_prf1(new CSeqdesc());
    desc_prf1->SetPrf();
    entry->SetSeq().SetDescr().Set().push_back(desc_prf1);
    CRef<CSeqdesc> desc_prf2(new CSeqdesc());
    desc_prf2->SetPrf();
    entry->SetSeq().SetDescr().Set().push_back(desc_prf2);

    CRef<CSeqdesc> desc_create1(new CSeqdesc());
    desc_create1->SetCreate_date().SetStd().SetYear(2009);
    desc_create1->SetCreate_date().SetStd().SetMonth(4);
    entry->SetSeq().SetDescr().Set().push_back(desc_create1);
    CRef<CSeqdesc> desc_create2(new CSeqdesc());
    desc_create2->SetCreate_date().SetStd().SetYear(2009);
    desc_create2->SetCreate_date().SetStd().SetMonth(3);
    entry->SetSeq().SetDescr().Set().push_back(desc_create2);
    CRef<CSeqdesc> desc_update(new CSeqdesc());
    desc_update->SetUpdate_date().SetStd().SetYear(2009);
    desc_update->SetUpdate_date().SetStd().SetMonth(2);
    entry->SetSeq().SetDescr().Set().push_back(desc_update);

    CRef<CSeqdesc> src_desc(new CSeqdesc());
    src_desc->SetSource().SetOrg().SetTaxname("Trichechus manatus");
    unit_test_util::SetTaxon (src_desc->SetSource(), 9778);
    src_desc->SetSource().SetOrg().SetOrgname().SetLineage("some lineage");
    entry->SetSeq().SetDescr().Set().push_back(src_desc);

    SetTech(entry, CMolInfo::eTech_genemap);
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_no_left);
    CRef<CSeqdesc> m_desc(new CSeqdesc());
    m_desc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_cRNA);
    m_desc->SetMolinfo().SetTech(CMolInfo::eTech_fli_cdna);
    m_desc->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_right);
    entry->SetSeq().SetDescr().Set().push_back(m_desc);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InconsistentTPA",
                              "TPA:experimental and TPA:inferential should not both be in the same set of keywords"));
    /*
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "InconsistentDates",
                              "Inconsistent create_dates [Mar 2009] and [Apr 2009]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "InconsistentDates",
                              "Inconsistent create_date [Apr 2009] and update_date [Feb 2009]"));
    */
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InconsistentTaxName",
                              "Inconsistent organism names [Trichechus manatus] and [Sebaea microphylla]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InconsistentMolInfo",
                              "Inconsistent Molinfo-biomol [1] and [11]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InconsistentMolInfoTechnique",
                              "Inconsistent Molinfo-tech [5] and [17]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InconsistentMolInfo",
                              "Inconsistent Molinfo-completeness [3] and [4]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InconsistentGenBankblocks",
                              "Multiple GenBank blocks"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Multiple EMBL blocks"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Multiple PIR blocks"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Multiple PDB blocks"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Multiple PRF blocks"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Multiple SWISS-PROT blocks"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mod [0] and [1]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mod [4] and [7]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mod [11] and [10]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mod [11] and [16]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mod [11] and [17]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mol [1] and [2]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "MolType descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "MolType descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                              "Modif descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadDate",
                              "Create date has error - BAD_DAY"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadDate",
                              "Create date has error - BAD_DAY"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadDate",
                              "Update date has error - BAD_DAY"));
    /*
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MultipleBioSources",
                              "Undesired multiple source descriptors"));
    */

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // try different WGS-style accessions, check for wgs_tech
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("ABCD12345678");
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("gb|ABCD12345678|", eDiag_Error, "InconsistentMolInfoTechnique",
                              "WGS accession should have Mol-info.tech of wgs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetEmbl().SetAccession("ABCE12345678");
    expected_errors[0]->SetAccession("emb|ABCE12345678|");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetDdbj().SetAccession("ABCF12345678");
    expected_errors[0]->SetAccession("dbj|ABCF12345678|");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // look for correct accession if WGS tech present
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetEmbl().SetAccession("AA123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetDdbj().SetAccession("AB123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AC123456");
    SetTech(entry, CMolInfo::eTech_wgs);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("gb|AC123456|", eDiag_Error, "InconsistentWGSFlags",
                              "Mol-info.tech of wgs should have WGS accession"));
    expected_errors.push_back(new CExpectedError("gb|AC123456|", eDiag_Warning, "UnexpectedIdentifierChange",
        "Loss of general ID (BCMHGSC: PROJECT_GXOU.BAYLOR) on gi (25008031) compared to the NCBI sequence repository"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NM_123456");
    expected_errors[0]->SetSeverity(eDiag_Error);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("ref|NM_123456|");
    delete expected_errors[1];
    expected_errors.pop_back();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NP_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("ref|NP_123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NG_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("ref|NG_123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NR_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("ref|NR_123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // no tech warning if other but not one of four starts
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NX_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // skip warning if segset accession
    vector<string> segset_accession_prefixes;
    segset_accession_prefixes.push_back("AH");
    segset_accession_prefixes.push_back("CH");
    segset_accession_prefixes.push_back("CM");
    segset_accession_prefixes.push_back("DS");
    segset_accession_prefixes.push_back("EM");
    segset_accession_prefixes.push_back("EN");
    segset_accession_prefixes.push_back("EP");
    segset_accession_prefixes.push_back("EQ");
    segset_accession_prefixes.push_back("FA");
    segset_accession_prefixes.push_back("GG");
    segset_accession_prefixes.push_back("GL");

    for (vector<string>::iterator it = segset_accession_prefixes.begin();
         it != segset_accession_prefixes.end();
         ++it) {
        scope.RemoveTopLevelSeqEntry(seh);
        entry->SetSeq().SetId().front()->SetOther().SetAccession(*it + "_123456");
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }
    
    // biomol on NC should be genomic or cRNA
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    SetTech(entry, CMolInfo::eTech_unknown);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_genomic);
    seh = scope.AddTopLevelSeqEntry(*entry);
    // no error expected
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_cRNA);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    // no error expected
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // expect errors
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_genomic_mRNA);
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Error, "InconsistentRefSeqMoltype",
                              "genomic RefSeq accession should use genomic or cRNA moltype"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_mRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_ncRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_pre_RNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_rRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_rRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_scRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_snoRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_snRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_tmRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_transcribed_RNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_tRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_ObsoleteSourceLocation)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetGenome (entry, CBioSource::eGenome_transposon);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ObsoleteSourceLocation",
                              "Transposon and insertion sequence are no longer legal locations"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetGenome (entry, CBioSource::eGenome_insertion_seq);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_ObsoleteSourceQual)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_transposon_name, "a");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_insertion_seq_name, "b");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ObsoleteSourceQual",
                              "Transposon name and insertion sequence name are no longer legal qualifiers"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ObsoleteSourceQual",
                              "Transposon name and insertion sequence name are no longer legal qualifiers"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_StructuredSourceNote)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "StructuredSourceNote",
                              "Source note has structured tag '"));

    vector<string> tag_prefixes;
    tag_prefixes.push_back("acronym:");
    tag_prefixes.push_back("anamorph:");
    tag_prefixes.push_back("authority:");
    tag_prefixes.push_back("biotype:");
    tag_prefixes.push_back("biovar:");
    tag_prefixes.push_back("bio_material:");
    tag_prefixes.push_back("breed:");
    tag_prefixes.push_back("cell_line:");
    tag_prefixes.push_back("cell_type:");
    tag_prefixes.push_back("chemovar:");
    tag_prefixes.push_back("chromosome:");
    tag_prefixes.push_back("clone:");
    tag_prefixes.push_back("clone_lib:");
    tag_prefixes.push_back("collected_by:");
    tag_prefixes.push_back("collection_date:");
    tag_prefixes.push_back("common:");
    tag_prefixes.push_back("country:");
    tag_prefixes.push_back("cultivar:");
    tag_prefixes.push_back("culture_collection:");
    tag_prefixes.push_back("dev_stage:");
    tag_prefixes.push_back("dosage:");
    tag_prefixes.push_back("ecotype:");
    tag_prefixes.push_back("endogenous_virus_name:");
    tag_prefixes.push_back("environmental_sample:");
    tag_prefixes.push_back("forma:");
    tag_prefixes.push_back("forma_specialis:");
    tag_prefixes.push_back("frequency:");
    tag_prefixes.push_back("fwd_pcr_primer_name");
    tag_prefixes.push_back("fwd_pcr_primer_seq");
    tag_prefixes.push_back("fwd_primer_name");
    tag_prefixes.push_back("fwd_primer_seq");
    tag_prefixes.push_back("genotype:");
    tag_prefixes.push_back("germline:");
    tag_prefixes.push_back("group:");
    tag_prefixes.push_back("haplogroup:");
    tag_prefixes.push_back("haplotype:");
    tag_prefixes.push_back("identified_by:");
    tag_prefixes.push_back("insertion_seq_name:");
    tag_prefixes.push_back("isolate:");
    tag_prefixes.push_back("isolation_source:");
    tag_prefixes.push_back("lab_host:");
    tag_prefixes.push_back("lat_lon:");
    tag_prefixes.push_back("left_primer:");
    tag_prefixes.push_back("linkage_group:");
    tag_prefixes.push_back("map:");
    tag_prefixes.push_back("mating_type:");
    tag_prefixes.push_back("metagenome_source:");
    tag_prefixes.push_back("metagenomic:");
    tag_prefixes.push_back("nat_host:");
    tag_prefixes.push_back("pathovar:");
    tag_prefixes.push_back("placement:");
    tag_prefixes.push_back("plasmid_name:");
    tag_prefixes.push_back("plastid_name:");
    tag_prefixes.push_back("pop_variant:");
    tag_prefixes.push_back("rearranged:");
    tag_prefixes.push_back("rev_pcr_primer_name");
    tag_prefixes.push_back("rev_pcr_primer_seq");
    tag_prefixes.push_back("rev_primer_name");
    tag_prefixes.push_back("rev_primer_seq");
    tag_prefixes.push_back("right_primer:");
    tag_prefixes.push_back("segment:");
    tag_prefixes.push_back("serogroup:");
    tag_prefixes.push_back("serotype:");
    tag_prefixes.push_back("serovar:");
    tag_prefixes.push_back("sex:");
    tag_prefixes.push_back("specimen_voucher:");
    tag_prefixes.push_back("strain:");
    tag_prefixes.push_back("subclone:");
    tag_prefixes.push_back("subgroup:");
    tag_prefixes.push_back("substrain:");
    tag_prefixes.push_back("subtype:");
    tag_prefixes.push_back("sub_species:");
    tag_prefixes.push_back("synonym:");
    tag_prefixes.push_back("taxon:");
    tag_prefixes.push_back("teleomorph:");
    tag_prefixes.push_back("tissue_lib:");
    tag_prefixes.push_back("tissue_type:");
    tag_prefixes.push_back("transgenic:");
    tag_prefixes.push_back("transposon_name:");
    tag_prefixes.push_back("type:");
    tag_prefixes.push_back("variety:");

    for (vector<string>::iterator it = tag_prefixes.begin();
         it != tag_prefixes.end();
         ++it) {
        expected_errors[0]->SetErrMsg("Source note has structured tag '" + *it + "'");
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_other, *it + "a");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_other, "");
        unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_other, *it + "a");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::SetOrgMod(entry, CSubSource::eSubtype_other, "");
    }


    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_UnnecessaryBioSourceFocus)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetFocus(entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "UnnecessaryBioSourceFocus",
                              "BioSource descriptor has focus, but no BioSource feature"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_RefGeneTrackingWithoutStatus)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().SetObjectType(CUser_object::eObjectType_RefGeneTracking);
    entry->SetSeq().SetDescr().Set().push_back(desc);

    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr("Label");
    field->SetData().SetStr("Data");
    desc->SetUser().SetData().push_back(field);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Error, "RefGeneTrackingWithoutStatus",
                              "RefGeneTracking object needs to have Status set"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_UnwantedCompleteFlag)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    unit_test_util::SetCompleteness(entry, CMolInfo::eCompleteness_complete);
    SetTitle(entry, "a title without the word");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gb|AY123456|", eDiag_Warning, "UnwantedCompleteFlag",
                              "Suspicious use of complete"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tech of HTGS3 lowers to warning
    SetTech(entry, CMolInfo::eTech_htgs_3);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // suppress if complete sequence or complete genome in title
    SetTitle(entry, "complete sequence");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // suppress if viral
    scope.RemoveTopLevelSeqEntry(seh);
    SetTitle(entry, "a title without the word");
    entry->SetSeq().SetId().front()->SetEmbl().SetAccession("AY123457");
    unit_test_util::SetLineage(entry, "Viruses");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // suppress if artificial
    unit_test_util::SetLineage(entry, "Bacteria");
    unit_test_util::SetOrigin(entry, CBioSource::eOrigin_artificial);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_Descr_CollidingPublications)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> pub1 = unit_test_util::BuildGoodPubSeqdesc();
    CRef<CAuthor> auth1 = unit_test_util::BuildGoodAuthor();
    CRef<CPub> otherpub1(new CPub());
    otherpub1->SetArticle().SetAuthors().SetNames().SetStd().push_back(auth1);
    CRef<CCit_art::TTitle::C_E> title1(new CCit_art::TTitle::C_E());
    title1->SetName("First title");
    otherpub1->SetArticle().SetTitle().Set().push_back(title1);
    pub1->SetPub().SetPub().Set().push_back(otherpub1);
    entry->SetSeq().SetDescr().Set().push_back(pub1);
    CRef<CSeqdesc> pub2 = unit_test_util::BuildGoodPubSeqdesc();
    CRef<CPub> otherpub2(new CPub());
    CRef<CAuthor> auth2 = unit_test_util::BuildGoodAuthor();
    otherpub2->SetArticle().SetAuthors().SetNames().SetStd().push_back(auth1);
    CRef<CCit_art::TTitle::C_E> title2(new CCit_art::TTitle::C_E());
    title2->SetName("Second title");
    otherpub2->SetArticle().SetTitle().Set().push_back(title2);
    pub2->SetPub().SetPub().Set().push_back(otherpub2);
    entry->SetSeq().SetDescr().Set().push_back(pub2);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CollidingPublications",
                              "Multiple publications with same identifier"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // should also report muid collisions
    pub1->SetPub().SetPub().Set().front()->SetMuid(2);
    pub2->SetPub().SetPub().Set().front()->SetMuid(2);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // look for same pub twice
    title2->SetName("First title");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CollidingPublications",
        "Multiple equivalent publications annotated on this sequence [Last|Ft; Last]"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[1];
    expected_errors.pop_back();

    // look for multiple IDs on same pub
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetDescr().Set().pop_back();
    CRef<CPub> extra_id(new CPub());
    extra_id->SetMuid(3);
    pub1->SetPub().SetPub().Set().push_back(extra_id);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Multiple conflicting muids in a single publication");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    extra_id->SetMuid(2);
    expected_errors[0]->SetErrMsg("Multiple redundant muids in a single publication");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    pub1->SetPub().SetPub().Set().front()->SetPmid((CPub::TPmid)2);
    extra_id->SetPmid((CPub::TPmid)3);
    expected_errors[0]->SetErrMsg("Multiple conflicting pmids in a single publication");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    extra_id->SetPmid((CPub::TPmid)2);
    expected_errors[0]->SetErrMsg("Multiple redundant pmids in a single publication");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_TransgenicProblem)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_transgenic, "true");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TransgenicProblem",
                              "Transgenic source descriptor requires presence of source feature"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    // adding source feature turns off warning
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetBiosrc().SetOrg().SetTaxname("Trichechus manatus");
    unit_test_util::SetTaxon (feat->SetData().SetBiosrc(), 127582);
    feat->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("some lineage");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(5);
    unit_test_util::AddFeat(feat, entry);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_TaxonomyLookupProblem)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(entry, "Not valid");
    unit_test_util::SetTaxon(entry, 0);

    STANDARD_SETUP
        
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NoTaxonID",
        "BioSource is missing taxon ID"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "OrganismNotFound",
                              "Organism not found in taxonomy database"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NoTaxonID",
        "BioSource is missing taxon ID"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TaxonomyIsSpeciesProblem",
                              "Taxonomy lookup reports is_species_level FALSE"));
    unit_test_util::SetTaxname(entry, "Poeciliinae");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NoTaxonID",
        "BioSource is missing taxon ID"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TaxonomyConsultRequired",
                              "Taxonomy lookup reports taxonomy consultation needed"));
    unit_test_util::SetTaxname(entry, "Anabaena circinalis");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    unit_test_util::SetTaxname(entry, "Homo sapiens");
    unit_test_util::SetGenome(entry, CBioSource::eGenome_nucleomorph);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadOrganelle",
                              "Only Chlorarachniophyceae and Cryptophyta have nucleomorphs"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NoTaxonID",
        "BioSource is missing taxon ID"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TaxonomyNucleomorphProblem",
                              "Taxonomy lookup does not have expected nucleomorph flag"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_MultipleTitles)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    SetTitle(entry, "First title");
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetTitle("Second title");
    entry->SetSeq().SetDescr().Set().push_back(desc);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MultipleTitles",
                              "Undesired multiple title descriptors"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_RefGeneTrackingOnNonRefSeq)
{
    // prepare entry
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    CRef<CSeq_entry> firstseq = unit_test_util::BuildGoodSeq();
    AddRefGeneTrackingUserObject (firstseq);
    entry->SetSet().SetSeq_set().push_back(firstseq);

    CRef<CSeq_entry> secondseq = unit_test_util::BuildGoodSeq();
    secondseq->SetSeq().SetId().front()->SetLocal().SetStr("good2");
    entry->SetSet().SetSeq_set().push_back(secondseq);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "RefGeneTrackingOnNonRefSeq",
                              "RefGeneTracking object should only be in RefSeq record"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // no error if any bioseq in record is RefSeq
    scope.RemoveTopLevelSeqEntry(seh);
    secondseq->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_BioSourceInconsistency)
{
   // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(entry, "Arabidopsis thaliana");
    unit_test_util::SetTaxon(entry, 0);
    unit_test_util::SetTaxon(entry, 3702);
    unit_test_util::SetLineage(entry, "Cyanobacteria");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_variety, "foo");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Variety value specified is not found in taxname"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "OrganismNotFound",
        "Organism not found in taxonomy database (suggested:Arabidopsis thaliana var. foo)"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
   
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_variety, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_forma, "foo");
    expected_errors[0]->SetErrMsg("Forma value specified is not found in taxname");
    expected_errors[1]->SetErrMsg("Organism not found in taxonomy database (suggested:Arabidopsis thaliana f. foo)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_forma, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_sub_species, "foo");
    expected_errors[0]->SetErrMsg("Subspecies value specified is not found in taxname");
    expected_errors[1]->SetErrMsg("Organism not found in taxonomy database (suggested:Arabidopsis thaliana subsp. foo)");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
    // this one does not cause taxname lookup to fail
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_sub_species, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_forma_specialis, "foo");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, 
                              "BioSourceInconsistency",
                              "Forma specialis value specified is not found in taxname"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // some don't produce errors
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_forma_specialis, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_biovar, "foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_biovar, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_pathovar, "foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // HIV location problems
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_pathovar, "");
    unit_test_util::SetTaxname(entry, "Human immunodeficiency virus");
    unit_test_util::SetTaxon(entry, 0);
    unit_test_util::SetTaxon(entry, 12721);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "HIV with moltype DNA should be proviral"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_mRNA);
    expected_errors[0]->SetErrMsg("HIV with mRNA molecule type is rare");
    expected_errors[0]->SetSeverity(eDiag_Info);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // descriptive text in non-text qualifiers
    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetTaxname(entry, "Arabidopsis thaliana");
    unit_test_util::SetTaxon(entry, 0);
    unit_test_util::SetTaxon(entry, 3702);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_germline, "a");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_rearranged, "a");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_transgenic, "a");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "a");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_metagenomic, "a");
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetBiosrc().SetOrg().SetTaxname("Trichechus manatus");
    unit_test_util::SetTaxon (feat->SetData().SetBiosrc(), 127582);
    feat->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("some lineage");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(5);
    unit_test_util::AddFeat (feat, entry);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Germline qualifier should not have descriptive text"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Rearranged qualifier should not have descriptive text"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Transgenic qualifier should not have descriptive text"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Environmental_sample qualifier should not have descriptive text"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Metagenomic qualifier should not have descriptive text"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Germline and rearranged should not both be present"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Transgenic and environmental sample should not both be present"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Environmental sample should also have isolation source or specific host annotated"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
 
    // unexpected sex qualifier
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetLineage(entry, "Viruses; foo");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_sex, "a");
    unit_test_util::SetLineage(entry, "Bacteria; foo");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Unexpected use of /sex qualifier"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Unexpected use of /sex qualifier"));
    unit_test_util::SetLineage(entry, "Archaea; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage(entry, "Eukaryota; Fungi; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage(entry, "");
    expected_errors[0]->SetErrMsg("Invalid value (a) for /sex qualifier");
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingLineage",
                              "No lineage for this BioSource."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingLineage",
                              "No lineage for this BioSource."));

    // no error if acceptable value
    vector<string> ok_sex_vals;
    ok_sex_vals.push_back("female");
    ok_sex_vals.push_back("male");
    ok_sex_vals.push_back("hermaphrodite");
    ok_sex_vals.push_back("unisexual");
    ok_sex_vals.push_back("bisexual");
    ok_sex_vals.push_back("asexual");
    ok_sex_vals.push_back("monoecious");
    ok_sex_vals.push_back("monecious");
    ok_sex_vals.push_back("dioecious");
    ok_sex_vals.push_back("diecious");

    for (vector<string>::iterator it = ok_sex_vals.begin();
         it != ok_sex_vals.end();
         ++it) {
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_sex, "");
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_sex, *it);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    CLEAR_ERRORS

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_sex, "");
    // mating-type error for animal
    unit_test_util::SetLineage(entry, "Eukaryota; Metazoa; foo");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_mating_type, "a");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Unexpected use of /mating_type qualifier"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // mating-type error for 3 plant lineages
    unit_test_util::SetLineage(entry, "Eukaryota; Viridiplantae; Streptophyta; Embryophyta; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage(entry, "Eukaryota; Rhodophyta; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage(entry, "Eukaryota; stramenopiles; Phaeophyceae; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // mating-type error for virus
    unit_test_util::SetLineage(entry, "Viruses; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // for other lineages, error if sex value
    unit_test_util::SetLineage(entry, "Eukaryota; Fungi; foo");
    for (vector<string>::iterator it = ok_sex_vals.begin();
         it != ok_sex_vals.end();
         ++it) {
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_mating_type, "");
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_mating_type, *it);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }
    CLEAR_ERRORS

    // no error if not valid sex value
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_mating_type, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_mating_type, "a");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // plasmid
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_mating_type, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_plasmid_name, "pfoo");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Plasmid subsource but not plasmid location"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // error goes away if plasmid genome
    CLEAR_ERRORS

    unit_test_util::SetGenome (entry, CBioSource::eGenome_plasmid);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // if plasmid genome, better have plasmid name
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_plasmid_name, "");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Plasmid location set but plasmid name missing. Add a plasmid source modifier with the plasmid name. Use unnamed if the name is not known."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    // plastid-name
    unit_test_util::SetGenome (entry, CBioSource::eGenome_unknown);
    vector<string> plastid_vals;
    plastid_vals.push_back("chloroplast");
    plastid_vals.push_back("chromoplast");
    plastid_vals.push_back("kinetoplast");
    plastid_vals.push_back("plastid");
    plastid_vals.push_back("apicoplast");
    plastid_vals.push_back("leucoplast");
    plastid_vals.push_back("proplastid");

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_plasmid_name, "");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Plastid name subsource chloroplast but not chloroplast location"));
    for (vector<string>::iterator it = plastid_vals.begin();
         it != plastid_vals.end();
         ++it) {
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_plastid_name, "");
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_plastid_name, *it);
        expected_errors[0]->SetErrMsg("Plastid name subsource " + *it + " but not " + *it + " location");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_plastid_name, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_plastid_name, "unrecognized");
    expected_errors[0]->SetErrMsg("Plastid name subsource contains unrecognized value");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_plastid_name, "");
    //frequency
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_frequency, "1");
    expected_errors[0]->SetSeverity(eDiag_Info);
    expected_errors[0]->SetErrMsg("bad frequency qualifier value 1");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_frequency, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_frequency, "abc");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetErrMsg("bad frequency qualifier value abc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_frequency, "");

    // unexpected qualifiers for viruses
    unit_test_util::SetLineage(entry, "Viruses; foo");
    unit_test_util::SetGenome(entry, CBioSource::eGenome_unknown);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_sex, ok_sex_vals[0]);
    expected_errors[0]->SetErrMsg("Virus has unexpected Sex qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_sex, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_cell_line, "foo");
    expected_errors[0]->SetErrMsg("Virus has unexpected Cell-line qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_cell_line, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_cell_type, "foo");
    expected_errors[0]->SetErrMsg("Virus has unexpected Cell-type qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_cell_type, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_tissue_type, "foo");
    expected_errors[0]->SetErrMsg("Virus has unexpected Tissue-type qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_tissue_type, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_dev_stage, "foo");
    expected_errors[0]->SetErrMsg("Virus has unexpected Dev-stage qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_dev_stage, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_breed, "bar");
    expected_errors[0]->SetErrMsg("Virus has unexpected Breed qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_breed, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_cultivar, "bar");
    expected_errors[0]->SetErrMsg("Virus has unexpected Cultivar qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_cultivar, "");

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_germline, "true");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_rearranged, "true");
    expected_errors[0]->SetErrMsg("Germline and rearranged should not both be present");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_germline, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_rearranged, "");

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_transgenic, "true");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "true");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_isolation_source, "foo");
    unit_test_util::SetFocus(entry);
    unit_test_util::AddGoodSourceFeature (entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Transgenic and environmental sample should not both be present"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_transgenic, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_isolation_source, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_metagenomic, "true");
    expected_errors[0]->SetErrMsg("Metagenomic should also have environmental sample annotated");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_metagenomic, "");
    unit_test_util::SetLineage(entry, "Eukaryota; foo");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_sex, "monecious");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_mating_type, "A");
    expected_errors[0]->SetErrMsg("Sex and mating type should not both be present");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_sex, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_mating_type, "");
    unit_test_util::SetLineage(entry, "Eukaryota; metagenomes");
    expected_errors[0]->SetErrMsg("If metagenomes appears in lineage, BioSource should have metagenomic qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    unit_test_util::SetTaxname (entry, "uncultured bacterium");
    unit_test_util::SetLineage (entry, "Bacteria; foo");
    unit_test_util::SetTaxon(entry, 0);
    unit_test_util::SetTaxon(entry, 77133);
    expected_errors[0]->SetErrMsg("Uncultured should also have /environmental_sample");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::MakeSeqLong(entry->SetSeq());
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "true");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_isolation_source, "foo");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Uncultured bacterium sequence length is suspiciously high"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "true");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Environmental sample should also have isolation source or specific host annotated");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "");
    unit_test_util::SetDiv(entry, "BCT");
    unit_test_util::SetGenome(entry, CBioSource::eGenome_apicoplast);
    expected_errors[0]->SetErrMsg("Bacterial or viral source should not have organelle location");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetDiv(entry, "VRL");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetDiv(entry, "ENV");
    unit_test_util::SetGenome(entry, CBioSource::eGenome_unknown);
    expected_errors[0]->SetErrMsg("BioSource with ENV division is missing environmental sample subsource");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetDiv(entry, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "true");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_isolation_source, "foo");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "bar");
    expected_errors[0]->SetErrMsg("Strain should not be present in an environmental sample");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_isolation_source, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_metagenome_source, "foo");
    expected_errors[0]->SetErrMsg("Metagenome source should also have metagenomic qualifier");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_metagenome_source, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_synonym, "synonym value");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_gb_synonym, "synonym value");
    expected_errors[0]->SetErrMsg("OrgMod synonym is identical to OrgMod gb_synonym");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_synonym, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_gb_synonym, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_other, "cRNA");
    expected_errors[0]->SetErrMsg("cRNA note conflicts with molecule type");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_cRNA);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    expected_errors[0]->SetErrMsg("cRNA note redundant with molecule type");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_other, "");
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_genomic);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
    unit_test_util::SetLineage (entry, "Viruses; no DNA stage");
    expected_errors[0]->SetErrMsg("Genomic DNA viral lineage indicates no DNA stage");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetLineage (entry, "Bacteria; foo");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_other, "cRNA");
    expected_errors[0]->SetErrMsg("cRNA note conflicts with molecule type");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_cRNA);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    expected_errors[0]->SetErrMsg("cRNA note redundant with molecule type");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Negative-strand virus with plus strand CDS should be mRNA or cRNA"));
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetLineage(entry, "Viruses; negative-strand viruses");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("lcl|nuc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // error goes away if mRNA or cRNA or ambisense or synthetic
    CLEAR_ERRORS

    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_mRNA);
    entry->SetSet().SetSeq_set().front()->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_cRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_genomic);
    unit_test_util::SetLineage (entry, "Viruses; negative-strand viruses; Arenavirus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage (entry, "Viruses; negative-strand viruses; Phlebovirus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage (entry, "Viruses; negative-strand viruses; Tospovirus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage (entry, "Viruses; negative-strand viruses; Tenuivirus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage(entry, "Viruses; negative-strand viruses");
    unit_test_util::SetOrigin (entry, CBioSource::eOrigin_synthetic);
    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_genomic);
    unit_test_util::SetDiv(entry, "VRL");
    unit_test_util::SetOrigin (entry, CBioSource::eOrigin_mut);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrigin (entry, CBioSource::eOrigin_artificial);
    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_other_genetic);
    unit_test_util::SetSynthetic_construct(entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrigin (entry, CBioSource::eOrigin_synthetic);
    unit_test_util::SetSebaea_microphylla(entry);
    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrigin(entry, CBioSource::eOrigin_unknown);
    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_genomic);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    // still no error if genomic
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // error if not genomic
    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_mRNA);
    entry->SetSet().SetSeq_set().front()->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "CDSonMinusStrandMRNA",
                                                "CDS should not be on minus strand of mRNA molecule"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BioSourceInconsistency",
                                                "Negative-strand virus with minus strand CDS should be genomic")); 
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetLineage(entry, "Viruses; negative-strand viruses");
    CRef<CSeq_feat> misc_feat = unit_test_util::AddMiscFeature (entry);
    misc_feat->SetComment("nonfunctional");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                                                "Negative-strand virus with nonfunctional plus strand misc_feature should be mRNA or cRNA"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // error goes away if mRNA or cRNA or ambisense or synthetic
    CLEAR_ERRORS

    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_mRNA);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_cRNA);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_genomic);
    unit_test_util::SetLineage (entry, "Viruses; negative-strand viruses; Arenavirus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage (entry, "Viruses; negative-strand viruses; Phlebovirus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage (entry, "Viruses; negative-strand viruses; Tospovirus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage (entry, "Viruses; negative-strand viruses; Tenuivirus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    // still no error if genomic
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // error if not genomic
    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_mRNA);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                                                "Negative-strand virus with nonfunctional minus strand misc_feature should be genomic")); 
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // report missing env_sample/strain/isolate if bacterial and biosample
    unit_test_util::SetLineage (entry, "Bacteria; foo");
    CRef<CSeqdesc> biosample(new CSeqdesc());
    biosample->SetUser().SetType().SetStr("DBLink");
    CRef<CUser_field> f(new CUser_field());
    f->SetLabel().SetStr("BioSample");
    f->SetData().SetStr("PRJNA12345");
    biosample->SetUser().SetData().push_back(f);
    entry->SetSeq().SetDescr().Set().push_back(biosample);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BioSourceInconsistency",
                              "Bacteria should have strain or isolate or environmental sample"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "DBLinkProblem",
                              "Bad BioSample format - PRJNA12345"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // no error if strain, isolate, or environmental sample set
    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "bar");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "DBLinkProblem",
        "Bad BioSample format - PRJNA12345"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_isolate, "bar");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_isolate, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "true");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Environmental sample should also have isolation source or specific host annotated"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // for VR-173
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetLineage(entry, "Bacteria; foo");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_tissue_type, "X");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "Y");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BioSourceInconsistency",
        "Tissue-type is inappropriate for bacteria"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);


}


BOOST_AUTO_TEST_CASE(Test_SingleStrandViruses)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetLineage(entry, "Viruses; ssRNA positive-strand viruses");
    unit_test_util::SetBiomol(entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_cRNA);
    entry->SetSet().SetSeq_set().front()->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BioSourceInconsistency",
        "Plus-strand virus with plus strand CDS should be genomic RNA or mRNA"));
    
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // error goes away if ambisense or synthetic
    CLEAR_ERRORS

    unit_test_util::SetLineage(entry, "Viruses; ssRNA positive-strand viruses; Arenavirus");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    unit_test_util::SetLineage(entry, "Viruses; ssRNA positive-strand viruses; Phlebovirus");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    unit_test_util::SetLineage(entry, "Viruses; ssRNA positive-strand viruses; Tospovirus");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    unit_test_util::SetLineage(entry, "Viruses; ssRNA positive-strand viruses; Tenuivirus");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    unit_test_util::SetLineage(entry, "Viruses; ssRNA positive-strand viruses");
    unit_test_util::SetOrigin(entry, CBioSource::eOrigin_synthetic);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "InvalidForType",
        "Molinfo-biomol other should be used if Biosource-location is synthetic"));
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS
    unit_test_util::SetDiv(entry, "VRL");
    unit_test_util::SetOrigin(entry, CBioSource::eOrigin_mut);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    unit_test_util::SetOrigin(entry, CBioSource::eOrigin_artificial);
    unit_test_util::SetSynthetic_construct(entry);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "InvalidForType",
        "artificial origin should have other-genetic"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "InvalidForType",
        "synthetic construct should have other-genetic"));
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_FastaBracketTitle)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    SetTitle (entry, "[a=b]");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FastaBracketTitle",
                              "Title may have unparsed [...=...] construct"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // no error if TMSMART or BankIt
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> other(new CSeq_id());
    other->SetGeneral().SetDb("TMSMART");
    other->SetGeneral().SetTag().SetStr("good");
    entry->SetSeq().SetId().push_back(other);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    other->SetGeneral().SetDb("BankIt");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_MissingText)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetComment();
    entry->SetSeq().SetDescr().Set().push_back(desc);

    STANDARD_SETUP

    // comment
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingText",
                              "Comment descriptor needs text"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // title
    scope.RemoveTopLevelSeqEntry(seh);
    desc->SetTitle();
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Title descriptor needs text");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // name
    scope.RemoveTopLevelSeqEntry(seh);
    desc->SetName();
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Name descriptor needs text");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // region
    scope.RemoveTopLevelSeqEntry(seh);
    desc->SetRegion();
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Region descriptor needs text");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadCollectionDate)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "May 1, 2010");

    STANDARD_SETUP

    // bad format
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadCollectionDate",
                              "Collection_date format is not in DD-Mmm-YYYY format"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // still bad format
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "1-05-2010");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    // range has bad format
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "21-Oct-2013-20-Oct-2015");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
   
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "31-Dec-2099");
    expected_errors[0]->SetErrMsg("Collection_date is in the future");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // range in future
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "21-Oct-2013/20-Oct-2030");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // ISO date should be ok
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "2003-09-29");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // range of dates should be ok
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "Aug-2012/Jan-2013");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "2012/2013");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_collection_date, "06-Aug-2004/07-Jan-2007");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    bool bad_format = false, in_future = false;
    CSubSource::IsCorrectDateFormat("29-Feb-2012", bad_format, in_future);
    BOOST_CHECK_EQUAL(bad_format, false);
    BOOST_CHECK_EQUAL(in_future, false);

    CSubSource::IsCorrectDateFormat("2014-06", bad_format, in_future);
    BOOST_CHECK_EQUAL(bad_format, false);
    BOOST_CHECK_EQUAL(in_future, false);

}


BOOST_AUTO_TEST_CASE(Test_Descr_BadPCRPrimerSequence)
{
    char bad_ch;
    BOOST_CHECK_EQUAL(CPCRPrimerSeq::IsValid("01-May-2010", bad_ch), false);
    BOOST_CHECK_EQUAL(bad_ch, '0');

    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_fwd_primer_seq, "May 1, 2010");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadPCRPrimerSequence",
                              "PCR forward primer sequence format is incorrect, first bad character is '?'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadPCRPrimerSequence",
                              "PCR primer does not have both sequences"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_fwd_primer_seq, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_seq, "01-May-2010");
    expected_errors[0]->SetErrMsg("PCR reverse primer sequence format is incorrect, first bad character is '0'");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_seq, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_seq, "AAATQAA");
    expected_errors[0]->SetErrMsg("PCR reverse primer sequence format is incorrect, first bad character is 'q'");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_seq, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_seq, "AAATGAA;AA");
    expected_errors[0]->SetErrMsg("PCR reverse primer sequence format is incorrect, first bad character is '?'");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_seq, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_seq, "(AAATGAA,WW)");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_fwd_primer_seq, "(AAATGAA,W:W)");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_fwd_primer_seq, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_seq, "");
    NON_CONST_ITERATE(CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsSource()) {
            CRef<CPCRPrimer> fwd(new CPCRPrimer());
            fwd->SetName().Set("AATTGGCCAATTGGC");
            fwd->SetSeq().Set("AATTGGCCAATTGG4C");
            CRef<CPCRReaction> reaction(new CPCRReaction());
            reaction->SetForward().Set().push_back(fwd);
            CRef<CPCRPrimer> rev(new CPCRPrimer());
            rev->SetName().Set("AATTGGCCAATTGGC");
            rev->SetSeq().Set("AATTGGCCAATTGG5C");
            reaction->SetReverse().Set().push_back(rev);
            (*it)->SetSource().SetPcr_primers().Set().push_back(reaction);
        }
    }

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadPCRPrimerSequence",
                              "PCR forward primer sequence format is incorrect, first bad character is '4'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadPCRPrimerName",
                              "PCR forward primer name appears to be a sequence"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadPCRPrimerSequence",
                              "PCR reverse primer sequence format is incorrect, first bad character is '5'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadPCRPrimerName",
                              "PCR reverse primer name appears to be a sequence"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_Descr_ModifyPCRPrimer)
{
    string fwd_seq;
    fwd_seq.assign("5-agtctctctc-");
    bool modified = CPCRPrimerSeq::TrimJunk(fwd_seq);
    BOOST_CHECK_EQUAL(modified, true);
    BOOST_CHECK_EQUAL(fwd_seq, string("agtctctctc"));
    
    fwd_seq.assign("5`aattggccaattg3'");
    modified = CPCRPrimerSeq::TrimJunk(fwd_seq);
    BOOST_CHECK_EQUAL(modified, true);
    BOOST_CHECK_EQUAL(fwd_seq, string("aattggccaattg"));

    fwd_seq.assign("aattggccaacct");
    modified = CPCRPrimerSeq::TrimJunk(fwd_seq);
    BOOST_CHECK_EQUAL(modified, false);
    BOOST_CHECK_EQUAL(fwd_seq, string("aattggccaacct"));

    fwd_seq.assign("agttt<I>tagaga<i>gac");
    modified = CPCRPrimerSeq::Fixi(fwd_seq);
    BOOST_CHECK_EQUAL(modified, true);
    BOOST_CHECK_EQUAL(fwd_seq, string("agttt<i>tagaga<i>gac"));

    fwd_seq.assign("agtccat<iagata>gtct");
    modified = CPCRPrimerSeq::Fixi(fwd_seq);
    BOOST_CHECK_EQUAL(modified, true);
    BOOST_CHECK_EQUAL(fwd_seq, string("agtccat<i>agata>gtct"));

    fwd_seq.assign("agtccat<i>gtctaaa");
    modified = CPCRPrimerSeq::Fixi(fwd_seq);
    BOOST_CHECK_EQUAL(modified, false);
    BOOST_CHECK_EQUAL(fwd_seq, string("agtccat<i>gtctaaa"));

}

BOOST_AUTO_TEST_CASE(Test_Descr_BadPunctuation)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetTitle("abc.");
    entry->SetSeq().SetDescr().Set().push_back(desc);

    STANDARD_SETUP

    // end with period
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadPunctuation",
                              "Title descriptor ends in bad punctuation"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // end with comma
    desc->SetTitle("abc,");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // end with semicolon
    desc->SetTitle("abc;");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // end with colon
    desc->SetTitle("abc:");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadPCRPrimerName)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_fwd_primer_name, "(AAATGAA,WW)");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadPCRPrimerName",
                              "PCR primer name appears to be a sequence"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_fwd_primer_name, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_name, "(AAATGAA,W:W)");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // no error if invalid sequence
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_name, "");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_name, "AAATQAA");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_BioSourceOnProtein)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::AddGoodSource (entry->SetSet().SetSeq_set().back());
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BioSourceOnProtein",
                              "Nuc-prot set has 1 protein with a BioSource descriptor"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BioSourceDbTagConflict)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetDbxref (entry, "AFTOL", 12345);
    unit_test_util::SetDbxref (entry, "AFTOL", 12346);
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceDbTagConflict",
                              "BioSource uses db AFTOL multiple times"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_DuplicatePCRPrimerSequence)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_fwd_primer_seq, "(AAATTTGGGCCC,AAATTTGGGCCC)");
    unit_test_util::SetSubSource (entry, CSubSource::eSubtype_rev_primer_seq, "(CCCTTTGGGCCC,CCCTTTGGGCCC)");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "DuplicatePCRPrimerSequence",
                              "PCR primer sequence has duplicates"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_MultipleNames)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> d1(new CSeqdesc());
    d1->SetName("name #1");
    entry->SetSeq().SetDescr().Set().push_back(d1);
    CRef<CSeqdesc> d2(new CSeqdesc());
    d2->SetName("name #1");
    entry->SetSeq().SetDescr().Set().push_back(d2);
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultipleNames",
                              "Undesired multiple name descriptors, identical text"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    d2->SetName("name #2");
    expected_errors[0]->SetErrMsg("Undesired multiple name descriptors, different text");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_MultipleComments)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> d1(new CSeqdesc());
    d1->SetComment("name 1");
    entry->SetSeq().SetDescr().Set().push_back(d1);
    CRef<CSeqdesc> d2(new CSeqdesc());
    d2->SetComment("name 1");
    entry->SetSeq().SetDescr().Set().push_back(d2);
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultipleComments",
                              "Undesired multiple comment descriptors, identical text"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // ok if different
    d2->SetComment("name 2");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_Descr_LatLonFormat)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "40 N 50 E, abc");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "LatLonFormat",
                              "lat_lon format has extra text after correct dd.dd N|S ddd.dd E|W format"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "40 E 50 N");
    expected_errors[0]->SetErrMsg("lat_lon format is incorrect - should be dd.dd N|S ddd.dd E|W");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_LatLonRange)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "90.1 N 181 E");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "LatLonRange",
                              "latitude value is out of range - should be between 90.00 N and 90.00 S"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "LatLonRange",
                              "longitude value is out of range - should be between 180.00 E and 180.00 W"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "90.1 S 181 W");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadAltitude)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_altitude, "123 m");
    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_altitude, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_altitude, "123");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadAltitude",
                              "bad altitude qualifier value 123"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // raise to error
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options | CValidator::eVal_genome_submission);
    CheckErrors(*eval, expected_errors);


    CLEAR_ERRORS

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_altitude, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_altitude, "123 ft.");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadAltitude",
                              "bad altitude qualifier value 123 ft."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // raise to error
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options | CValidator::eVal_genome_submission);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    BOOST_CHECK_EQUAL(CSubSource::FixAltitude("123 ft."), "37 m");
}


void TestSpecificHostNoError(const string& host)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, host);
    
    STANDARD_SETUP
    options |= CValidator::eVal_use_entrez;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadSpecificHost)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Metapone madagascaria");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadSpecificHost",
                              "Specific host value is misspelled: Metapone madagascaria"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Homo Sapiens");
    expected_errors[0]->SetErrMsg("Specific host value is incorrectly capitalized: Homo Sapiens");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Homo nonrecognizedus");
    expected_errors[0]->SetErrMsg("Invalid value for specific host: Homo nonrecognizedus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Aedes sp.");
    expected_errors[0]->SetErrCode("AmbiguousSpecificHost");
    expected_errors[0]->SetSeverity(eDiag_Info);
    expected_errors[0]->SetErrMsg("Specific host value is ambiguous: Aedes sp.");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    // should not generate an error
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Bovine");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    // also, can ignore text after semicolon
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Homo sapiens; sex: female");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // should see errors for bad lineages
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Lentinula edodes");
    unit_test_util::SetLineage(entry, "Streptophyta");

    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "BadSpecificHost",
        "Suspect Host Value - a prokaryote, fungus or virus is suspect as a host for a plant or animal"));
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS

    // others
    TestSpecificHostNoError("Racoon");
    TestSpecificHostNoError("SNAKE");
    TestSpecificHostNoError("Snake");
    TestSpecificHostNoError("Turtle");


}

BOOST_AUTO_TEST_CASE(Test_Validity_SpecificHost)
{
	string host, error_msg;

    host = "Racoon";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);

    host = "SNAKE";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);

    host = "Snake";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);

    host = "Turtle";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);


	host = "Homo sapiens";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);
	
	host = "Homo supiens";
	BOOST_CHECK_EQUAL(false, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, string("Invalid value for specific host: Homo supiens"));
	
	host = "Pinus sp.";
	BOOST_CHECK_EQUAL(false, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, string("Specific host value is ambiguous: Pinus"));
	
	host = "Gallus Gallus";
	BOOST_CHECK_EQUAL(false, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, string("Specific host value is incorrectly capitalized: Gallus Gallus"));
	
	host = "Eschericia coli";
	BOOST_CHECK_EQUAL(false, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, string("Specific host value is misspelled: Eschericia coli"));
	
	host = "Avian";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);
	
	host = "Bovine";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);
	
	host = "Pig";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);
	
	host = "Chicken";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);
	
    host = "turtle";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);

    host = "Homo sapiens; sex: female";
	BOOST_CHECK_EQUAL(true, IsSpecificHostValid(host, error_msg));
	BOOST_CHECK_EQUAL(error_msg, kEmptyStr);

}


BOOST_AUTO_TEST_CASE(Test_FixSpecificHost)
{
	string hostfix, host;

    host = "homo sapiens";
    hostfix = FixSpecificHost(host);
    BOOST_CHECK_EQUAL(hostfix, "Homo sapiens");

	host = "Homo supiens";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, kEmptyStr);
	
	host = "Pinus sp.";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, kEmptyStr);
		
	host = "Gallus Gallus";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, string("Gallus gallus"));
		
	host = "Eschericia coli";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, string("Escherichia coli"));
	
	host = "Avian";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, host);
	
	host = "";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, kEmptyStr);
	
	host = "Bovine";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, string("Bovine"));
	
	host = "Homo sapiens";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, string("Homo sapiens"));
	
	host = "Pig";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, string("Pig"));
	
	host = " Chicken";	
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, string("Chicken"));
	
    host = "Homo sapiens; sex: female";
	hostfix = FixSpecificHost(host);
	BOOST_CHECK_EQUAL(hostfix, host);

    host = "HUMAN";
    hostfix = FixSpecificHost(host);
    BOOST_CHECK_EQUAL(hostfix, "Homo sapiens");
}


BOOST_AUTO_TEST_CASE(Test_Descr_RefGeneTrackingIllegalStatus)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    AddRefGeneTrackingUserObject(entry);
    SetRefGeneTrackingStatus(entry, "unknown");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Error, "RefGeneTrackingIllegalStatus",
                              "RefGeneTracking object has illegal Status 'unknown'"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_ReplacedCountryCode)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    vector<string> old_countries;
    old_countries.push_back("Belgian Congo");
    old_countries.push_back("British Guiana");
    old_countries.push_back("Burma");
    old_countries.push_back("Czechoslovakia");
    old_countries.push_back("Korea");
    old_countries.push_back("Serbia and Montenegro");
    old_countries.push_back("Siam");
    old_countries.push_back("USSR");
    old_countries.push_back("Yugoslavia");
    old_countries.push_back("Zaire");
    old_countries.push_back("Macedonia");

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "ReplacedCountryCode",
                              ""));

    ITERATE (vector<string>, it, old_countries) {
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, *it);
        expected_errors[0]->SetErrMsg("Replaced country name [" + *it + "]");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "");
    }

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadInstitutionCode)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadInstitutionCode",
                              "Voucher is missing institution code"));
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, ":foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, ":foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, ":foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "");

    // codes that need disambiguating country
    expected_errors[0]->SetSeverity(eDiag_Warning);
    vector<string> ambig;
    // specimen voucher codes
    ambig.push_back("BAH");
    ambig.push_back("ACE");
    ambig.push_back("SLU");
    ambig.push_back("UAB");
    ambig.push_back("CAIM");
    ambig.push_back("HER");
    ambig.push_back("DSC");
    ambig.push_back("NMW");
    ambig.push_back("DNHM");
    ambig.push_back("BNHM");
    ambig.push_back("UI");
    ambig.push_back("KMK");
    ambig.push_back("MZUT");
    ambig.push_back("MT");
    ambig.push_back("MP");
    ambig.push_back("NASC");
    ambig.push_back("IZAC");
    ambig.push_back("CCG");
    ambig.push_back("PIN");
    ambig.push_back("ARC");
    ambig.push_back("HSU");
    ambig.push_back("CAUP");
    ambig.push_back("ISU");
    ambig.push_back("SDSU");
    ambig.push_back("GC");
    ambig.push_back("UNL");
    ambig.push_back("MZUP");
    ambig.push_back("MG");
    ambig.push_back("HNHM");
    ambig.push_back("PMS");
    ambig.push_back("LE");
    ambig.push_back("GCM");
    ambig.push_back("SU");
    ambig.push_back("TMP");
    ambig.push_back("DMNH");
    ambig.push_back("ZMUH");
    ambig.push_back("UMO");
    ambig.push_back("SMF");
    ambig.push_back("ZSP");
    ambig.push_back("TAU");
    ambig.push_back("MJG");
    ambig.push_back("DUM");
    ambig.push_back("ANU");
    ambig.push_back("CPAP");
    ambig.push_back("CSU");
    ambig.push_back("WACA");
    ambig.push_back("MMNH");
    ambig.push_back("ALA");
    ambig.push_back("RV");
    ambig.push_back("ABS");
    ambig.push_back("FM");
    ambig.push_back("HNU");
    ambig.push_back("PO");
    ambig.push_back("UNR");
    ambig.push_back("GAM");
    ambig.push_back("MCM");
    ambig.push_back("LU");
    ambig.push_back("SDM");
    ambig.push_back("PMK");
    ambig.push_back("VI");
    ambig.push_back("IMM");
    ambig.push_back("R");
    ambig.push_back("CHM");
    ambig.push_back("CMC");
    ambig.push_back("JSPC");
    ambig.push_back("YU");
    ambig.push_back("STM");
    ambig.push_back("RSM");
    ambig.push_back("BB");
    ambig.push_back("BHM");
    ambig.push_back("CBU");
    ambig.push_back("MCCM");
    ambig.push_back("SBM");
    ambig.push_back("MHNC");
    ambig.push_back("NMSU");
    ambig.push_back("OTM");
    ambig.push_back("LP");
    ambig.push_back("SME");
    ambig.push_back("OSU");
    ambig.push_back("PEM");
    ambig.push_back("UMF");
    ambig.push_back("CIS");
    ambig.push_back("LBG");
    ambig.push_back("CCAC");
    ambig.push_back("SNP");
    ambig.push_back("UT");
    ambig.push_back("IBA");
    ambig.push_back("UNCC");
    ambig.push_back("NHMC");
    ambig.push_back("BAC");
    ambig.push_back("PMG");
    ambig.push_back("MRC");
    ambig.push_back("ETH");
    ambig.push_back("OMC");
    ambig.push_back("NMV");
    ambig.push_back("MLS");
    ambig.push_back("NJM");
    ambig.push_back("INA");
    ambig.push_back("BCM");
    ambig.push_back("YM");
    ambig.push_back("CAM");
    ambig.push_back("UA");
    ambig.push_back("OSM");
    ambig.push_back("CPS");
    ambig.push_back("POKM");
    ambig.push_back("VSM");
    ambig.push_back("ZMG");
    ambig.push_back("IO");
    ambig.push_back("USM");
    ambig.push_back("UCS");
    ambig.push_back("CN");
    ambig.push_back("PCM");
    ambig.push_back("MB");
    ambig.push_back("MU");
    ambig.push_back("ISC");
    ambig.push_back("CIB");
    ambig.push_back("GML");
    ambig.push_back("NU");
    ambig.push_back("NCSC");
    ambig.push_back("MHNN");
    ambig.push_back("NCC");
    ambig.push_back("MSM");
    ambig.push_back("NMBA");
    ambig.push_back("RM");
    ambig.push_back("MBM");
    ambig.push_back("UPM");
    ambig.push_back("MSU");
    ambig.push_back("PI");
    ambig.push_back("CENA");
    ambig.push_back("IBRP");
    ambig.push_back("CUMZ");
    ambig.push_back("CRE");
    ambig.push_back("FSC");
    ambig.push_back("CHAS");
    ambig.push_back("ENCB");
    ambig.push_back("BAS");
    ambig.push_back("GOE");
    ambig.push_back("PSS");
    ambig.push_back("NHMB");
    ambig.push_back("CCB");
    ambig.push_back("SMNH");
    ambig.push_back("SUM");
    ambig.push_back("NMPG");
    ambig.push_back("USP");
    ambig.push_back("IPB");
    ambig.push_back("BCC");
    ambig.push_back("FNU");
    ambig.push_back("SHM");
    ambig.push_back("UM");
    ambig.push_back("TNSC");
    ambig.push_back("LS");
    ambig.push_back("TMC");
    ambig.push_back("HUT");
    ambig.push_back("ZMUO");
    ambig.push_back("ALM");
    ambig.push_back("ITCC");
    ambig.push_back("TM");
    ambig.push_back("WB");
    ambig.push_back("ZMK");
    ambig.push_back("LBM");
    ambig.push_back("NI");
    ambig.push_back("TF");
    ambig.push_back("CB");
    ambig.push_back("AMP");
    ambig.push_back("OMNH");
    ambig.push_back("MM");
    ambig.push_back("PMU");
    ambig.push_back("DM");
    ambig.push_back("RIVE");
    ambig.push_back("TARI");
    ambig.push_back("CSCS");
    ambig.push_back("PSU");
    ambig.push_back("IMT");
    ambig.push_back("MZV");
    ambig.push_back("SZE");
    ambig.push_back("NSMC");
    ambig.push_back("CUVC");
    ambig.push_back("LMJ");
    ambig.push_back("UC");
    ambig.push_back("ZIUS");
    ambig.push_back("FRI");
    ambig.push_back("CDA");
    ambig.push_back("ZMUA");
    ambig.push_back("MZUC");
    ambig.push_back("UCR");
    ambig.push_back("CS");
    ambig.push_back("BR");
    ambig.push_back("UG");
    ambig.push_back("MDH");
    ambig.push_back("USD");
    ambig.push_back("MNHM");
    ambig.push_back("MAD");
    ambig.push_back("PMA");
    ambig.push_back("ICN");
    ambig.push_back("TU");
    ambig.push_back("PMNH");
    ambig.push_back("SAU");
    ambig.push_back("UKMS");
    ambig.push_back("KM");
    ambig.push_back("GMNH");
    ambig.push_back("SSM");
    ambig.push_back("MZ");
    ambig.push_back("WSU");
    ambig.push_back("CIAN");
    ambig.push_back("ZMT");
    ambig.push_back("IMS");
    ambig.push_back("TCDU");
    ambig.push_back("SIAC");
    ambig.push_back("DFEC");
    ambig.push_back("CBD");
    ambig.push_back("SWC");
    ambig.push_back("MD");
    ambig.push_back("FU");
    ambig.push_back("UV");
    ambig.push_back("URM");
    ambig.push_back("JNU");
    ambig.push_back("IZ");
    ambig.push_back("UCM");
    ambig.push_back("UAIC");
    ambig.push_back("LEB");
    ambig.push_back("MCSN");
    ambig.push_back("UU");
    ambig.push_back("PUC");
    ambig.push_back("OS");
    ambig.push_back("SNM");
    ambig.push_back("AKU");
    ambig.push_back("MH");
    ambig.push_back("MOR");
    ambig.push_back("IM");
    ambig.push_back("MSNT");
    ambig.push_back("IGM");
    ambig.push_back("NAP");
    ambig.push_back("NHMR");
    ambig.push_back("MW");
    ambig.push_back("PPCC");
    ambig.push_back("CNHM");
    ambig.push_back("NSM");
    ambig.push_back("IAL");
    ambig.push_back("PCU");
    ambig.push_back("HM");

    ITERATE (vector<string>, it, ambig) {
        expected_errors[0]->SetErrMsg("Institution code " + *it + " needs to be qualified with a <COUNTRY> designation");
        unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, *it + ":foo");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "");
    }

    // bio-material
    ambig.clear();
    ambig.push_back("NASC");
    ambig.push_back("TCDU");

    ITERATE (vector<string>, it, ambig) {
        expected_errors[0]->SetErrMsg("Institution code " + *it + " needs to be qualified with a <COUNTRY> designation");
        unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, *it + ":foo");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");
    }

    // culture-collection
    ambig.clear();
    ambig.push_back("CAIM");
    ambig.push_back("STM");
    ambig.push_back("HER");
    ambig.push_back("FSC");
    ambig.push_back("MDH");
    ambig.push_back("DSC");
    ambig.push_back("IFM");
    ambig.push_back("MCCM");
    ambig.push_back("CCB");
    ambig.push_back("LBG");
    ambig.push_back("BCC");
    ambig.push_back("CCAC");
    ambig.push_back("CCF");
    ambig.push_back("IBA");
    ambig.push_back("CAUP");
    ambig.push_back("MRC");
    ambig.push_back("ETH");
    ambig.push_back("TMC");
    ambig.push_back("CBD");
    ambig.push_back("HUT");
    ambig.push_back("URM");
    ambig.push_back("NJM");
    ambig.push_back("INA");
    ambig.push_back("BTCC");
    ambig.push_back("YM");
    ambig.push_back("UCM");
    ambig.push_back("IZ");
    ambig.push_back("ITCC");
    ambig.push_back("WB");
    ambig.push_back("LE");
    ambig.push_back("LCC");
    ambig.push_back("LBM");
    ambig.push_back("NI");
    ambig.push_back("CB");
    ambig.push_back("AMP");
    ambig.push_back("RIVE");
    ambig.push_back("DUM");
    ambig.push_back("AKU");
    ambig.push_back("CN");
    ambig.push_back("CCDM");
    ambig.push_back("PCM");
    ambig.push_back("MU");
    ambig.push_back("ISC");
    ambig.push_back("IMT");
    ambig.push_back("NU");
    ambig.push_back("RV");
    ambig.push_back("UC");
    ambig.push_back("NCSC");
    ambig.push_back("CCY");
    ambig.push_back("NCC");
    ambig.push_back("FRI");
    ambig.push_back("GAM");
    ambig.push_back("RM");
    ambig.push_back("MCM");
    ambig.push_back("PPCC");
    ambig.push_back("CDA");
    ambig.push_back("IAL");
    ambig.push_back("VI");
    ambig.push_back("CS");
    ambig.push_back("PCU");
    ambig.push_back("CVCC");
    ambig.push_back("BR");
    ambig.push_back("MSU");
    ITERATE (vector<string>, it, ambig) {
        expected_errors[0]->SetErrMsg("Institution code " + *it + " needs to be qualified with a <COUNTRY> designation");
        unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, *it + ":foo");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "");
    }

    expected_errors[0]->SetErrMsg("Institution code zzz is not in list");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "zzz:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "zzz:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "zzz:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "");

    expected_errors[0]->SetErrMsg("Institution code abrc exists, but correct capitalization is ABRC");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "abrc:x");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");

    expected_errors[0]->SetErrMsg("Institution code a exists, but correct capitalization is A");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "a:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "");

    expected_errors[0]->SetErrMsg("Institution code abkmi exists, but correct capitalization is ABKMI");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "abkmi:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "");

    CLEAR_ERRORS

    // should be ok
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "CCS2009-043");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_BadCollectionCode)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadCollectionCode",
                              "Institution code ABRC exists, but collection ABRC:bar is not in list"));
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "ABRC:bar:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");

    expected_errors[0]->SetErrMsg("Institution code A exists, but collection A:bar is not in list");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "A:bar:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "");

    expected_errors[0]->SetErrMsg("Institution code ABKMI exists, but collection ABKMI:bar is not in list");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "ABKMI:bar:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "");

    CLEAR_ERRORS

    // DNA is ok for biomaterial
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "ABRC:DNA:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");

}


BOOST_AUTO_TEST_CASE(Test_Descr_BadVoucherID)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadVoucherID",
                              "Voucher is missing specific identifier"));
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "ABRC:");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "AAPI:");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "");

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "ABKMI:");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "");

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_UnstructuredVoucher)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "UnstructuredVoucher",
                              "Culture_collection should be structured, but is not"));
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "ABKMI");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "");

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_ChromosomeLocation)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "ChromosomeLocation",
                              "INDEXER_ONLY - BioSource location is chromosome"));
    unit_test_util::SetGenome(entry, CBioSource::eGenome_chromosome);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_MultipleSourceQualifiers)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultipleSourceQualifiers",
                              "Multiple country qualifiers present"));
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "USA");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "Zimbabwe");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "");

    expected_errors[0]->SetErrMsg("Multiple lat_lon qualifiers present");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "35 N 50 W");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "50 N 35 W");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "");

    expected_errors[0]->SetErrMsg("Multiple fwd_primer_seq qualifiers present");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultipleSourceQualifiers", "Multiple rev_primer_seq qualifiers present"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultipleSourceQualifiers", "Multiple fwd_primer_name qualifiers present"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultipleSourceQualifiers", "Multiple rev_primer_name qualifiers present"));
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_fwd_primer_seq, "AATTGGCC");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_fwd_primer_seq, "CCTTAAAA");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_rev_primer_seq, "AATTGGCC");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_rev_primer_seq, "CCTTAAAA");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_fwd_primer_name, "fwd1");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_fwd_primer_name, "fwd2");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_rev_primer_name, "rev1");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_rev_primer_name, "rev2");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static bool SubSourceHasOtherRules (CSubSource::TSubtype subtype) 
{
    if (subtype == CSubSource::eSubtype_sex
        || subtype == CSubSource::eSubtype_frequency
        || subtype == CSubSource::eSubtype_plasmid_name
        || subtype == CSubSource::eSubtype_transposon_name
        || subtype == CSubSource::eSubtype_insertion_seq_name
        || subtype == CSubSource::eSubtype_plastid_name
        || subtype == CSubSource::eSubtype_country
        || subtype == CSubSource::eSubtype_lat_lon
        || subtype == CSubSource::eSubtype_collection_date
        || subtype == CSubSource::eSubtype_fwd_primer_name
        || subtype == CSubSource::eSubtype_fwd_primer_seq
        || subtype == CSubSource::eSubtype_rev_primer_name
        || subtype == CSubSource::eSubtype_rev_primer_seq
        || subtype == CSubSource::eSubtype_country) {
        return true;
    } else {
        return false;
    }
}


static bool OrgModHasOtherRules (COrgMod::TSubtype subtype) 
{
    if (subtype == COrgMod::eSubtype_variety
        || subtype == COrgMod::eSubtype_sub_species
        || subtype == COrgMod::eSubtype_forma
        || subtype == COrgMod::eSubtype_forma_specialis
        || subtype == COrgMod::eSubtype_culture_collection
        || subtype == COrgMod::eSubtype_bio_material
        || subtype == COrgMod::eSubtype_specimen_voucher
        || subtype == COrgMod::eSubtype_metagenome_source) {
        return true;
    } else {
        return false;
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_UnbalancedParentheses)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "UnbalancedParentheses",
                              "Unbalanced parentheses in taxname 'Malio malefi (abc'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "OrganismNotFound",
                              "Organism not found in taxonomy database"));
    unit_test_util::SetTaxname(entry, "Malio malefi (abc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Unbalanced parentheses in taxname 'Malio malefi )abc'");
    unit_test_util::SetTaxname(entry, "Malio malefi )abc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSebaea_microphylla(entry);
    delete expected_errors[1];
    expected_errors.pop_back();

    for (CSubSource::TSubtype subtype = CSubSource::eSubtype_chromosome;
         subtype <= CSubSource::eSubtype_haplogroup;
         subtype++) {
        if (subtype != CSubSource::eSubtype_germline
            && subtype != CSubSource::eSubtype_rearranged
            && subtype != CSubSource::eSubtype_transgenic
            && subtype != CSubSource::eSubtype_environmental_sample
            && subtype != CSubSource::eSubtype_metagenomic) {
            if (SubSourceHasOtherRules(subtype)) {
                continue;
            }

            unit_test_util::SetSubSource(entry, subtype, "");
            unit_test_util::SetSubSource(entry, subtype, "no left (abc");
            expected_errors[0]->SetErrMsg("Unbalanced parentheses in subsource 'no left (abc'");
            eval = validator.Validate(seh, options);
            CheckErrors (*eval, expected_errors);
            unit_test_util::SetSubSource(entry, subtype, "");
            expected_errors[0]->SetErrMsg("Unbalanced parentheses in subsource 'no right )abc'");
            unit_test_util::SetSubSource(entry, subtype, "no right )abc");
            eval = validator.Validate(seh, options);
            CheckErrors (*eval, expected_errors);
            unit_test_util::SetSubSource(entry, subtype, "");
            if (subtype == CSubSource::eSubtype_chromosome) {
                unit_test_util::SetSubSource(entry, subtype, "1");
            }
        }
    }
    // also check other
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_other, "no left (abc");
    expected_errors[0]->SetErrMsg("Unbalanced parentheses in subsource 'no left (abc'");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_other, "");
    expected_errors[0]->SetErrMsg("Unbalanced parentheses in subsource 'no right )abc'");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_other, "no right )abc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_other, "");

    for (COrgMod::TSubtype subtype = COrgMod::eSubtype_strain;
         subtype <= COrgMod::eSubtype_metagenome_source;
         subtype++) {
        if (OrgModHasOtherRules(subtype)) {
            continue;
        }
        unit_test_util::SetOrgMod(entry, subtype, "no left (abc");
        expected_errors[0]->SetErrMsg("Unbalanced parentheses in orgmod 'no left (abc'");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::SetOrgMod(entry, subtype, "");
        expected_errors[0]->SetErrMsg("Unbalanced parentheses in orgmod 'no right )abc'");
        unit_test_util::SetOrgMod(entry, subtype, "no right )abc");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::SetOrgMod(entry, subtype, "");
    }
    // also check old_lineage and other
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_old_lineage, "no left (abc");
    expected_errors[0]->SetErrMsg("Unbalanced parentheses in orgmod 'no left (abc'");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_old_lineage, "");
    expected_errors[0]->SetErrMsg("Unbalanced parentheses in orgmod 'no right )abc'");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_old_lineage, "no right )abc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_old_lineage, "");

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_other, "no left (abc");
    expected_errors[0]->SetErrMsg("Unbalanced parentheses in orgmod 'no left (abc'");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_other, "");
    expected_errors[0]->SetErrMsg("Unbalanced parentheses in orgmod 'no right )abc'");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_other, "no right )abc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_other, "");

    CLEAR_ERRORS
    // should get no error for unbalanced parentheses in old name
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_old_name, "no left (abc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_Descr_MultipleSourceVouchers)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    // no errors if different institutions
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "ABRC:foo");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "AGRITEC:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // no errors if collection is DNA
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "ABRC:DNA:foo");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "ABRC:DNA:bar");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // errors if same institition:collection
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultipleSourceVouchers",
        "Multiple vouchers with same institution:collection"));
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "USDA:CFRA:foo");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "USDA:CFRA:bar");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // errors if same institition:collection
    expected_errors[0]->SetErrMsg("Multiple vouchers with same institution");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "USDA:CFRA:foo");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "USDA:GRIN:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadCountryCapitalization)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadCountryCapitalization",
        "Bad country capitalization [saint pierre and miquelon]"));
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_country, "saint pierre and miquelon");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_WrongVoucherType)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "WrongVoucherType",
        "Institution code ABRC should be bio_material"));
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "ABRC:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "ABRC:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "");

    expected_errors[0]->SetErrMsg("Institution code ABKMI should be culture_collection");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "ABKMI:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "ABKMI:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_specimen_voucher, "");

    expected_errors[0]->SetErrMsg("Institution code AA should be specimen_voucher");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "AA:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "AA:foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_culture_collection, "");

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_TitleHasPMID)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    SetTitle (entry, "foo bar something something (PMID 1)");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "TitleHasPMID",
                              "Title descriptor has internal PMID"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadKeyword)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetGenbank().SetKeywords().push_back("BARCODE");
    entry->SetSeq().SetDescr().Set().push_back(desc);
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadKeyword",
                              "BARCODE keyword without Molinfo.tech barcode"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetDescr().Set().pop_back();
    SetTech (entry, CMolInfo::eTech_barcode);
    expected_errors[0]->SetSeverity(eDiag_Info);
    expected_errors[0]->SetErrMsg("Molinfo.tech barcode without BARCODE keyword");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_NoOrganismInTitle)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123458");
    SetTitle(entry, "Something that does not start with organism");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("ref|NC_123458|", eDiag_Error, "NoOrganismInTitle",
                              "RefSeq nucleotide title does not start with organism name"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_id> other_id(new CSeq_id());
    other_id->SetOther().SetAccession("NP_123456");
    unit_test_util::ChangeProtId (entry, other_id);
    SetTitle(entry->SetSet().SetSeq_set().back(), "Something that does not end with organism");
    seh = scope.AddTopLevelSeqEntry(*entry);    

    expected_errors.push_back(new CExpectedError("ref|NP_123456|", eDiag_Error, "NoOrganismInTitle",
                                                 "RefSeq protein title does not end with organism name"));
    expected_errors.push_back(new CExpectedError("ref|NP_123456|", eDiag_Warning, "InconsistentProteinTitle",
        "Instantiated protein title does not match automatically generated title"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_MissingChromosome)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_chromosome, "");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Error, "MissingChromosome",
                              "Missing chromosome qualifier on NC or AC RefSeq record"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // error is suppressed if prokaryote or organelle
    unit_test_util::SetLineage (entry, "Viruses; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage (entry, "Bacteria; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
    unit_test_util::SetLineage (entry, "Archaea; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage (entry, "some lineage");
    unit_test_util::SetDiv(entry, "BCT");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetDiv(entry, "VRL");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetDiv(entry, "");

    // error is suppressed if linkage group
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_linkage_group, "x");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_linkage_group, "");

    // error is suppressed if organelle
    unit_test_util::SetGenome (entry, CBioSource::eGenome_chloroplast);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_chromoplast);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_kinetoplast);
    unit_test_util::SetLineage(entry, "some lineage; Kinetoplastida");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_mitochondrion);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_cyanelle);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_nucleomorph);
    unit_test_util::SetTaxname(entry, "Bigelowiella natans");
    unit_test_util::SetTaxon(entry, 0);
    unit_test_util::SetTaxon(entry, 227086);
    unit_test_util::SetLineage(entry, "some lineage; Chlorarachniophyceae");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    unit_test_util::SetGenome (entry, CBioSource::eGenome_apicoplast);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_leucoplast);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_proplastid);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_hydrogenosome);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_BadStructuredCommentFormat)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().SetType().SetStr("StructuredComment");
    entry->SetSeq().SetDescr().Set().push_back(desc);

    STANDARD_SETUP

    // no prefix only empty errors
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UserObjectProblem",
                                                 "Structured Comment user object descriptor is empty"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "UserObjectProblem",
                                                 "User object with no data"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "StructuredCommentPrefixOrSuffixMissing",
                                                 "Structured Comment lacks prefix"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // unrecognized prefix
    CRef<CUser_field> prefix_field(new CUser_field());
    prefix_field->SetLabel().SetStr("StructuredCommentPrefix");
    prefix_field->SetData().SetStr("Unknown prefix");
    desc->SetUser().SetData().push_back(prefix_field);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadStrucCommInvalidFieldValue",
                                    "Unknown prefix is not a valid value for StructuredCommentPrefix"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // should complain about missing required fields
    prefix_field->SetData().SetStr("##Genome-Assembly-Data-START##");
    vector<string> required_fields;
    /*
    required_fields.push_back("Finishing Goal");
    required_fields.push_back("Current Finishing Status");
    */
    required_fields.push_back("Assembly Method");
    required_fields.push_back("Genome Coverage");
    required_fields.push_back("Sequencing Technology");

    EDiagSev levels[] = { eDiag_Warning, eDiag_Warning, eDiag_Warning, eDiag_Warning, eDiag_Warning };

    int i = 0;
    ITERATE(vector<string>, it, required_fields) {
        expected_errors.push_back(new CExpectedError("lcl|good", levels[i], "BadStrucCommMissingField",
                                  "Required field " + *it + " is missing"));
        i++;
    }
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // add fields in wrong order, with bad values where appropriate
    const vector<string>& const_required_fields = required_fields;
    REVERSE_ITERATE(vector<string>, it, const_required_fields) {
        CRef<CUser_field> field(new CUser_field());
        field->SetLabel().SetStr(*it);
        field->SetData().SetStr("bad value");
        desc->SetUser().SetData().push_back(field);
    }

    size_t pos = 0;
    ITERATE(vector<string>, it, required_fields) {
        if (pos < required_fields.size() - 1) {
            expected_errors.push_back(new CExpectedError("lcl|good", levels[pos], "BadStrucCommFieldOutOfOrder",
                                      *it + " field is out of order"));
        }
        if (!NStr::Equal(*it, "Genome Coverage") && !NStr::Equal(*it, "Sequencing Technology")) {
            expected_errors.push_back(new CExpectedError("lcl|good", levels[pos], "BadStrucCommInvalidFieldValue",
                                      "bad value is not a valid value for " + *it));
        }
        ++pos;
    }
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));
    
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    prefix_field->SetData().SetStr("##MIGS-Data-START##");
    required_fields.clear();
    required_fields.push_back("alt_elev");
    required_fields.push_back("assembly");
    required_fields.push_back("collection_date");
    required_fields.push_back("country");
    required_fields.push_back("depth");
    required_fields.push_back("environment");
    required_fields.push_back("investigation_type");
    required_fields.push_back("isol_growth_condt");
    required_fields.push_back("lat_lon");
    required_fields.push_back("project_name");
    required_fields.push_back("sequencing_meth");

    ITERATE(vector<string>, it, required_fields) {
        expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommMissingField",
                                  "Required field " + *it + " is missing"));
    }
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    prefix_field->SetData().SetStr("##MIGS:4.0-Data-START##");
    required_fields.clear();
    required_fields.push_back("assembly");
    required_fields.push_back("collection_date");
    required_fields.push_back("env_biome");
    required_fields.push_back("env_feature");
    required_fields.push_back("env_material");
    required_fields.push_back("env_package");
    required_fields.push_back("geo_loc_name");
    required_fields.push_back("investigation_type");
    required_fields.push_back("isol_growth_condt");
    required_fields.push_back("lat_lon");
    required_fields.push_back("project_name");
    required_fields.push_back("seq_meth");

    ITERATE(vector<string>, it, required_fields) {
        expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommMissingField",
                                  "Required field " + *it + " is missing"));
    }
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // should complain about missing required field for specific values of sequencing technology
    prefix_field->SetData().SetStr("##Assembly-Data-START##");
    desc->SetUser().ResetData();
    desc->SetUser().SetData().push_back(prefix_field);

    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr("Sequencing Technology");
    field->SetData().SetStr("Singer");
    desc->SetUser().SetData().push_back(field);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadStrucCommMissingField",
                                  "Required field Assembly Method is missing when Sequencing Technology has value 'Singer'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    field->SetData().SetStr("something else");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadStrucCommMissingField",
                                  "Required field Assembly Method is missing when Sequencing Technology has value 'something else'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    prefix_field->SetData().SetStr("##HumanSTR-START##");

    eval = validator.Validate(seh, options);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadStrucCommMissingField",
        "Required field STR locus name is missing"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadStrucCommMissingField",
        "Required field Length-based allele is missing"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadStrucCommMissingField",
        "Required field Bracketed repeat is missing"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


CRef<CUser_field> MkField(const string& label, const string& val)
{
    CRef<CUser_field> f(new CUser_field());
    f->SetLabel().SetStr(label);
    f->SetData().SetStr(val);
    return f;
}


BOOST_AUTO_TEST_CASE(Test_VR_709)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CUser_object> user(new CUser_object());
    user->SetType().SetStr("StructuredComment");
    user->SetData().push_back(MkField("StructuredCommentPrefix", "##Genome-Assembly-Data-START##"));
    user->SetData().push_back(MkField("Assembly Method", "a v. b"));
    user->SetData().push_back(MkField("Assembly Name", "NCBI1234"));
    user->SetData().push_back(MkField("Genome Coverage", "1"));
    user->SetData().push_back(MkField("Sequencing Technology", "2"));

    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().Assign(*user);
    entry->SetSeq().SetDescr().Set().push_back(desc);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue",
            "Assembly Name should not start with 'NCBI' or 'GenBank'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));

    eval = validator.Validate(seh, options);

    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_BioSourceNeedsChromosome)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_genomic);
    unit_test_util::SetCompleteness (entry, CMolInfo::eCompleteness_complete);
    SetTitle (entry, "Sebaea microphylla, complete genome.");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceNeedsChromosome",
                              "Non-viral complete genome not labeled as chromosome"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // error goes away if viruses in lineage
    unit_test_util::SetLineage(entry, "Viruses; ");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage(entry, "some lineage");

    // if not genomic
    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_mRNA);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetBiomol (entry, CMolInfo::eBiomol_genomic);

    // if not end with complete genome
    SetTitle (entry, "Sebaea microphylla, complete sequence.");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    SetTitle (entry, "Sebaea microphylla, complete genome.");

    // if source location chromosome
    CLEAR_ERRORS
    unit_test_util::SetGenome (entry, CBioSource::eGenome_chromosome);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "ChromosomeLocation",
                   "INDEXER_ONLY - BioSource location is chromosome"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_MolInfoConflictsWithBioSource)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    // test for single-strand RNA viruses
    unit_test_util::SetLineage (entry, "Viruses; ssRNA viruses; foo");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MolInfoConflictsWithBioSource",
                              "Taxonomy indicates single-stranded RNA, sequence does not agree."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetLineage (entry, "Viruses; ssRNA negative-strand viruses; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetLineage (entry, "Viruses; unassigned ssRNA viruses; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency",
                              "Genomic DNA viral lineage indicates no DNA stage"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MolInfoConflictsWithBioSource",
                              "Taxonomy indicates single-stranded RNA, sequence does not agree."));

    unit_test_util::SetLineage (entry, "Viruses; ssRNA positive-strand viruses, no DNA stage; foo");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // error should go away if mol is rna
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    CLEAR_ERRORS
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tests for double-stranded RNA viruses
    unit_test_util::SetLineage (entry, "Viruses; dsRNA viruses; foo");
    // should be no error because rna
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // error if not rna
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MolInfoConflictsWithBioSource",
                              "Taxonomy indicates double-stranded RNA, sequence does not agree."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // test for single-stranded DNS viruses
    unit_test_util::SetLineage (entry, "Viruses; ssDNA viruses; foo");
    // no errors because is dna
    CLEAR_ERRORS
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // error if not dna
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MolInfoConflictsWithBioSource",
                              "Taxonomy indicates single-stranded DNA, sequence does not agree."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // test for double-stranded DNS viruses
    unit_test_util::SetLineage (entry, "Viruses; dsDNA viruses; foo");
    // error because not dna
    expected_errors.front()->SetErrMsg("Taxonomy indicates double-stranded DNA, sequence does not agree.");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    //no error if dna
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
    CLEAR_ERRORS
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_Descr_MissingKeyword)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> sdesc(new CSeqdesc());
    sdesc->SetUser().SetType().SetStr("StructuredComment");
    entry->SetSeq().SetDescr().Set().push_back(sdesc);

    sdesc->SetUser().AddField("StructuredCommentPrefix", "##MIGS-Data-START##", CUser_object::eParse_String);
    sdesc->SetUser().AddField("alt_elev", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("assembly", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("collection_date", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("country", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("depth", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("environment", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("investigation_type", "eukaryote", CUser_object::eParse_String);
    sdesc->SetUser().AddField("isol_growth_condt", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("sequencing_meth", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("project_name", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("ploidy", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("num_replicons", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("estimated_size", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("trophic_level", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("propagation", "foo", CUser_object::eParse_String);
    sdesc->SetUser().AddField("lat_lon", "foo", CUser_object::eParse_String);

    CRef<CSeqdesc> gdesc(new CSeqdesc());
    gdesc->SetGenbank().SetKeywords().push_back("GSC:MIGS:2.1");
    entry->SetSeq().SetDescr().Set().push_back(gdesc);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadKeyword",
                                                 "Structured Comment is non-compliant, keyword should be removed"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommMissingField",
                                                 "Required field finishing_strategy is missing when investigation_type has value 'eukaryote'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "BadStrucCommInvalidFieldValue", "Structured Comment invalid"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
  
    // if no keyword, no badkeyword error
    entry->SetSeq().SetDescr().Set().pop_back();
    delete expected_errors[0];
    expected_errors[0] = NULL;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // make the comment valid, should complain about missing keyword
    sdesc->SetUser().AddField("finishing_strategy", "foo", CUser_object::eParse_String);
    /*
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "MissingKeyword",
                                                 "Structured Comment compliant, keyword should be added"));
    */
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    // put keyword back, should have no errors
    entry->SetSeq().SetDescr().Set().push_back(gdesc);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_FakeStructuredComment)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> sdesc(new CSeqdesc());
    sdesc->SetComment("This comment contains ::");
    entry->SetSeq().SetDescr().Set().push_back(sdesc);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "FakeStructuredComment",
                                                 "Comment may be formatted to look like a structured comment."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
  
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Descr_StructuredCommentPrefixOrSuffixMissing)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> sdesc(new CSeqdesc());
    sdesc->SetUser().SetType().SetStr("StructuredComment");
    entry->SetSeq().SetDescr().Set().push_back(sdesc);

    sdesc->SetUser().AddField("OneField", "some value", CUser_object::eParse_String);
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "StructuredCommentPrefixOrSuffixMissing",
                                                 "Structured Comment lacks prefix"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_NonAsciiAsn)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr); 
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    CConstRef<CValidError> eval;
    CValidator validator(*objmgr);
    unsigned int options = CValidator::eVal_need_isojta 
                          | CValidator::eVal_far_fetch_mrna_products
                          | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
                          | CValidator::eVal_use_entrez
                          | CValidator::eVal_non_ascii;
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "NonAsciiAsn",
                              "Non-ascii chars in input ASN.1 strings"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // error should only appear once
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("lcl|nuc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_DESCR_MissingPersonalCollectionName)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_bio_material, "personal:1234");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MissingPersonalCollectionName",
                              "Personal collection does not have name of collector"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_AuthorListHasEtAl)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CAuthor> author(new CAuthor());
    author->SetName().SetName().SetLast("et al.");
    CRef<CPub> pub(new CPub());
    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author);
    CRef<CCit_art::TTitle::C_E> art_title(new CCit_art::TTitle::C_E());
    art_title->SetName("article title");
    pub->SetArticle().SetTitle().Set().push_back(art_title);
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetDescr().Set().push_back(desc);
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "AuthorListHasEtAl",
                              "Author list ends in et al."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetMan().SetCit().SetAuthors().SetNames().SetStd().push_back(author);
    CRef<CCit_book::TTitle::C_E> book_title(new CCit_book::TTitle::C_E());
    book_title->SetName("book title");
    pub->SetMan().SetCit().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetBook().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetBook().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetProc().SetBook().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetProc().SetBook().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetGen().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetGen().SetTitle("gen title");
    pub->SetGen().SetDate().SetStd().SetYear(2009);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetSub().SetAuthors().SetAffil().SetStr("some affiliation");

    pub->SetSub().SetDate().SetStd().SetYear(2009);
    pub->SetSub().SetDate().SetStd().SetMonth(12);
    pub->SetSub().SetDate().SetStd().SetDay(31);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try as pub feature
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetDescr().Set().pop_back();
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(10);
    feat->SetData().SetPub().SetPub().Set().push_back(pub);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry->SetSeq().SetAnnot().push_back(annot);
    seh = scope.AddTopLevelSeqEntry(*entry);

    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetArticle().SetTitle().Set().push_back(art_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    pub->SetMan().SetCit().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetMan().SetCit().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetBook().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetBook().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetProc().SetBook().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetProc().SetBook().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetGen().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetGen().SetTitle("gen title");
    pub->SetGen().SetDate().SetStd().SetYear(2009);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetSub().SetAuthors().SetAffil().SetStr("some affiliation");

    pub->SetSub().SetDate().SetStd().SetYear(2009);
    pub->SetSub().SetDate().SetStd().SetMonth(12);
    pub->SetSub().SetDate().SetStd().SetDay(31);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // look for contains instead of ends with
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetAnnot().pop_back();
    entry->SetDescr().Set().push_back(desc);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors[0]->SetErrMsg("Author list contains et al.");
    CRef<CAuthor> author2 = unit_test_util::BuildGoodAuthor();

    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetArticle().SetTitle().Set().push_back(art_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetMan().SetCit().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetMan().SetCit().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetMan().SetCit().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetBook().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetBook().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetBook().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetProc().SetBook().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetProc().SetBook().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetProc().SetBook().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetGen().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetGen().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetGen().SetTitle("gen title");
    pub->SetGen().SetDate().SetStd().SetYear(2009);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetSub().SetAuthors().SetAffil().SetStr("some affiliation");

    pub->SetSub().SetDate().SetStd().SetYear(2009);
    pub->SetSub().SetDate().SetStd().SetMonth(12);
    pub->SetSub().SetDate().SetStd().SetDay(31);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try as pub feature
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetDescr().Set().pop_back();
    entry->SetSeq().SetAnnot().push_back(annot);
    seh = scope.AddTopLevelSeqEntry(*entry);

    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetArticle().SetTitle().Set().push_back(art_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    pub->SetMan().SetCit().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetMan().SetCit().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetMan().SetCit().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetBook().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetBook().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetBook().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetProc().SetBook().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetProc().SetBook().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetProc().SetBook().SetTitle().Set().push_back(book_title);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetGen().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetGen().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetGen().SetTitle("gen title");
    pub->SetGen().SetDate().SetStd().SetYear(2009);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author);
    pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author2);
    pub->SetSub().SetAuthors().SetAffil().SetStr("some affiliation");

    pub->SetSub().SetDate().SetStd().SetYear(2009);
    pub->SetSub().SetDate().SetStd().SetMonth(12);
    pub->SetSub().SetDate().SetStd().SetDay(31);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_MissingPubRequirement)
{
    // validate cit-sub
    CRef<CSeq_submit> submit(new CSeq_submit());

    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    submit->SetData().SetEntrys().push_back(entry);
    CRef<CAuthor> author = unit_test_util::BuildGoodAuthor();
    submit->SetSub().SetCit().SetAuthors().SetNames().SetStd().push_back(author);
    submit->SetSub().SetCit().SetAuthors().SetAffil().SetStd().SetAffil("some affiliation");

    submit->SetSub().SetCit().SetDate().SetStd().SetYear(2009);
    submit->SetSub().SetCit().SetDate().SetStd().SetMonth(12);
    submit->SetSub().SetCit().SetDate().SetStd().SetDay(31);

    STANDARD_SETUP

    vector<string> ids;
    ids.push_back("good");
    ids.push_back("NC_123456");

    ITERATE(vector<string>, id_it, ids) {
        EDiagSev sev = eDiag_Warning;
        scope.RemoveTopLevelSeqEntry(seh);
        if (NStr::StartsWith(*id_it, "NC_")) {
            entry->SetSeq().SetId().front()->SetOther().SetAccession(*id_it);
        } else {
            entry->SetSeq().SetId().front()->SetLocal().SetStr(*id_it);
            sev = eDiag_Critical;
        }
        seh = scope.AddTopLevelSeqEntry(*entry);

        submit->SetSub().SetCit().SetAuthors().ResetAffil();
        submit->SetSub().SetCit().SetAuthors().SetAffil().SetStd().SetAffil("some affiliation");
        submit->SetSub().ResetContact();
        string msg_acc = NStr::StartsWith(*id_it, "NC") ? "ref|" + *id_it + "|" : "lcl|" + *id_it;
        expected_errors.push_back(new CExpectedError(msg_acc,
                                  sev, "MissingPubRequirement",
                                  "Submission citation affiliation has no country"));
        eval = validator.Validate(*submit, &scope, options);
        CheckErrors (*eval, expected_errors);

        submit->SetSub().SetCit().SetAuthors().SetAffil().SetStd().SetCountry("USA");
        expected_errors[0]->SetErrMsg("Submission citation affiliation has no state");
        expected_errors[0]->SetSeverity(eDiag_Warning);
        eval = validator.Validate(*submit, &scope, options);
        CheckErrors (*eval, expected_errors);
        CLEAR_ERRORS

        submit->SetSub().SetCit().SetAuthors().SetAffil().SetStd().SetSub("VA");
        submit->SetSub().SetContact().SetContact().SetAffil().SetStd().SetAffil("some affiliation");
        expected_errors.push_back(new CExpectedError(msg_acc, sev, "MissingPubRequirement",
                                  "Submission citation affiliation has no country"));
        expected_errors[0]->SetAccession("");
        expected_errors[0]->SetSeverity(eDiag_Warning);
        eval = validator.Validate(*submit, &scope, options);
        CheckErrors (*eval, expected_errors);

        submit->SetSub().SetContact().SetContact().SetAffil().SetStd().SetCountry("USA");
        expected_errors[0]->SetErrMsg("Submission citation affiliation has no state");
        expected_errors[0]->SetSeverity(eDiag_Warning);
        eval = validator.Validate(*submit, &scope, options);
        CheckErrors (*eval, expected_errors);
        CLEAR_ERRORS

        scope.RemoveTopLevelSeqEntry(seh);
        CRef<CPub> pub(new CPub());
        CRef<CSeqdesc> desc(new CSeqdesc());
        desc->SetPub().SetPub().Set().push_back(pub);
        entry->SetDescr().Set().push_back(desc);
        pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author);
        pub->SetSub().SetAuthors().SetAffil().SetStd().SetAffil("some affiliation");

        pub->SetSub().SetDate().SetStd().SetYear(2009);
        pub->SetSub().SetDate().SetStd().SetMonth(12);
        pub->SetSub().SetDate().SetStd().SetDay(31);

        seh = scope.AddTopLevelSeqEntry(*entry);

        expected_errors.push_back(new CExpectedError(msg_acc, sev, "MissingPubRequirement",
                                  "Submission citation affiliation has no country"));
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        pub->SetSub().SetAuthors().SetAffil().SetStd().SetCountry("USA");
        expected_errors[0]->SetErrMsg("Submission citation affiliation has no state");
        expected_errors[0]->SetSeverity(eDiag_Warning);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        pub->SetSub().SetAuthors().SetAffil().SetStd().SetSub("VA");
        pub->SetSub().SetAuthors().SetNames().SetStd().pop_back();

        expected_errors[0]->SetErrMsg("Submission citation has no author names");
        expected_errors[0]->SetSeverity(eDiag_Critical);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        CLEAR_ERRORS

        pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author);
        pub->SetSub().SetAuthors().SetAffil().SetStd().ResetCountry();
        pub->SetSub().SetAuthors().SetAffil().SetStd().ResetSub();
        pub->SetSub().SetAuthors().SetAffil().SetStd().ResetAffil();
        expected_errors.push_back(new CExpectedError(msg_acc,
                NStr::StartsWith(*id_it, "NC_") ? eDiag_Warning : eDiag_Critical,
                                  "MissingPubRequirement",
                                  "Submission citation has no affiliation"));
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        
        pub->SetSub().SetAuthors().ResetAffil();
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        SetTech(entry, CMolInfo::eTech_htgs_0);
        expected_errors[0]->SetSeverity(eDiag_Warning);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        SetTech(entry, CMolInfo::eTech_htgs_1);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        SetTech(entry, CMolInfo::eTech_htgs_3);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        SetTech(entry, CMolInfo::eTech_unknown);

        CLEAR_ERRORS

        pub->SetGen().SetAuthors().SetNames().SetStd().push_back(author);
        pub->SetGen().SetCit("Does not start with expected text");

        expected_errors.push_back(new CExpectedError(msg_acc, eDiag_Error, "MissingPubRequirement",
            "Unpublished citation text invalid"));
        expected_errors.push_back(new CExpectedError(msg_acc, eDiag_Warning, "MissingPubRequirement",
            "Publication date missing"));

        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        delete expected_errors[1];
        expected_errors.pop_back();

        pub->SetGen().SetCit("submitted starts with expected text");
        pub->SetGen().SetDate().SetStr("?");
        expected_errors[0]->SetErrMsg("Publication date marked as '?'");
        expected_errors[0]->SetSeverity(eDiag_Warning);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        pub->SetGen().SetDate().SetStd().SetYear(0);
        expected_errors[0]->SetErrMsg("Publication date not set");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        pub->SetGen().ResetDate();
        pub->SetGen().SetAuthors().SetNames().SetStd().pop_back();
        if (!NStr::StartsWith(*id_it, "NC_")) {
            expected_errors[0]->SetSeverity(eDiag_Error);
        }
        expected_errors[0]->SetErrMsg("Publication has no author names");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author);
        expected_errors[0]->SetSeverity(eDiag_Error);
        expected_errors[0]->SetErrMsg("Publication has no title");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        CRef<CCit_art::TTitle::C_E> art_title(new CCit_art::TTitle::C_E());
        art_title->SetName("article title");
        pub->SetArticle().SetTitle().Set().push_back(art_title);
        pub->SetArticle().SetAuthors().SetNames().SetStd().pop_back();
        expected_errors[0]->SetErrMsg("Publication has no author names");
        if (NStr::StartsWith(*id_it, "NC_")) {
            expected_errors[0]->SetSeverity(eDiag_Warning);
        }
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author);
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetVolume("vol 1");
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("14-32");
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetDate().SetStd().SetYear(2009);
        expected_errors[0]->SetSeverity(eDiag_Error);
        expected_errors[0]->SetErrMsg("Journal title missing");
        expected_errors.push_back(new CExpectedError(msg_acc, eDiag_Warning, "MissingISOJTA",
                                  "ISO journal title abbreviation missing"));
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        CRef<CCit_jour::TTitle::C_E> journal_title(new CCit_jour::TTitle::C_E());
        journal_title->SetName("journal_title");
        pub->SetArticle().SetFrom().SetJournal().SetTitle().Set().push_back(journal_title);
        delete expected_errors[0];
        expected_errors[0] = NULL;
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        CLEAR_ERRORS

        expected_errors.push_back(new CExpectedError(msg_acc, eDiag_Warning, "MissingVolume",
                                  "Journal volume missing"));
        expected_errors.push_back(new CExpectedError(msg_acc, eDiag_Warning, "MissingPages",
                                  "Journal pages missing"));
        CRef<CCit_jour::TTitle::C_E> iso_jta(new CCit_jour::TTitle::C_E());   
        iso_jta->SetIso_jta("abbr");
        pub->SetArticle().SetFrom().SetJournal().SetTitle().Set().push_back(iso_jta);
        pub->SetArticle().SetFrom().SetJournal().SetImp().ResetVolume();
        pub->SetArticle().SetFrom().SetJournal().SetImp().ResetPages();
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        CLEAR_ERRORS
        expected_errors.push_back(new CExpectedError(msg_acc, eDiag_Warning, "MissingPages",
                                  "Journal pages missing"));
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetVolume("vol 1");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        CLEAR_ERRORS
        expected_errors.push_back(new CExpectedError(msg_acc, eDiag_Warning, "MissingVolume",
                                  "Journal volume missing"));
        pub->SetArticle().SetFrom().SetJournal().SetImp().ResetVolume();
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("14-32");
        expected_errors[0]->SetErrMsg("Journal volume missing");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        CLEAR_ERRORS
        expected_errors.push_back(new CExpectedError(msg_acc, eDiag_Warning, "MissingPubRequirement", 
                                  "Publication date missing"));
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetVolume("vol 1");
        pub->SetArticle().SetFrom().SetJournal().SetImp().ResetDate();
        expected_errors[0]->SetErrMsg("Publication date missing");
        expected_errors[0]->SetSeverity(eDiag_Warning);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetDate().SetStr("?");
        expected_errors[0]->SetErrMsg("Publication date marked as '?'");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetDate().SetStd().SetYear(0);
        expected_errors[0]->SetErrMsg("Publication date not set");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        CLEAR_ERRORS
        //suppress ISOJTA warning if electronic journal
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetDate().SetStd().SetYear(2009);
        pub->SetArticle().SetFrom().SetJournal().SetTitle().Set().pop_back();
        journal_title->SetName("(er) Journal Title");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        journal_title->SetName("(journal title");
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetPubstatus(ePubStatus_epublish);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetPubstatus(ePubStatus_aheadofprint);
        pub->SetArticle().SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
        expected_errors.push_back(new CExpectedError(msg_acc, eDiag_Warning, "PublicationInconsistency",
                                  "In-press is not expected to have page numbers"));
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        CLEAR_ERRORS

        entry->SetDescr().Set().pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_Generic_UnnecessaryPubEquiv)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CPub> pub(new CPub());
    pub->SetEquiv();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetDescr().Set().push_back(desc);
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UnnecessaryPubEquiv",
                              "Publication has unexpected internal Pub-equiv"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_BadPageNumbering)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CPub> pub = unit_test_util::BuildGoodArticlePub();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetDescr().Set().push_back(desc);

    STANDARD_SETUP

    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("0-32");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadPageNumbering",
                              "Page numbering has zero value"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("14-0");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Page numbering has negative value");
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("14--32");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Page numbering out of order");
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("32-14");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Page numbering greater than 50");
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("14-65");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Page numbering stop looks strange");
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("14-A");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Page numbering start looks strange");
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages(".14-32");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_MedlineEntryPub)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CPub> pub(new CPub());
    pub->SetMedline();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetDescr().Set().push_back(desc);
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MedlineEntryPub",
                              "Publication is medline entry"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static void MakeBadSeasonDate(CDate& date)
{
    date.SetStd().SetYear(2009);
    date.SetStd().SetMonth(12);
    date.SetStd().SetDay(31);
    date.SetStd().SetSeason("1");
}


BOOST_AUTO_TEST_CASE(Test_Generic_BadDate)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    // find sub pub and other pub
    CRef<CPub> subpub(NULL);
    CRef<CPub> otherpub(NULL);
    NON_CONST_ITERATE(CBioseq::TDescr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsPub()) {
            if ((*it)->GetPub().GetPub().Get().front()->IsSub()) {
                subpub = (*it)->SetPub().SetPub().Set().front();
            } else {
                otherpub = (*it)->SetPub().SetPub().Set().front();
            }
        }
    }

    STANDARD_SETUP

    subpub->SetSub().SetDate().SetStr("?");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadDate",
                              "Submission citation date has error - BAD_STR"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    subpub->SetSub().SetDate().SetStd().SetYear(0);
    expected_errors[0]->SetErrMsg("Submission citation date has error - BAD_YEAR");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    subpub->SetSub().SetDate().SetStd().SetYear(2009);
    subpub->SetSub().SetDate().SetStd().SetMonth(13);
    expected_errors[0]->SetErrMsg("Submission citation date has error - BAD_MONTH");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    subpub->SetSub().SetDate().SetStd().SetYear(2009);
    subpub->SetSub().SetDate().SetStd().SetMonth(12);
    subpub->SetSub().SetDate().SetStd().SetDay(32);
    expected_errors[0]->SetErrMsg("Submission citation date has error - BAD_DAY");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    MakeBadSeasonDate(subpub->SetSub().SetDate());
    expected_errors[0]->SetErrMsg("Submission citation date has error - BAD_SEASON");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    subpub->Assign(*(unit_test_util::BuildGoodCitSubPub()));

    CRef<CAuthor> author = unit_test_util::BuildGoodAuthor();
    CRef<CPub> gen(new CPub());
    gen->SetGen().SetAuthors().SetNames().SetStd().push_back(author);
    gen->SetGen().SetTitle("gen title");
    MakeBadSeasonDate(gen->SetGen().SetDate());
    otherpub->Assign(*gen);
    expected_errors[0]->SetErrMsg("Publication date has error - BAD_SEASON");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    otherpub->Assign(*(unit_test_util::BuildGoodArticlePub()));
    MakeBadSeasonDate(otherpub->SetArticle().SetFrom().SetJournal().SetImp().SetDate());
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    otherpub->Assign(*(unit_test_util::BuildGoodArticlePub()));
   
    CRef<CSeqdesc> desc(new CSeqdesc());
    entry->SetDescr().Set().push_back(desc);
    MakeBadSeasonDate(desc->SetCreate_date());
    expected_errors[0]->SetErrMsg("Create date has error - BAD_SEASON");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    MakeBadSeasonDate(desc->SetUpdate_date());
    expected_errors[0]->SetErrMsg("Update date has error - BAD_SEASON");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_StructuredCitGenCit)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CPub> pub(new CPub());
    pub->SetGen().SetAuthors().SetNames().SetStd().push_back(unit_test_util::BuildGoodAuthor());
    pub->SetGen().SetTitle("gen title");
    pub->SetGen().SetDate().SetStd().SetYear(2009);
    pub->SetGen().SetCit("submitted something Title=foo");
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetDescr().Set().push_back(desc);
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "StructuredCitGenCit",
                              "Unpublished citation has embedded Title"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetGen().SetCit("submitted something Journal=bar");
    expected_errors[0]->SetErrMsg("Unpublished citation has embedded Journal");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_CollidingSerialNumbers)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CAuthor> blank;

    CRef<CPub> pub = unit_test_util::BuildGoodCitGenPub(blank, 1234);
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetDescr().Set().push_back(desc);

    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(15);
    feat->SetData().SetPub().SetPub().Set().push_back(unit_test_util::BuildGoodCitGenPub(blank, 1234));
    unit_test_util::AddFeat(feat, entry);
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CollidingSerialNumbers",
                              "Multiple publications have serial number 1234"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_EmbeddedScript)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CAuthor> author = unit_test_util::BuildGoodAuthor();
    author->SetName().SetName().SetLast("foo<script");

    CRef<CPub> pub = unit_test_util::BuildGoodCitGenPub(author, -1);
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetDescr().Set().push_back(desc);

    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadCharInAuthorLastName",
                              "Bad characters in author foo<script"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "EmbeddedScript",
                              "Script tag found in item"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    author->SetName().SetName().SetLast("Last");
    delete expected_errors[0];
    expected_errors[0] = NULL;

    feat->SetComment("<object");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->ResetComment();
    feat->SetComment("misc_feature needs a comment");

    feat->SetTitle("<applet");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->ResetTitle();

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_cell_line, "<embed");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_cell_line, "");

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_acronym, "<form");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_acronym, "");

    pub->SetGen().SetTitle("javascript:");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    pub->SetGen().SetTitle("good title");

    unit_test_util::SetLineage(entry, "vbscript:");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetLineage(entry, "");

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_PublicationInconsistency)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> desc(new CSeqdesc());
    CRef<CPub> pub = unit_test_util::BuildGoodArticlePub();
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPubstatus(ePubStatus_aheadofprint);
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetSeq().SetDescr().Set().push_back(desc);
    
    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PublicationInconsistency",
                                                 "Ahead-of-print without in-press"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPubstatus(ePubStatus_epublish);
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PublicationInconsistency",
                                                 "In-press is not expected to have page numbers"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PublicationInconsistency",
                                                 "Electronic-only publication should not also be in-press"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    pub->SetArticle().SetFrom().SetJournal().SetImp().ResetPubstatus();
    pub->SetArticle().SetFrom().SetJournal().SetImp().ResetPrepub();
    
    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PublicationInconsistency",
                                                 "Empty consortium"));
    CRef<CAuthor> consortium(new CAuthor());
    consortium->SetName().SetConsortium("");
    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(consortium);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    consortium->SetName().SetConsortium("duplicate");
    CRef<CAuthor> consortium2(new CAuthor());
    consortium2->SetName().SetConsortium("duplicate");
    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(consortium2);
    expected_errors[0]->SetErrMsg("Duplicate consortium 'duplicate'");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PublicationInconsistency",
                                                 "In-press is not expected to have page numbers"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PublicationInconsistency",
                                                 "Duplicate consortium 'duplicate'"));
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPages("75-84");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    pub->SetArticle().SetFrom().SetJournal().SetImp().ResetPrepub();
    pub->SetArticle().SetFrom().SetJournal().SetImp().ResetPages();
    
    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_Generic_SgmlPresentInText)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP

    vector<string> sgml_tags;

    sgml_tags.push_back("&gt;");
    sgml_tags.push_back("&lt;");
    sgml_tags.push_back("&amp;");
    sgml_tags.push_back("&agr;");
    sgml_tags.push_back("&Agr;");
    sgml_tags.push_back("&bgr;");
    sgml_tags.push_back("&Bgr;");
    sgml_tags.push_back("&ggr;");
    sgml_tags.push_back("&Ggr;");
    sgml_tags.push_back("&dgr;");
    sgml_tags.push_back("&Dgr;");
    sgml_tags.push_back("&egr;");
    sgml_tags.push_back("&Egr;");
    sgml_tags.push_back("&zgr;");
    sgml_tags.push_back("&Zgr;");
    sgml_tags.push_back("&eegr;");
    sgml_tags.push_back("&EEgr;");
    sgml_tags.push_back("&thgr;");
    sgml_tags.push_back("&THgr;");
    sgml_tags.push_back("&igr;");
    sgml_tags.push_back("&Igr;");
    sgml_tags.push_back("&kgr;");
    sgml_tags.push_back("&Kgr;");
    sgml_tags.push_back("&lgr;");
    sgml_tags.push_back("&Lgr;");
    sgml_tags.push_back("&mgr;");
    sgml_tags.push_back("&Mgr;");
    sgml_tags.push_back("&ngr;");
    sgml_tags.push_back("&Ngr;");
    sgml_tags.push_back("&xgr;");
    sgml_tags.push_back("&Xgr;");
    sgml_tags.push_back("&ogr;");
    sgml_tags.push_back("&Ogr;");
    sgml_tags.push_back("&pgr;");
    sgml_tags.push_back("&Pgr;");
    sgml_tags.push_back("&rgr;");
    sgml_tags.push_back("&Rgr;");
    sgml_tags.push_back("&sgr;");
    sgml_tags.push_back("&Sgr;");
    sgml_tags.push_back("&sfgr;");
    sgml_tags.push_back("&tgr;");
    sgml_tags.push_back("&Tgr;");
    sgml_tags.push_back("&ugr;");
    sgml_tags.push_back("&Ugr;");
    sgml_tags.push_back("&phgr;");
    sgml_tags.push_back("&PHgr;");
    sgml_tags.push_back("&khgr;");
    sgml_tags.push_back("&KHgr;");
    sgml_tags.push_back("&psgr;");
    sgml_tags.push_back("&PSgr;");
    sgml_tags.push_back("&ohgr;");
    sgml_tags.push_back("&OHgr;");

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "SgmlPresentInText",
                              "taxname %s has SGML"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "OrganismNotFound",
                              "Organism not found in taxonomy database"));
    ITERATE(vector<string>, it, sgml_tags) {
        string taxname = "a" + *it + "b";
        unit_test_util::SetTaxname(entry, taxname);
        expected_errors[0]->SetErrMsg("taxname " + taxname + " has SGML");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    unit_test_util::SetSebaea_microphylla(entry);
    delete expected_errors[1];
    expected_errors.pop_back();

    size_t tag_num = 0;

    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_cell_line, sgml_tags[tag_num]);
    expected_errors[0]->SetErrMsg("subsource " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_cell_line, "");

    ++tag_num;
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_acronym, sgml_tags[tag_num]);
    expected_errors[0]->SetErrMsg("orgmod " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_acronym, "");

    CLEAR_ERRORS
    tag_num++;
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "SgmlPresentInText",
                              "dbxref database " + sgml_tags[tag_num] + " has SGML"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "IllegalDbXref",
                              "Illegal db_xref type " + sgml_tags[tag_num] + " (1234)"));

    unit_test_util::SetDbxref (entry, sgml_tags[tag_num], 1234);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::RemoveDbxref (entry, sgml_tags[tag_num], 1234);

    tag_num++;
    delete expected_errors[1];
    expected_errors.pop_back();
    unit_test_util::SetDbxref (entry, "AFTOL", sgml_tags[tag_num]);
    expected_errors[0]->SetErrMsg("dbxref value " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::RemoveDbxref (entry, "AFTOL", 0);

    ++tag_num;
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature (entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("dbxref database " + sgml_tags[tag_num] + " has SGML");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "IllegalDbXref",
                              "Illegal db_xref type " + sgml_tags[tag_num] + " (1234)"));
    unit_test_util::SetDbxref(feat, sgml_tags[tag_num], 1234);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::RemoveDbxref (feat, sgml_tags[tag_num], 1234);

    tag_num++;
    delete expected_errors[1];
    expected_errors.pop_back();
    unit_test_util::SetDbxref (feat, "AFTOL", sgml_tags[tag_num]);
    expected_errors[0]->SetErrMsg("dbxref value " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::RemoveDbxref (feat, "AFTOL", 0);

    tag_num++;
    scope.RemoveTopLevelSeqEntry(seh);
    string foo = sgml_tags[tag_num] + "foo";
    feat->SetData().SetGene().SetLocus(foo);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("gene locus " + foo + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetData().SetGene().SetLocus("good locus");

    tag_num++;
    feat->SetData().SetGene().SetLocus_tag(sgml_tags[tag_num]);
    expected_errors[0]->SetErrMsg("gene locus_tag " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetData().SetGene().ResetLocus_tag();

    tag_num++;
    feat->SetData().SetGene().SetDesc(sgml_tags[tag_num]);
    expected_errors[0]->SetErrMsg("gene description " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetData().SetGene().ResetDesc();

    tag_num++;
    feat->SetData().SetGene().SetSyn().push_back(sgml_tags[tag_num]);
    expected_errors[0]->SetErrMsg("gene synonym " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetData().SetGene().ResetDesc();

    CLEAR_ERRORS

    tag_num++;
    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    foo = sgml_tags[tag_num] + "foo";
    feat->SetData().SetRna().SetExt().SetName(foo);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                              "No CDS location match for 1 mRNA"));

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "SgmlPresentInText",
                              "mRNA name " + foo + " has SGML"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS;

    tag_num++;
    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    foo = sgml_tags[tag_num] + "foo";
    feat->SetData().SetRna().SetExt().SetName(foo);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "SgmlPresentInText",
                              "rRNA name " + foo + " has SGML"));
//    expected_errors[0]->SetErrMsg("rRNA name " + foo + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetData().SetRna().SetExt().SetName("good name");

    tag_num++;
    feat->SetComment(sgml_tags[tag_num]);
    expected_errors[0]->SetErrMsg("feature comment " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->ResetComment();

    tag_num++;
    CRef<CGb_qual> qual(new CGb_qual());
    qual->SetQual("standard_name");
    qual->SetVal(sgml_tags[tag_num]);
    feat->SetQual().push_back(qual);
    expected_errors[0]->SetErrMsg("feature qualifier " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetQual().pop_back();

    tag_num++;
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet ();
    feat = entry->SetSet().SetSeq_set().back()->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    foo = sgml_tags[tag_num] + "foo";
    feat->SetData().SetProt().SetName().front().assign(foo);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("lcl|prot");
    expected_errors[0]->SetErrMsg("protein name " + foo + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetData().SetProt().SetName().pop_back();
    feat->SetData().SetProt().SetName().push_back("bar");


    tag_num++;
    feat->SetData().SetProt().SetDesc(sgml_tags[tag_num]);
    expected_errors[0]->SetErrMsg("protein description " + sgml_tags[tag_num] + " has SGML");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetData().SetProt().ResetDesc();
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_Generic_UnexpectedPubStatusComment)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CPub> pub = unit_test_util::BuildGoodArticlePub();
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPubstatus(ePubStatus_epublish);
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    desc->SetPub().SetComment("Publication Status");
    entry->SetSeq().SetDescr().Set().push_back(desc);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UnexpectedPubStatusComment",
                                                 "Publication status is in comment for pmid 0"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPubstatus(ePubStatus_ppublish);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PublicationInconsistency",
                              "In-press is not expected to have page numbers"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UnexpectedPubStatusComment",
                                                 "Publication status is in comment for pmid 0"));
    
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPubstatus(ePubStatus_aheadofprint);
    pub->SetArticle().SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetPub().SetComment("Publication-Status");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetPub().SetComment("Publication_Status");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_NoCdRegionPtr)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();

    CRef<CSeq_entry> pentry = unit_test_util::BuildGoodProtSeq();
    EDIT_EACH_DESCRIPTOR_ON_BIOSEQ (it, pentry->SetSeq()) {
        if ((*it)->IsSource() || (*it)->IsPub()) {
            ERASE_DESCRIPTOR_ON_BIOSEQ(it, pentry->SetSeq());
        }
    }

    entry->SetSet().SetSeq_set().push_back(pentry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "NoCdRegionPtr",
                                                 "No CdRegion in nuc-prot set points to this protein"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_NucProtProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nentry = entry->SetSet().SetSeq_set().front();
    entry->SetSet().SetSeq_set().pop_front();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    entry->SetSet().SetAnnot().front()->SetData().SetFtable().pop_front();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "NoCdRegionPtr",
                                                 "No CdRegion in nuc-prot set points to this protein"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "NucProtProblem",
                                                 "No nucleotides in nuc-prot set"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_entry> pentry = entry->SetSet().SetSeq_set().front();
    entry->SetSet().SetSeq_set().pop_front();
    entry->SetSet().SetSeq_set().push_back(nentry);
    entry->SetSet().SetAnnot().front()->SetData().SetFtable().push_back(cds);
    seh = scope.AddTopLevelSeqEntry(*entry);
    delete expected_errors[0];
    expected_errors[0] = NULL;
    expected_errors[1]->SetErrMsg("No proteins in nuc-prot set");
    expected_errors[1]->SetAccession("lcl|nuc");
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "MissingCDSproduct",
                                                 "Unable to find product Bioseq from CDS feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_entry> nentry2 = unit_test_util::BuildGoodSeq();
    EDIT_EACH_DESCRIPTOR_ON_BIOSEQ (it, nentry2->SetSeq()) {
        if ((*it)->IsSource() || (*it)->IsPub()) {
            ERASE_DESCRIPTOR_ON_BIOSEQ(it, nentry2->SetSeq());
        }
    }
    entry->SetSet().SetSeq_set().push_back(nentry2);
    entry->SetSet().SetSeq_set().push_back(pentry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[1]->SetSeverity(eDiag_Critical);
    expected_errors[1]->SetErrMsg("Multiple unsegmented nucleotides in nuc-prot set");
    delete expected_errors[2];
    expected_errors.pop_back();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_SegSetProblem)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_segset);
    entry->SetSet().SetSeq_set().push_back(unit_test_util::BuildGoodSeq());
    entry->SetSet().SetSeq_set().push_back(unit_test_util::BuildGoodSeq());
    entry->SetSet().SetSeq_set().back()->SetSeq().SetId().front()->SetLocal().SetStr("good2");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "SegSetProblem",
                                                 "No segmented Bioseq in segset"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_EmptySet)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    entry->SetSet().SetSeq_set().push_back(unit_test_util::BuildGoodSeq());
    CRef<CSeq_entry> centry(new CSeq_entry());
    centry->SetSet().SetClass(CBioseq_set::eClass_gi);
    entry->SetSet().SetSeq_set().push_back(centry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("", eDiag_Warning, "EmptySet",
                                                 "No Bioseqs in this set"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    centry->SetSet().SetClass(CBioseq_set::eClass_gibb);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    centry->SetSet().SetClass(CBioseq_set::eClass_pir);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    centry->SetSet().SetClass(CBioseq_set::eClass_pub_set);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    centry->SetSet().SetClass(CBioseq_set::eClass_equiv);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    centry->SetSet().SetClass(CBioseq_set::eClass_swissprot);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    centry->SetSet().SetClass(CBioseq_set::eClass_pdb_entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_NucProtNotSegSet)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> centry(new CSeq_entry());
    centry->SetSet().SetClass(CBioseq_set::eClass_eco_set);
    entry->SetSet().SetSeq_set().push_back(centry);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError ("", eDiag_Warning, "EmptySet", 
                                                  "Pop/Phy/Mut/Eco set has no components"));
    expected_errors.push_back(new CExpectedError("", eDiag_Critical, "NucProtNotSegSet",
                                                 "Nuc-prot Bioseq-set contains wrong Bioseq-set, its class is \"eco-set\"."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


#if 0
// we don't care about segsets any more
BOOST_AUTO_TEST_CASE(Test_PKG_SegSetNotParts)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSegSet();
    entry->SetSet().SetSeq_set().back()->SetSet().SetClass(CBioseq_set::eClass_eco_set);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("part1", eDiag_Critical, "SegSetNotParts",
                                                 "Segmented set contains wrong Bioseq-set, its class is \"eco-set\"."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_SegSetMixedBioseqs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSegSet();
    CRef<CSeq_entry> prot = unit_test_util::BuildGoodProtSeq();
    unit_test_util::RemoveDescriptorType(prot, CSeqdesc::e_Pub);
    entry->SetSet().SetSeq_set().push_back(prot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("master", eDiag_Critical, "SegSetMixedBioseqs",
                                                 "Segmented set contains mixture of nucleotides and proteins"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_PartsSetMixedBioseqs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSegSet();
    CRef<CSeq_entry> parts_set = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_entry> last_part = parts_set->SetSet().SetSeq_set().back();
    last_part->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    unit_test_util::SetBiomol (last_part, CMolInfo::eBiomol_peptide);
    last_part->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("part3", eDiag_Error, "NoProtRefFound",
                                                 "No full length Prot-ref feature applied to this Bioseq"));
    expected_errors.push_back(new CExpectedError("part1", eDiag_Critical, "PartsSetMixedBioseqs",
                                                 "Parts set contains mixture of nucleotides and proteins"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_PartsSetHasSets)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSegSet();
    CRef<CSeq_entry> parts_set = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_entry> np = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::RemoveDescriptorType(np, CSeqdesc::e_Pub);
    parts_set->SetSet().SetSeq_set().push_back(np);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "PartsOutOfOrder",
                                                 "Parts set contains too many Bioseqs"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Critical, "PartsSetHasSets",
                                                 "Parts set contains unwanted Bioseq-set, its class is \"nuc-prot\"."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_FeaturePackagingProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSegSet();
    CRef<CSeq_entry> parts_set = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat> misc_feat = unit_test_util::AddMiscFeature(parts_set->SetSet().SetSeq_set().front());
    misc_feat->SetLocation().SetInt().SetId().SetLocal().SetStr("part3");

    STANDARD_SETUP

    //expected_errors.push_back(new CExpectedError("master", eDiag_Critical, "FeaturePackagingProblem",
    //                                             "There is 1 mispackaged feature in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "LocOnSegmentedBioseq",
                                                 "Feature location on segmented bioseq, not on parts"));
    //expected_errors.push_back(new CExpectedError("master", eDiag_Critical, "FeaturePackagingProblem",
    //                                             "There are 2 mispackaged features in this record."));
    expected_errors.push_back(new CExpectedError("master", eDiag_Critical, "FeaturePackagingProblem",
                                                 "There is 1 mispackaged feature in this record."));
    scope.RemoveTopLevelSeqEntry(seh);
    misc_feat = unit_test_util::AddMiscFeature(parts_set);
    misc_feat->SetLocation().SetInt().SetId().SetLocal().SetStr("master");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}
#endif


BOOST_AUTO_TEST_CASE(Test_PKG_GenomicProductPackagingProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_entry> contig = entry->SetSet().SetSeq_set().front();

    CRef<CSeq_entry> stray = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = stray->SetSet().SetSeq_set().front();
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAA");
    nuc->SetSeq().SetInst().SetLength(27);
    nuc->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    unit_test_util::SetBiomol(nuc, CMolInfo::eBiomol_mRNA);

    unit_test_util::ChangeId(stray, "2");
    entry->SetSet().SetSeq_set().push_back(stray);
    CRef<CSeq_feat> cds(new CSeq_feat());
    cds->SetData().SetCdregion();
    cds->SetLocation().SetInt().SetFrom(30);
    cds->SetLocation().SetInt().SetTo(56);
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    cds->SetProduct().SetWhole().SetLocal().SetStr("prot2");
    unit_test_util::AddFeat(cds, contig);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc2", eDiag_Warning, "GenomicProductPackagingProblem",
                                                 "Nucleotide bioseq should be product of mRNA feature on contig, but is not"));

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSwithNoMRNA",
                                                 "Unmatched CDS"));


    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    // take CDS away and add mrna - that way protein is orphan, nucleotide is product
    contig->SetSeq().SetAnnot().front()->SetData().SetFtable().pop_back();

    CRef<CSeq_feat> mrna (new CSeq_feat());
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->SetData().SetRna().SetExt().SetName("fake protein name");
    mrna->SetLocation().SetInt().SetFrom(30);
    mrna->SetLocation().SetInt().SetTo(56);
    mrna->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    mrna->SetProduct().SetWhole().SetLocal().SetStr("nuc2");
    unit_test_util::AddFeat(mrna, contig);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                                                 "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|prot2", eDiag_Warning, "GenomicProductPackagingProblem",
                                                 "Protein bioseq should be product of CDS feature on contig, but is not"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // put CDS back, move annotation to gen-prod-set
    scope.RemoveTopLevelSeqEntry(seh);
    contig->SetSeq().SetAnnot().front()->SetData().SetFtable().push_back(cds);
    CRef<CSeq_feat> gene(new CSeq_feat());
    gene->SetLocation().SetInt().SetFrom(30);
    gene->SetLocation().SetInt().SetTo(56);
    gene->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    gene->SetData().SetGene().SetLocus("gene locus");
    unit_test_util::AddFeat(gene, entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrCode("GenomicProductPackagingProblem");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    expected_errors[0]->SetAccession("lcl|good");
    expected_errors[0]->SetErrMsg("Seq-annot packaged directly on genomic product set");
    expected_errors.pop_back();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().ResetAnnot();
    CRef<CSeq_feat> mrna2 (new CSeq_feat());
    mrna2->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna2->SetData().SetRna().SetExt().SetName("second protein name");
    mrna2->SetLocation().SetInt().SetFrom(27);
    mrna2->SetLocation().SetInt().SetTo(29);
    mrna2->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    mrna2->SetProduct().SetWhole().SetLocal().SetStr("nuc3");
    unit_test_util::AddFeat(mrna2, contig);
    seh = scope.AddTopLevelSeqEntry(*entry);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ProductFetchFailure",
                                                 "Unable to fetch mRNA transcript 'lcl|nuc3'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingMRNAproduct",
                                                 "Product Bioseq of mRNA feature is not packaged in the record"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GenomicProductPackagingProblem",
                                                 "Product of mRNA feature (lcl|nuc3) not packaged in genomic product set"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    // remove product from first mRNA
    mrna->ResetProduct();
    // remove second mRNA
    contig->SetSeq().SetAnnot().front()->SetData().SetFtable().pop_back();
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FeatureProductInconsistency",
                 "2 mRNA features have 1 product references"));
    expected_errors.push_back(new CExpectedError("lcl|nuc2", eDiag_Warning, "GenomicProductPackagingProblem",
                              "Nucleotide bioseq should be product of mRNA feature on contig, but is not"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GenomicProductPackagingProblem",
                 "Product of mRNA feature (?) not packaged in genomic product set"));
    CheckErrors(*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    mrna->SetPseudo(true);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|nuc2", eDiag_Warning, "GenomicProductPackagingProblem",
        "Nucleotide bioseq should be product of mRNA feature on contig, but is not"));
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


#define TESTPOPPHYMUTECO(seh, entry) \
    entry->SetSet().SetClass(CBioseq_set::eClass_pop_set); \
    eval = validator.Validate(seh, options); \
    CheckErrors (*eval, expected_errors); \
    entry->SetSet().SetClass(CBioseq_set::eClass_phy_set); \
    eval = validator.Validate(seh, options); \
    CheckErrors (*eval, expected_errors); \
    entry->SetSet().SetClass(CBioseq_set::eClass_mut_set); \
    eval = validator.Validate(seh, options); \
    CheckErrors (*eval, expected_errors); \
    entry->SetSet().SetClass(CBioseq_set::eClass_eco_set); \
    eval = validator.Validate(seh, options); \
    CheckErrors (*eval, expected_errors); \
    entry->SetSet().SetClass(CBioseq_set::eClass_small_genome_set); \
    scope.RemoveTopLevelSeqEntry(seh); \
    unit_test_util::RemoveDescriptorType(entry, CSeqdesc::e_Title); \
    seh = scope.AddTopLevelSeqEntry(*entry); \
    eval = validator.Validate(seh, options); \
    CheckErrors (*eval, expected_errors);

#define TESTWGS(seh, entry) \
    entry->SetSet().SetClass(CBioseq_set::eClass_wgs_set); \
    eval = validator.Validate(seh, options); \
    CheckErrors (*eval, expected_errors);


BOOST_AUTO_TEST_CASE(Test_PKG_InconsistentMolInfoBiomols)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();

    STANDARD_SETUP

    unit_test_util::SetBiomol(entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_cRNA);
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "InconsistentMolType",
                                                 "Molecule type (DNA) does not match biomol (RNA)"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "InconsistentMoltypeSet",
                                                 "Pop/phy/mut/eco set contains inconsistent moltype"));

    TESTPOPPHYMUTECO (seh, entry)

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RemoveDescriptorType(entry, CSeqdesc::e_Title);
    seh = scope.AddTopLevelSeqEntry(*entry);

    TESTWGS (seh, entry);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_GraphPackagingProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetAnnot().push_back(unit_test_util::BuildGoodGraphAnnot("notgood"));

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "GraphPackagingProblem",
                                                 "There is 1 mispackaged graph in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetAnnot().push_back(unit_test_util::BuildGoodGraphAnnot("alsonotgood"));
    expected_errors[0]->SetErrMsg("There are 2 mispackaged graphs in this record.");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_InternalGenBankSet)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_entry> set(new CSeq_entry());
    set->SetSet().SetClass(CBioseq_set::eClass_genbank);
    entry->SetSet().SetSeq_set().push_back(set);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Info, "InternalGenBankSet",
                                                 "Bioseq-set contains internal GenBank Bioseq-set"));

    expected_errors.push_back(new CExpectedError("", eDiag_Warning, "ImproperlyNestedSets",
                                                 "Nested sets within Pop/Phy/Mut/Eco/Wgs set"));

    TESTPOPPHYMUTECO (seh, entry)

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Info, "InternalGenBankSet",
                                                 "Bioseq-set contains internal GenBank Bioseq-set"));
    expected_errors.push_back(new CExpectedError("", eDiag_Warning, "ImproperlyNestedSets",
                                                 "Nested sets within Pop/Phy/Mut/Eco/Wgs set"));
    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RemoveDescriptorType(entry, CSeqdesc::e_Title);
    seh = scope.AddTopLevelSeqEntry(*entry);

    TESTWGS (seh, entry);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_ConSetProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    entry->SetSet().SetClass(CBioseq_set::eClass_conset);
    unit_test_util::RemoveDescriptorType(entry, CSeqdesc::e_Title);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "ConSetProblem",
                                                 "Set class should not be conset"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_NoBioseqFound)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_eco_set);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("", eDiag_Error, "NoBioseqFound",
                                                 "No Bioseqs in this entire record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_INSDRefSeqPackaging)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().front()->SetEmbl().SetAccession("EA123456");
    entry->SetSet().SetSeq_set().back()->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("emb|EA123456|", eDiag_Error, "INSDRefSeqPackaging",
                                                 "INSD and RefSeq records should not be present in the same set"));
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Error, "NoOrganismInTitle",
                                                 "RefSeq nucleotide title does not start with organism name"));
    expected_errors.push_back(new CExpectedError("emb|EA123456|", eDiag_Warning, "ComponentMissingTitle",
                              "Nucleotide component of pop/phy/mut/eco/wgs set is missing its title"));
    expected_errors.push_back(new CExpectedError("lcl|good2", eDiag_Warning, "ComponentMissingTitle",
                              "Nucleotide component of pop/phy/mut/eco/wgs set is missing its title"));
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Warning, "ComponentMissingTitle",
                              "Nucleotide component of pop/phy/mut/eco/wgs set is missing its title"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_GPSnonGPSPackaging)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    entry->SetSet().SetSeq_set().push_back(unit_test_util::BuildGoodEcoSet());
    entry->SetSet().SetSeq_set().push_back(unit_test_util::BuildGoodGenProdSet());

    WriteOutTemp (entry);
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "GPSnonGPSPackaging",
                                                 "Genomic product set and mut/pop/phy/eco set records should not be present in the same set"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "InconsistentMoltypeSet",
                                                 "Pop/phy/mut/eco set contains inconsistent moltype"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "ImproperlyNestedSets",
                                                 "Nested sets within Pop/Phy/Mut/Eco/Wgs set"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ImproperlyNestedSets",
                                                 "Nested sets within Pop/Phy/Mut/Eco/Wgs set"));


    TESTPOPPHYMUTECO (seh, entry)

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "GPSnonGPSPackaging",
                                                 "Genomic product set and mut/pop/phy/eco set records should not be present in the same set"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "InconsistentMoltypeSet",
                                                 "Pop/phy/mut/eco set contains inconsistent moltype"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "ImproperlyNestedSets",
                                                 "Nested sets within Pop/Phy/Mut/Eco/Wgs set"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ImproperlyNestedSets",
                                                 "Nested sets within Pop/Phy/Mut/Eco/Wgs set"));

    TESTWGS (seh, entry);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_RefSeqPopSet)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    entry->SetSet().SetClass(CBioseq_set::eClass_pop_set);
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Error, "NoOrganismInTitle",
                                                 "RefSeq nucleotide title does not start with organism name"));
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Critical, "RefSeqPopSet",
                                                 "RefSeq record should not be a Pop-set"));
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Warning, "ComponentMissingTitle",
                              "Nucleotide component of pop/phy/mut/eco/wgs set is missing its title"));
    expected_errors.push_back(new CExpectedError("lcl|good2", eDiag_Warning, "ComponentMissingTitle",
                              "Nucleotide component of pop/phy/mut/eco/wgs set is missing its title"));
    expected_errors.push_back(new CExpectedError("lcl|good3", eDiag_Warning, "ComponentMissingTitle",
                              "Nucleotide component of pop/phy/mut/eco/wgs set is missing its title"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_BioseqSetClassNotSet)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    entry->SetSet().SetClass(CBioseq_set::eClass_not_set);
    unit_test_util::RemoveDescriptorType(entry, CSeqdesc::e_Title);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "BioseqSetClassNotSet",
                                                 "Bioseq_set class not set"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_OrphanedProtein)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodProtSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AYZ12345");
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetGenbank().SetAccession("AYZ12345");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gb|AYZ12345|", eDiag_Error, "OrphanedProtein",
                                                 "Orphaned stand-alone protein"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetEmbl().SetAccession("AQZ12345");
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetEmbl().SetAccession("AQZ12345");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors[0]->SetAccession("emb|AQZ12345|");
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetDdbj().SetAccession("ARZ12345");
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetDdbj().SetAccession("ARZ12345");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors[0]->SetAccession("dbj|ARZ12345|");
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("ref|NC_123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_PKG_MisplacedMolInfo)
{
    
}


BOOST_AUTO_TEST_CASE(Test_PKG_ImproperlyNestedSets)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();

    STANDARD_SETUP

    // no error first

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // insert nested set
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().SetSeq_set().clear();
    entry->SetSet().SetSeq_set().push_back (unit_test_util::BuildGoodEcoSet());
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "SingleItemSet",
                              "Pop/Phy/Mut/Eco set has only one component and no alignments"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "ImproperlyNestedSets",
                                                 "Nested sets within Pop/Phy/Mut/Eco/Wgs set"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_InvalidForType)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(5);
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("prot");
    feat->SetData().SetCdregion();
    feat->SetPseudo(true);
    unit_test_util::AddFeat (feat, entry->SetSet().SetSeq_set().back());

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "InvalidForType",
                                                 "Invalid feature for a protein Bioseq."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetData().SetRna();
    feat->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetData().SetRsite();
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetData().SetTxinit();
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetData().SetGene().SetLocus("good locus");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().SetSeq_set().back()->SetSeq().SetAnnot().front()->SetData().SetFtable().pop_back();
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("nuc");
    feat->SetData().SetProt().SetName().push_back("prot name");
    unit_test_util::AddFeat(feat, entry->SetSet().SetSeq_set().front());
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Invalid feature for a nucleotide Bioseq.");
    expected_errors[0]->SetAccession("lcl|nuc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetData().SetPsec_str();
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_mRNA);
    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetFrom(0);
    loc1->SetInt().SetTo(10);
    loc1->SetInt().SetId().Assign(*(entry->SetSeq().GetId().front()));
    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetInt().SetFrom(21);
    loc2->SetInt().SetTo(35);
    loc2->SetInt().SetId().Assign(*(entry->SetSeq().GetId().front()));
    CRef<CSeq_feat> cds(new CSeq_feat());
    cds->SetLocation().SetMix().Set().push_back(loc1);
    cds->SetLocation().SetMix().Set().push_back(loc2);
    cds->SetData().SetCdregion();
    cds->SetPseudo(true);
    unit_test_util::AddFeat(cds, entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("lcl|good");
    expected_errors[0]->SetErrMsg("Multi-interval CDS feature is invalid on an mRNA (cDNA) Bioseq.");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // different warning level if RefSeq
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> rsid(new CSeq_id());
    rsid->SetOther().SetAccession("NY_123456");
    unit_test_util::ChangeId(entry, rsid);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetAccession("ref|NY_123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> good_id(new CSeq_id());
    good_id->SetLocal().SetStr("good");
    unit_test_util::ChangeId(entry, good_id);
    cds->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("lcl|good");
    expected_errors[0]->SetErrCode("CDSmRNAMismatchLocation");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetErrMsg("No CDS location match for 1 mRNA");

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                                                 "mRNA feature is invalid on an mRNA (cDNA) Bioseq."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetData().SetImp().SetKey("CAAT_signal");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidForType",
                                                 "Invalid feature for an mRNA Bioseq."));
//    expected_errors[0]->SetErrMsg("Invalid feature for an mRNA Bioseq.");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetBiomol(entry, CMolInfo::eBiomol_pre_RNA);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Invalid feature for an pre-RNA Bioseq.");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    vector<string> peptide_feat;
    peptide_feat.push_back("mat_peptide");
    peptide_feat.push_back("sig_peptide");
    peptide_feat.push_back("transit_peptide");
    peptide_feat.push_back("preprotein");
    peptide_feat.push_back("proprotein");
    
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> imp(new CSeq_feat());
    imp->SetLocation().SetInt().SetFrom(0);
    imp->SetLocation().SetInt().SetTo(5);
    imp->SetLocation().SetInt().SetId().SetLocal().SetStr("prot");
    unit_test_util::AddFeat(imp, entry->SetSet().SetSeq_set().back());
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors[0]->SetErrCode("PeptideFeatureLacksCDS");
    expected_errors[0]->SetErrMsg("Peptide processing feature should be converted to the appropriate protein feature subtype");
    CRef<CSeq_id> local_id(new CSeq_id());
    local_id->SetLocal().SetStr("good");
    ITERATE(vector<string>, key, peptide_feat) {
        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeProtId(entry, local_id);
        imp->SetData().SetImp().SetKey(*key);
        seh = scope.AddTopLevelSeqEntry(*entry);
        expected_errors[0]->SetAccession("lcl|good");
        expected_errors[0]->SetSeverity(eDiag_Warning);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeProtId(entry, rsid);
        imp->SetData().SetImp().SetKey(*key);
        seh = scope.AddTopLevelSeqEntry(*entry);
        expected_errors[0]->SetAccession("ref|NY_123456|");
        expected_errors[0]->SetSeverity(eDiag_Error);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    vector<string> rna_feat;
    rna_feat.push_back("mRNA");
    rna_feat.push_back("tRNA");
    rna_feat.push_back("rRNA");
    rna_feat.push_back("snRNA");
    rna_feat.push_back("scRNA");
    rna_feat.push_back("snoRNA");
    rna_feat.push_back("misc_RNA");
    rna_feat.push_back("precursor_RNA");

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors[0]->SetErrCode("InvalidForType");
    expected_errors[0]->SetErrMsg("RNA feature should be converted to the appropriate RNA feature subtype, location should be converted manually");
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[0]->SetAccession("lcl|good");
    ITERATE(vector<string>, key, rna_feat) {
        scope.RemoveTopLevelSeqEntry(seh);
        entry->SetSeq().ResetAnnot();
        CRef<CSeq_feat> rna = unit_test_util::AddMiscFeature(entry);
        rna->SetData().SetImp().SetKey(*key);
        seh = scope.AddTopLevelSeqEntry(*entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    vector<CProt_ref::TProcessed> prot_types;
    prot_types.push_back(CProt_ref::eProcessed_mature);
    prot_types.push_back(CProt_ref::eProcessed_transit_peptide);
    prot_types.push_back(CProt_ref::eProcessed_signal_peptide);
    prot_types.push_back(CProt_ref::eProcessed_preprotein);

    entry->SetSeq().ResetAnnot();
    CRef<CSeq_feat> prot(new CSeq_feat());
    prot->SetLocation().SetInt().SetFrom(0);
    prot->SetLocation().SetInt().SetTo(10);
    prot->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    prot->SetData().SetProt().SetName().push_back("unnamed");
    unit_test_util::AddFeat(prot, entry);
    expected_errors[0]->SetErrMsg("Invalid feature for a nucleotide Bioseq.");
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "InvalidForType",
        "Peptide processing feature should be remapped to the appropriate protein bioseq"));

    ITERATE(vector<CProt_ref::TProcessed>, key, prot_types) {
        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeId(entry, local_id);
        prot->SetData().SetProt().SetProcessed(*key);
        seh = scope.AddTopLevelSeqEntry(*entry);
        expected_errors[0]->SetAccession("lcl|good");
        expected_errors[1]->SetAccession("lcl|good");
        expected_errors[1]->SetSeverity(eDiag_Warning);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        scope.RemoveTopLevelSeqEntry(seh);
        unit_test_util::ChangeId(entry, rsid);
        prot->SetData().SetProt().SetProcessed(*key);
        seh = scope.AddTopLevelSeqEntry(*entry);
        expected_errors[0]->SetAccession("ref|NY_123456|");
        expected_errors[1]->SetAccession("ref|NY_123456|");
        expected_errors[1]->SetSeverity(eDiag_Error);
        expected_errors.push_back(new CExpectedError("ref|NY_123456|", eDiag_Warning, "UndesiredProteinName",
                                                     "Uninformative protein name 'unnamed'"));
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        delete expected_errors[2];
        expected_errors.pop_back();
    }

    CLEAR_ERRORS
}


static CRef<CSeq_entry> BuildGoodSpliceNucProtSet (void)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet ();
    CRef<CSeq_entry> nseq = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> pseq = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat>  cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat>  prot = pseq->SetSeq().SetAnnot().front()->SetData().SetFtable().front();

    nseq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGGTATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");

    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(nseq->SetSeq().SetId().front()));
#if 0
    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetId().SetLocal().SetStr("nuc");
    loc1->SetInt().SetFrom(0);
    loc1->SetInt().SetTo(15);

    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetInt().SetId().SetLocal().SetStr("nuc");
    loc2->SetInt().SetFrom(46);
    loc2->SetInt().SetTo(56);

    cds->SetLocation().SetMix().Set().push_back(loc1);
    cds->SetLocation().SetMix().Set().push_back(loc2);
#endif

    return entry;
}
   

BOOST_AUTO_TEST_CASE(Test_FEAT_PartialProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetPartial(true);
    unit_test_util::SetCompleteness (entry->SetSet().SetSeq_set().back(), CMolInfo::eCompleteness_complete);
    CRef<CSeq_entry> nuc_seq = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> prot_seq = entry->SetSet().SetSeq_set().back();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistent",
                                                 "Inconsistent: Product= complete, Location= partial, Feature.partial= TRUE"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
                                                 "CDS is partial but protein is complete"));
    // cds 5' partial, protein complete
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    // cds 5' complete, protein 5' partial
    cds->SetLocation().SetPartialStart(false, eExtreme_Biological);
    cds->SetPartial(false);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_left);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistent",
                                                 "Inconsistent: Product= partial, Location= complete, Feature.partial= FALSE"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
                                                 "CDS is 5' complete but protein is NH2 partial"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    // cds 5' partial, protein 3' partial
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetPartial(true);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_right);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblemHasStop",
                                                 "Got stop codon, but 3'end is labeled partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
                                                 "CDS is 3' complete but protein is CO2 partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblem",
                                                 "CDS is 5' partial but protein is CO2 partial"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    // cds 3' partial, protein 5' partial
    cds->SetLocation().SetPartialStart(false, eExtreme_Biological);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_left);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemNotConsensus3Prime",
                                                 "3' partial is not at end of sequence, gap, or consensus splice site"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblemHasStop",
                                                 "Got stop codon, but 3'end is labeled partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
                                                 "CDS is 5' complete but protein is NH2 partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
                                                 "CDS is 3' partial but protein is NH2 partial"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    // cds 5' partial, protein no ends
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetLocation().SetPartialStop(false, eExtreme_Biological);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_ends);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblemHasStop",
                                                 "Got stop codon, but 3'end is labeled partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblem",
                                                 "CDS is 5' partial but protein has neither end"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    // cds 3' partial, protein no ends
    cds->SetLocation().SetPartialStart(false, eExtreme_Biological);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_ends);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemNotConsensus3Prime",
                                                 "3' partial is not at end of sequence, gap, or consensus splice site"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblemHasStop",
                                                 "Got stop codon, but 3'end is labeled partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
                                                 "CDS is 3' partial but protein has neither end"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    // cds complete, protein no ends
    cds->SetLocation().SetPartialStart(false, eExtreme_Biological);
    cds->SetLocation().SetPartialStop(false, eExtreme_Biological);
    cds->SetPartial(false);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_ends);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistent",
        "Inconsistent: Product= partial, Location= complete, Feature.partial= FALSE"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblemHasStop",
                                                 "Got stop codon, but 3'end is labeled partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
                                                 "CDS is complete but protein has neither end"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    // misc feature with location whole but not marked partial
    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_complete);
    unit_test_util::SetCompleteness (nuc_seq, CMolInfo::eCompleteness_no_left);
    CRef<CSeq_feat> misc_feat = unit_test_util::AddMiscFeature (nuc_seq);
    misc_feat->SetLocation().SetWhole().SetLocal().SetStr("nuc");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "WholeLocation",
        "Feature may not have whole location"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Info, "PartialProblem",
        "On partial Bioseq, SeqFeat.partial should be TRUE"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
 
    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetCompleteness (nuc_seq, CMolInfo::eCompleteness_unknown);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_left);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetLocation().SetPartialStop(false, eExtreme_Biological);
    cds->SetPartial(true);
    nuc_seq->SetSeq().SetAnnot().front()->SetData().SetFtable().pop_back();
    misc_feat = unit_test_util::AddMiscFeature (nuc_seq);
    misc_feat->SetPartial(true);
    misc_feat->SetProduct().SetWhole().SetLocal().SetStr("prot");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblem",
        "When SeqFeat.product is a partial Bioseq, SeqFeat.location should also be partial"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    nuc_seq->SetSeq().ResetAnnot();
    CRef<CSeq_loc> first(new CSeq_loc());
    first->SetInt().SetId().SetLocal().SetStr("nuc");
    first->SetInt().SetFrom(0);
    first->SetInt().SetTo(5);
    CRef<CSeq_loc> middle(new CSeq_loc());
    middle->SetNull();
    CRef<CSeq_loc> last(new CSeq_loc());
    last->SetInt().SetId().SetLocal().SetStr("nuc");
    last->SetInt().SetFrom(7);
    last->SetInt().SetTo(10);

    CRef<CSeq_feat> gene_feat(new CSeq_feat());
    gene_feat->SetData().SetGene().SetLocus("locus value");
    gene_feat->SetLocation().SetMix().Set().push_back(first);
    gene_feat->SetLocation().SetMix().Set().push_back(middle);
    gene_feat->SetLocation().SetMix().Set().push_back(last);
    unit_test_util::AddFeat (gene_feat, nuc_seq);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "MultiIntervalGene",
        "Gene feature on non-segmented sequence should not have multiple intervals"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSgeneRange",
        "gene [locus value:[lcl|nuc:1-6, ~, 8-11]] overlaps CDS but does not completely contain it"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblem",
        "Gene of 'order' with otherwise complete location should have partial flag set"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    nuc_seq->SetSeq().ResetAnnot();
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetLocation().SetPartialStop(false, eExtreme_Biological);
    cds->SetPartial(true);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_partial);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblem",
        "5' or 3' partial location should not have unclassified partial in product molinfo descriptor"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodSpliceNucProtSet();
    misc_feat = unit_test_util::AddMiscFeature (entry->SetSet().SetSeq_set().front(), 15);
    misc_feat->SetLocation().SetPartialStop(true, eExtreme_Biological);
    misc_feat->SetPartial(true);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, 
                              "PartialProblem3Prime", 
                              "Stop does not include first/last residue of sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    misc_feat->SetLocation().SetInt().SetFrom(46);
    misc_feat->SetLocation().SetInt().SetTo(56);
    misc_feat->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc_feat->SetLocation().SetPartialStop(false, eExtreme_Biological);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, 
                              "PartialProblem5Prime", 
                              "Start does not include first/last residue of sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    // take misc_feat away
    entry->SetSet().SetSeq_set().front()->SetSeq().ResetAnnot();
    // cds, but splicing not expected
    // do not report, per V-763
    unit_test_util::SetDiv (entry, "BCT");
    entry->SetSet().ResetAnnot();
    cds.Reset(new CSeq_feat());
    cds->SetData().SetCdregion();
    cds->SetProduct().SetWhole().SetLocal().SetStr("prot");
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr("nuc");
    cds->SetLocation().SetInt().SetFrom(0);
    cds->SetLocation().SetInt().SetTo(15);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    cds->SetPartial(true);
    unit_test_util::AddFeat (cds, entry->SetSet().SetSeq_set().front());
    prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKT");
    prot_seq->SetSeq().SetInst().SetLength(5);
    prot_seq->SetSeq().ResetAnnot();
    CRef<CSeq_feat> prot_feat = unit_test_util::AddProtFeat(prot_seq);   
    prot_feat->SetLocation().SetPartialStop(true, eExtreme_Biological);
    prot_feat->SetPartial(true);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_right);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // splicing expected but on mRNA
    unit_test_util::SetDiv (entry, "PRI");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    unit_test_util::SetBiomol (entry->SetSet().SetSeq_set().front(), CMolInfo::eBiomol_mRNA);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemmRNASequence3Prime",
        "Stop does not include first/last residue of mRNA sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetFrom(3);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetPartial(true);
    nuc_seq = entry->SetSet().SetSeq_set().front();
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[2] = '#';
    prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("PRKTEIN");
    prot_seq->SetSeq().SetInst().SetLength(7);
    prot_feat = prot_seq->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    prot_feat->SetLocation().SetInt().SetTo(6);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_left);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Critical, "InvalidResidue", "Invalid residue '#' at position [3]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Info, "PartialProblem",
        "PartialLocation: Start does not include first/last residue of sequence (and is at bad sequence)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetTo(23);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    cds->SetPartial(true);
    nuc_seq = entry->SetSet().SetSeq_set().front();
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[24] = '#';
    prot_seq = entry->SetSet().SetSeq_set().back();
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_right);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Invalid residue '#' at position [25]");
    expected_errors[2]->SetErrMsg("PartialLocation: Stop does not include first/last residue of sequence (and is at bad sequence)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetFrom(3);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetPartial(true);
    prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("PRKTEIN");
    prot_seq->SetSeq().SetInst().SetLength(7);
    prot_feat = prot_seq->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    prot_feat->SetLocation().SetInt().SetTo(6);
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_left);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemNotConsensus5Prime",
        "5' partial is not at beginning of sequence, gap, or consensus splice site"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetTo(23);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    cds->SetPartial(true);
    prot_seq = entry->SetSet().SetSeq_set().back();
    unit_test_util::SetCompleteness (prot_seq, CMolInfo::eCompleteness_no_right);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemNotConsensus3Prime",
        "3' partial is not at end of sequence, gap, or consensus splice site"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    misc_feat = unit_test_util::AddMiscFeature (entry);
    misc_feat->SetLocation().SetInt().SetFrom(3);
    misc_feat->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc_feat->SetPartial(true);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PartialProblem5Prime",
        "Start does not include first/last residue of sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    misc_feat->SetLocation().SetPartialStart(false, eExtreme_Biological);
    misc_feat->SetLocation().SetPartialStop(true, eExtreme_Biological);
    expected_errors[0]->SetErrCode("PartialProblem3Prime");
    expected_errors[0]->SetErrMsg("Stop does not include first/last residue of sequence");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    misc_feat->SetLocation().Assign(*unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front()));
    misc_feat->SetLocation().SetMix().Set().front()->SetPartialStop(true, eExtreme_Biological);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "PartialProblem",
        "PartialLocation: Internal partial intervals do not include first/last residue of sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // suppress for RefSeq
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> refseq_id(new CSeq_id());
    refseq_id->SetOther().SetAccession("NC_123456");
    entry->SetSeq().SetId().push_back(refseq_id);
    seh = scope.AddTopLevelSeqEntry(*entry); 
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetPartial(true);
    prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("KPRKTEIN");
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistent",
        "Inconsistent: Product= complete, Location= complete, Feature.partial= TRUE"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
        "Start of location should probably be partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
        "This SeqFeat should not be partial"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetPartial(true);
    cds->SetLocation().SetInt().SetTo(23);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistent",
        "Inconsistent: Product= complete, Location= complete, Feature.partial= TRUE"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
        "End of location should probably be partial"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetPartial(true);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors[1]->SetErrMsg("This SeqFeat should not be partial");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemNotConsensus3Prime",
                                                 "3' partial is not at end of sequence, gap, or consensus splice site"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistent",
        "Inconsistent: Product= complete, Location= partial, Feature.partial= TRUE"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblemHasStop",
                                                 "Got stop codon, but 3'end is labeled partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem",
                                                 "CDS is partial but protein is complete"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


void SetUpMiscForPartialTest(CSeq_feat& feat, TSeqPos start, TSeqPos stop, bool pseudo)
{
    feat.SetLocation().SetInt().SetFrom(start);
    feat.SetLocation().SetInt().SetTo(stop);
    if (pseudo) {
        feat.SetPseudo(true);
    } else {
        feat.ResetPseudo();
    }
}


void CheckMiscPartialErrors(CRef<CSeq_entry> entry, bool expect_bad_5, bool expect_bad_3)
{
    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    if (expect_bad_5) {
        expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
            "PartialProblem5Prime",
            "Start does not include first/last residue of sequence"));
    }
    if (expect_bad_3) {
        expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
            "PartialProblem3Prime",
            "Stop does not include first/last residue of sequence"));
    }
    if (entry->GetSeq().GetAnnot().front()->GetData().GetFtable().front()->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
        expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
            "CDSmRNAMismatchLocation", "No CDS location match for 1 mRNA"));
    }
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS
}


void TestOneMiscPartial(CRef<CSeq_entry> entry, TSeqPos good_start, TSeqPos bad_start, TSeqPos good_stop, TSeqPos bad_stop, bool is_mrna)
{
    entry->SetSeq().ResetAnnot();
    CRef<CSeq_feat> misc = AddMiscFeature(entry);
    if (is_mrna) {
        misc->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
        misc->SetData().SetRna().SetExt().SetName("fake mRNA name");
    }
    misc->SetLocation().SetPartialStart(true, eExtreme_Biological);
    misc->SetLocation().SetPartialStop(true, eExtreme_Biological);
    misc->SetPartial(true);

    SetUpMiscForPartialTest(*misc, good_start, good_stop, false);
    CheckMiscPartialErrors(entry, false, false);

    SetUpMiscForPartialTest(*misc, good_start, good_stop, true);
    CheckMiscPartialErrors(entry, false, false);

    SetUpMiscForPartialTest(*misc, bad_start, good_stop, false);
    CheckMiscPartialErrors(entry, true, false);

    SetUpMiscForPartialTest(*misc, bad_start, good_stop, true);
    CheckMiscPartialErrors(entry, false, false);

    SetUpMiscForPartialTest(*misc, good_start, bad_stop, false);
    CheckMiscPartialErrors(entry, false, true);

    SetUpMiscForPartialTest(*misc, good_start, bad_stop, true);
    CheckMiscPartialErrors(entry, false, false);

    SetUpMiscForPartialTest(*misc, bad_start, bad_stop, false);
    CheckMiscPartialErrors(entry, true, true);

    SetUpMiscForPartialTest(*misc, bad_start, bad_stop, true);
    CheckMiscPartialErrors(entry, false, false);
}


BOOST_AUTO_TEST_CASE(Test_VR_763)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    // ends
    TestOneMiscPartial(entry, 0, 1, entry->GetSeq().GetLength() - 1, entry->GetSeq().GetLength() - 2, false);
#if 0
    TestOneMiscPartial(entry, 0, 1, entry->GetSeq().GetLength() - 1, entry->GetSeq().GetLength() - 2, true);

    // gap
    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATGCCCAAATTTGGGAAAA", objects::CSeq_inst::eMol_dna);
    CRef<objects::CDelta_seq> gap1(new objects::CDelta_seq());
    gap1->SetLiteral().SetSeq_data().SetGap();
    gap1->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap1);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCATGATGATGAAATTTGGGCCCC", objects::CSeq_inst::eMol_dna);
    CRef<objects::CDelta_seq> gap2(new objects::CDelta_seq());
    gap2->SetLiteral().SetSeq_data().SetGap();
    gap2->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap2);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("AAACCCATGATGATGCCAATTCCCG", objects::CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetLength(95);
    TestOneMiscPartial(entry, 36, 37, 58, 57, false);
    TestOneMiscPartial(entry, 36, 37, 58, 57, true);

    // splice
    entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AGTTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCGT");
    TestOneMiscPartial(entry, 0, 2, 59, 57, false);
    TestOneMiscPartial(entry, 2, 3, 57, 56, true);

#endif
}


BOOST_AUTO_TEST_CASE(Test_FEAT_InvalidType)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature (entry);
    misc->SetData().Reset();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidType",
                                                 "Invalid SeqFeat type [0]"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_Range)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front());
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(14);
    unit_test_util::AddFeat (trna, entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "Range",
                                                 "Anticodon is not 3 bases in length"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Range",
                                                 "Anticodon location not in tRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "Range",
        "Anticodon location [lcl|good:15-14] out of range"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(100);
    expected_errors[2]->SetErrMsg("Anticodon location [lcl|good:15-101] out of range");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(50);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(kInvalidSeqPos);
    expected_errors[2]->SetErrMsg("Anticodon location [lcl|good:0-51] out of range");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    CRef<CCode_break> codebreak(new CCode_break());
    codebreak->SetLoc().SetInt().SetId().SetLocal().SetStr("nuc");
    codebreak->SetLoc().SetInt().SetFrom(27);
    codebreak->SetLoc().SetInt().SetTo(29);
    cds->SetData().SetCdregion().SetCode_break().push_back(codebreak);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "Range",
                                                 "Code-break location not in coding region"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    codebreak->SetLoc().SetInt().SetFrom(0);
    codebreak->SetLoc().SetInt().SetTo(1);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    CRef<CSeq_entry> nentry = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetTo(nentry->GetSeq().GetInst().GetLength() - 1);
    unit_test_util::SetNucProtSetPartials (entry, true, true);
    unit_test_util::RetranslateCdsForNucProtSet (entry, scope);
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "Range",
                                                 "Code-break location not in coding region - may be frame problem"));
    SetDiagFilter(eDiagFilter_All, "!(1210.8)");
    eval = validator.Validate(seh, options);
    SetDiagFilter(eDiagFilter_All, "");
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature (entry);
    misc->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    misc->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('N');
    misc->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetId().SetLocal().SetStr("good");
    misc->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(11);
    misc->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(13);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "Range",
                                                 "Anticodon location not in tRNA"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    misc->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(6);
    misc->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(10);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetErrMsg("Anticodon is not 3 bases in length");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    misc = unit_test_util::AddMiscFeature (entry);
    misc->SetLocation().SetInt().SetFrom(11);
    SetDiagFilter(eDiagFilter_All, "!(1204.1)");
    seh = scope.AddTopLevelSeqEntry(*entry);
    SetDiagFilter(eDiagFilter_All, "");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    expected_errors[0]->SetErrMsg("Location: SeqLoc [lcl|good:12-11] out of range");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    misc = unit_test_util::AddMiscFeature (entry);
    misc->SetLocation().SetInt().SetTo(100);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Location: SeqLoc [lcl|good:1-101] out of range");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_tRNA_Mixed_Loc) // Jira: VR_133
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front()); // N(Asn)
    CRef<CSeq_loc> anticodon_loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    anticodon_loc->SetMix().Set().front()->SetInt().SetFrom(0);	// A
    anticodon_loc->SetMix().Set().front()->SetInt().SetTo(0);
    anticodon_loc->SetMix().Set().front()->SetInt().SetStrand(eNa_strand_plus);
    anticodon_loc->SetMix().Set().back()->SetInt().SetFrom(2); // TT
    anticodon_loc->SetMix().Set().back()->SetInt().SetTo(3);
    anticodon_loc->SetMix().Set().back()->SetInt().SetStrand(eNa_strand_plus);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().Assign(*anticodon_loc);
    unit_test_util::AddFeat (trna, entry);

    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_MixedStrand)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front());
    CRef<CSeq_loc> anticodon_loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    anticodon_loc->SetMix().Set().front()->SetInt().SetFrom(0);
    anticodon_loc->SetMix().Set().front()->SetInt().SetTo(0);
    anticodon_loc->SetMix().Set().front()->SetInt().SetStrand(eNa_strand_minus);
    anticodon_loc->SetMix().Set().back()->SetInt().SetFrom(9);
    anticodon_loc->SetMix().Set().back()->SetInt().SetTo(10);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().Assign(*anticodon_loc);
    unit_test_util::AddFeat (trna, entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MixedStrand",
                                                 "Mixed strands in Anticodon [[lcl|good:c1-1, 10-11]]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadAnticodonAA",
                                                 "Codons predicted from anticodon (UAA) cannot produce amino acid (N/Asn)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front());
    anticodon_loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    anticodon_loc->SetMix().Set().front()->SetInt().SetFrom(0);
    anticodon_loc->SetMix().Set().front()->SetInt().SetTo(0);
    anticodon_loc->SetMix().Set().front()->SetInt().SetStrand(eNa_strand_plus);
    anticodon_loc->SetMix().Set().back()->SetInt().SetFrom(9);
    anticodon_loc->SetMix().Set().back()->SetInt().SetTo(10);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().Assign(*anticodon_loc);
    unit_test_util::AddFeat (trna, entry);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors[0]->SetErrMsg("Mixed plus and unknown strands in Anticodon [[lcl|good:1-1, 10-11]]");
    expected_errors[1]->SetErrMsg("Codons predicted from anticodon (AAA) cannot produce amino acid (N/Asn)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene = AddMiscFeature(entry);
    CRef<CSeq_loc> gene_loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    gene_loc->SetMix().Set().front()->SetInt().SetFrom(0);
    gene_loc->SetMix().Set().front()->SetInt().SetTo(0);
    gene_loc->SetMix().Set().front()->SetInt().SetStrand(eNa_strand_minus);
    gene_loc->SetMix().Set().back()->SetInt().SetFrom(9);
    gene_loc->SetMix().Set().back()->SetInt().SetTo(10);
    gene->SetLocation().Assign(*gene_loc);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MixedStrand",
        "Location: Mixed strands in SeqLoc [(lcl|good:c1-1, 10-11)]"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // warning if gene is pseudo
    scope.RemoveTopLevelSeqEntry(seh);
    gene->SetPseudo(true);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_SeqLocOrder)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front());
    CRef<CSeq_loc> anticodon_loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    anticodon_loc->SetMix().Set().front()->SetInt().SetFrom(9);
    anticodon_loc->SetMix().Set().front()->SetInt().SetTo(10);
    anticodon_loc->SetMix().Set().back()->SetInt().SetFrom(0);
    anticodon_loc->SetMix().Set().back()->SetInt().SetTo(0);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().Assign(*anticodon_loc);
    unit_test_util::AddFeat (trna, entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "SeqLocOrder",
                                                 "Intervals out of order in Anticodon [[lcl|good:10-11, 1-1]]"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadAnticodonAA",
                                                 "Codons predicted from anticodon (AAA) cannot produce amino acid (N/Asn)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature (entry);
    misc->SetLocation().Assign(*anticodon_loc);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "SeqLocOrder",
                                                 "Location: Intervals out of order in SeqLoc [(lcl|good:10-11, 1-1)]"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_CdTransFail)
{
    SetDiagFilter(eDiagFilter_All, "!(1204.1)");
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetFrom(27);
    STANDARD_SETUP

    BOOST_CHECK_EQUAL(validator::HasNoStop(*cds, &scope), true);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Critical, "Range",
                                                 "Location: SeqLoc [lcl|nuc:28-27] out of range"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "ProductLength",
                                                 "Protein product length [8] is more than 120% of the translation length [0]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "TransLen",
                                                 "Given protein length [8] does not match translation length [0]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "NoStop",
                                                 "Missing stop codon"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    SetDiagFilter(eDiagFilter_All, "");
}


BOOST_AUTO_TEST_CASE(Test_FEAT_StartCodon)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetFrom(1);
    cds->SetLocation().SetInt().SetTo(27);

    CRef<CSeq_entry> nuc(new CSeq_entry());
    nuc->Assign(*unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry));
    CRef<CSeq_feat> nuc_only_cds(new CSeq_feat());
    nuc_only_cds->Assign(*cds);
    unit_test_util::AddFeat(nuc_only_cds, nuc);

    STANDARD_SETUP

    BOOST_CHECK_EQUAL(validator::HasInternalStop(*cds, scope, false), true);
    BOOST_CHECK_EQUAL(validator::HasBadStartCodon(*cds, scope, false), true);
    BOOST_CHECK_EQUAL(validator::HasNoStop(*cds, &scope), true);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "StartCodon",
                              "Illegal start codon (and 1 internal stops). Probably wrong genetic code [0]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "InternalStop",
                              "1 internal stops (and illegal start codon). Genetic code [0]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "NoStop",
                                                 "Missing stop codon"));
    CExpectedError* protlen = new CExpectedError("lcl|nuc", eDiag_Error, "TransLen",
                                                 "Given protein length [8] does not match translation length [9]");
    expected_errors.push_back(protlen);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*nuc);
    eval = validator.Validate(seh, options);
    CExpectedError* no_pub = new CExpectedError("lcl|nuc", eDiag_Error, "NoPubFound", "No publications anywhere on this entire record.");
    CExpectedError* no_sub = new CExpectedError("lcl|nuc", eDiag_Info, "MissingPubRequirement", "No submission citation anywhere on this entire record.");
    CExpectedError* no_org = new CExpectedError("lcl|nuc", eDiag_Error, "NoOrgFound", "No organism name anywhere on this entire record.");
    expected_errors.pop_back();
    expected_errors.push_back(no_pub);
    expected_errors.push_back(no_sub);
    expected_errors.push_back(no_org);
    CheckErrors(*eval, expected_errors);
    expected_errors.pop_back();
    expected_errors.pop_back();
    expected_errors.pop_back();
    expected_errors.pop_back();
    expected_errors.push_back(protlen);

    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);

    // don't report start codon if unclassified exception
    cds->SetExcept(true);
    cds->SetExcept_text("unclassified translation discrepancy");
    delete expected_errors[0];
    expected_errors[0] = new CExpectedError("lcl|nuc", eDiag_Error, "ExceptionProblem", "unclassified translation discrepancy is not a legal exception explanation");
    expected_errors[1]->SetSeverity(eDiag_Warning);
    delete expected_errors[2];
    expected_errors.pop_back();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    nuc_only_cds->Assign(*cds);
    seh = scope.AddTopLevelSeqEntry(*nuc);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(no_pub);
    expected_errors.push_back(no_sub);
    expected_errors.push_back(no_org);
    CheckErrors(*eval, expected_errors);
    expected_errors.pop_back();
    expected_errors.pop_back();
    expected_errors.pop_back();

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetExcept(false);
    cds->ResetExcept_text();
    CRef<CSeq_entry> nuc_seq = entry->SetSet().SetSeq_set().front();
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[0] = 'C';
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[1] = 'C';
    seh = scope.AddTopLevelSeqEntry(*entry);
    delete expected_errors[0];
    expected_errors[0] = new CExpectedError("lcl|nuc", eDiag_Error, "StartCodon",
                              "Illegal start codon used. Wrong genetic code [0] or protein should be partial");
    delete expected_errors[1];
    expected_errors.pop_back();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // don't report start codon if unclassified exception
    cds->SetExcept(true);
    cds->SetExcept_text("unclassified translation discrepancy");
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "ExceptionProblem", "unclassified translation discrepancy is not a legal exception explanation"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    delete no_pub;
    delete no_sub;
    delete no_org;
}


BOOST_AUTO_TEST_CASE(Test_FEAT_InternalStop)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetFrom(1);
    cds->SetLocation().SetInt().SetTo(27);

    STANDARD_SETUP

    BOOST_CHECK_EQUAL(validator::HasInternalStop(*cds, scope, false), true);
    BOOST_CHECK_EQUAL(validator::HasBadStartCodon(*cds, scope, false), true);
    BOOST_CHECK_EQUAL(validator::HasNoStop(*cds, &scope), true);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "StartCodon",
                              "Illegal start codon (and 1 internal stops). Probably wrong genetic code [0]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "InternalStop",
                              "1 internal stops (and illegal start codon). Genetic code [0]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "TransLen",
                                                 "Given protein length [8] does not match translation length [9]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "NoStop",
                                                 "Missing stop codon"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc_seq = entry->SetSet().SetSeq_set().front();
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[9] = 'T';
    entry->SetSet().SetSeq_set().back()->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPR*TEIN");
    seh = scope.AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(validator::HasStopInProtein(*cds, scope), true);
    BOOST_CHECK_EQUAL(validator::HasInternalStop(*cds, scope, false), true);

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "StopInProtein",
                              "[1] termination symbols in protein sequence (gene? - fake protein name)"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "InternalStop",
                              "1 internal stops. Genetic code [0]"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    CValidErrorFormat format(*objmgr);
    string rval = format.FormatForSubmitterReport(*eval, scope, eErr_SEQ_FEAT_InternalStop);
    BOOST_CHECK_EQUAL(rval, "InternalStop\nlcl|nuc:CDS\t fake protein name\tlcl|nuc:1-27\n");

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_NoProtein)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    entry->SetSet().SetSeq_set().pop_back();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->ResetProduct();
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "NucProtProblem",
                              "No proteins in nuc-prot set"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "NoProtein",
                              "No protein Bioseq given"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "MissingCDSproduct",
                              "Expected CDS product absent"));

    options |= CValidator::eVal_far_fetch_cds_products;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_FEAT_MisMatchAA)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::MakeNucProtSet3Partial(entry);
    CRef<CSeq_entry> prot = entry->SetSet().SetSeq_set().back();
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set()[0] = 'A';

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "MisMatchAA",
    "Residue 1 in protein [A] != translation [M] at lcl|nuc:1-3"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    for (int i = 0; i < 11; i++) {
      prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set()[i] = 'A';
    }


    expected_errors[0]->SetErrMsg("11 mismatches found.  First mismatch at 1, residue in protein [A] != translation [M] at lcl|nuc:1-3.  Last mismatch at 11, residue in protein [A] != translation [M] at lcl|nuc:31-33.  Genetic code [0]");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_FEAT_TransLen)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEI");
    prot_seq->SetSeq().SetInst().SetLength(7);
    unit_test_util::AdjustProtFeatForNucProtSet (entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "TransLen",
                                                 "Given protein length [7] does not match translation length [9]"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc_seq = entry->SetSet().SetSeq_set().front();
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[27] = 'A';
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[28] = 'T';
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetTo(28);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "TransLen",
                              "Coding region extends 2 base(s) past stop codon"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEINQQLLLLLLLLLLQQQQQQQQQQ");
    prot_seq->SetSeq().SetInst().SetLength(30);
    unit_test_util::AdjustProtFeatForNucProtSet (entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "ProductLength",
                              "Protein product length [30] is more than 120% of the translation length [9]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "TransLen",
        "Given protein length [30] does not match translation length [9]"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // setting this exception suppresses the error
    cds->SetExcept(true);
    cds->SetExcept_text("annotated by transcript or proteomic data");
    // inference is required for exception
    cds->AddQualifier("inference", "similar to DNA sequence:INSD:AY123456.1");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);


}


BOOST_AUTO_TEST_CASE(Test_FEAT_NoStop)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetTo(23);

    STANDARD_SETUP

    BOOST_CHECK_EQUAL(validator::HasNoStop(*cds, &scope), true);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "NoStop",
                                                 "Missing stop codon"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_TranslExcept)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->AddQualifier("transl_except", "abc");
    CRef<CSeq_entry> prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set()[4] = 'E';

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "MisMatchAA",
    "Residue 5 in protein [E] != translation [T] at lcl|nuc:13-15"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "TranslExcept",
                                                 "Unparsed transl_except qual. Skipped"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->AddQualifier("transl_except", "abc");
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "TranslExcept",
                                                 "Unparsed transl_except qual (but protein is okay). Skipped"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_FEAT_NoProtRefFound)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> prot_seq = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat> prot_feat = prot_seq->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    prot_feat->SetLocation().SetInt().SetTo (6);

    STANDARD_SETUP

    // see this error if prot-ref present, but wrong size, or if absent completely
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "NoProtRefFound",
    "No full length Prot-ref feature applied to this Bioseq"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    prot_seq->SetSeq().ResetAnnot();
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_OrfCdsHasProduct)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetData().SetCdregion().SetOrf(true);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "OrfCdsHasProduct",
    "An ORF coding region should not have a product"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_GeneRefHasNoData)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature (nuc);
    gene->SetData().SetGene();
    gene->SetLocation().SetInt().SetTo(26);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "GeneRefHasNoData",
    "There is a gene feature where all fields are empty"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_ExceptInconsistent)
{
    string except_text = "trans-splicing";
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->AddQualifier("exception", except_text);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "ExceptInconsistent",
                              "Exception flag should be set in coding region"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    cds->ResetQual();
    cds->SetExcept_text(except_text);
    expected_errors[0]->SetErrMsg("Exception text is present, but exception flag is not set");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    cds->ResetExcept_text();
    cds->SetExcept(true);

    expected_errors[0]->SetErrMsg("Exception flag is set, but exception text is empty");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_ProtRefHasNoData)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> prot_feat = entry->SetSet().SetSeq_set().back()->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    prot_feat->SetData().SetProt().Reset();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "ProtRefHasNoData",
                              "There is a protein feature where all fields are empty"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "NoNameForProtein",
                              "Protein feature has no name"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_GenCodeMismatch)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
    ce->SetId(3);
    CRef<CGenetic_code> gcode(new CGenetic_code());
    cds->SetData().SetCdregion().SetCode().Set().push_back(ce);
    unit_test_util::SetGenome (entry, CBioSource::eGenome_apicoplast);
    unit_test_util::SetGcode (entry, 2);
    CRef<CSeq_entry> prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set()[6] = 'M';

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "GenCodeMismatch",
                              "Genetic code conflict between CDS (code 3) and BioSource.genome biological context (apicoplast) (uses code 11)"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetGenome (entry, CBioSource::eGenome_unknown);

    expected_errors[0]->SetErrMsg("Genetic code conflict between CDS (code 3) and BioSource (code 2)");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // ignore gencode mismatch for specified exception text
    cds->SetExcept(true);
    cds->SetExcept_text("genetic code exception");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_FEAT_RNAtype0)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> rna = unit_test_util::AddMiscFeature (entry);
    rna->SetData().SetRna().SetType(CRNA_ref::eType_unknown);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "RNAtype0",
                              "RNA type 0 (unknown) not supported"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_UnknownImpFeatKey)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature (entry);
    misc->SetData().SetImp().SetKey("bad value");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UnknownImpFeatKey",
                              "Unknown feature key bad value"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    misc->SetData().SetImp().SetKey("");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("NULL feature key");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    vector<string> illegal_keys;
    illegal_keys.push_back ("virion");
    illegal_keys.push_back ("mutation");
    illegal_keys.push_back ("allele");
    illegal_keys.push_back ("Import");

    expected_errors[0]->SetSeverity(eDiag_Error);
    ITERATE (vector<string>, it, illegal_keys) {
        scope.RemoveTopLevelSeqEntry(seh);
        misc->SetData().SetImp().SetKey(*it);
        seh = scope.AddTopLevelSeqEntry(*entry);
        expected_errors[0]->SetErrMsg("Feature key " + *it + " is no longer legal");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FEAT_UnknownImpFeatQual)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature (entry);
    misc->AddQualifier("bad name", "some value");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UnknownImpFeatQual",
                              "Unknown qualifier bad name"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    misc->SetQual().front()->SetQual("");
    expected_errors[0]->SetErrMsg("NULL qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


// begin automatically generated section
BOOST_AUTO_TEST_CASE(Test_FEAT_MissingQualOnImpFeat)
{

    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc_feat = unit_test_util::AddMiscFeature (entry);

    STANDARD_SETUP

    scope.RemoveTopLevelSeqEntry (seh);
    entry = unit_test_util::BuildGoodSeq();
    misc_feat = unit_test_util::AddMiscFeature (entry);
    misc_feat->SetData().SetImp().SetKey("conflict");
    seh = scope.AddTopLevelSeqEntry (*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MissingQualOnImpFeat",
                              "Missing qualifier citation for feature conflict"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry (seh);
    entry = unit_test_util::BuildGoodSeq();
    misc_feat = unit_test_util::AddMiscFeature (entry);
    misc_feat->SetData().SetImp().SetKey("misc_binding");
    seh = scope.AddTopLevelSeqEntry (*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MissingQualOnImpFeat",
                              "Missing qualifier bound_moiety for feature misc_binding"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry (seh);
    entry = unit_test_util::BuildGoodSeq();
    misc_feat = unit_test_util::AddMiscFeature (entry);
    misc_feat->SetData().SetImp().SetKey("modified_base");
    seh = scope.AddTopLevelSeqEntry (*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MissingQualOnImpFeat",
                              "Missing qualifier mod_base for feature modified_base"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry (seh);
    entry = unit_test_util::BuildGoodSeq();
    misc_feat = unit_test_util::AddMiscFeature (entry);
    misc_feat->SetData().SetImp().SetKey("old_sequence");
    seh = scope.AddTopLevelSeqEntry (*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MissingQualOnImpFeat",
                              "Missing qualifier citation for feature old_sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry (seh);
    entry = unit_test_util::BuildGoodSeq();
    misc_feat = unit_test_util::AddMiscFeature (entry);
    misc_feat->SetData().SetImp().SetKey("operon");
    seh = scope.AddTopLevelSeqEntry (*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MissingQualOnImpFeat",
                              "Missing qualifier operon for feature operon"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry (seh);
    entry = unit_test_util::BuildGoodSeq();
    misc_feat = unit_test_util::AddMiscFeature (entry);
    misc_feat->SetData().SetImp().SetKey("protein_bind");
    seh = scope.AddTopLevelSeqEntry (*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MissingQualOnImpFeat",
                              "Missing qualifier bound_moiety for feature protein_bind"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry (seh);
    entry = unit_test_util::BuildGoodSeq();
    misc_feat = unit_test_util::AddMiscFeature (entry);
    misc_feat->SetData().SetImp().SetKey("source");
    seh = scope.AddTopLevelSeqEntry (*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MissingQualOnImpFeat",
                              "Missing qualifier organism for feature source"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}
//end automatically generated section


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_PseudoCdsHasProduct)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetPseudo(true);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(cds);
    gene->SetPseudo(true);
    unit_test_util::AddFeat (gene, nuc);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PseudoCdsHasProduct", "A pseudo coding region should not have a product"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gene->SetPseudo(false);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetPseudo(true);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static string MakeWrongCap (const string& str)
{
    string bad = "";
    char add[2];
    add[1] = 0;

    ITERATE(string, it, str) {
        add[0] = *it;
        if (isupper (*it)) {
            add[0] = tolower(*it);
        } else if (islower(*it)) {
            add[0] = toupper(*it);
        }
        bad.append(add);
    }
    return bad;
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_IllegalDbXref)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    STANDARD_SETUP

    vector<string> legal_strings;
    legal_strings.push_back ("AceView/WormGenes");
    legal_strings.push_back ("AFTOL");
    legal_strings.push_back ("AntWeb");
    legal_strings.push_back ("APHIDBASE");
    legal_strings.push_back ("ApiDB");
    legal_strings.push_back ("ApiDB_CryptoDB");
    legal_strings.push_back ("ApiDB_PlasmoDB");
    legal_strings.push_back ("ApiDB_ToxoDB");
    legal_strings.push_back ("ASAP");
    legal_strings.push_back ("ATCC");
    legal_strings.push_back ("ATCC(in host)");
    legal_strings.push_back ("ATCC(dna)");
    legal_strings.push_back ("Axeldb");
    legal_strings.push_back ("BDGP_EST");
    legal_strings.push_back ("BDGP_INS");
    legal_strings.push_back ("BEETLEBASE");
    legal_strings.push_back ("BOLD");
    legal_strings.push_back ("CDD");
    legal_strings.push_back ("CK");
    legal_strings.push_back ("COG");
    legal_strings.push_back ("dbClone");
    legal_strings.push_back ("dbCloneLib");
    legal_strings.push_back ("dbEST");
    legal_strings.push_back ("dbProbe");
    legal_strings.push_back ("dbSNP");
    legal_strings.push_back ("dbSTS");
    legal_strings.push_back ("dictyBase");
    legal_strings.push_back ("DDBJ");
    legal_strings.push_back ("EcoGene");
    legal_strings.push_back ("EMBL");
    // legal_strings.push_back ("ENSEMBL");
    legal_strings.push_back ("Ensembl");
    legal_strings.push_back ("ESTLIB");
    legal_strings.push_back ("FANTOM_DB");
    legal_strings.push_back ("FLYBASE");
    legal_strings.push_back ("GABI");
    legal_strings.push_back ("GDB");
    legal_strings.push_back ("GeneDB");
    legal_strings.push_back ("GeneID");
    legal_strings.push_back ("GO");
    legal_strings.push_back ("GOA");
    legal_strings.push_back ("Greengenes");
    legal_strings.push_back ("GRIN");
    legal_strings.push_back ("H-InvDB");
    legal_strings.push_back ("HGNC");
    legal_strings.push_back ("HMP");
    legal_strings.push_back ("HOMD");
    legal_strings.push_back ("HSSP");
    legal_strings.push_back ("IMGT/GENE-DB");
    legal_strings.push_back ("IMGT/HLA");
    legal_strings.push_back ("IMGT/LIGM");
    legal_strings.push_back ("InterimID");
    legal_strings.push_back ("InterPro");
    legal_strings.push_back ("IRD");
    legal_strings.push_back ("ISD");
    legal_strings.push_back ("ISFinder");
    legal_strings.push_back ("JCM");
    legal_strings.push_back ("JGIDB");
    legal_strings.push_back ("LocusID");
    legal_strings.push_back ("MaizeGDB");
    legal_strings.push_back ("MGI");
    legal_strings.push_back ("MIM");
    legal_strings.push_back ("miRBase");
    legal_strings.push_back ("MycoBank");
    legal_strings.push_back ("NBRC");
    legal_strings.push_back ("NextDB");
    legal_strings.push_back ("niaEST");
    legal_strings.push_back ("NMPDR");
    legal_strings.push_back ("NRESTdb");
    legal_strings.push_back ("Osa1");
    legal_strings.push_back ("Pathema");
    legal_strings.push_back ("PBmice");
    legal_strings.push_back ("PDB");
    legal_strings.push_back ("PFAM");
    legal_strings.push_back ("PGN");
    legal_strings.push_back ("PIR");
    legal_strings.push_back ("PSEUDO");
    legal_strings.push_back ("PseudoCap");
    legal_strings.push_back ("RAP-DB");
    legal_strings.push_back ("RATMAP");
    legal_strings.push_back ("RFAM");
    legal_strings.push_back ("RGD");
    legal_strings.push_back ("RiceGenes");
    legal_strings.push_back ("RZPD");
    legal_strings.push_back ("SEED");
    legal_strings.push_back ("SGD");
    legal_strings.push_back ("SGN");
    legal_strings.push_back ("SoyBase");
    legal_strings.push_back ("SubtiList");
    legal_strings.push_back ("TAIR");
    legal_strings.push_back ("taxon");
    legal_strings.push_back ("TIGRFAM");
    legal_strings.push_back ("UniGene");
    legal_strings.push_back ("UNILIB");
    legal_strings.push_back ("UniProtKB/Swiss-Prot");
    legal_strings.push_back ("UniProtKB/TrEMBL");
    legal_strings.push_back ("UniSTS");
    legal_strings.push_back ("UNITE");
    legal_strings.push_back ("VBASE2");
    legal_strings.push_back ("VectorBase");
    legal_strings.push_back ("WorfDB");
    legal_strings.push_back ("WormBase");
    legal_strings.push_back ("Xenbase");
    legal_strings.push_back ("ZFIN");
    vector<string> src_strings;
    src_strings.push_back ("AFTOL");
    src_strings.push_back ("AntWeb");
    src_strings.push_back ("ATCC");
    src_strings.push_back ("ATCC(dna)");
    src_strings.push_back ("ATCC(in host)");
    src_strings.push_back ("BOLD");
    src_strings.push_back ("FANTOM_DB");
    src_strings.push_back ("FLYBASE");
    src_strings.push_back ("Greengenes");
    src_strings.push_back ("GRIN");
    src_strings.push_back ("HMP");
    src_strings.push_back ("HOMD");
    src_strings.push_back ("IMGT/HLA");
    src_strings.push_back ("IMGT/LIGM");
    src_strings.push_back ("JCM");
    src_strings.push_back ("MGI");
    src_strings.push_back ("MycoBank");
    src_strings.push_back ("NBRC");
    src_strings.push_back ("RZPD");
    src_strings.push_back ("taxon");
    src_strings.push_back ("UNILIB");
    src_strings.push_back ("UNITE");
    vector<string> refseq_strings;
    refseq_strings.push_back ("CCDS");
    refseq_strings.push_back ("CGNC");
    refseq_strings.push_back ("CloneID");
    refseq_strings.push_back ("HPRD");
    refseq_strings.push_back ("LRG");
    refseq_strings.push_back ("PBR");
    refseq_strings.push_back ("REBASE");
    refseq_strings.push_back ("SK-FST");
    refseq_strings.push_back ("VBRC");

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "IllegalDbXref", 
                                         "db_xref type %s (1234) should not be used on an OrgRef"));

    string bad;
    ITERATE (vector<string>, sit, src_strings) {
        if (NStr::Equal(*sit, "taxon")) {
            unit_test_util::RemoveDbxref (entry, *sit, 0);
        }
        bad = MakeWrongCap(*sit);
        unit_test_util::SetDbxref (entry, bad, 1234);
        expected_errors[0]->SetErrMsg("Illegal db_xref type " + bad + " (1234), legal capitalization is " + *sit);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::RemoveDbxref (entry, bad, 0);
        if (NStr::Equal(*sit, "taxon")) {
            unit_test_util::SetTaxon(entry, 592768);
        }
    }

    ITERATE (vector<string>, sit, legal_strings) {
        bool found = false;
        ITERATE (vector<string>, ss, src_strings) {
            if (NStr::Equal(*ss, *sit)) {
                found = true;
                break;
            }
        }
        if (found) {
            continue;
        }
        bad = MakeWrongCap(*sit);
        unit_test_util::SetDbxref (entry, bad, 1234);
        expected_errors[0]->SetErrMsg("Illegal db_xref type " + bad + " (1234), legal capitalization is " + *sit
                                      + ", but should not be used on an OrgRef");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::RemoveDbxref (entry, bad, 0);

        unit_test_util::SetDbxref (entry, *sit, 1234);
        expected_errors[0]->SetErrMsg("db_xref type " + *sit + " (1234) should not be used on an OrgRef");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::RemoveDbxref (entry, *sit, 0);
    }

    ITERATE (vector<string>, sit, refseq_strings) {
        unit_test_util::SetDbxref (entry, *sit, 1234);
        expected_errors[0]->SetErrMsg("RefSeq-specific db_xref type " + *sit + " (1234) should not be used on a non-RefSeq OrgRef");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::RemoveDbxref (entry, *sit, 0);
    }

    unit_test_util::SetDbxref (entry, "unrecognized", 1234);
    expected_errors[0]->SetErrMsg("Illegal db_xref type unrecognized (1234)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::RemoveDbxref (entry, "unrecognized", 0);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("ref|NC_123456|");
    ITERATE (vector<string>, sit, refseq_strings) {
        unit_test_util::SetDbxref (entry, *sit, 1234);
        expected_errors[0]->SetErrMsg("RefSeq-specific db_xref type " + *sit + " (1234) should not be used on an OrgRef");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::RemoveDbxref (entry, *sit, 0);
    }

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetLocal().SetStr("good");
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("lcl|good");
    
    ITERATE (vector<string>, sit, legal_strings) {
        bad = MakeWrongCap(*sit);
        unit_test_util::SetDbxref (feat, bad, 1234);
        if (NStr::Equal(*sit, "taxon")) {
            expected_errors[0]->SetErrMsg("Illegal db_xref type TAXON (1234), legal capitalization is taxon, but should only be used on an OrgRef");
        } else {
            expected_errors[0]->SetErrMsg("Illegal db_xref type " + bad + " (1234), legal capitalization is " + *sit);
        }
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::RemoveDbxref (feat, bad, 0);
    }
    
    ITERATE (vector<string>, sit, refseq_strings) {
        unit_test_util::SetDbxref (feat, *sit, 1234);
        expected_errors[0]->SetErrMsg("db_xref type " + *sit + " (1234) is only legal for RefSeq");
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        unit_test_util::RemoveDbxref (feat, *sit, 0);
    }

    unit_test_util::SetDbxref(feat, "taxon", 1234);
    expected_errors[0]->SetErrMsg("db_xref type taxon (1234) should only be used on an OrgRef");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::RemoveDbxref (feat, "taxon", 0);

    unit_test_util::SetDbxref (feat, "unrecognized", 1234);
    expected_errors[0]->SetErrMsg("Illegal db_xref type unrecognized (1234)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::RemoveDbxref (feat, "unrecognized", 0);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FarLocation)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature (entry);
    misc->SetLocation().Assign(*unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front()));
    misc->SetLocation().SetMix().Set().back()->SetInt().SetId().SetGenbank().SetAccession("AY123456");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FarLocation", "Feature has 'far' location - accession not packaged in record"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadLocation", "Feature location intervals should all be on the same sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_DuplicateFeat)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat1 = unit_test_util::AddMiscFeature (entry);
    CRef<CSeq_feat> feat2 = unit_test_util::AddMiscFeature (entry);
    feat2->SetComment("a");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "DuplicateFeat", "Features have identical intervals, but labels differ"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // error if genbank accession
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    feat1 = unit_test_util::AddMiscFeature (entry);
    feat1->SetData().SetGene().SetLocus("locus1");
    feat2 = unit_test_util::AddMiscFeature (entry);
    feat2->SetData().SetGene().SetLocus("locus2");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("gb|AY123456|");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetDrosophila_melanogaster (entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning if genes are partial
    unit_test_util::SetSebaea_microphylla(entry);
    feat1->SetPartial(true);
    feat1->SetLocation().SetPartialStart(true, eExtreme_Biological);
    feat2->SetPartial(true);
    feat2->SetLocation().SetPartialStart(true, eExtreme_Biological);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning if genes are pseudo
    feat1->SetPartial(false);
    feat1->SetLocation().SetPartialStart(false, eExtreme_Biological);
    feat2->SetPartial(false);
    feat2->SetLocation().SetPartialStart(false, eExtreme_Biological);
    feat1->SetPseudo(true);
    feat2->SetPseudo(true);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // error if general ID
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGeneral().SetDb("abc");
    entry->SetSeq().SetId().front()->SetGeneral().SetTag().SetId(123456);
    feat1 = unit_test_util::AddMiscFeature (entry);
    feat1->SetData().SetGene().SetLocus("locus1");
    feat2 = unit_test_util::AddMiscFeature (entry);
    feat2->SetData().SetGene().SetLocus("locus2");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("gnl|abc|123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    unit_test_util::SetDrosophila_melanogaster (entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning if genes are partial
    unit_test_util::SetSebaea_microphylla(entry);
    feat1->SetPartial(true);
    feat1->SetLocation().SetPartialStart(true, eExtreme_Biological);
    feat2->SetPartial(true);
    feat2->SetLocation().SetPartialStart(true, eExtreme_Biological);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning if genes are pseudo
    feat1->SetPartial(false);
    feat1->SetLocation().SetPartialStart(false, eExtreme_Biological);
    feat2->SetPartial(false);
    feat2->SetLocation().SetPartialStart(false, eExtreme_Biological);
    feat1->SetPseudo(true);
    feat2->SetPseudo(true);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // always warning if on different annots 
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    feat1 = unit_test_util::AddMiscFeature (entry);
    CRef<CSeq_annot> annot2(new CSeq_annot());
    feat2->Assign(*feat1);
    feat2->SetComment("a");
    annot2->SetData().SetFtable().push_back(feat2);
    entry->SetSeq().SetAnnot().push_back(annot2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("lcl|good");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetErrMsg("Features have identical intervals, but labels differ (packaged in different feature table)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_UnnecessaryGeneXref)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat1 = unit_test_util::AddMiscFeature (entry, 15);
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature (entry, 15);
    gene->SetData().SetGene().SetLocus("foo");
    feat1->SetGeneXref().SetLocus("foo");

    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now gene xref is necessary
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> gene2 = unit_test_util::AddMiscFeature (entry, 15);
    gene2->SetLocation().SetPartialStart(true, eExtreme_Biological);
    gene2->SetPartial(true);
    gene2->SetData().SetGene().SetLocus("bar");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CLEAR_ERRORS
    CheckErrors (*eval, expected_errors);

    // error if gene references itself
    gene2->SetGeneXref().SetLocus("bar");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UnnecessaryGeneXref", 
                              "Gene feature has gene cross-reference"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_TranslExceptPhase)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene = MakeGeneForFeature(cds);
    gene->SetData().SetGene().SetLocus_tag("xyz");
    AddFeat(gene, nuc);

    CRef<CCode_break> codebreak(new CCode_break());
    codebreak->SetLoc().SetInt().SetId().SetLocal().SetStr("nuc");
    codebreak->SetLoc().SetInt().SetFrom(4);
    codebreak->SetLoc().SetInt().SetTo(6);
    cds->SetData().SetCdregion().SetCode_break().push_back(codebreak);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "Range",
                                                 "Code-break location not in coding region - may be frame problem"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "TranslExceptPhase", 
                              "transl_except qual out of frame."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    CValidErrorFormat format(*objmgr);
    vector<string> expected;
    expected.push_back("Range");
    expected.push_back("lcl|nuc:CDS\t fake protein name\tlcl|nuc:1-27\txyz");
    expected.push_back("");
    expected.push_back("TranslExceptPhase");
    expected.push_back("lcl|nuc:CDS\t fake protein name\tlcl|nuc:1-27\txyz");
    expected.push_back("");
    vector<string> seen;
    vector<string> cat_list = format.FormatCompleteSubmitterReport(*eval, scope);
    ITERATE(vector<string>, it, cat_list) {
        vector<string> sublist;
        NStr::Split(*it, "\n", sublist, 0);
        ITERATE(vector<string>, sit, sublist) {
            seen.push_back(*sit);
        }
    }

    CheckStrings(seen, expected);


    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_TrnaCodonWrong)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::AddMiscFeature (entry);
    trna->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna->SetData().SetRna().SetExt().SetTRNA().SetCodon().push_back(0);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('A');

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "TrnaCodonWrong", 
                      "Codon recognized by tRNA (UUU) does not match amino acid (A/Ala) specified by genetic code (1/Standard)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // drop to warning if aa is 'U' or 'O'
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('U');
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetErrMsg("Codon recognized by tRNA (UUU) does not match amino acid (U/Sec) specified by genetic code (1/Standard)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('O');
    expected_errors[0]->SetErrMsg("Codon recognized by tRNA (UUU) does not match amino acid (O/Pyl) specified by genetic code (1/Standard)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BothStrands)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature (entry);
    feat->SetData().SetGene().SetLocus("X");
    feat->SetLocation().SetInt().SetStrand(eNa_strand_both);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BothStrands", 
                      "gene may not be on both (forward) strands"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    feat = unit_test_util::AddMiscFeature (entry);
    feat->SetData().SetGene().SetLocus("X");
    feat->SetLocation().Assign(*unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front()));
    feat->SetLocation().SetMix().Set().front()->SetInt().SetStrand(eNa_strand_both);
    feat->SetLocation().SetMix().Set().back()->SetInt().SetStrand(eNa_strand_both_rev);
    // set trans-splicing exception to prevent mixed-strand error
    feat->SetExcept(true);
    feat->SetExcept_text("trans-splicing");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("gene may not be on both (forward and reverse) strands");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    feat = unit_test_util::AddMiscFeature (entry);
    feat->SetData().SetGene().SetLocus("X");
    feat->SetLocation().Assign(*unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front()));
    feat->SetLocation().SetMix().Set().front()->SetInt().SetStrand(eNa_strand_both_rev);
    feat->SetLocation().SetMix().Set().back()->SetInt().SetStrand(eNa_strand_both_rev);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("gene may not be on both (reverse) strands");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultiIntervalGene",
        "Gene feature on non-segmented sequence should not have multiple intervals"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    feat = unit_test_util::AddMiscFeature (entry);
    feat->SetLocation().Assign(*unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front()));
    feat->SetLocation().SetMix().Set().front()->SetInt().SetStrand(eNa_strand_both);
    feat->SetLocation().SetMix().Set().back()->SetInt().SetStrand(eNa_strand_both_rev);
    feat->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    feat->SetData().SetRna().SetExt().SetName("mRNA product");
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (feat);
    unit_test_util::AddFeat(gene, entry);
    // make pseudo to prevent splice errors
    feat->SetPseudo(true);
    // set trans-splicing exception to prevent mixed-strand error
    feat->SetExcept(true);
    feat->SetExcept_text("trans-splicing");
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                              "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BothStrands", 
                      "mRNA may not be on both (forward and reverse) strands"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "mRNAgeneRange",
                      "gene [gene locus:lcl|good:1-57] overlaps mRNA but does not completely contain it"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    feat = unit_test_util::AddMiscFeature (entry);
    feat->SetLocation().Assign(*unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front()));
    feat->SetLocation().SetMix().Set().front()->SetInt().SetStrand(eNa_strand_both_rev);
    feat->SetLocation().SetMix().Set().back()->SetInt().SetStrand(eNa_strand_both_rev);
    feat->SetPseudo(true);
    feat->SetData().SetCdregion();
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BothStrands", 
                      "CDS may not be on both (reverse) strands"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_CDSmRNArange)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc_seq = entry->SetSet().SetSeq_set().front();
    unit_test_util::SetSpliceForMixLoc (nuc_seq->SetSeq());
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(nuc_seq->SetSeq().SetId().front()));
    CRef<CSeq_feat> mrna(new CSeq_feat());
    mrna->SetLocation().Assign(*unit_test_util::MakeMixLoc(nuc_seq->SetSeq().SetId().front()));
    mrna->SetData().SetRna().SetType (CRNA_ref::eType_mRNA);
    mrna->SetData().SetRna().SetExt().SetName("mRNA product");
    mrna->SetLocation().SetMix().Set().front()->SetInt().SetTo(17);
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[18] = 'G';
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[19] = 'T';
    unit_test_util::AddFeat (mrna, entry->SetSet().SetSeq_set().front());

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSwithNoMRNA",
                              "Unmatched CDS"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSmRNAMismatchLocation",
                              "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSmRNArange", 
                      "mRNA contains CDS but internal intron-exon boundaries do not match"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // turn off error for ribosomal slippage and trans-splicing
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetExcept(true);
    cds->SetExcept_text("ribosomal slippage");
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[16] = 'A';
    seh = scope.AddTopLevelSeqEntry(*entry);
    
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    cds->SetExcept_text("trans-splicing");
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[16] = 'G';
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSwithNoMRNA",
                              "Unmatched CDS"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSmRNAMismatchLocation",
                              "No CDS location match for 1 mRNA"));
    CheckErrors(*eval, expected_errors);

    // overlap problem rather than internal boundary problem
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    nuc_seq = entry->SetSet().SetSeq_set().front();
    unit_test_util::SetSpliceForMixLoc (nuc_seq->SetSeq());
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(nuc_seq->SetSeq().SetId().front()));
    mrna = new CSeq_feat();
    mrna->SetLocation().Assign(cds->GetLocation());
    mrna->SetData().SetRna().SetType (CRNA_ref::eType_mRNA);
    mrna->SetData().SetRna().SetExt().SetName("mRNA product");
    mrna->SetLocation().SetMix().Set().front()->SetInt().SetTo(12);
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[13] = 'G';
    nuc_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[14] = 'T';
    unit_test_util::AddFeat (mrna, entry->SetSet().SetSeq_set().front());
    CRef<CSeq_entry> prot_seq = entry->SetSet().SetSeq_set().back();
    prot_seq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set()[4] = 'S';
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSmRNArange", 
                      "mRNA overlaps or contains CDS but does not completely contain intervals"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_OverlappingPeptideFeat)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> prot_seq = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat> p1 = unit_test_util::AddMiscFeature (prot_seq, 4);
    p1->SetData().SetProt().SetProcessed(CProt_ref::eProcessed_signal_peptide);
    p1->SetData().SetProt().SetName().push_back("unnamed");
    CRef<CSeq_feat> p2 = unit_test_util::AddMiscFeature (prot_seq, 5);
    p2->SetData().SetProt().SetProcessed(CProt_ref::eProcessed_signal_peptide);
    p2->SetData().SetProt().SetName().push_back("unnamed");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "OverlappingPeptideFeat", 
                      "Signal, Transit, or Mature peptide features overlap (parent CDS is on lcl|nuc)"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "OverlappingPeptideFeat", 
                      "Signal, Transit, or Mature peptide features overlap (parent CDS is on lcl|nuc)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodProtSeq();
    p1 = unit_test_util::AddMiscFeature (entry, 4);
    p1->SetData().SetProt().SetProcessed(CProt_ref::eProcessed_mature);
    p1->SetData().SetProt().SetName().push_back("unnamed");
    p2 = unit_test_util::AddMiscFeature (entry, 5);
    p2->SetData().SetProt().SetProcessed(CProt_ref::eProcessed_transit_peptide);
    p2->SetData().SetProt().SetName().push_back("unnamed");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "OverlappingPeptideFeat", 
                                   "Signal, Transit, or Mature peptide features overlap"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "OverlappingPeptideFeat", 
                                   "Signal, Transit, or Mature peptide features overlap"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    //no error if peptide exceptions
    p1->SetExcept(true);
    p1->SetExcept_text("alternative processing");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_SerialInComment)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature (entry);
    misc->SetComment("blah blah [123456]");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "SerialInComment", 
                      "Feature comment may refer to reference by serial number - attach reference specific comments to the reference REMARK instead."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MultipleCDSproducts)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds2 = unit_test_util::AddMiscFeature (entry);
    cds2->SetData().SetCdregion();
    cds2->SetProduct().SetWhole().Assign(*(entry->SetSet().SetSeq_set().back()->SetSeq().SetId().front()));
    cds2->SetLocation().SetInt().SetFrom(30);
    cds2->SetLocation().SetInt().SetTo(56);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Critical, "MultipleCDSproducts", 
                      "Same product Bioseq from multiple CDS features"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FocusOnBioSourceFeature)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> src = unit_test_util::AddGoodSourceFeature (entry);
    src->SetData().SetBiosrc().SetIs_focus();
    unit_test_util::SetFocus(entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "FocusOnBioSourceFeature", 
                      "Focus must be on BioSource descriptor, not BioSource feature."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_PeptideFeatOutOfFrame)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc_seq = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_feat> peptide = unit_test_util::AddMiscFeature (nuc_seq, 6);
    peptide->SetData().SetImp().SetKey("sig_peptide");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PeptideFeatureLacksCDS", 
                      "Peptide processing feature should be converted to the appropriate protein feature subtype"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PeptideFeatOutOfFrame", 
                      "Stop of sig_peptide is out of frame with CDS codons"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    nuc_seq = entry->SetSet().SetSeq_set().front();
    peptide = unit_test_util::AddMiscFeature (nuc_seq, 5);
    peptide->SetLocation().SetInt().SetFrom(1);
    peptide->SetData().SetImp().SetKey("sig_peptide");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[1]->SetErrMsg("Start of sig_peptide is out of frame with CDS codons");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    nuc_seq = entry->SetSet().SetSeq_set().front();
    peptide = unit_test_util::AddMiscFeature (nuc_seq, 6);
    peptide->SetLocation().SetInt().SetFrom(1);
    peptide->SetData().SetImp().SetKey("sig_peptide");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[1]->SetErrMsg("Start and stop of sig_peptide are out of frame with CDS codons");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


// insertion point
BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_CDSgeneRange)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (cds);
    gene->SetLocation().SetInt().SetFrom(1);
    unit_test_util::AddFeat (gene, entry->SetSet().SetSeq_set().front());

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSgeneRange", 
                      "gene [gene locus:lcl|nuc:2-27] overlaps CDS but does not completely contain it"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MultipleMRNAproducts)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_entry> contig = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(contig);
    feat->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    feat->SetData().SetRna().SetExt().SetName("fake protein name");
    feat->SetProduct().SetWhole().SetLocal().SetStr("nuc");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FeatureProductInconsistency",
                      "mRNA products are not unique"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                       "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "TranscriptLen", 
                      "Transcript length [11] less than product length [27], and tail < 95% polyA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "MultipleMRNAproducts", 
                      "Same product Bioseq from multiple mRNA features"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_mRNAgeneRange)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("locus");
    gene->SetLocation().SetInt().SetFrom(5);
    CRef<CSeq_feat> mrna = unit_test_util::AddMiscFeature(entry);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->SetLocation().SetInt().SetTo(10);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "mRNAgeneRange",
                      "gene [locus:lcl|good:6-11] overlaps mRNA but does not completely contain it"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                       "No CDS location match for 1 mRNA"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // if there is an overlapping gene or operon, error is suppressed
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> overlap = unit_test_util::AddMiscFeature(entry);
    overlap->SetData().SetGene().SetLocus("locus2");
    overlap->SetLocation().SetInt().SetTo(10);    
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                       "No CDS location match for 1 mRNA"));
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    overlap->SetData().SetImp().SetKey("operon");
    overlap->AddQualifier ("operon", "operon name");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_TranscriptLen)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_entry> contig = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_feat> mrna = contig->SetSeq().SetAnnot().front()->SetData().SetFtable().back();
    mrna->SetLocation().SetInt().SetTo(10);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSwithNoMRNA",
                              "Unmatched CDS"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                      "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNArange",
                      "mRNA overlaps or contains CDS but does not completely contain intervals"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "TranscriptLen", 
                      "Transcript length [11] less than product length [27], and tail < 95% polyA"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // allow for polyA tail
    scope.RemoveTopLevelSeqEntry(seh);
    mrna->SetLocation().SetInt().SetTo(25);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[3]->SetErrCode ("PolyATail");
    expected_errors[3]->SetSeverity(eDiag_Info);
    expected_errors[3]->SetErrMsg ("Transcript length [26] less than product length [27], but tail is 100% polyA");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    mrna->SetLocation().SetInt().SetTo(37);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "TranscriptLen", 
                      "Transcript length [38] greater than product length [27]"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_TranscriptMismatches)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_entry> np = unit_test_util::GetNucProtSetFromGenProdSet(entry);
    CRef<CSeq_entry> mrna_seq = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(np);
    mrna_seq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAATTAA");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "TranscriptMismatches",
                      "There are 1 mismatches out of 27 bases between the transcript and product sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // suppress error if exception
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_entry> contig = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_feat> mrna = contig->SetSeq().SetAnnot().front()->SetData().SetFtable().back();
    mrna->SetExcept(true);
    mrna->SetExcept_text ("mismatches in transcription");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_CDSproductPackagingProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    entry->SetSet().SetClass (CBioseq_set::eClass_eco_set);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "CDSproductPackagingProblem",
                      "Protein product not packaged in nuc-prot set with nucleotide"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Critical, "FeaturePackagingProblem",
        "There is 1 mispackaged feature in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_DuplicateInterval)
{
    // error for duplicate in tRNA anticodon location
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front());
    CRef<CSeq_loc> anticodon_loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    anticodon_loc->SetMix().Set().front()->SetInt().SetFrom(8);
    anticodon_loc->SetMix().Set().front()->SetInt().SetTo(10);
    anticodon_loc->SetMix().Set().back()->SetInt().SetFrom(8);
    anticodon_loc->SetMix().Set().back()->SetInt().SetTo(10);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().Assign(*anticodon_loc);
    unit_test_util::AddFeat (trna, entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "Range",
                      "Anticodon is not 3 bases in length"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "DuplicateInterval",
                      "Duplicate anticodon exons in location"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // different error for feature location
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    CRef<CSeq_loc> loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    loc->SetMix().Set().back()->SetInt().SetFrom(0);
    loc->SetMix().Set().back()->SetInt().SetTo(15);
    feat->SetLocation().Assign(*loc);

    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "DuplicateInterval", 
                      "Duplicate exons in location"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_PolyAsiteNotPoint)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetData().SetImp().SetKey("polyA_site");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "PolyAsiteNotPoint", 
                      "PolyA_site should be a single point"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    // error should go away if feature location is single point
    feat->SetLocation().SetPnt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetPnt().SetPoint(5);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ImpFeatBadLoc)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetData().SetImp().SetLoc("one-of three");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ImpFeatBadLoc", 
                      "ImpFeat loc one-of three has obsolete 'one-of' text for feature misc_feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    feat->SetData().SetImp().SetLoc("5..12");
    expected_errors[0]->SetErrMsg("ImpFeat loc 5..12 does not equal feature location 1..11 for feature misc_feature");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_UnnecessaryCitPubEquiv)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    CRef<CPub> pub(new CPub());
    pub->SetPmid((CPub::TPmid)1);
    feat->SetCit().SetPub().push_back(pub);
    CRef<CPub> pub2(new CPub());
    pub2->SetEquiv();
    feat->SetCit().SetPub().push_back(pub2);
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UnnecessaryCitPubEquiv", 
                      "Citation on feature has unexpected internal Pub-equiv"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ImpCDShasTranslation)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetPseudo(true);
    feat->SetData().SetImp().SetKey("CDS");
    feat->AddQualifier("translation", "unexpected translation");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ImpCDShasTranslation", 
                      "ImpFeat CDS with /translation found"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ImpCDSnotPseudo)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetData().SetImp().SetKey("CDS");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "ImpCDSnotPseudo", 
                      "ImpFeat CDS should be pseudo"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // overlapping pseudogene should suppress
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(feat);
    gene->SetPseudo (true);
    unit_test_util::AddFeat (gene, entry);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MissingMRNAproduct)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_entry> contig = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(contig);
    feat->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    feat->SetData().SetRna().SetExt().SetName("fake protein name");
    feat->SetProduct().SetWhole().SetLocal().SetStr("not_present_ever");

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                      "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "ProductFetchFailure", 
                      "Unable to fetch mRNA transcript 'lcl|not_present_ever'"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingMRNAproduct", 
                      "Product Bioseq of mRNA feature is not packaged in the record"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GenomicProductPackagingProblem", 
                      "Product of mRNA feature (lcl|not_present_ever) not packaged in genomic product set"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_AbuttingIntervals)
{
    // error for abutting tRNA anticodon location
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front());
    CRef<CSeq_loc> anticodon_loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    anticodon_loc->SetMix().Set().front()->SetInt().SetFrom(8);
    anticodon_loc->SetMix().Set().front()->SetInt().SetTo(8);
    anticodon_loc->SetMix().Set().back()->SetInt().SetFrom(9);
    anticodon_loc->SetMix().Set().back()->SetInt().SetTo(10);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().Assign(*anticodon_loc);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('F');
    unit_test_util::AddFeat (trna, entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "AbuttingIntervals",
                      "Adjacent intervals in Anticodon"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // different error for feature location
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    CRef<CSeq_loc> loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    loc->SetMix().Set().front()->SetInt().SetFrom(0);
    loc->SetMix().Set().front()->SetInt().SetTo(7);
    loc->SetMix().Set().back()->SetInt().SetFrom(8);
    loc->SetMix().Set().back()->SetInt().SetTo(15);
    feat->SetLocation().Assign(*loc);

    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "AbuttingIntervals", 
      "Location: Adjacent intervals in SeqLoc [(lcl|good:1-8, 9-16)]"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_CollidingGeneNames)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene1 = unit_test_util::AddMiscFeature(entry);
    gene1->SetLocation().SetInt().SetFrom(0);
    gene1->SetLocation().SetInt().SetTo(7);
    gene1->SetData().SetGene().SetLocus("see_it_twice");

    CRef<CSeq_feat> gene2 = unit_test_util::AddMiscFeature(entry);
    gene2->SetLocation().SetInt().SetFrom(15);
    gene2->SetLocation().SetInt().SetTo(20);
    gene2->SetData().SetGene().SetLocus("see_it_twice");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CollidingGeneNames", 
      "Colliding names in gene features"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    gene2->SetData().SetGene().SetLocus("See_It_Twice");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Colliding names (with different capitalization) in gene features");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    gene2->SetLocation().SetInt().SetFrom(0);
    gene2->SetLocation().SetInt().SetTo(7);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "DuplicateFeat",
           "Features have identical intervals, but labels differ"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "MultiplyAnnotatedGenes", 
          "Colliding names (with different capitalization) in gene features, but feature locations are identical"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    gene2->SetLocation().SetInt().SetFrom(10);
    gene2->SetLocation().SetInt().SetTo(17);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "ReplicatedGeneSequence", 
          "Colliding names (with different capitalization) in gene features, but underlying sequences are identical"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MultiIntervalGene)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("multi-interval");   
    CRef<CSeq_loc> loc = unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front());
    gene->SetLocation().Assign (*loc);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MultiIntervalGene", 
          "Gene feature on non-segmented sequence should not have multiple intervals"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FeatContentDup)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat1 = unit_test_util::AddMiscFeature(entry);
    CRef<CSeq_feat> feat2 = unit_test_util::AddMiscFeature(entry);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FeatContentDup", 
      "Duplicate feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // many suppression conditions
    // region
    scope.RemoveTopLevelSeqEntry(seh);
    feat1->SetData().SetRegion("region");
    feat2->SetData().SetRegion("region");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
    //suppress if different dbxrefs
    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetDbxref (feat1, "ASAP", "first");
    unit_test_util::SetDbxref (feat2, "ASAP", "second");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // variation
    scope.RemoveTopLevelSeqEntry(seh);
    feat1->SetData().SetImp().SetKey("variation");
    feat2->SetData().SetImp().SetKey("variation");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FeatContentDup", 
      "Duplicate feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
    // suppress if different replace qualifiers
    scope.RemoveTopLevelSeqEntry(seh);
    feat1->AddQualifier("replace", "a");
    feat2->AddQualifier("replace", "t");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // coding regions/mRNAs with different links
    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds1 = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds2 = unit_test_util::MakeCDSForGoodNucProtSet("nuc", "prot2");
    unit_test_util::AddFeat (cds2, entry);
    CRef<CSeq_entry> pentry = unit_test_util::MakeProteinForGoodNucProtSet("prot2");
    entry->SetSet().SetSeq_set().push_back(pentry);
    CRef<CSeq_entry> nentry = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_feat> mrna1 = unit_test_util::MakeCDSForGoodNucProtSet("nuc", "prot1");
    mrna1->ResetProduct();
    mrna1->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    unit_test_util::AddFeat (mrna1, nentry);
    CRef<CSeq_feat> mrna2 = unit_test_util::MakeCDSForGoodNucProtSet("nuc", "prot1");
    mrna2->ResetProduct();
    mrna2->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    unit_test_util::AddFeat (mrna2, nentry);
    seh = scope.AddTopLevelSeqEntry(*entry);

    // two duplicate feature errors, one for cds, one for mRNA
//    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Info, "CDSwithMultipleMRNAs",
//      "CDS overlapped by 2 mRNAs, but product locations are unique"));
//    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Info, "CDSwithMultipleMRNAs",
//      "CDS overlapped by 2 mRNAs, but product locations are unique"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "FeatContentDup", 
      "Duplicate feature"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "FeatContentDup", 
      "Duplicate feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    // suppress errors if cdss and mrnas are linked
    //CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry(seh);
    cds1->SetId().SetLocal().SetId(1);
    cds2->SetId().SetLocal().SetId(2);
    mrna1->SetId().SetLocal().SetId(3);
    mrna2->SetId().SetLocal().SetId(4);
    cds1->AddSeqFeatXref(mrna1->GetId());
    cds2->AddSeqFeatXref(mrna2->GetId());
    mrna1->AddSeqFeatXref(cds1->GetId());
    mrna2->AddSeqFeatXref(cds2->GetId());
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static void ChangeGoodNucProtSetIdToGenbankName (CRef<CSeq_entry> entry, string name)
{
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc_seq = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_entry> prot_seq = unit_test_util::GetProteinSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_feat> prot_feat = unit_test_util::GetProtFeatFromGoodNucProtSet (entry);

    cds->SetProduct().SetWhole().SetGenbank().SetName(name);
    prot_seq->SetSeq().SetId().front()->SetGenbank().SetName(name);
    prot_feat->SetLocation().SetInt().SetId().SetGenbank().SetName(name);
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadProductSeqId)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    // try one that looks like a valid ID
    ChangeGoodNucProtSetIdToGenbankName(entry, "AY123456");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BadProductSeqId",
              "Feature product should not put an accession in the Textseq-id 'name' slot"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BadProductSeqId",
              "Protein bioseq has Textseq-id 'name' that looks like it is derived from a nucleotide accession"));      
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    // try one that looks like a local ID
    scope.RemoveTopLevelSeqEntry(seh);
    ChangeGoodNucProtSetIdToGenbankName(entry, "lcl|prot");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BadProductSeqId",
              "Feature product should not use Textseq-id 'name' slot"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BadProductSeqId",
              "Protein bioseq has Textseq-id 'name' and no accession"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_RnaProductMismatch)
{
    CRef<CSeq_entry> nuc = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> rna_feat = unit_test_util::AddMiscFeature (nuc);
    rna_feat->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    rna_feat->SetLocation().SetInt().SetTo(59);
    rna_feat->SetProduct().SetWhole().SetLocal().SetStr("rna");

    CRef<CSeq_entry> rna_seq = unit_test_util::BuildGoodSeq();
    rna_seq->SetSeq().SetId().front()->SetLocal().SetStr("rna");
    
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_gen_prod_set);
    entry->SetSet().SetSeq_set().push_back (nuc);
    entry->SetSet().SetSeq_set().push_back (rna_seq);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                      "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "RnaProductMismatch",
              "Type of RNA does not match MolInfo of product Bioseq"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // make error go away
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry(seh);
    rna_seq->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    unit_test_util::SetBiomol (rna_seq, CMolInfo::eBiomol_mRNA);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                      "No CDS location match for 1 mRNA"));
    CheckErrors (*eval, expected_errors);
    
   
    CLEAR_ERRORS
    // also get errors for tRNA
    scope.RemoveTopLevelSeqEntry(seh);
    rna_feat->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    rna_feat->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('N');
    rna_feat->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetId().SetLocal().SetStr("good");
    rna_feat->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(11);
    rna_feat->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(13);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "RnaProductMismatch",
              "Type of RNA does not match MolInfo of product Bioseq"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // make error go away
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetBiomol (rna_seq, CMolInfo::eBiomol_tRNA);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // also get errors for rRNA
    scope.RemoveTopLevelSeqEntry(seh);
    rna_feat->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rna_feat->SetData().SetRna().SetExt().SetName("a ribosomal RNA");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "RnaProductMismatch",
              "Type of RNA does not match MolInfo of product Bioseq"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // make error go away
    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetBiomol (rna_seq, CMolInfo::eBiomol_rRNA);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MissingCDSproduct)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    CRef<CSeq_entry> nuc = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(nuc);
    cds->SetData().SetCdregion();
    cds->SetProduct().SetWhole().SetLocal().SetStr("not_present_ever");
    entry->SetSet().SetSeq_set().push_back (nuc);
    STANDARD_SETUP

    BOOST_CHECK_EQUAL(validator::HasBadStartCodon(*cds, scope, false), true);
    BOOST_CHECK_EQUAL(validator::HasNoStop(*cds, &scope), true);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "StartCodon",
              "Illegal start codon used. Wrong genetic code [0] or protein should be partial"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "NoStop",
              "Missing stop codon"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MissingCDSproduct",
              "Unable to find product Bioseq from CDS feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    cds->ResetProduct();
    expected_errors[2]->SetErrMsg("Expected CDS product absent");
    expected_errors[2]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // ok if pseudo
    CLEAR_ERRORS
    cds->SetPseudo(true);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // also ok if exception
    cds->ResetPseudo();
    cds->SetExcept(true);
    cds->SetExcept_text("rearrangement required for product");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // also ok if CDS contains just stop codon
    scope.RemoveTopLevelSeqEntry(seh);
    cds->ResetExcept();
    cds->ResetExcept_text();
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATAAGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    cds->SetLocation().SetInt().SetTo(4);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetPartial(true);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadTrnaCodon)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildGoodtRNA(entry->SetSeq().SetId().front());
    trna->SetData().SetRna().SetExt().SetTRNA().SetCodon().push_back(64);
    unit_test_util::AddFeat (trna, entry);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadTrnaCodon", 
                  "tRNA codon value 64 is greater than maximum 63"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadTrnaAA)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildGoodtRNA(entry->SetSeq().SetId().front());
    trna->SetData().SetRna().SetExt().SetTRNA().ResetAa();
    unit_test_util::AddFeat (trna, entry);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadTrnaAA", 
                  "Missing tRNA amino acid"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa(29);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadAnticodonAA",
                  "Codons predicted from anticodon (AAA) cannot produce amino acid ( /OTHER)"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadTrnaAA", 
                  "Invalid tRNA amino acid"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_OnlyGeneXrefs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetGeneXref().SetLocus("foo");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GeneXrefWithoutGene",
                  "Feature has gene locus cross-reference but no equivalent gene feature exists"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "OnlyGeneXrefs", 
                  "There are 1 gene xrefs and no gene features in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_UTRdoesNotAbutCDS)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_entry> nseq = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_entry> pseq = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    cds->SetLocation().SetInt().SetFrom(3);
    nseq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("CCCATGAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    pseq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MRKTEIN");
    pseq->SetSeq().SetInst().SetLength(7);
    unit_test_util::AdjustProtFeatForNucProtSet (entry);

    CRef<CSeq_feat> utr5 = unit_test_util::AddMiscFeature(nseq);
    utr5->SetData().SetImp().SetKey("5'UTR");
    utr5->SetLocation().SetInt().SetTo(1);

    CRef<CSeq_feat> utr3 = unit_test_util::AddMiscFeature(nseq);
    utr3->SetData().SetImp().SetKey("3'UTR");
    utr3->SetLocation().SetInt().SetFrom(28);
    utr3->SetLocation().SetInt().SetTo(59);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc",eDiag_Warning,"UTRdoesNotAbutCDS",
                              "5'UTR does not abut CDS"));
    expected_errors.push_back(new CExpectedError("lcl|nuc",eDiag_Warning,"UTRdoesNotAbutCDS",
                              "CDS does not abut 3'UTR"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    utr5->SetLocation().SetInt().SetTo(2);
    utr5->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    utr3->SetLocation().SetInt().SetFrom(27);
    utr3->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors[0]->SetErrMsg("5'UTR is not on plus strand");
    expected_errors[1]->SetErrMsg("3'UTR is not on plus strand");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("3'UTR is not on minus strand");
    expected_errors[1]->SetErrMsg("5'UTR is not on minus strand");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS


}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ExceptionProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ExceptionProblem", "Exception explanation text is also found in feature comment"));

    feat->SetExcept(true);

    // look for exception in comment
    feat->SetExcept_text("RNA editing");
    feat->SetComment("RNA editing");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // look for one exception in comment
    feat->SetExcept_text("RNA editing, rearrangement required for product");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no citation
    feat->SetExcept_text("reasons given in citation");
    expected_errors[0]->SetErrMsg("Reasons given in citation exception does not have the required citation");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no inference
    feat->SetExcept_text("annotated by transcript or proteomic data");
    expected_errors[0]->SetErrMsg("Annotated by transcript or proteomic data exception does not have the required inference qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // not legal
    feat->SetExcept_text("not a legal exception");
    expected_errors[0]->SetErrMsg("not a legal exception is not a legal exception explanation");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // change to ref-seq
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    feat->SetLocation().SetInt().SetId().SetOther().SetAccession("NC_123456");


    // multiple ref-seq exceptions
    feat->SetExcept_text("unclassified transcription discrepancy, RNA editing");
    feat->SetComment("misc_feature needs a comment");
    expected_errors[0]->SetErrMsg("Genome processing exception should not be combined with other explanations");
    expected_errors[0]->SetAccession("ref|NC_123456|");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    // not legal (is warning for NC or NT)
    feat->SetExcept_text("not a legal exception");
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Warning, "ExceptionProblem", "not a legal exception is not a legal exception explanation"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // these are now legal for RefSeq
    feat->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    feat->SetData().SetRna().SetExt().SetName("23S ribosomal RNA");
    feat->ResetComment();
    feat->SetExcept_text("23S ribosomal RNA and 5S ribosomal RNA overlap");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetExcept_text("5S ribosomal RNA and 16S ribosomal RNA overlap");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetExcept_text("5S ribosomal RNA and 23S ribosomal RNA overlap");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    feat->SetExcept_text("23S ribosomal RNA and 16S ribosomal RNA overlap");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_SeqDataLenWrong)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    // need to call this statement before calling AddDefaults 
    // to make sure that we can fetch the sequence referenced by the
    // delta sequence so that we can detect that the loc in the
    // delta sequence is longer than the referenced sequence
    CGBDataLoader::RegisterInObjectManager(*objmgr);
    CScope scope(*objmgr);
    scope.AddDefaults();

    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
                          | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
                          | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    // validate - should be fine
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // longer and shorter for iupacna
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "SeqDataLenWrong", "Bioseq.seq_data too short [60] for given length [65]"));
    entry->SetSeq().SetInst().SetLength(65);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetLength(55);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [60] than given length [55]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try other divisors
    entry->SetSeq().SetInst().SetLength(60);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('A');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('T');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('G');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('C');
    CRef<CSeq_data> packed_data(new CSeq_data);
    // convert seq data to another format
    // (NCBI2na = 2 bit nucleic acid code)
    CSeqportUtil::Convert(entry->SetSeq().SetInst().GetSeq_data(),
                          packed_data,
                          CSeq_data::e_Ncbi2na);
    entry->SetSeq().SetInst().SetSeq_data(*packed_data);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbi2na().Set().pop_back();
    entry->SetSeq().SetInst().SetSeq_data().SetNcbi2na().Set().pop_back();
    expected_errors[0]->SetErrMsg("Bioseq.seq_data too short [56] for given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CSeqportUtil::Convert(entry->SetSeq().SetInst().GetSeq_data(),
                          packed_data,
                          CSeq_data::e_Ncbi4na);
    entry->SetSeq().SetInst().SetSeq_data(*packed_data);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na().Set().push_back('1');
    entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na().Set().push_back('8');
    entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na().Set().push_back('1');
    entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na().Set().push_back('8');
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CRef<CSeq_id> id(new CSeq_id("gb|AY123456"));
#if 0
    // removed per VR-779
    // now try seg and ref
    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_seg);
    CRef<CSeq_loc> loc(new CSeq_loc(*id, 0, 55));
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data too short [56] for given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
  
    loc->SetInt().SetTo(63);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_ref);
    entry->SetSeq().SetInst().SetExt().SetRef().SetInt().SetId(*id);
    entry->SetSeq().SetInst().SetExt().SetRef().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetRef().SetInt().SetTo(55);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data too short [56] for given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().SetRef().SetInt().SetTo(63);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
#endif
    
    CLEAR_ERRORS
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "SeqDataLenWrong",
        "Bioseq.seq_data too short [56] for given length [60]"));
    // delta sequence
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*id, 0, 55);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    entry->SetSeq().SetInst().SetExt().Reset();
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*id, 0, 30);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*id, 40, 72);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().Reset();
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*id, 0, 59);
    CRef<CDelta_seq> delta_seq;
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(delta_seq);
    expected_errors[0]->SetErrMsg("NULL pointer in delta seq_ext valnode (segment 2)");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().Reset();
    CRef<CDelta_seq> delta_seq2(new CDelta_seq());
    delta_seq2->SetLoc().SetInt().SetId(*id);
    delta_seq2->SetLoc().SetInt().SetFrom(0);
    delta_seq2->SetLoc().SetInt().SetTo(485);
    entry->SetSeq().SetInst().SetLength(486);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(delta_seq2);
    expected_errors[0]->SetErrMsg("Seq-loc extent (486) greater than length of gb|AY123456| (485)");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadConflictFlag)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds_feat = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds_feat->SetData().SetCdregion().SetConflict(true);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "BadConflictFlag",
                              "Coding region conflict flag should not be set"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ConflictFlagSet)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds_feat = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds_feat->SetData().SetCdregion().SetConflict(true);
    CRef<CSeq_entry> prot = entry->SetSet().SetSeq_set().back();
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEIXX");
    prot->SetSeq().SetInst().SetLength(9);
    CRef<CSeq_feat> prot_feat = prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    prot_feat->SetLocation().SetInt().SetTo(8);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "ConflictFlagSet",
                              "Coding region conflict flag is set"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_LocusTagProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat>gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus_tag("a b c");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "LocusTagHasSpace",
                              "Gene locus_tag 'a b c' should be a single word without any spaces"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gene->AddQualifier("old_locus_tag", "a b c");
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "RedundantFields",
                       "old_locus_tag has same value as gene locus_tag"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "LocusTagProblem",
                       "Gene locus_tag and old_locus_tag 'a b c' match"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    gene->ResetQual();
    gene->SetData().SetGene().SetLocus_tag("abc");
    gene->SetData().SetGene().SetLocus("abc");
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "LocusTagGeneLocusMatch",
                       "Gene locus and locus_tag 'abc' match"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    gene->SetData().SetGene().ResetLocus();
    gene->AddQualifier ("old_locus_tag", "a, b, c");
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "OldLocusTagBadFormat",
                       "old_locus_tag has comma, multiple old_locus_tags should be split into separate qualifiers"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_AltStartCodon)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nseq = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetExcept(true);
    cds->SetExcept_text("alternative start codon");
    
    STANDARD_SETUP

    // first, no errors because not refseq
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // report error if refseq
    scope.RemoveTopLevelSeqEntry(seh);
    nseq->SetSeq().SetId().front()->SetOther().SetAccession("NM_123456");
    cds->SetLocation().SetInt().SetId().SetOther().SetAccession("NM_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("ref|NM_123456|", eDiag_Warning, "AltStartCodon",
                              "Unnecessary alternative start codon exception"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_GenesInconsistent)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_entry> np = unit_test_util::GetNucProtSetFromGenProdSet(entry);
    CRef<CSeq_entry> mrna_seq = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(np);
    CRef<CSeq_feat> mgene = unit_test_util::AddMiscFeature(mrna_seq);
    mgene->SetLocation().SetInt().SetTo(26);
    mgene->SetData().SetGene().SetLocus("locus1");

    CRef<CSeq_entry> g_seq = unit_test_util::GetGenomicFromGenProdSet(entry);
    CRef<CSeq_feat> cgene = unit_test_util::AddMiscFeature (g_seq);
    cgene->SetLocation().SetInt().SetTo(26);
    cgene->SetData().SetGene().SetLocus("locus2");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "GenesInconsistent",
                              "Gene on mRNA bioseq does not match gene on genomic bioseq"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_DuplicateTranslExcept)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CCode_break> codebreak1(new CCode_break());
    codebreak1->SetLoc().SetInt().SetId().SetLocal().SetStr("nuc");
    codebreak1->SetLoc().SetInt().SetFrom(24);
    codebreak1->SetLoc().SetInt().SetTo(26);
    cds->SetData().SetCdregion().SetCode_break().push_back(codebreak1);
    CRef<CCode_break> codebreak2(new CCode_break());
    codebreak2->SetLoc().SetInt().SetId().SetLocal().SetStr("nuc");
    codebreak2->SetLoc().SetInt().SetFrom(24);
    codebreak2->SetLoc().SetInt().SetTo(26);
    cds->SetData().SetCdregion().SetCode_break().push_back(codebreak2);

    STANDARD_SETUP

    
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "DuplicateTranslExcept",
                               "Multiple code-breaks at same location [lcl|nuc:25-27]"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_TranslExceptAndRnaEditing)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CCode_break> codebreak1(new CCode_break());
    codebreak1->SetLoc().SetInt().SetId().SetLocal().SetStr("nuc");
    codebreak1->SetLoc().SetInt().SetFrom(24);
    codebreak1->SetLoc().SetInt().SetTo(26);
    cds->SetData().SetCdregion().SetCode_break().push_back(codebreak1);
    cds->SetExcept(true);
    cds->SetExcept_text("RNA editing");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "TranslExceptAndRnaEditing",
                               "CDS has both RNA editing /exception and /transl_except qualifiers"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "UnnecessaryException",
                               "CDS has exception but passes translation test"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    BOOST_CHECK_EQUAL(validator::DoesFeatureHaveUnnecessaryException(*cds, scope), true);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_NoNameForProtein)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> prot_feat = unit_test_util::GetProtFeatFromGoodNucProtSet (entry);
    prot_feat->SetData().SetProt().ResetName();
    prot_feat->SetData().SetProt().SetDesc("protein description");
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "NoNameForProtein",
                               "Protein feature has description but no name"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    prot_feat->SetData().SetProt().ResetDesc();
    prot_feat->SetData().SetProt().SetActivity().push_back ("activity");
    expected_errors[0]->SetErrMsg("Protein feature has function but no name");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    prot_feat->SetData().SetProt().ResetActivity();
    prot_feat->SetData().SetProt().SetEc().push_back("1.2.3.4");
    expected_errors[0]->SetErrMsg("Protein feature has EC number but no name");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "ProtRefHasNoData",
                               "There is a protein feature where all fields are empty"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "NoNameForProtein",
                               "Protein feature has no name"));

    prot_feat->SetData().SetProt().ResetEc();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_CDSmRNAmismatch)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nseq = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds1 = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (cds1);
    gene->SetLocation().SetInt().SetTo(40);
    unit_test_util::AddFeat(gene, nseq);
    CRef<CSeq_feat> mrna1 = unit_test_util::MakemRNAForCDS (cds1);
    mrna1->SetData().SetRna().SetExt().SetName("product 1");
    unit_test_util::AddFeat (mrna1, nseq);

    CRef<CSeq_feat>mrna2 = unit_test_util::MakemRNAForCDS (cds1);
    mrna2->SetData().SetRna().SetExt().SetName("product 2");
    mrna2->SetLocation().SetInt().SetTo(40);
    unit_test_util::AddFeat (mrna2, nseq);
    
    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSmRNAmismatchCount",
                               "mRNA count (2) does not match CDS (1) count for gene"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSwithMultipleMRNAs",
                              "CDS matches 2 mRNAs"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSmRNAMismatchLocation",
                              "No CDS location match for 1 mRNA"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_UnnecessaryException)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGenProdSet(entry);
    CRef<CSeq_feat> mrna = unit_test_util::GetmRNAFromGenProdSet (entry);
    cds->SetExcept(true);
    cds->SetExcept_text("RNA editing");
    mrna->SetExcept(true);
    mrna->SetExcept_text("transcribed product replaced");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UnnecessaryException",
                               "CDS has exception but passes translation test"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "UnnecessaryException",
                               "mRNA has exception but passes transcription test"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    BOOST_CHECK_EQUAL(DoesFeatureHaveUnnecessaryException(*cds, scope), true);
    BOOST_CHECK_EQUAL(DoesFeatureHaveUnnecessaryException(*mrna, scope), true);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc_seq = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    unit_test_util::SetSpliceForMixLoc (nuc_seq->SetSeq());
    cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(nuc_seq->SetSeq().SetId().front()));
    mrna = unit_test_util::MakemRNAForCDS (cds);
    unit_test_util::AddFeat (mrna, nuc_seq);
    CRef<CSeq_feat> exon = unit_test_util::AddMiscFeature(nuc_seq);
    exon->SetData().SetImp().SetKey("exon");
    exon->SetLocation().Assign(*(cds->SetLocation().SetMix().Set().front()));
    cds->SetExcept(true);
    cds->SetExcept_text("artificial frameshift");
    mrna->SetExcept(true);
    mrna->SetExcept_text("artificial frameshift");
    exon->SetExcept(true);
    exon->SetExcept_text("artificial frameshift");
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "UnnecessaryException",
                               "feature has exception but passes splice site test"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "UnnecessaryException",
                               "feature has exception but passes splice site test"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "UnnecessaryException",
                               "feature has exception but passes splice site test"));
 
    options |= CValidator::eVal_val_exons;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    BOOST_CHECK_EQUAL(DoesFeatureHaveUnnecessaryException(*cds, scope), true);
    BOOST_CHECK_EQUAL(DoesFeatureHaveUnnecessaryException(*mrna, scope), true);
    BOOST_CHECK_EQUAL(DoesFeatureHaveUnnecessaryException(*exon, scope), true);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_LocusTagProductMismatch)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_entry> prot = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_id> id(new CSeq_id());
    id->SetGeneral().SetDb("a");
    id->SetGeneral().SetTag().SetStr("good");
    unit_test_util::ChangeNucProtSetProteinId(entry, id);
    CRef<CSeq_id> lcl_id(new CSeq_id());
    lcl_id->SetLocal().SetStr("x");
    prot->SetSeq().SetId().push_back(lcl_id);

    CRef<CSeq_id> ref_id = unit_test_util::BuildRefSeqId();
    unit_test_util::ChangeNucProtSetNucId (entry, ref_id);

    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(cds);
    gene->SetData().SetGene().SetLocus_tag("something");
    unit_test_util::AddFeat(gene, nuc);

    STANDARD_SETUP

    options |= CValidator::eVal_locus_tag_general_match;
    expected_errors.push_back(new CExpectedError("ref|NC_123456|", eDiag_Error, "LocusTagProductMismatch",
                               "Gene locus_tag does not match general ID of product"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_PseudoCdsViaGeneHasProduct)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(cds);
    gene->SetPseudo(true);
    unit_test_util::AddFeat(gene, nuc);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "PseudoCdsViaGeneHasProduct",
                               "A coding region overlapped by a pseudogene should not have a product"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MissingGeneXref)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetLocation().SetInt().SetFrom (5);

    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature(misc);
    gene1->SetData().SetGene().SetLocus("first");
    gene1->SetLocation().SetInt().SetFrom (0);
    unit_test_util::AddFeat(gene1, entry);
    CRef<CSeq_feat> gene2 = unit_test_util::MakeGeneForFeature(misc);
    gene2->SetData().SetGene().SetLocus("second");
    gene2->SetLocation().SetInt().SetTo(misc->GetLocation().GetInt().GetTo() + 5);
    unit_test_util::AddFeat(gene2, entry);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "MissingGeneXref",
                               "Feature overlapped by 2 identical-length genes but has no cross-reference"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FeatureCitationProblem) 
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    CRef<CPub> pub(new CPub());
    pub->SetPmid((CPub::TPmid)2);
    misc->SetCit().SetPub().push_back(pub);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "FeatureCitationProblem",
                               "Citation on feature refers to uid [2] not on a publication in the record"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_NestedSeqLocMix)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetId().SetLocal().SetStr("good");
    loc1->SetInt().SetFrom(0);
    loc1->SetInt().SetTo(10);
    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetInt().SetId().SetLocal().SetStr("good");
    loc2->SetInt().SetFrom(20);
    loc2->SetInt().SetTo(30);
    CRef<CSeq_loc> loc3(new CSeq_loc());
    loc3->SetInt().SetId().SetLocal().SetStr("good");
    loc3->SetInt().SetFrom(40);
    loc3->SetInt().SetTo(50);
    CRef<CSeq_loc> loc4(new CSeq_loc());
    loc4->SetMix().Set().push_back (loc2);
    loc4->SetMix().Set().push_back(loc3);
    
    misc->SetLocation().SetMix().Set().push_back (loc1);
    misc->SetLocation().SetMix().Set().push_back (loc4);
    misc->SetProduct().Assign (misc->SetLocation());

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "NestedSeqLocMix",
    "Location: SeqLoc [[lcl|good:1-11, [21-31, 41-51]]] has nested SEQLOC_MIX elements"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "NestedSeqLocMix",
                               "Product: SeqLoc [[lcl|good:1-11, [21-31, 41-51]]] has nested SEQLOC_MIX elements"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "SelfReferentialProduct",
                               "Self-referential feature product"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "ProductShouldBeWhole",
                               "Feature products should be entire sequences."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_CodonQualifierUsed)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->AddQualifier("codon", "1");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "CodonQualifierUsed",
                               "Use the proper genetic code, if available, or set transl_excepts on specific codons"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadCharInAuthorName)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeqdesc> desc(new CSeqdesc());
    CRef<CPub> pub = unit_test_util::BuildGoodArticlePub();
    CRef<CAuthor> auth = unit_test_util::BuildGoodAuthor();
    auth->SetName().SetName().SetFirst("F1rst");
    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(auth);
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetSeq().SetDescr().Set().push_back (desc);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadCharInAuthorName",
                               "Bad characters in author F1rst"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_PolyATail)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_entry> contig = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_feat> mrna = contig->SetSeq().SetAnnot().front()->SetData().SetFtable().back();
    mrna->SetLocation().SetInt().SetTo(25);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSwithNoMRNA",
                      "Unmatched CDS"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                      "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNArange",
                      "mRNA overlaps or contains CDS but does not completely contain intervals"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "PolyATail", 
                      "Transcript length [26] less than product length [27], but tail is 100% polyA"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_entry> np = unit_test_util::GetNucProtSetFromGenProdSet(entry);
    CRef<CSeq_entry> transcript = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(np);
    transcript->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAAAAAAAAAAAAAAAAAATAA");
    transcript->SetSeq().SetInst().SetLength(46);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[3]->SetErrMsg("Transcript length [26] less than product length [46], but tail >= 95% polyA");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_CDSwithMultipleMRNAs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_entry> genomic = unit_test_util::GetGenomicFromGenProdSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGenProdSet(entry);
    CRef<CSeq_feat> second_mrna = unit_test_util::MakemRNAForCDS(cds);
    second_mrna->SetProduct().SetWhole().SetLocal().SetStr("nuc");
    unit_test_util::AddFeat (second_mrna, genomic);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FeatureProductInconsistency",
                              "mRNA products are not unique"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSwithMultipleMRNAs",
                              "CDS matches 2 mRNAs"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                              "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FeatContentDup",
                              "Duplicate feature"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "MultipleMRNAproducts",
                              "Same product Bioseq from multiple mRNA features"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // now try with unique products
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> nuc_id(new CSeq_id());
    nuc_id->SetLocal().SetStr("nuc2");
    CRef<CSeq_id> prot_id(new CSeq_id());
    prot_id->SetLocal().SetStr("prot2");
    CRef<CSeq_entry> np = unit_test_util::BuildGenProdSetNucProtSet (nuc_id, prot_id);
    entry->SetSet().SetSeq_set().push_back (np);
    second_mrna->SetProduct().SetWhole().Assign(*nuc_id);
    seh = scope.AddTopLevelSeqEntry(*entry);

    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "CDSwithMultipleMRNAs",
                              "CDS matches 2 mRNAs, but product locations are unique"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                              "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FeatContentDup", "Duplicate feature"));
    expected_errors.push_back(new CExpectedError("lcl|prot2", eDiag_Warning, "GenomicProductPackagingProblem",
                              "Protein bioseq should be product of CDS feature on contig, but is not"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MultipleEquivBioSources)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> src1 = unit_test_util::AddMiscFeature (entry);
    src1->SetData().SetBiosrc().SetOrg().SetTaxname("Homo sapiens");
    src1->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("some lineage");
    CRef<CSeq_feat> src2 = unit_test_util::AddMiscFeature (entry);
    src2->SetData().SetBiosrc().SetOrg().SetTaxname("Homo sapiens");
    src2->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("some lineage");
    src2->SetLocation().SetInt().SetFrom(30);
    src2->SetLocation().SetInt().SetTo(40);
    unit_test_util::SetTransgenic(entry, true);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "MultipleEquivBioSources",
                               "Multiple equivalent source features should be combined into one multi-interval feature"));

    options |= CValidator::eVal_seqsubmit_parent;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    src1->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("Viruses");
    src2->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("Viruses");

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MultipleEquivPublications)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat1 = unit_test_util::AddMiscFeature (entry);
    CRef<CPub> pub1(new CPub());
    pub1->SetPmid((CPub::TPmid)2);
    feat1->SetData().SetPub().SetPub().Set().push_back(pub1);
    CRef<CSeq_feat> feat2 = unit_test_util::AddMiscFeature (entry);
    CRef<CPub> pub2(new CPub());
    pub2->SetPmid((CPub::TPmid)2);
    feat2->SetData().SetPub().SetPub().Set().push_back(pub2);
    feat2->SetLocation().SetInt().SetFrom(30);
    feat2->SetLocation().SetInt().SetTo(40);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "MultipleEquivPublications",
                               "Multiple equivalent publication features should be combined into one multi-interval feature"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadFullLengthFeature)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> src1 = unit_test_util::AddMiscFeature (entry);
    src1->SetData().SetBiosrc().SetOrg().SetTaxname("Homo sapiens");
    src1->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("some lineage");
    src1->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength() - 1);
    CRef<CSeq_feat> feat1 = unit_test_util::AddMiscFeature (entry);
    CRef<CPub> pub1(new CPub());
    pub1->SetPmid((CPub::TPmid)2);
    feat1->SetData().SetPub().SetPub().Set().push_back(pub1);
    feat1->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength() - 1);
    unit_test_util::SetTransgenic(entry, true);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadFullLengthFeature",
                               "Source feature is full length, should be descriptor"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadFullLengthFeature",
                               "Publication feature is full length, should be descriptor"));
    options |= CValidator::eVal_seqsubmit_parent;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> src2 = unit_test_util::AddMiscFeature (entry);
    src2->SetData().SetBiosrc().SetOrg().SetTaxname("Drosophila melanogaster");
    src2->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("some lineage");
    src2->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength() - 1);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "DuplicateFeat",
                               "Features have identical intervals, but labels differ"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadFullLengthFeature",
                               "Source feature is full length, should be descriptor"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadFullLengthFeature",
                               "Multiple full-length source features, should only be one if descriptor is transgenic"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadFullLengthFeature",
                               "Publication feature is full length, should be descriptor"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_RedundantFields)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (cds);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    unit_test_util::AddFeat (gene, nuc);
    gene->SetData().SetGene().SetLocus ("redundant_g");
    gene->SetComment ("redundant_g");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "RedundantFields",
                               "Comment has same value as gene locus"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
 
    CLEAR_ERRORS
    gene->SetData().SetGene().ResetLocus();
    gene->SetData().SetGene().SetLocus_tag("redundant_g");
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "RedundantFields",
                               "Comment has same value as gene locus_tag"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
 
    CLEAR_ERRORS

    gene->ResetComment();
    gene->AddQualifier("old_locus_tag", "redundant_g");
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "RedundantFields",
                               "old_locus_tag has same value as gene locus_tag"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "LocusTagProblem",
                               "Gene locus_tag and old_locus_tag 'redundant_g' match"));
  
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
 
    CLEAR_ERRORS

    gene->ResetQual();

    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().SetName().front().assign("redundant_p");
    prot->SetComment("redundant_p");
    prot->SetData().SetProt().SetDesc("redundant_p");

    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "RedundantFields",
                               "Comment has same value as protein name"));
    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "RedundantFields",
                               "Comment has same value as protein description"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


static void AddCDSAndProtForBigGoodNucProtSet (CRef<CSeq_entry> entry, string nuc_id, string prot_id, TSeqPos offset)
{
    CRef<CSeq_feat> cds (new CSeq_feat());
    cds->SetData().SetCdregion();
    cds->SetProduct().SetWhole().SetLocal().SetStr(prot_id);
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr(nuc_id);
    cds->SetLocation().SetInt().SetFrom(offset + 0);
    cds->SetLocation().SetInt().SetTo(offset + 26);
    unit_test_util::AddFeat (cds, entry);

    CRef<CSeq_entry> pentry = unit_test_util::MakeProteinForGoodNucProtSet(prot_id);

    entry->SetSet().SetSeq_set().push_back(pentry);

}


static CRef<CSeq_entry> BuildBigGoodNucProtSet(void)
{
    CRef<CBioseq_set> set(new CBioseq_set());
    set->SetClass(CBioseq_set::eClass_nuc_prot);

    // make nucleotide
    CRef<CBioseq> nseq(new CBioseq());
    nseq->SetInst().SetMol(CSeq_inst::eMol_dna);
    nseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    nseq->SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    nseq->SetInst().SetLength(360);

    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr ("nuc");
    nseq->SetId().push_back(id);

    CRef<CSeqdesc> mdesc(new CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);    
    nseq->SetDescr().Set().push_back(mdesc);

    CRef<CSeq_entry> nentry(new CSeq_entry());
    nentry->SetSeq(*nseq);

    set->SetSeq_set().push_back(nentry);

    CRef<CSeq_entry> set_entry(new CSeq_entry());
    set_entry->SetSet(*set);

    int i = 1;
    for (TSeqPos offset = 0; offset < nseq->GetInst().GetLength() - 26; offset += 30, i++) { 
        string prot_id = "prot" + NStr::IntToString(i);
        AddCDSAndProtForBigGoodNucProtSet (set_entry, "nuc", prot_id, offset);
    }

    unit_test_util::AddGoodSource (set_entry);
    unit_test_util::AddGoodPub(set_entry);
    return set_entry;
}


BOOST_AUTO_TEST_CASE (Test_SEQ_FEAT_CDSwithNoMRNA)
{
    CRef<CSeq_entry> entry = BuildBigGoodNucProtSet();
    // make mRNA for first CDS
    CRef<CSeq_feat> first_cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    CSeq_annot::TData::TFtable::iterator cds_it = entry->SetSet().SetAnnot().front()->SetData().SetFtable().begin();

    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS (*cds_it);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    unit_test_util::AddFeat (mrna, nuc);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (*cds_it);
    unit_test_util::AddFeat (gene, nuc);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSwithNoMRNA",
                "11 out of 12 CDSs unmatched"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS    
    scope.RemoveTopLevelSeqEntry(seh);
    for (int i = 0; i < 3; i++) {
        ++cds_it;
        CRef<CSeq_feat> new_mrna = unit_test_util::MakemRNAForCDS (*cds_it);
        unit_test_util::AddFeat (new_mrna, nuc);
    }
    seh = scope.AddTopLevelSeqEntry(*entry);
    for (int i = 0; i < 8; i++) {
        expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSwithNoMRNA",
                "Unmatched CDS"));
    }

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FeatureProductInconsistency)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    unit_test_util::AddFeat (mrna, nuc);
    CRef<CSeq_feat> bad_cds = unit_test_util::AddMiscFeature(nuc);
    bad_cds->SetData().SetCdregion();
    bad_cds->SetLocation().SetInt().SetFrom(30);
    bad_cds->SetLocation().SetInt().SetTo(56);
    CRef<CSeq_feat> bad_mrna = unit_test_util::MakemRNAForCDS(bad_cds);
    unit_test_util::AddFeat (bad_mrna, nuc);

    STANDARD_SETUP_NO_DATABASE

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "FeatureProductInconsistency",
                                "2 CDS features have 1 product references"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "MissingCDSproduct", "Expected CDS product absent"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "NoProtein", "No protein Bioseq given"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    

    scope.RemoveTopLevelSeqEntry(seh);
    bad_cds->SetProduct().SetWhole().SetLocal().SetStr("prot");
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "FeatureProductInconsistency",
                                "CDS products are not unique"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Critical, "MultipleCDSproducts",
                                "Same product Bioseq from multiple CDS features"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    

    scope.RemoveTopLevelSeqEntry(seh);
    nuc->SetSeq().ResetAnnot();
    AddCDSAndProtForBigGoodNucProtSet (entry, "nuc", "prot1", 30);
    bad_mrna = unit_test_util::MakemRNAForCDS(entry->SetSet().SetAnnot().front()->SetData().SetFtable().back());
    unit_test_util::AddFeat (bad_mrna, nuc);
    mrna = unit_test_util::MakemRNAForCDS (cds);
    mrna->SetProduct().SetWhole().SetGenbank().SetAccession("AY123456");
    unit_test_util::AddFeat (mrna, nuc);
    
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "FeatureProductInconsistency",
                                "2 mRNA features have 1 product references"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "ProductFetchFailure", 
                                "Unable to fetch mRNA transcript 'gb|AY123456|'"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    

    scope.RemoveTopLevelSeqEntry(seh);
    bad_mrna->SetProduct().SetWhole().SetGenbank().SetAccession("AY123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "FeatureProductInconsistency",
                                "mRNA products are not unique"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "ProductFetchFailure", 
                                "Unable to fetch mRNA transcript 'gb|AY123456|'"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "ProductFetchFailure", 
                                "Unable to fetch mRNA transcript 'gb|AY123456|'"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


static void SetFeatureLocationBond (CRef<CSeq_feat> feat, string id, TSeqPos pt1, TSeqPos pt2)
{
    feat->SetLocation().SetBond().SetA().SetId().SetLocal().SetStr(id);
    feat->SetLocation().SetBond().SetA().SetPoint(0);
    feat->SetLocation().SetBond().SetB().SetId().SetLocal().SetStr(id);
    feat->SetLocation().SetBond().SetB().SetPoint(5);
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ImproperBondLocation)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> f1 = unit_test_util::AddMiscFeature(entry);
    SetFeatureLocationBond(f1, "good", 0, 5);

    CRef<CSeq_feat> f2 = unit_test_util::AddMiscFeature(entry);
    f2->SetData().SetHet();
    SetFeatureLocationBond(f2, "good", 0, 5);

    CRef<CSeq_feat> f3 = unit_test_util::AddMiscFeature(entry);
    f3->SetData().SetCdregion();
    f3->SetPseudo(true);
    SetFeatureLocationBond(f3, "good", 0, 5);

    CRef<CSeq_feat> f4 = unit_test_util::AddMiscFeature(entry);
    f4->SetData().SetBond();
    SetFeatureLocationBond(f4, "good", 0, 5);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "ImproperBondLocation",
                                "Bond location should only be on bond features"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "ImproperBondLocation",
                                "Bond location should only be on bond features"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "ImproperBondLocation",
                                "Bond location should only be on bond features"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_GeneXrefWithoutGene)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature (entry);
    feat->SetGeneXref().SetLocus("missing");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "GeneXrefWithoutGene",
                                "Feature has gene locus cross-reference but no equivalent gene feature exists"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "OnlyGeneXrefs",
                                "There are 1 gene xrefs and no gene features in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS    

    feat->SetGeneXref().ResetLocus();
    feat->SetGeneXref().SetLocus_tag("missing");

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "GeneXrefWithoutGene",
                                "Feature has gene locus_tag cross-reference but no equivalent gene feature exists"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "OnlyGeneXrefs",
                                "There are 1 gene xrefs and no gene features in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS    
}


void CreateReciprocalLinks(CSeq_feat& f1, CSeq_feat& f2)
{
    f1.AddSeqFeatXref(f2.GetId());
    f2.AddSeqFeatXref(f1.GetId());
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_SeqFeatXrefProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    // add ID to CDS
    cds->SetId().SetLocal().SetId(1);
    
    // create mRNA feature
    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetId().SetLocal().SetId(2);
    unit_test_util::AddFeat (mrna, nuc);

    // create gene feature
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(cds);
    gene->SetId().SetLocal().SetId(3);
    unit_test_util::AddFeat (gene, nuc);

    // add misc_feature
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(nuc);
    misc->SetId().SetLocal().SetId(4);
        
    STANDARD_SETUP

    // add broken SeqFeatXref to coding region
    CRef<CSeqFeatXref> x1(new CSeqFeatXref());
    cds->SetXref().push_back(x1);

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefProblem",
                                "SeqFeatXref with no id or data field"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    cds->ResetXref();

    CLEAR_ERRORS    

    // xref between CDS and misc_feat is not allowed,
    // triggers error for non-ambiguous CDS/mRNA
    scope.RemoveTopLevelSeqEntry(seh);
    CreateReciprocalLinks(*cds, *misc);
    seh = scope.AddTopLevelSeqEntry(*entry);

//    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefNotReciprocal",
//                                "CDS/mRNA unambiguous pair have erroneous cross-references"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefProblem",
                                "Cross-references are not between CDS and mRNA pair or between a gene and a CDS or mRNA (misc_feature,CDS)"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefProblem",
                                "Cross-references are not between CDS and mRNA pair or between a gene and a CDS or mRNA (CDS,misc_feature)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
    
    // complain if linked-to feature has no xrefs of its own
    scope.RemoveTopLevelSeqEntry(seh);
    misc->ResetXref();
    seh = scope.AddTopLevelSeqEntry(*entry);
//    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefNotReciprocal",
//                                "CDS/mRNA unambiguous pair have erroneous cross-references"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefProblem",
                                "Cross-referenced feature does not have its own cross-reference"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    cds->ResetXref();
    
    CLEAR_ERRORS    

    // create xref between mRNA and coding region - this is allowed
    scope.RemoveTopLevelSeqEntry(seh);
    CreateReciprocalLinks(*cds, *mrna);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // create xref between coding region and gene - this is allowed
    scope.RemoveTopLevelSeqEntry(seh);
    CreateReciprocalLinks(*cds, *gene);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // create xref between mRNA and gene - this is allowed
    scope.RemoveTopLevelSeqEntry(seh);
    CreateReciprocalLinks(*mrna, *gene);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // shouldn't matter what order the links are created in
    scope.RemoveTopLevelSeqEntry(seh);
    mrna->ResetXref();
    cds->ResetXref();
    gene->ResetXref();
    CreateReciprocalLinks(*cds, *gene);
    CreateReciprocalLinks(*mrna, *gene);
    CreateReciprocalLinks(*cds, *mrna);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // if feature has gene xref AND a feature ID xref to a gene feature, 
    // they should not conflict
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> other_gene = unit_test_util::AddMiscFeature(nuc);
    other_gene->SetData().SetGene().SetLocus("mismatch");
    // note that gene and other_gene cannot have the same location or will 
    // trigger duplicate feature errors, gene xref gene should not be the
    // gene mapped to by overlap
    other_gene->SetLocation().Assign(gene->GetLocation());
    other_gene->SetLocation().SetInt().SetTo(other_gene->GetLocation().GetInt().GetTo() + 1);
    seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CSeqFeatXref> gene_xref(new CSeqFeatXref());
    gene_xref->SetData().SetGene().SetLocus("mismatch");
    cds->SetXref().push_back(gene_xref);

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefProblem",
                                "Feature gene xref does not match Feature ID cross-referenced gene feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // ignore if gene xref and linked gene feature match
    scope.RemoveTopLevelSeqEntry(seh);
    gene_xref->SetData().SetGene().SetLocus("gene locus");
    other_gene->SetLocation().Assign(gene->GetLocation());
    gene->SetLocation().SetInt().SetTo(gene->GetLocation().GetInt().GetTo() + 1);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE (Test_SEQ_FEAT_MissingTrnaAA)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature (entry);
    feat->SetData().SetRna().SetType (CRNA_ref::eType_tRNA);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "MissingTrnaAA",
                                "Missing encoded amino acid qualifier in tRNA"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE (Test_SEQ_FEAT_CollidingFeatureIDs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature (entry);
    feat->SetId().SetLocal().SetId(1);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (feat);
    gene->SetId().SetLocal().SetId(1);
    unit_test_util::AddFeat (gene, entry);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Critical, "CollidingFeatureIDs",
                                "Colliding feature ID 1"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Critical, "CollidingFeatureIDs",
                                "Colliding feature ID 1"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE (Test_SEQ_FEAT_PolyAsignalNotRange)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature (entry);
    feat->SetData().SetImp().SetKey("polyA_signal");
    feat->SetLocation().SetInt().SetTo(0);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "PolyAsignalNotRange",
                                "PolyA_signal should be a range"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE (Test_SEQ_FEAT_OldLocusTagMismtach)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->AddQualifier("old_locus_tag", "one value");

    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (feat);
    gene->AddQualifier ("old_locus_tag", "another value");
    unit_test_util::AddFeat (gene, entry);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "OldLocusTagMismtach",
                                "Old locus tag on feature (one value) does not match that on gene (another value)"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "OldLocusTagWithoutLocusTag",
                                "old_locus_tag without inherited locus_tag"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


static CRef<CUser_field> MakeGoTerm (string text = "something", string evidence = "some evidence")
{
    CRef<CUser_field> go_term (new CUser_field());
    go_term->SetLabel().SetStr("a go term");

    CRef<CUser_field> go_id(new CUser_field());
    go_id->SetLabel().SetStr("go id");
    go_id->SetData().SetStr("123");
    go_term->SetData().SetFields().push_back (go_id);

    CRef<CUser_field> pmid(new CUser_field());
    pmid->SetLabel().SetStr("pubmed id");
    pmid->SetData().SetInt(4);
    go_term->SetData().SetFields().push_back (pmid);

    CRef<CUser_field> term(new CUser_field());
    term->SetLabel().SetStr("text string");
    term->SetData().SetStr(text);
    go_term->SetData().SetFields().push_back (term);

    CRef<CUser_field> ev(new CUser_field());
    ev->SetLabel().SetStr("evidence");
    ev->SetData().SetStr(evidence);
    go_term->SetData().SetFields().push_back (ev);

    return go_term;
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_DuplicateGeneOntologyTerm)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetExt().SetType().SetStr("GeneOntology");
    CRef<CUser_field> go_list(new CUser_field());
    go_list->SetLabel().SetStr("Process");
    go_list->SetData().SetFields().push_back(MakeGoTerm());
    go_list->SetData().SetFields().push_back(MakeGoTerm());
    feat->SetExt().SetData().push_back(go_list);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Info, "DuplicateGeneOntologyTerm",
                                "Duplicate GO term on feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_InvalidInferenceValue)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->AddQualifier("inference", " ");

    STANDARD_SETUP

    feat->SetQual().front()->SetVal("bad");
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "InvalidInferenceValue",
                                 "Inference qualifier problem - bad inference prefix (bad)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    feat->SetQual().front()->SetVal("similar to sequence");
    expected_errors[0]->SetErrMsg("Inference qualifier problem - bad inference body (similar to sequence)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    feat->SetQual().front()->SetVal("profile(same species): INSD:AY123456.1");
    expected_errors[0]->SetErrMsg("Inference qualifier problem - same species misused (profile(same species): INSD:AY123456.1)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    feat->SetQual().front()->SetVal("similar to RNA sequence: INSD:AY123456.1 INSD:AY123457");
    expected_errors[0]->SetErrMsg("Inference qualifier problem - spaces in inference (similar to RNA sequence: INSD:AY123456.1 INSD:AY123457)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    feat->SetQual().front()->SetVal("similar to RNA sequence: INSD:AY123456");
    expected_errors[0]->SetErrMsg("Inference qualifier problem - bad inference accession version (similar to RNA sequence: INSD:AY123456)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    feat->SetQual().front()->SetVal("similar to RNA sequence: RefSeq:AY123456.1");
    expected_errors[0]->SetErrMsg("Inference qualifier problem - bad accession type (similar to RNA sequence: RefSeq:AY123456.1)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    feat->SetQual().front()->SetVal("similar to RNA sequence: BLAST:AY123456.1");
    expected_errors[0]->SetErrMsg("Inference qualifier problem - bad accession type (similar to RNA sequence: BLAST:AY123456.1)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    feat->SetQual().front()->SetVal("similar to AA sequence:RefSeq:gi|21240850|ref|NP_640432.1|");
    eval = validator.Validate(seh, options);
    expected_errors[0]->SetErrMsg("Inference qualifier problem - the value in the accession field is not legal. The only allowed value is accession.version, eg AF123456.1. Problem = (similar to AA sequence:RefSeq:gi|21240850|ref|NP_640432.1|)");
    CheckErrors(*eval, expected_errors);


    CLEAR_ERRORS    
    
    // SRA inferences are ok
    feat->SetQual().front()->SetVal("similar to RNA sequence:INSD:ERP003431");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_HpotheticalProteinMismatch) {
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();

    CRef<CSeq_id> protid(new CSeq_id());
    protid->SetOther().SetAccession("XP_654321");
    unit_test_util::ChangeProtId (entry, protid);
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().ResetName();
    prot->SetData().SetProt().SetName().push_back("hypothetical protein XP_123");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("ref|XP_654321|", eDiag_Warning, "HpotheticalProteinMismatch",
                               "Hypothetical protein reference does not match accession"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_SelfReferentialProduct)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(entry);
    cds->SetData().SetCdregion();
    cds->SetLocation().SetInt().SetTo(59);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    cds->SetPartial(true);
    cds->SetProduct().SetWhole().Assign(*(entry->SetSeq().SetId().front()));

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "SelfReferentialProduct",
                               "Self-referential feature product"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "PartialsInconsistent", 
                               "Inconsistent: Product= complete, Location= partial, Feature.partial= TRUE"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "CDSproductPackagingProblem",
                               "Protein product not packaged in nuc-prot set with nucleotide"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ITSdoesNotAbutRRNA)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> rrna = unit_test_util::AddMiscFeature (entry);
    rrna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rrna->SetData().SetRna().SetExt().SetName("18s ribosomal subunit");

    CRef<CSeq_feat> its = unit_test_util::AddMiscFeature (entry);
    its->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    its->SetData().SetRna().SetExt().SetName("internal transcribed spacer 1");
    its->SetLocation().SetInt().SetFrom(rrna->GetLocation().GetInt().GetTo() + 2);
    its->SetLocation().SetInt().SetTo(rrna->GetLocation().GetInt().GetTo() + 12);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "ITSdoesNotAbutRRNA",
                               "ITS does not abut adjacent rRNA component"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp (entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    rrna->SetData().SetRna().SetExt().SetName("5.8S ribosomal subunit");
    its->SetData().SetRna().SetExt().SetName("internal transcribed spacer 2");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp (entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FeatureSeqIDCaseDifference)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("Good");
    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "FeatureSeqIDCaseDifference",
                               "Sequence identifier in feature location differs in capitalization with identifier on Bioseq"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FeatureLocationIsGi0)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGi(ZERO_GI);
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("gi|0", eDiag_Critical, "ZeroGiNumber",
                               "Invalid GI number"));
    expected_errors.push_back (new CExpectedError("gi|0", eDiag_Error, "GiWithoutAccession",
                               "No accession on sequence with gi number"));
    expected_errors.push_back (new CExpectedError("gi|0", eDiag_Critical, "FeatureLocationIsGi0",
                               "Feature has 1 gi|0 location on Bioseq gi|0"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_GapFeatureProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetSeq_data().SetIupacna().Set("CNCATGATGATG");

    CRef<CSeq_feat> gap = unit_test_util::AddMiscFeature(entry);
    gap->SetData().SetImp().SetKey("gap");
    gap->AddQualifier("estimated_length", "11");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "GapFeatureProblem",
                               "Gap feature over 11 real bases"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gap->SetLocation().SetInt().SetFrom(10);
    gap->SetLocation().SetInt().SetTo(20);
    expected_errors[0]->SetErrMsg("Gap feature over 2 real bases");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gap->SetLocation().SetInt().SetFrom(20);
    gap->SetLocation().SetInt().SetTo(30);
    expected_errors[0]->SetErrMsg("Gap feature over 8 real bases and 1 Ns");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gap->SetLocation().SetInt().SetFrom(12);
    gap->SetLocation().SetInt().SetTo(21);
    expected_errors[0]->SetErrMsg("Gap feature estimated_length 11 does not match 10 feature length");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static CRef<CSeq_entry> BuildGenProdSetBigNucProtSet (CRef<CSeq_id> nuc_id, CRef<CSeq_id> prot_id)
{
    CRef<CSeq_entry> np = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(np);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATTTTTTTTTTTTTTTTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATTTTTTTTTTTTTTTTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATAA");
    nuc->SetSeq().SetInst().SetLength(366);
    nuc->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    unit_test_util::SetBiomol(nuc, CMolInfo::eBiomol_mRNA);
    CRef<CSeq_entry> prot = unit_test_util::GetProteinSequenceFromGoodNucProtSet(np);
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MFFFFFFFFFFPPPPPPPPPPGGGGGGGGGGKKKKKKKKKKFFFFFFFFFFPPPPPPPPPPGGGGGGGGGGKKKKKKKKKKFFFFFFFFFFPPPPPPPPPPGGGGGGGGGGKKKKKKKKKK");
    prot->SetSeq().SetInst().SetLength(121);
    unit_test_util::AdjustProtFeatForNucProtSet (np);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(np);
    cds->SetLocation().SetInt().SetFrom(0);
    cds->SetLocation().SetInt().SetTo(nuc->GetSeq().GetInst().GetLength()-1);
    if (nuc_id) {
        unit_test_util::ChangeNucProtSetNucId(np, nuc_id);
    }
    if (prot_id) {
        unit_test_util::ChangeNucProtSetProteinId(np, prot_id);
    }
    return np;
}


static CRef<CSeq_entry> BuildGenProdSetWithBigProduct()
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_gen_prod_set);
    CRef<CSeq_entry> contig = unit_test_util::BuildGoodSeq();
    contig->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATTTTTTTTTTTTTTTTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATTTTTTTTTTTTTTTTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATAAGGGCCCTTT");
    contig->SetSeq().SetInst().SetLength(375);
    entry->SetSet().SetSeq_set().push_back (contig);
    CRef<CSeq_id> nuc_id(new CSeq_id());
    nuc_id->SetLocal().SetStr("nuc");
    CRef<CSeq_id> prot_id(new CSeq_id());
    prot_id->SetLocal().SetStr("prot");
    CRef<CSeq_entry> np = BuildGenProdSetBigNucProtSet(nuc_id, prot_id);
    entry->SetSet().SetSeq_set().push_back (np);

    CRef<CSeq_feat> cds(new CSeq_feat());
    cds->Assign (*(unit_test_util::GetCDSFromGoodNucProtSet(np)));
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    unit_test_util::AddFeat (cds, contig);
    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetProduct().SetWhole().Assign(*nuc_id);
    unit_test_util::AddFeat (mrna, contig);

    return entry;
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ErroneousException)
{
    CRef<CSeq_entry> entry = BuildGenProdSetWithBigProduct();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGenProdSet (entry);
    cds->SetExcept(true);
    cds->SetExcept_text("unclassified translation discrepancy");
    CRef<CSeq_feat> mrna = unit_test_util::GetmRNAFromGenProdSet(entry);
    mrna->SetExcept(true);
    mrna->SetExcept_text("unclassified transcription discrepancy");
    CRef<CSeq_entry> genomic = unit_test_util::GetGenomicFromGenProdSet(entry);
    genomic->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGTTTCTTTTTTTTTTTTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATTTTTTTTTTTTTTTTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATTTTTTTTTTTTTTTTTTTTTTTTTTTTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATAAGGGCCCTTT");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "ErroneousException",
                               "CDS has unclassified exception but only difference is 1 mismatches out of 121 residues"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "ErroneousException",
                               "mRNA has unclassified exception but only difference is 1 mismatches out of 366 bases"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_WholeLocation)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetLocation().SetWhole().Assign(*(entry->SetSeq().SetId().front()));
    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(entry);
    cds->SetData().SetCdregion();
    cds->SetLocation().SetWhole().Assign(*(entry->SetSeq().SetId().front()));
    cds->SetPseudo(true);

    CRef<CSeq_feat> mrna = unit_test_util::AddMiscFeature(entry);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->SetLocation().SetWhole().Assign(*(entry->SetSeq().SetId().front()));
    mrna->SetPseudo(true);


    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "WholeLocation",
                               "Feature may not have whole location"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "WholeLocation",
                               "CDS may not have whole location"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "WholeLocation",
                               "mRNA may not have whole location"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_EcNumberProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetComment("EC:1.1.1.10");
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().SetName().front().append("; EC:1.1.1.10");
    prot->SetComment("EC:1.1.1.10");
    prot->SetData().SetProt().SetEc().push_back("");

    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> exon = unit_test_util::AddMiscFeature(nuc);
    exon->SetData().SetImp().SetKey("exon");
    exon->AddQualifier("EC_number", ""); 

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "InvalidPunctuation",
                               "Qualifier other than replace has just quotation marks"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "EcNumberEmpty",
                               "EC number should not be empty"));
    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "EcNumberInProteinName",
                               "Apparent EC number in protein title"));
    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "EcNumberInProteinComment",
                               "Apparent EC number in protein comment"));
    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "EcNumberEmpty",
                               "EC number should not be empty"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    

    prot->SetData().SetProt().ResetEc();
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "InvalidPunctuation",
                               "Qualifier other than replace has just quotation marks"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "EcNumberEmpty",
                               "EC number should not be empty"));
    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "EcNumberInProteinName",
                               "Apparent EC number in protein title"));
    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "EcNumberInProteinComment",
                               "Apparent EC number in protein comment"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Info, "EcNumberInCDSComment",
                               "Apparent EC number in CDS comment"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_VectorContamination)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->AddQualifier("standard_name", "Vector Contamination");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "VectorContamination",
                               "Vector Contamination region should be trimmed from sequence"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MinusStrandProtein)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodProtSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    misc->SetLocation().SetInt().SetTo(5);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "MinusStrandProtein",
                               "Feature on protein indicates negative strand"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadProteinName)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().ResetName();
    prot->SetData().SetProt().SetName().push_back("Hypothetical protein");
    prot->SetData().SetProt().SetEc().push_back("1.1.1.20");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "BadProteinName",
                               "Unknown or hypothetical protein should not have EC number"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    prot->SetData().SetProt().ResetName();
    prot->SetData().SetProt().SetName().push_back("hypothetical protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    prot->SetData().SetProt().ResetName();
    prot->SetData().SetProt().SetName().push_back("Unknown protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    prot->SetData().SetProt().ResetName();
    prot->SetData().SetProt().SetName().push_back("unknown protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_GeneXrefWithoutLocus)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat>  misc = unit_test_util::AddMiscFeature(entry);
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature (misc);
    unit_test_util::AddFeat(gene1, entry);
    CRef<CSeq_feat> gene2 = unit_test_util::MakeGeneForFeature (misc);
    gene2->SetData().SetGene().SetLocus_tag("locus_tag");
    gene2->SetData().SetGene().SetLocus ("second locus");
    gene2->SetLocation().SetInt().SetTo(misc->GetLocation().GetInt().GetTo() + 5);
    unit_test_util::AddFeat(gene2, entry);
    CRef<CSeqFeatXref> x(new CSeqFeatXref());
    x->SetData().SetGene().SetLocus_tag("locus_tag");
    misc->SetXref().push_back(x);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "GeneXrefWithoutLocus",
                               "Feature has Gene Xref with locus_tag but no locus, gene with locus_tag and locus exists"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_UTRdoesNotExtendToEnd)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAAAAAGGGAAA");
    nuc->SetSeq().SetInst().SetLength(36);
    nuc->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    unit_test_util::SetBiomol(nuc, CMolInfo::eBiomol_mRNA);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> utr3 = unit_test_util::AddMiscFeature(nuc);
    utr3->SetData().SetImp().SetKey("3'UTR");
    utr3->SetLocation().SetInt().SetFrom(cds->GetLocation().GetInt().GetTo() + 1);
    utr3->SetLocation().SetInt().SetTo(30);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "UTRdoesNotExtendToEnd",
                               "3'UTR does not extend to end of mRNA"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_CDShasTooManyXs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGNNNNNNNNNNNNNNNATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    CRef<CSeq_entry> prot = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MXXXXXIN");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "FeatureIsMostlyNs",
                               "Feature contains more than 50% Ns"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "CDShasTooManyXs",
                               "CDS translation consists of more than 50% X residues"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_SuspiciousFrame)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
    cds->SetLocation().SetInt().SetTo(21);

    STANDARD_SETUP
    string tmp;
    CSeqTranslator::Translate(*cds, scope, tmp, false, false);
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_entry> prot = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set(tmp);
    prot->SetSeq().SetInst().SetLength(tmp.length());
    unit_test_util::AdjustProtFeatForNucProtSet (entry);
    CRef<CSeq_feat> prot_feat = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "SuspiciousFrame",
                               "Suspicious CDS location - reading frame > 1 but not 5' partial"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    cds->SetLocation().SetInt().SetFrom(1);
    cds->SetLocation().SetInt().SetTo(26);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    cds->SetPartial(true);
    tmp.clear();
    CSeqTranslator::Translate(*cds, scope, tmp, false, false);
    scope.RemoveTopLevelSeqEntry(seh);
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set(tmp);
    prot->SetSeq().SetInst().SetLength(tmp.length());
    unit_test_util::AdjustProtFeatForNucProtSet (entry);
    unit_test_util::SetCompleteness (prot, CMolInfo::eCompleteness_no_left);
    prot_feat->SetLocation().SetPartialStart(true, eExtreme_Biological);
    prot_feat->SetPartial(true);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS    

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemNotConsensus5Prime",
                               "5' partial is not at beginning of sequence, gap, or consensus splice site"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SuspiciousFrame",
                               "Suspicious CDS location - reading frame > 1 and not at consensus splice site"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_TerminalXDiscrepancy)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACNAAGGG");
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetPartial(true);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    cds->SetLocation().SetInt().SetFrom(30);
    cds->SetLocation().SetInt().SetTo(nuc->GetSeq().GetInst().GetLength() - 1);
    CRef<CSeq_entry> prot = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEINXX");
    prot->SetSeq().SetInst().SetLength(10);
    unit_test_util::SetCompleteness (prot, CMolInfo::eCompleteness_no_right);
    CRef<CSeq_feat> prot_feat = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    unit_test_util::AdjustProtFeatForNucProtSet (entry);
    prot_feat->SetPartial(true);
    prot_feat->SetLocation().SetPartialStop(true, eExtreme_Biological);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "TransLen", 
                               "Given protein length [8] does not match translation length [10]"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "TerminalXDiscrepancy",
                               "Terminal X count for CDS translation (0) and protein product sequence (2) are not equal"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_UnnecessaryTranslExcept)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CCode_break> codebreak(new CCode_break());
    codebreak->SetLoc().SetInt().SetId().SetLocal().SetStr("nuc");
    codebreak->SetLoc().SetInt().SetFrom(3);
    codebreak->SetLoc().SetInt().SetTo(5);
    codebreak->SetAa().SetNcbieaa('P');
    cds->SetData().SetCdregion().SetCode_break().push_back(codebreak);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "UnnecessaryTranslExcept",
                               "Unnecessary transl_except P at position 2"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    codebreak->SetLoc().SetInt().SetFrom(0);
    codebreak->SetLoc().SetInt().SetTo(2);
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "TranslExcept",
                               "Suspicious transl_except P at first codon of complete CDS"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "MisMatchAA",
                               "Residue 1 in protein [M] != translation [P] at lcl|nuc:1-3"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_InvalidMatchingReplace)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetData().SetImp().SetKey("misc_difference");
    feat->AddQualifier("replace", "aattggccaaa");

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Info, "InvalidMatchingReplace",
                               "/replace already matches underlying sequence (aattggccaaa)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_NotSpliceConsensusDonor)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(nuc->SetSeq().SetId().front()));
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[44] = 'A';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[45] = 'G';
    CRef<CSeq_feat> intron = unit_test_util::MakeIntronForMixLoc(nuc->SetSeq().SetId().front());
    unit_test_util::AddFeat(intron, nuc);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusDonor",
                               "Splice donor consensus (GT) not found at start of intron, position 17 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusDonor",
                               "Splice donor consensus (GT) not found after exon ending at position 16 of lcl|nuc"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Splice donor consensus (GT) not found at start of intron, position 44 of lcl|nuc");
    expected_errors[1]->SetErrMsg("Splice donor consensus (GT) not found after exon ending at position 45 of lcl|nuc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[16] = '\xFB';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[17] = '\xFB';
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [17]"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [18]"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusDonor", "Splice donor consensus (GT) not found at start of intron, position 17 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusDonor", "Bad sequence at splice donor after exon ending at position 16 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "NonAsciiAsn", "Non-ASCII character '251' found in item"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [43]"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [44]"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusDonor", "Splice donor consensus (GT) not found at start of intron, position 44 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusDonor", "Bad sequence at splice donor after exon ending at position 45 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "NonAsciiAsn", "Non-ASCII character '251' found in item"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    intron = unit_test_util::AddMiscFeature(entry);
    intron->SetData().SetImp().SetKey("intron");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Info, "NotSpliceConsensusDonorTerminalIntron",
                               "Splice donor consensus (GT) not found at start of terminal intron, position 1 of lcl|good"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                               "Splice acceptor consensus (AG) not found at end of intron, position 11 of lcl|good"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Info, "NotSpliceConsensusDonorTerminalIntron",
                               "Splice donor consensus (GT) not found at start of terminal intron, position 60 of lcl|good"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                               "Splice acceptor consensus (AG) not found at end of intron, position 50 of lcl|good"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_NotSpliceConsensusAcceptor)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(nuc->SetSeq().SetId().front()));
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[16] = 'G';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[17] = 'T';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[44] = 'T';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[45] = 'C';
    CRef<CSeq_feat> intron = unit_test_util::MakeIntronForMixLoc(nuc->SetSeq().SetId().front());
    unit_test_util::AddFeat(intron, nuc);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusAcceptor",
                               "Splice acceptor consensus (AG) not found at end of intron, position 46 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusAcceptor",
                               "Splice acceptor consensus (AG) not found before exon starting at position 47 of lcl|nuc"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Splice acceptor consensus (AG) not found at end of intron, position 15 of lcl|nuc");
    expected_errors[1]->SetErrMsg("Splice acceptor consensus (AG) not found before exon starting at position 14 of lcl|nuc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[44] = '\xFB';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[45] = '\xFB';
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Critical, "InvalidResidue",
                                                  "Invalid residue [251] at position [45]"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Critical, "InvalidResidue", 
                                                  "Invalid residue [251] at position [46]"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                                                  "Splice acceptor consensus (AG) not found at end of intron, position 46 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                                                  "Bad sequence at splice acceptor before exon starting at position 47 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "NonAsciiAsn", "Non-ASCII character '251' found in item"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Critical, 
                               "InvalidResidue", "Invalid residue [251] at position [15]"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Critical, "InvalidResidue", 
                               "Invalid residue [251] at position [16]"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                               "Splice acceptor consensus (AG) not found at end of intron, position 15 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                               "Bad sequence at splice acceptor before exon starting at position 14 of lcl|nuc"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Error, "NonAsciiAsn", "Non-ASCII character '251' found in item"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodSeq();
    intron = unit_test_util::AddMiscFeature(entry);
    intron->SetData().SetImp().SetKey("intron");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Info, "NotSpliceConsensusDonorTerminalIntron",
                               "Splice donor consensus (GT) not found at start of terminal intron, position 1 of lcl|good"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                               "Splice acceptor consensus (AG) not found at end of intron, position 11 of lcl|good"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Info, "NotSpliceConsensusDonorTerminalIntron",
                               "Splice donor consensus (GT) not found at start of terminal intron, position 60 of lcl|good"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                               "Splice acceptor consensus (AG) not found at end of intron, position 50 of lcl|good"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_RareSpliceConsensusDonor)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(nuc->SetSeq().SetId().front()));
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[16] = 'G';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[17] = 'C';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[44] = 'A';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[45] = 'G';
    CRef<CSeq_feat> intron = unit_test_util::MakeIntronForMixLoc(nuc->SetSeq().SetId().front());
    unit_test_util::AddFeat(intron, nuc);

    STANDARD_SETUP
    // no longer report
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_RareSpliceConsensusDonor_VR_65)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(nuc->SetSeq().SetId().front()));
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[16] = 'A';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[17] = 'T';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[44] = 'A';
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set()[45] = 'C';
    CRef<CSeq_feat> intron = unit_test_util::MakeIntronForMixLoc(nuc->SetSeq().SetId().front());
    unit_test_util::AddFeat(intron, nuc);

    STANDARD_SETUP

    // no longer report
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CLEAR_ERRORS
    // no longer report
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_SeqFeatXrefNotReciprocal)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetId().SetLocal().SetId(1);
    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetId().SetLocal().SetId(2);
    unit_test_util::AddFeat (mrna, nuc);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (mrna);
    unit_test_util::AddFeat (gene, nuc);
    gene->SetId().SetLocal().SetId(3);

    cds->AddSeqFeatXref(mrna->GetId());
    mrna->AddSeqFeatXref(gene->GetId());


    STANDARD_SETUP
//    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefNotReciprocal",
//                               "CDS/mRNA unambiguous pair have erroneous cross-references"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefProblem",
                               "Cross-referenced feature does not have its own cross-reference"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefNotReciprocal",
                               "Cross-referenced feature does not link reciprocally"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_SeqFeatXrefFeatureMissing)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetId().SetLocal().SetId(1);
    CRef<CSeqFeatXref> x1(new CSeqFeatXref());
    x1->SetId().SetLocal().SetId(2);
    cds->SetXref().push_back(x1);
    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "SeqFeatXrefFeatureMissing",
                               "Cross-referenced feature cannot be found"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FeatureInsideGap)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetLocation().SetInt().SetFrom(12);
    misc->SetLocation().SetInt().SetTo(20);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "FeatureInsideGap",
                               "Feature inside sequence gap"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CDelta_seq> gap_seg(new CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCANNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNTGATGATG", CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetLength(116);
    misc->SetLocation().SetInt().SetFrom(48);
    misc->SetLocation().SetInt().SetTo(98);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
                               "Sequence contains 51 percent Ns"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "FeatureInsideGap",
                               "Feature inside gap of Ns"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FeatureCrossesGap)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    NON_CONST_ITERATE (CDelta_ext::Tdata, it, entry->SetSeq().SetInst().SetExt().SetDelta().Set()) {
        if ((*it)->IsLiteral() && (*it)->GetLiteral().GetSeq_data().IsGap()) {
            (*it)->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
        }
    }

    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    misc->SetLocation().SetInt().SetFrom(5);
    misc->SetLocation().SetInt().SetTo(30);

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                              "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "FeatureCrossesGap",
                              "Feature crosses gap of unknown length"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_loc> int1(new CSeq_loc());
    int1->SetInt().SetFrom(3);
    int1->SetInt().SetTo(15);
    int1->SetInt().SetId().SetLocal().SetStr("good");
    CRef<CSeq_loc> int2(new CSeq_loc());
    int2->SetInt().SetFrom(22);
    int2->SetInt().SetTo(30);
    int2->SetInt().SetId().SetLocal().SetStr("good");
    misc->SetLocation().SetMix().Set().push_back(int1);
    misc->SetLocation().SetMix().Set().push_back(int2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
        "No CDS location match for 1 mRNA"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "IntervalBeginsOrEndsInGap",
        "Internal interval begins or ends in gap"));

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadAuthorSuffix)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CPub> pub = unit_test_util::BuildGoodArticlePub();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetDescr().Set().push_back(desc);
    pub->SetArticle().SetAuthors().SetNames().SetStd().front()->SetName().SetName().SetSuffix("foo");

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadAuthorSuffix",
                               "Bad author suffix foo"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    // don't report good suffixes
    pub->SetArticle().SetAuthors().SetNames().SetStd().front()->SetName().SetName().SetSuffix("3rd");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadAnticodonAA)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front());
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(8);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(10);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('S');
    unit_test_util::AddFeat(trna, entry);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadAnticodonAA",
                               "Codons predicted from anticodon (AAA) cannot produce amino acid (S/Ser)"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadAnticodonCodon)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front());
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(8);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(10);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('K');
    trna->SetData().SetRna().SetExt().SetTRNA().SetCodon().push_back(42);
    unit_test_util::AddFeat(trna, entry);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadAnticodonAA",
                               "Codons predicted from anticodon (AAA) cannot produce amino acid (K/Lys)"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadAnticodonCodon",
                               "Codon recognized cannot be produced from anticodon (AAA)"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadAnticodonStrand)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::BuildtRNA(entry->SetSeq().SetId().front());
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(8);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(10);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetStrand (eNa_strand_minus);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('K');
    unit_test_util::AddFeat(trna, entry);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Error, "BadAnticodonStrand",
                               "Anticodon should be on plus strand"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().ResetStrand();
    trna->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetIupacaa('F');
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Anticodon should be on minus strand");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


#define test_gene_syn(name) \
    gene->SetData().SetGene().ResetSyn(); \
    gene->SetData().SetGene().SetSyn().push_back(name); \
    msg = "Uninformative gene synonym '"; \
    msg.append(name); \
    msg.append("'"); \
    expected_errors[0]->SetErrMsg(msg); \
    eval = validator.Validate(seh, options); \
    CheckErrors (*eval, expected_errors);
    



BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_UndesiredGeneSynonym)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("something");
    string msg = "";

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("ref|NC_123456|", eDiag_Warning, "UndesiredGeneSynonym", ""));

    test_gene_syn("alpha")
    test_gene_syn("alternative")
    test_gene_syn("beta")
    test_gene_syn("cellular")
    test_gene_syn("cytokine")
    test_gene_syn("delta")
    test_gene_syn("drosophila")
    test_gene_syn("epsilon")
    test_gene_syn("gamma")
    test_gene_syn("HLA")
    test_gene_syn("homolog")
    test_gene_syn("mouse")
    test_gene_syn("orf")
    test_gene_syn("partial")
    test_gene_syn("plasma")
    test_gene_syn("precursor")
    test_gene_syn("pseudogene")
    test_gene_syn("putative")
    test_gene_syn("rearranged")
    test_gene_syn("small")
    test_gene_syn("trna")
    test_gene_syn("unknown")
    test_gene_syn("unknown function")
    test_gene_syn("unknown protein")
    test_gene_syn("unnamed")


    gene->SetData().SetGene().ResetSyn();
    gene->SetData().SetGene().SetSyn().push_back("same_as");
    gene->SetData().SetGene().SetLocus("same_as");
    expected_errors[0]->SetErrMsg("gene synonym has same value as gene locus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gene->SetData().SetGene().ResetSyn();
    gene->SetData().SetGene().SetDesc("same_as");
    expected_errors[0]->SetErrMsg("gene description has same value as gene locus");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gene->SetData().SetGene().ResetDesc();
    gene->SetData().SetGene().ResetLocus();
    gene->SetData().SetGene().SetSyn().push_back("only_syn");
    expected_errors[0]->SetErrMsg("gene synonym without gene locus or description");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    CLEAR_ERRORS
}


#define test_undesired_protein_name(name) \
    prot->SetData().SetProt().ResetName(); \
    prot->SetData().SetProt().SetName().push_back(name); \
    msg = "Uninformative protein name '"; \
    msg.append(name); \
    msg.append("'"); \
    expected_errors[0]->SetErrMsg(msg); \
    eval = validator.Validate(seh, options); \
    CheckErrors (*eval, expected_errors);

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_UndesiredProteinName) 
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_id> id (new CSeq_id());
    id->SetOther().SetAccession("NC_123456");
    unit_test_util::ChangeNucProtSetNucId(entry, id);
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);

    STANDARD_SETUP
    
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "UndesiredProteinName",
                              ""));
    string msg;

    test_undesired_protein_name("a=b")
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "BadInternalCharacter",
                              "Protein name contains undesired character"));
    test_undesired_protein_name("a~b")
    delete expected_errors[1];
    expected_errors.pop_back();
    test_undesired_protein_name("uniprot protein")
    test_undesired_protein_name("uniprotkb protein")
    test_undesired_protein_name("refers to pmid 23")
    test_undesired_protein_name("refers to dbxref")
    test_undesired_protein_name("hypothetical protein")
    test_undesired_protein_name("uncharacterized conserved membrane protein")
    test_undesired_protein_name("unknown; predicted coding region")
    test_undesired_protein_name("unnamed")

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_FeatureBeginsOrEndsInGap)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    NON_CONST_ITERATE (CDelta_ext::Tdata, it, entry->SetSeq().SetInst().SetExt().SetDelta().Set()) {
        if ((*it)->IsLiteral() && (*it)->GetLiteral().GetSeq_data().IsGap()) {
            (*it)->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
        }
    }

    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetLocation().SetInt().SetFrom(5);
    misc->SetLocation().SetInt().SetTo(20);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "FeatureBeginsOrEndsInGap",
                               "Feature begins or ends in gap starting at 13"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    misc->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    misc->SetLocation().SetInt().SetFrom(14);
    misc->SetLocation().SetInt().SetTo(30);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    misc->SetLocation().SetInt().ResetStrand();
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_GeneOntologyTermMissingGOID)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetExt().SetType().SetStr("GeneOntology");
    CRef<CUser_field> go_list(new CUser_field());
    go_list->SetLabel().SetStr("Process");
    CRef<CUser_field> go_term (new CUser_field());
    go_term->SetLabel().SetStr("a go term");

    CRef<CUser_field> pmid(new CUser_field());
    pmid->SetLabel().SetStr("pubmed id");
    pmid->SetData().SetInt(4);
    go_term->SetData().SetFields().push_back (pmid);

    CRef<CUser_field> term(new CUser_field());
    term->SetLabel().SetStr("text string");
    term->SetData().SetStr("something");
    go_term->SetData().SetFields().push_back (term);

    CRef<CUser_field> ev(new CUser_field());
    ev->SetLabel().SetStr("evidence");
    ev->SetData().SetStr("some evidence");
    go_term->SetData().SetFields().push_back (ev);

    go_list->SetData().SetFields().push_back(go_term);
    feat->SetExt().SetData().push_back(go_list);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "GeneOntologyTermMissingGOID",
                                "GO term does not have GO identifier"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


// note - this test also covers PseudoRnaViaGeneHasProduct
BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_PseudoRnaHasProduct)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> rna = unit_test_util::AddMiscFeature(entry);
    rna->ResetComment();
    rna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rna->SetPseudo(true);
    rna->SetProduct().SetWhole().SetGenbank().SetAccession("AY123456");

    STANDARD_SETUP_NO_DATABASE

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "PseudoRnaHasProduct",
                                "A pseudo RNA should not have a product"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // this exception should turn off the warning 
    rna->SetExcept(true);
    rna->SetExcept_text("transcribed pseudogene");
    CLEAR_ERRORS
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // should get error if overlapping gene is pseudo (and not except text)
    scope.RemoveTopLevelSeqEntry(seh);
    rna->ResetExcept();
    rna->ResetExcept_text();
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(rna);
    gene->SetPseudo(true);
    unit_test_util::AddFeat(gene, entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "PseudoRnaHasProduct",
                                "A pseudo RNA should not have a product"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // now get PseudoRnaViaGeneHasProduct when rna is not pseudo itself
    rna->ResetPseudo();
    CLEAR_ERRORS
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "PseudoRnaViaGeneHasProduct",
                                "An RNA overlapped by a pseudogene should not have a product"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadRRNAcomponentOrder)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> r1(new CSeq_feat());
    r1->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    r1->SetData().SetRna().SetExt().SetName("26S ribosomal RNA");
    r1->SetLocation().SetInt().SetId().Assign(*(entry->SetSeq().SetId().front()));
    r1->SetLocation().SetInt().SetFrom(0);
    r1->SetLocation().SetInt().SetTo(10);
    unit_test_util::AddFeat(r1, entry);
    CRef<CSeq_feat> r2(new CSeq_feat());
    r2->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    r2->SetData().SetRna().SetExt().SetName("internal transcribed spacer 2");
    r2->SetLocation().SetInt().SetId().Assign(*(entry->SetSeq().SetId().front()));
    r2->SetLocation().SetInt().SetFrom(11);
    r2->SetLocation().SetInt().SetTo(20);
    unit_test_util::AddFeat(r2, entry);
    CRef<CSeq_feat> r3(new CSeq_feat());
    r3->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    r3->SetData().SetRna().SetExt().SetName("16S ribosomal RNA");
    r3->SetLocation().SetInt().SetId().Assign(*(entry->SetSeq().SetId().front()));
    r3->SetLocation().SetInt().SetFrom(21);
    r3->SetLocation().SetInt().SetTo(30);
    unit_test_util::AddFeat(r3, entry);


    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadRRNAcomponentOrder",
                                "Problem with order of abutting rRNA components"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BadRRNAcomponentOrder",
        "Problem with order of abutting rRNA components"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MissingGeneLocusTag)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    CRef<CSeq_feat> gene1 = unit_test_util::AddMiscFeature (entry);
    gene1->ResetComment();
    gene1->SetData().SetGene().SetLocus("a");
    gene1->SetData().SetGene().SetLocus_tag("tag1");
    CRef<CSeq_feat> gene2 = unit_test_util::AddMiscFeature (entry);
    gene2->ResetComment();
    gene2->SetData().SetGene().SetLocus("b");
    gene2->SetLocation().SetInt().SetFrom(20);
    gene2->SetLocation().SetInt().SetTo(30);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("ref|NC_123456|", eDiag_Warning, "MissingGeneLocusTag",
                                "Missing gene locus tag"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS    
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MultipleProtRefs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> prot_seq = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> prot2 = unit_test_util::AddMiscFeature(prot_seq);
    prot2->SetData().SetProt().SetName().push_back("a second protein name");
    prot2->SetLocation().SetInt().SetTo(prot_seq->GetSeq().GetInst().GetLength()-1);
    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "MultipleProtRefs",
                               "2 full-length protein features present on protein"));
    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "DuplicateFeat", 
                              "Features have identical intervals, but labels differ"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "ExtraProteinFeature", 
                              "Protein sequence has multiple unprocessed protein features"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "ExtraProteinFeature", 
                              "Protein sequence has multiple unprocessed protein features"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadInternalCharacter)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().ResetName();
    prot->SetData().SetProt().SetName().push_back("name~something");
    CRef<CSeq_feat>  cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetData().SetRna().SetExt().SetName("name~something");
    unit_test_util::AddFeat(mrna, nuc);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(mrna);
    gene->SetData().SetGene().SetLocus("gene?something");
    unit_test_util::AddFeat(gene, nuc);

    CRef<CSeq_feat> rrna = unit_test_util::AddMiscFeature(nuc);
    rrna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rrna->SetData().SetRna().SetExt().SetName("rna!something");

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "BadInternalCharacter", 
                              "mRNA name contains undesired character"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "BadInternalCharacter", 
                              "Gene locus contains undesired character"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BadInternalCharacter",
                              "rRNA name contains undesired character"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "BadInternalCharacter",
                              "Protein name contains undesired character"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_VR_746)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat>  cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(cds);
    gene->SetData().SetGene().SetLocus("gene|synonym");
    unit_test_util::AddFeat(gene, nuc);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "BadInternalCharacter", 
                              "Gene locus contains undesired character"));

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadTrailingCharacter)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().ResetName();
    prot->SetData().SetProt().SetName().push_back("name something,");
    CRef<CSeq_feat>  cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetData().SetRna().SetExt().SetName("name something_");
    unit_test_util::AddFeat(mrna, nuc);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(mrna);
    gene->SetData().SetGene().SetLocus("gene something;");
    unit_test_util::AddFeat(gene, nuc);

    CRef<CSeq_feat> rrna = unit_test_util::AddMiscFeature(nuc);
    rrna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rrna->SetData().SetRna().SetExt().SetName("rna something:");

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "BadTrailingCharacter", 
                              "mRNA name ends with undesired character"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "BadTrailingCharacter", 
                              "Gene locus ends with undesired character"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BadTrailingCharacter",
                              "rRNA name ends with undesired character"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "BadTrailingCharacter",
                              "Protein name ends with undesired character"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadTrailingHyphen)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().ResetName();
    prot->SetData().SetProt().SetName().push_back("name something-");
    CRef<CSeq_feat>  cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetData().SetRna().SetExt().SetName("name something-");
    unit_test_util::AddFeat(mrna, nuc);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(mrna);
    gene->SetData().SetGene().SetLocus("gene something-");
    unit_test_util::AddFeat(gene, nuc);

    CRef<CSeq_feat> rrna = unit_test_util::AddMiscFeature(nuc);
    rrna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rrna->SetData().SetRna().SetExt().SetName("rna something-");

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "BadTrailingHyphen", 
                              "mRNA name ends with hyphen"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "BadTrailingHyphen", 
                              "Gene locus ends with hyphen"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "BadTrailingHyphen",
                              "rRNA name ends with hyphen"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "BadTrailingHyphen",
                              "Protein name ends with hyphen"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MultipleGeneOverlap)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene1 = unit_test_util::AddMiscFeature(entry);
    gene1->SetData().SetGene().SetLocus("a");
    gene1->SetLocation().SetInt().SetFrom(0);
    gene1->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength()-1);
    CRef<CSeq_feat> gene2 = unit_test_util::AddMiscFeature(entry);
    gene2->SetData().SetGene().SetLocus("b");
    CRef<CSeq_feat> gene3 = unit_test_util::AddMiscFeature(entry);
    gene3->SetData().SetGene().SetLocus("c");
    gene3->SetLocation().SetInt().SetFrom(11);
    gene3->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength()-1);

    STANDARD_SETUP
    // no error for only two genes
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> gene4 = unit_test_util::AddMiscFeature(entry);
    gene4->SetData().SetGene().SetLocus("d");
    gene4->SetLocation().SetInt().SetFrom(15);
    gene4->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength()-1);
    CRef<CSeq_feat> gene5 = unit_test_util::AddMiscFeature(entry);
    gene5->SetData().SetGene().SetLocus("e");
    gene5->SetLocation().SetInt().SetFrom(20);
    gene5->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength()-1);
    CRef<CSeq_feat> gene6 = unit_test_util::AddMiscFeature(entry);
    gene6->SetData().SetGene().SetLocus("f");
    gene6->SetLocation().SetInt().SetFrom(25);
    gene6->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength()-1);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "MultipleGeneOverlap", 
                              "Gene contains 5 other genes"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadCharInAuthorLastName)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CAuthor> author(new CAuthor());
    author->SetName().SetName().SetLast("Gr@nt");
    CRef<CPub> pub(new CPub());
    pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author);
    CRef<CCit_art::TTitle::C_E> art_title(new CCit_art::TTitle::C_E());
    art_title->SetName("article title");
    pub->SetArticle().SetTitle().Set().push_back(art_title);
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetPub().SetPub().Set().push_back(pub);
    entry->SetDescr().Set().push_back(desc);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadCharInAuthorLastName", 
                              "Bad characters in author Gr@nt"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_PseudoCDSmRNArange)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(entry);
    cds->ResetComment();
    cds->SetData().SetCdregion();
    cds->SetPseudo(true);
    cds->SetLocation().Assign(*unit_test_util::MakeMixLoc(entry->SetSeq().SetId().front()));
    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetLocation().SetMix().Set().front()->SetInt().SetTo(16);
    unit_test_util::AddFeat(mrna, entry);
    mrna->SetPseudo(true);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "CDSmRNAMismatchLocation",
                               "No CDS location match for 1 mRNA"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Info, "PseudoCDSmRNArange", 
                              "mRNA contains CDS but internal intron-exon boundaries do not match"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    mrna->SetLocation().SetMix().Set().back()->SetInt().SetTo(55);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[1]->SetErrMsg("mRNA overlaps or contains CDS but does not completely contain intervals");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_GeneXrefNeeded)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    AddCDSAndProtForBigGoodNucProtSet(entry, "nuc", "prot2", 30);
    CRef<CSeq_feat> cds = entry->SetSet().SetAnnot().front()->SetData().SetFtable().back();
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature(cds);
    gene1->SetLocation().SetInt().SetFrom(gene1->GetLocation().GetInt().GetFrom() - 3);
    gene1->SetData().SetGene().SetLocus("a1");
    gene1->SetData().SetGene().SetAllele("x");
    unit_test_util::AddFeat(gene1, nuc);
    CRef<CSeq_feat> gene2 = unit_test_util::MakeGeneForFeature(cds);
    gene2->SetData().SetGene().SetLocus("a1");
    gene2->SetData().SetGene().SetAllele("y");
    gene2->SetLocation().SetInt().SetTo(gene2->GetLocation().GetInt().GetTo() + 3);
    unit_test_util::AddFeat(gene2, nuc);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "CollidingGeneNames",
                               "Colliding names in gene features"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "GeneXrefNeeded", 
                              "Feature overlapped by 2 identical-length equivalent genes but has no cross-reference"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_RubiscoProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().SetName().pop_back();
    prot->SetData().SetProt().SetName().push_back("ribulose bisphosphate");
    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "RubiscoProblem", 
                              "Nonstandard ribulose bisphosphate protein name"));
    options |= CValidator::eVal_do_rubisco_test;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_UnqualifiedException)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
    CRef<CSeq_feat> mrna = unit_test_util::GetmRNAFromGenProdSet(entry);
    mrna->SetExcept(true);
    mrna->SetExcept_text("transcribed product replaced");
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGenProdSet(entry);
    cds->SetExcept(true);
    cds->SetExcept_text("translated product replaced");
    CRef<CSeq_entry> genomic = unit_test_util::GetGenomicFromGenProdSet(entry);
    genomic->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGGGGAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "UnqualifiedException", 
                              "CDS has unqualified translated product replaced exception"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "UnqualifiedException", 
                              "mRNA has unqualified transcribed product replaced exception"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ProteinNameHasPMID)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().SetName().front().assign("(PMID 1234)");
    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|prot", eDiag_Warning, "ProteinNameHasPMID", 
                              "Protein name has internal PMID"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadGeneOntologyFormat)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetExt().SetType().SetStr("GeneOntology");
    CRef<CUser_field> go_list(new CUser_field());
    go_list->SetData().SetStr("something");
    feat->SetExt().SetData().push_back(go_list);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "BadGeneOntologyFormat", 
                              "Bad data format for GO term"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CRef<CUser_field> go_term (new CUser_field());
    go_list->SetData().SetFields().push_back (go_term);
    expected_errors[0]->SetErrMsg("Unrecognized GO term label [blank]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    go_list->SetLabel().SetStr("something");
    expected_errors[0]->SetErrMsg("Unrecognized GO term label something");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    go_list->SetLabel().SetStr("Process");
    expected_errors[0]->SetErrMsg("Bad GO term format");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CRef<CUser_field> go_field(new CUser_field());
    go_term->SetData().SetFields().push_back(go_field);
    expected_errors[0]->SetErrMsg("Unrecognized label on GO term qualifier field [blank]");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GeneOntologyTermMissingGOID", 
                              "GO term does not have GO identifier"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    go_field->SetLabel().SetStr("notlabel");
    expected_errors[0]->SetErrMsg("Unrecognized label on GO term qualifier field notlabel");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    go_field->SetLabel().SetStr("go id");
    expected_errors[0]->SetErrMsg("Bad data format for GO term qualifier GO ID");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    go_field->SetData().SetInt(123);
    CRef<CUser_field> go_field2(new CUser_field());
    go_field2->SetLabel().SetStr("text string");
    go_field2->SetData().SetInt(123);
    go_term->SetData().SetFields().push_back(go_field2);
    expected_errors[0]->SetErrMsg("Bad data format for GO term qualifier term");
    delete expected_errors[1];
    expected_errors.pop_back();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    go_field2->SetData().SetStr("some text");
    CRef<CUser_field> go_field3(new CUser_field());
    go_field3->SetLabel().SetStr("pubmed id");
    go_field3->SetData().SetStr("some text");
    go_term->SetData().SetFields().push_back(go_field3);
    expected_errors[0]->SetErrMsg("Bad data format for GO term qualifier PMID");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    go_field3->SetData().SetInt(123);
    CRef<CUser_field> go_field4(new CUser_field());
    go_field4->SetLabel().SetStr("evidence");
    go_field4->SetData().SetInt(123);
    go_term->SetData().SetFields().push_back(go_field4);
    expected_errors[0]->SetErrMsg("Bad data format for GO term qualifier evidence");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_InconsistentGeneOntologyTermAndId)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetExt().SetType().SetStr("GeneOntology");
    CRef<CUser_field> go_list(new CUser_field());
    go_list->SetLabel().SetStr("Process");
    go_list->SetData().SetFields().push_back(MakeGoTerm("a1", "evidence 1"));
    go_list->SetData().SetFields().push_back(MakeGoTerm("a2", "evidence 2"));
    feat->SetExt().SetData().push_back(go_list);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "InconsistentGeneOntologyTermAndId", 
                              "Inconsistent GO terms for GO ID 123"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MultiplyAnnotatedGenes)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene1 = unit_test_util::AddMiscFeature(entry);
    gene1->SetData().SetGene().SetLocus("gene1");
    CRef<CSeq_feat> gene2 = unit_test_util::AddMiscFeature(entry);
    gene2->SetData().SetGene().SetLocus("gene1");


    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "FeatContentDup", 
                               "Duplicate feature"));
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Info, "MultiplyAnnotatedGenes", 
                              "Colliding names in gene features, but feature locations are identical"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    gene2->SetData().SetGene().SetLocus("GENE1");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "DuplicateFeat",
        "Features have identical intervals, but labels differ"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "MultiplyAnnotatedGenes",
        "Colliding names (with different capitalization) in gene features, but feature locations are identical"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ReplicatedGeneSequence)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene1 = unit_test_util::AddMiscFeature(entry);
    gene1->SetData().SetGene().SetLocus("gene1");
    CRef<CSeq_feat> gene2 = unit_test_util::AddMiscFeature(entry);
    gene2->SetData().SetGene().SetLocus("gene1");
    gene2->SetLocation().SetInt().SetFrom(30);
    gene2->SetLocation().SetInt().SetTo(30 + gene1->GetLocation().GetInt().GetTo());

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Info, "ReplicatedGeneSequence", 
                              "Colliding names in gene features, but underlying sequences are identical"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gene2->SetData().SetGene().SetLocus("GENE1");
    expected_errors[0]->SetErrMsg("Colliding names (with different capitalization) in gene features, but underlying sequences are identical");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_GeneXrefStrandProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetGeneXref().SetLocus("gene locus");
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature (feat);
    gene->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    gene->SetData().SetGene().SetLocus("gene locus");
    unit_test_util::AddFeat(gene, entry);

    STANDARD_SETUP
    expected_errors.push_back (new CExpectedError("lcl|good", eDiag_Warning, "GeneXrefStrandProblem", 
                              "Gene cross-reference is not on expected strand"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    feat->SetGeneXref().ResetLocus();
    feat->SetGeneXref().SetLocus_tag("LOCUSTAG");
    gene->SetData().SetGene().ResetLocus();
    gene->SetData().SetGene().SetLocus_tag("LOCUSTAG");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_CDSmRNAXrefLocationProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetId().SetLocal().SetId(1);
    CRef<CSeqFeatXref> x1(new CSeqFeatXref());
    x1->SetId().SetLocal().SetId(2);
    cds->SetXref().push_back(x1);

    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetId().SetLocal().SetId(2);
    CRef<CSeqFeatXref> x2(new CSeqFeatXref());
    x2->SetId().SetLocal().SetId(1);
    mrna->SetXref().push_back(x2);
    mrna->SetLocation().SetInt().SetTo(mrna->GetLocation().GetInt().GetTo() - 1);
    unit_test_util::AddFeat(mrna, nuc);

    STANDARD_SETUP

    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "CDSmRNAXrefLocationProblem", 
                               "CDS not contained within cross-referenced mRNA"));
    expected_errors.push_back (new CExpectedError("lcl|nuc", eDiag_Warning, "CDSmRNArange",
                               "mRNA overlaps or contains CDS but does not completely contain intervals"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_IdenticalGeneSymbolAndSynonym)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> gene1 (new CSeq_feat());
    gene1->SetData().SetGene().SetLocus("gene1");
    gene1->SetLocation().SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
    gene1->SetLocation().SetInt().SetFrom(0);
    gene1->SetLocation().SetInt().SetTo(3);
    unit_test_util::AddFeat (gene1, entry);

    CRef<CSeq_feat> gene2 (new CSeq_feat());
    gene2->SetData().SetGene().SetLocus("gene2");
    gene2->SetData().SetGene().SetSyn().push_back("gene1");
    gene2->SetLocation().SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
    gene2->SetLocation().SetInt().SetFrom(4);
    gene2->SetLocation().SetInt().SetTo(entry->GetSeq().GetLength() - 1);
    unit_test_util::AddFeat (gene2, entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "IdenticalGeneSymbolAndSynonym",
                              "gene synonym has same value (gene1) as locus of another gene feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_PartialProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> prot = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat> prot_feat = prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    CRef<CSeq_feat> cds_feat = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    
    // make coding region shorter, 5' partial
    cds_feat->SetLocation().SetInt().SetFrom(3);
    cds_feat->SetLocation().SetPartialStart(true, eExtreme_Biological);
    // shorten protein sequence
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("PRKTEIN");
    prot->SetSeq().SetInst().SetLength(7);
    unit_test_util::AdjustProtFeatForNucProtSet (entry);
    // make protein sequence 3' partial
    unit_test_util::SetCompleteness (prot, CMolInfo::eCompleteness_no_right);


    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistentCDSProtein",
                                                 "Coding region and protein feature partials conflict"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemNotConsensus5Prime",
                              "5' partial is not at beginning of sequence, gap, or consensus splice site"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialsInconsistent",
                              "Inconsistent: Product= partial, Location= partial, Feature.partial= FALSE"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblemHasStop", "Got stop codon, but 3'end is labeled partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem", "CDS is 3' complete but protein is CO2 partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblem", "CDS is 5' partial but protein is CO2 partial"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // set partial on CDS, third error should go away
    cds_feat->SetPartial (true);
    delete expected_errors[2];
    expected_errors[2] = NULL;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ProteinNameEndsInBracket)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetNucProtSetProductName (entry, "something [ends with bracket]");

    STANDARD_SETUP
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "ProteinNameEndsInBracket", 
                              "Protein name ends with bracket and may contain organism name"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // report if no beginning bracket
    unit_test_util::SetNucProtSetProductName (entry, "something NAD with bracket]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
    // no report if [NAD

    unit_test_util::SetNucProtSetProductName (entry, "something [NAD with bracket]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ShortIntron)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_id> id = entry->SetSeq().SetId().front();

    // add gene
    CRef<CSeq_feat> gene (new CSeq_feat());
    gene->SetData().SetGene().SetLocus("locus");
    gene->SetLocation().SetInt().SetFrom(0);
    gene->SetLocation().SetInt().SetTo(59);
    gene->SetLocation().SetInt().SetId().Assign(*id);
    unit_test_util::AddFeat(gene, entry);

    // add coding region
    CRef<CSeq_feat> cds (new CSeq_feat());
    cds->SetData().SetCdregion();

    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetFrom(0);
    loc1->SetInt().SetTo(15);
    loc1->SetInt().SetId().Assign(*id);
    
    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetInt().SetFrom(19);
    loc2->SetInt().SetTo(59);
    loc2->SetInt().SetId().Assign(*id);

    cds->SetLocation().SetMix().Set().push_back(loc1);
    cds->SetLocation().SetMix().Set().push_back(loc2);
    unit_test_util::AddFeat(cds, entry);

    // add intron
    CRef<CSeq_feat> intron (new CSeq_feat());
    intron->SetData().SetImp().SetKey("intron");
    intron->SetLocation().SetInt().SetFrom(16);
    intron->SetLocation().SetInt().SetTo(18);
    intron->SetLocation().SetInt().SetId().Assign(*id);
    unit_test_util::AddFeat(intron, entry);

    STANDARD_SETUP

    BOOST_CHECK_EQUAL(validator::HasBadStartCodon(*cds, scope, false), true);
    BOOST_CHECK_EQUAL(validator::HasNoStop(*cds, &scope), true);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "StartCodon",
                              "Illegal start codon used. Wrong genetic code [0] or protein should be partial"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "NoStop",
                              "Missing stop codon"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusDonor",
                              "Splice donor consensus (GT) not found after exon ending at position 16 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor",
                              "Splice acceptor consensus (AG) not found before exon starting at position 20 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingCDSproduct", 
                              "Expected CDS product absent"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ShortIntron",
                              "Introns should be at least 10 nt long"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ShortIntron",
                              "Introns should be at least 10 nt long"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusDonor",
                              "Splice donor consensus (GT) not found at start of intron, position 17 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                              "Splice acceptor consensus (AG) not found at end of intron, position 19 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "NoProtein", "No protein Bioseq given"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // set CDS pseudo, one ShortIntron error should go away
    cds->SetPseudo(true);
    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ShortIntron",
                              "Introns should be at least 10 nt long"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusDonor",
                              "Splice donor consensus (GT) not found at start of intron, position 17 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                              "Splice acceptor consensus (AG) not found at end of intron, position 19 of lcl|good"));


    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // make cds not pseudo, intron pseudo, should still get one ShortIntron error
    cds->ResetPseudo();
    intron->SetPseudo(true);

    BOOST_CHECK_EQUAL(validator::HasBadStartCodon(*cds, scope, false), true);
    BOOST_CHECK_EQUAL(validator::HasNoStop(*cds, &scope), true);

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "StartCodon",
                              "Illegal start codon used. Wrong genetic code [0] or protein should be partial"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "NoStop",
                              "Missing stop codon"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusDonor",
                              "Splice donor consensus (GT) not found after exon ending at position 16 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor",
                              "Splice acceptor consensus (AG) not found before exon starting at position 20 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MissingCDSproduct", 
                              "Expected CDS product absent"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "ShortIntron",
                              "Introns should be at least 10 nt long"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusDonor",
                              "Splice donor consensus (GT) not found at start of intron, position 17 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                              "Splice acceptor consensus (AG) not found at end of intron, position 19 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "NoProtein", "No protein Bioseq given"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // clear both pseudo, make gene pseudo, both errors should go away
    intron->ResetPseudo();
    gene->SetPseudo(true);
    CLEAR_ERRORS

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusDonor",
                              "Splice donor consensus (GT) not found at start of intron, position 17 of lcl|good"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NotSpliceConsensusAcceptor", 
                              "Splice acceptor consensus (AG) not found at end of intron, position 19 of lcl|good"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_NeedsNote)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->ResetComment();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NeedsNote",
                              "A note or other qualifier is required for a misc_feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_RptUnitRangeProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetData().SetImp().SetKey("repeat_region");
    CRef<CGb_qual> qual(new CGb_qual());
    qual->SetQual("rpt_unit_range");
    qual->SetVal("1..70");
    misc->SetQual().push_back(qual);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "RptUnitRangeProblem",
                              "/rpt_unit_range is not within sequence length"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_TooManyInferenceAccessions)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    for (int i = 0; i < 50; i++) {
        CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry, i + 10);
        for (int j = 0; j < 10; j++) {
            CRef<CGb_qual> qual(new CGb_qual());
            qual->SetQual("inference");
            string val = "similar to DNA sequence:";
            for (int k = 0; k < 10; k++) {
                val += "INSD:AY" + NStr::IntToString (k + j * 100 + 123400) + ".1";
                if (k < 9) {
                    val += ",";
                }
            }
            qual->SetVal(val);
            misc->SetQual().push_back(qual);
        }
    }
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "TooManyInferenceAccessions",
                              "Skipping validation of 500 /inference qualifiers with 5000 accessions"));
    eval = validator.Validate(seh, options | CValidator::eVal_inference_accns);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static CRef<CSeq_align> BuildSetAlign(CRef<CSeq_entry> entry)
{
    CRef<CSeq_align> align(new CSeq_align());
    align->SetType(CSeq_align::eType_global);
    align->SetSegs().SetDenseg().SetNumseg(1);

    int dim = 0;
    int len = 0;

    FOR_EACH_SEQENTRY_ON_SEQSET (s, entry->GetSet()) {
        dim++;
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*((*s)->GetSeq().GetId().front()));
        align->SetSegs().SetDenseg().SetIds().push_back(id);
        align->SetSegs().SetDenseg().SetStarts().push_back(0);

        len = (*s)->GetSeq().GetInst().GetLength();
    }
    align->SetDim(dim);
    align->SetSegs().SetDenseg().SetDim(dim);
    align->SetSegs().SetDenseg().SetLens().push_back(len);

    return align;
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_SeqIdProblem)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_annot> annot(new CSeq_annot());
    CRef<CSeq_align> align = BuildSetAlign(entry);
    align->SetSegs().SetDenseg().SetIds().back()->SetLocal().SetStr("good4");
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE


    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "FastaLike", 
      "Fasta: This may be a fasta-like alignment for SeqId: lcl|good1 in the context of good1"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "SeqIdProblem", 
                              "SeqId: The sequence corresponding to SeqId lcl|good4 could not be found."));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "PercentIdentity", 
      "PercentIdentity: This alignment has a percent identity of 0%"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_DensegLenStart)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();

    CRef<CSeq_align> align(new CSeq_align());
    align->SetType(CSeq_align::eType_global);
    align->SetSegs().SetDenseg().SetNumseg(2);

    int dim = 0;

    FOR_EACH_SEQENTRY_ON_SEQSET (s, entry->GetSet()) {
        dim++;
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*((*s)->GetSeq().GetId().front()));
        align->SetSegs().SetDenseg().SetIds().push_back(id);
        align->SetSegs().SetDenseg().SetStarts().push_back(0);
    }
    align->SetDim(dim);
    align->SetSegs().SetDenseg().SetDim(dim);

    align->SetSegs().SetDenseg().SetLens().push_back(5);
    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    align->SetSegs().SetDenseg().SetStarts().push_back(6);
    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    align->SetSegs().SetDenseg().SetLens().push_back(10);
    
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "DensegLenStart", 
             "Start/Length: There is a problem with sequence lcl|good2, in segment 1 (near sequence position 0), context good1: the segment is too long or short or the next segment has an incorrect start position"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_SumLenStart)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_align> align = BuildSetAlign(entry);
    align->SetSegs().SetDenseg().SetNumseg(2);
    align->SetSegs().SetDenseg().SetLens()[0] = 5;
    align->SetSegs().SetDenseg().SetLens().push_back(60);

    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "SumLenStart", 
                  "Start: In sequence lcl|good1, segment 2 (near sequence position 5) context good1, the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "SumLenStart", 
                  "Start: In sequence lcl|good2, segment 2 (near sequence position 5) context good1, the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "SumLenStart", 
                  "Start: In sequence lcl|good3, segment 2 (near sequence position 5) context good1, the alignment claims to contain residue coordinates that are past the end of the sequence.  Either the sequence is too short, or there are extra characters or formatting errors in the alignment"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_AlignDimSeqIdNotMatch)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_align> align = BuildSetAlign(entry);
    align->SetSegs().SetDenseg().SetDim(4);
    
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    SetDiagFilter(eDiagFilter_All, "!(1207.5)");
    STANDARD_SETUP_WITH_DATABASE
    SetDiagFilter(eDiagFilter_All, "");

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "AlignDimSeqIdNotMatch", 
                  "SeqId: The Seqalign has more or fewer ids than the number of rows in the alignment (context good1).  Look for possible formatting errors in the ids."));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "SegsStartsMismatch", 
                  "The number of Starts (3) does not match the expected size of dim * numseg (4)"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_FastaLike)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    unit_test_util::RevComp(entry->SetSet().SetSeq_set().front());
    CRef<CSeq_align> align = BuildSetAlign(entry);
    
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "FastaLike", 
                  "Fasta: This may be a fasta-like alignment for SeqId: lcl|good1 in the context of good1"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "PercentIdentity", 
                  "PercentIdentity: This alignment has a percent identity of 0%"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // fasta like error should disappear if there are 5' gaps or internal gaps
    align->SetSegs().SetDenseg().SetNumseg(2);
    align->SetSegs().SetDenseg().SetLens()[0] = 5;
    align->SetSegs().SetDenseg().SetLens().push_back(55);
    align->SetSegs().SetDenseg().SetStarts()[2] = -1;
    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    align->SetSegs().SetDenseg().SetStarts().push_back(5);

    CLEAR_ERRORS
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "PercentIdentity", 
                  "PercentIdentity: This alignment has a percent identity of 0%"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_NullSegs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_align> align = BuildSetAlign(entry);
    align->ResetSegs();
    
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("", eDiag_Error, "NullSegs", 
                  "Segs: This alignment is missing all segments.  This is a non-correctable error -- look for serious formatting problems."));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_SegmentGap)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_align> align = BuildSetAlign(entry);
    align->SetSegs().SetDenseg().SetNumseg(3);
    align->SetSegs().SetDenseg().SetLens()[0] = 5;
    align->SetSegs().SetDenseg().SetLens().push_back(10);    
    align->SetSegs().SetDenseg().SetLens().push_back(55);    
    align->SetSegs().SetDenseg().SetStarts().push_back(-1);
    align->SetSegs().SetDenseg().SetStarts().push_back(-1);
    align->SetSegs().SetDenseg().SetStarts().push_back(-1);
    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    align->SetSegs().SetDenseg().SetStarts().push_back(5);
    
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "SegmentGap", 
                  "Segs: Segment 2 (near alignment position 5) in the context of good1 contains only gaps.  Each segment must contain at least one actual sequence -- look for columns with all gaps and delete them."));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_AlignDimOne)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_align> align = BuildSetAlign(entry);
    align->SetSegs().SetDenseg().SetDim(1);
    align->SetSegs().SetDenseg().SetIds().pop_back();
    align->SetSegs().SetDenseg().SetIds().pop_back();
    align->SetSegs().SetDenseg().SetStarts().pop_back();
    align->SetSegs().SetDenseg().SetStarts().pop_back();
    
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "AlignDimOne", 
                  "Dim: This seqalign apparently has only one sequence.  Each alignment must have at least two sequences.  context lcl|good1"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_Segtype)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_align> align(new CSeq_align());
    align->SetSegs().SetSparse();

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("", eDiag_Warning, "Segtype", 
                  "Segs: This alignment has an undefined or unsupported Seqalign segtype 7 (alignment number 1)"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    align->SetSegs().SetSpliced();
    expected_errors[0]->SetErrMsg("Segs: This alignment has an undefined or unsupported Seqalign segtype 6 (alignment number 1)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_BlastAligns)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_align> align = BuildSetAlign(entry);

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);

    CRef<CAnnotdesc> ad(new CAnnotdesc());
    ad->SetUser().SetType().SetStr("Blast Type");
    annot->SetDesc().Set().push_back(ad);
    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "BlastAligns", 
                  "Record contains BLAST alignments"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_PercentIdentity)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    entry->SetSet().SetSeq_set().front()->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCTTGGCCAAAATTGGCCAA");
    CRef<CSeq_align> align = BuildSetAlign(entry);

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);

    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "FastaLike", 
      "Fasta: This may be a fasta-like alignment for SeqId: lcl|good1 in the context of good1"));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "PercentIdentity", 
                              "PercentIdentity: This alignment has a percent identity of 43%"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}



static CRef<CSeq_align> BuildSetDendiagAlign(CRef<CSeq_entry> entry)
{
    CRef<CSeq_align> align(new CSeq_align());
    align->SetType(CSeq_align::eType_global);

    CRef<CDense_diag> diag(new CDense_diag());


    int dim = 0;
    int len = 0;

    FOR_EACH_SEQENTRY_ON_SEQSET (s, entry->GetSet()) {
        dim++;
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*((*s)->GetSeq().GetId().front()));
        diag->SetIds().push_back(id);
        diag->SetStarts().push_back(0);

        len = (*s)->GetSeq().GetInst().GetLength();
    }
    align->SetDim(dim);
    diag->SetDim(dim);
    diag->SetLen(len);
    align->SetSegs().SetDendiag().push_back(diag);

    return align;
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ALIGN_UnexpectedAlignmentType)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_align> align = BuildSetDendiagAlign(entry);

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);

    entry->SetSet().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "UnexpectedAlignmentType", 
                              "UnexpectedAlignmentType: This is not a DenseSeg alignment."));
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Warning, "PercentIdentity", 
                              "PercentIdentity: This alignment has a percent identity of 0%"));
    options |= CValidator::eVal_val_align | CValidator::eVal_remote_fetch;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


static CRef<CSeq_graph> BuildGoodByteGraph(CRef<CSeq_entry> entry, TSeqPos offset = 0, TSeqPos len = kInvalidSeqPos)
{
    CRef<CSeq_graph> graph (new CSeq_graph());
    graph->SetTitle("Phrap Quality");
    if (len == kInvalidSeqPos) {
      len = entry->GetSeq().GetInst().GetLength() - offset;
    }
    graph->SetNumval(len);
    graph->SetLoc().SetInt().SetFrom(offset);
    graph->SetLoc().SetInt().SetTo(offset + len - 1);
    graph->SetLoc().SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));

    for (size_t pos = 0; pos < len; pos++) {
        graph->SetGraph().SetByte().SetValues().push_back(40);
    }


    graph->SetGraph().SetByte().SetMax(40);
    graph->SetGraph().SetByte().SetMin(40);
    graph->SetGraph().SetByte().SetAxis(40);
    return graph;
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphMin)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_graph> graph = BuildGoodByteGraph(entry);
    graph->SetGraph().SetByte().SetMin(-1);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(graph);
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphMin", 
                              "Graph min (-1) out of range"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    graph->SetGraph().SetByte().SetMin(101);
    expected_errors[0]->SetErrMsg("Graph min (101) out of range");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphBelow", 
                              "60 quality scores have values below the reported minimum or 0"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphMax)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_graph> graph = BuildGoodByteGraph(entry);
    graph->SetGraph().SetByte().SetMax(-1);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(graph);
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphMax", 
                              "Graph max (-1) out of range"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphAbove", 
                              "60 quality scores have values above the reported maximum or 100"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[1];
    expected_errors.pop_back();

    graph->SetGraph().SetByte().SetMax(101);
    expected_errors[0]->SetErrMsg("Graph max (101) out of range");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphByteLen)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_graph> graph = BuildGoodByteGraph(entry);
    graph->SetNumval(40);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(graph);
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphByteLen", 
                              "SeqGraph (40) and ByteStore (60) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphBioseqLen", 
                              "SeqGraph (40) and Bioseq (60) length mismatch"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphOutOfOrder)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 20, 20));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 20));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 40, 20));
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphOutOfOrder", 
                              "Graph components are out of order - may be a software bug"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphSeqLitLen)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 11));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 22, 12));
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphBioseqLen", 
                              "SeqGraph (23) and Bioseq (24) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphSeqLitLen", 
                              "SeqGraph (11) and SeqLit (12) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphStopPhase", 
                              "SeqGraph (10) and SeqLit (11) stop do not coincide"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphSeqLocLen)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetId().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(11);

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 13));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 22, 12));
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphGapScore", 
                              "1 gap bases have positive score value"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphBioseqLen", 
                              "SeqGraph (25) and Bioseq (24) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphSeqLocLen", 
                              "SeqGraph (13) and SeqLoc (12) length mismatch"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphStartPhase)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 12));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 21, 13));
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphGapScore", 
                              "1 gap bases have positive score value"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphBioseqLen", 
                              "SeqGraph (25) and Bioseq (24) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphSeqLitLen", 
                              "SeqGraph (13) and SeqLit (12) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphStartPhase", 
                              "SeqGraph (21) and SeqLit (22) start do not coincide"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}

// note - GraphStopPhase exercised in Test_SEQ_GRAPH_GraphSeqLitLen


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphDiffNumber)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 6));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 6, 6));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 22, 12));
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphSeqLitLen", 
                              "SeqGraph (6) and SeqLit (12) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphStopPhase", 
                              "SeqGraph (5) and SeqLit (11) stop do not coincide"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphSeqLitLen", 
                              "SeqGraph (6) and SeqLit (12) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphStartPhase", 
                              "SeqGraph (6) and SeqLit (22) start do not coincide"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphStopPhase", 
                              "SeqGraph (11) and SeqLit (33) stop do not coincide"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphDiffNumber", 
                              "Different number of SeqGraph (3) and SeqLit (2) components"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphACGTScore)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 12));
    CRef<CSeq_graph> graph = BuildGoodByteGraph(entry, 22, 12);
    graph->SetGraph().SetByte().SetValues().pop_back();
    graph->SetGraph().SetByte().SetValues().push_back(0);
    graph->SetGraph().SetByte().SetMin(0);
    annot->SetData().SetGraph().push_back(graph);

    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphACGTScore", 
                              "1 ACGT bases have zero score value - first one at position 34"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphNScore)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetSeq_data().SetIupacna().Set("CCCATNATGATG");

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 12));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 22, 12));

    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphNScore", 
                              "1 N bases have positive score value - first one at position 28"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphGapScore)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 12));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 12, 10));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 22, 12));

    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphGapScore", 
                              "10 gap bases have positive score value"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphSeqLitLen", 
                              "SeqGraph (10) and SeqLit (12) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphStartPhase", 
                              "SeqGraph (12) and SeqLit (22) start do not coincide"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphStopPhase", 
                              "SeqGraph (21) and SeqLit (33) stop do not coincide"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphDiffNumber", 
                              "Different number of SeqGraph (3) and SeqLit (2) components"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphOverlap)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 31));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 30, 30));

    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphOverlap", 
                              "Graph components overlap, with multiple scores for a single base"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphBioseqLen", 
                              "SeqGraph (61) and Bioseq (60) length mismatch"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphBioseqId)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_annot> annot(new CSeq_annot());
    CRef<CSeq_graph> graph = BuildGoodByteGraph(entry);
    graph->SetLoc().SetInt().SetId().SetLocal().SetStr("good2");
    annot->SetData().SetGraph().push_back(graph);
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good2", eDiag_Warning, "GraphBioseqId", 
                              "Bioseq not found for Graph location good2"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "GraphPackagingProblem",
                              "There is 1 mispackaged graph in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphACGTScoreMany)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 12));
    CRef<CSeq_graph> graph = BuildGoodByteGraph(entry, 22, 12);
    graph->SetGraph().SetByte().ResetValues();
    for (size_t i = 0; i < graph->GetNumval(); i++) {
        graph->SetGraph().SetByte().SetValues().push_back(0);
    }
    graph->SetGraph().SetByte().SetMin(0);
    annot->SetData().SetGraph().push_back(graph);

    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphACGTScoreMany", 
                              "12 ACGT bases (50.00%) have zero score value - first one at position 23"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphNScoreMany)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetSeq_data().SetIupacna().Set("ANNNNNNTGATG");

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 0, 12));
    annot->SetData().SetGraph().push_back(BuildGoodByteGraph(entry, 22, 12));

    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphNScoreMany", 
                              "6 N bases (25.00%) have positive score value - first one at position 24"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the first 10 bases or more than 15 Ns in the first 50 bases"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "HighNContentPercent",
        "Sequence has more than 5 Ns in the last 10 bases or more than 15 Ns in the last 50 bases"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

#if 0

    scope.RemoveTopLevelSeqEntry(seh);
    CSeq_literal& first_part = entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLiteral();
    first_part.SetSeq_data().SetIupacna().Set("AAAAAAAAAAAAAAAAAAAANNNNNNNNNNNNNNNNNNNNTTTTTTTTTTTTTTTTTTTT");
    first_part.SetLength(60);
    entry->SetSeq().SetInst().SetLength(82);
    entry->SetSeq().ResetAnnot();
    CRef<CSeq_graph> bad_graph = BuildGoodByteGraph(entry, 0, 79);
    CSeq_graph_Base::C_Graph::TByte& bytes = bad_graph->SetGraph().SetByte();
    bytes.ResetValues();
    for (size_t pos = 0; pos < 20; pos++) {
        bytes.SetValues().push_back(0);
    }
    for (size_t pos = 20; pos < 40; pos++) {
        bytes.SetValues().push_back(114);
    }
    for (size_t pos = 40; pos < 70; pos++) {
        bytes.SetValues().push_back(21);
    }
    bytes.SetMax(-1);
    bytes.SetMin(0);
    bytes.SetAxis(5);
    CRef<CSeq_annot> annot2(new CSeq_annot());
    annot2->SetData().SetGraph().push_back(bad_graph);
    entry->SetSeq().SetAnnot().push_back(annot2);

    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphBioseqLen", "SeqGraph(79) and Bioseq(72) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphMax", "Graph max(-1) out of range"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphByteLen", "SeqGraph(79) and ByteStore(70) length mismatch"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphACGTScoreMany", "23 ACGT bases(29.11%) have zero score value - first one at position 1"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphNScoreMany", "20 N bases(25.32%) have positive score value - first one at position 21"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphGapScore", "10 gap bases have positive score value"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "GraphAbove", "79 quality scores have values above the reported maximum or 100"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
#endif
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphLocInvalid_1)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_annot> annot(new CSeq_annot());
    CRef<CSeq_graph> graph = BuildGoodByteGraph(entry);
    graph->SetLoc().SetInt().SetTo(61);
    annot->SetData().SetGraph().push_back(graph);
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "GraphLocInvalid", 
                           "SeqGraph location (lcl|good:1-62) is invalid"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_GRAPH_GraphLocInvalid_2)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_annot> annot(new CSeq_annot());
    CRef<CSeq_graph> graph = BuildGoodByteGraph(entry);
    graph->ResetLoc();
    annot->SetData().SetGraph().push_back(graph);
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("", eDiag_Error, "GraphLocInvalid", 
                           "SeqGraph location (Unknown) is invalid"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "GraphPackagingProblem",
                              "There is 1 mispackaged graph in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ANNOT_AnnotIDs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetIds();
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "AnnotIDs", 
                              "Record contains Seq-annot.data.ids"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_ANNOT_AnnotLOCs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetLocs();
    entry->SetSeq().SetAnnot().push_back(annot);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "AnnotLOCs", 
                              "Record contains Seq-annot.data.locs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_WrongQualOnCDS)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CGb_qual> qual(new CGb_qual("gene_synonym", "anything"));
    cds->SetQual().push_back(qual);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "WrongQualOnCDS", 
                              "gene_synonym should not be a gbqual on a CDS feature"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FixLatLonFormat)
{
    string to_fix;
    string fixed;


    bool format_correct;
    bool precision_correct;
    bool lat_in_range;
    bool lon_in_range;
    double lat_value;
    double lon_value;

    CSubSource::IsCorrectLatLonFormat ("53.43.20 N 7.43.20 E", format_correct, precision_correct,
                                     lat_in_range, lon_in_range,
                                     lat_value, lon_value);
    BOOST_CHECK(!format_correct);

}


BOOST_AUTO_TEST_CASE(Test_FixLatLonCountry)
{
    string latlon;
    string country;
    string error;
    CSubSource::ELatLonCountryErr errcode;

    latlon = "35 N 80 E";
    country = "USA";
    error = CSubSource::ValidateLatLonCountry(country, latlon, false, errcode);
    BOOST_CHECK_EQUAL(errcode, CSubSource::eLatLonCountryErr_Value);
    BOOST_CHECK_EQUAL(error, "Longitude should be set to W (western hemisphere)");
    BOOST_CHECK_EQUAL(latlon, "35.00 N 80.00 W");
    
    latlon = "25 N 47 E";
    country = "Madagascar";
    error = CSubSource::ValidateLatLonCountry(country, latlon, false, errcode);
    BOOST_CHECK_EQUAL(errcode, CSubSource::eLatLonCountryErr_Value);
    BOOST_CHECK_EQUAL(error, "Latitude should be set to S (southern hemisphere)");
    BOOST_CHECK_EQUAL(latlon, "25.00 S 47.00 E");

    latlon = "15 N 47 E";
    country = "Austria";
    error = CSubSource::ValidateLatLonCountry(country, latlon, false, errcode);
    BOOST_CHECK_EQUAL(errcode, CSubSource::eLatLonCountryErr_Value);
    BOOST_CHECK_EQUAL(error, "Latitude and longitude values appear to be exchanged");
    BOOST_CHECK_EQUAL(latlon, "47.00 N 15.00 E");

}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ShortExon)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet ();
    CRef<CSeq_entry> nseq = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> pseq = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat>  cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat>  prot = pseq->SetSeq().SetAnnot().front()->SetData().SetFtable().front();

    string start = "ATG";
    string stop = "TAA";
    string splice_left = "GT";
    string splice_right = "AG";
    string fifteen = "CCCAGAAAAACAGGT";

    string first_exon = start + fifteen;
    string intron = splice_left + fifteen + splice_right;
    string second_exon = fifteen;
    string third_exon = fifteen + stop;

    string nuc_str = first_exon + intron + second_exon + intron + third_exon; 
    nseq->SetSeq().SetInst().SetSeq_data().SetIupacna().Set(nuc_str);
    nseq->SetSeq().SetInst().SetLength(nuc_str.length());

    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetId().SetLocal().SetStr("nuc");
    loc1->SetInt().SetFrom(0);
    TSeqPos offset = first_exon.length();
    loc1->SetInt().SetTo(offset - 1);

    offset += intron.length();
    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetInt().SetId().SetLocal().SetStr("nuc");
    loc2->SetInt().SetFrom(offset);
    offset += second_exon.length();
    loc2->SetInt().SetTo(offset - 1);


    offset += intron.length();
    CRef<CSeq_loc> loc3(new CSeq_loc());
    loc3->SetInt().SetId().SetLocal().SetStr("nuc");
    loc3->SetInt().SetFrom(offset);
    offset += third_exon.length();
    loc3->SetInt().SetTo(offset - 1);

    cds->SetLocation().SetMix().Set().push_back(loc1);
    cds->SetLocation().SetMix().Set().push_back(loc2);
    cds->SetLocation().SetMix().Set().push_back(loc3);

    string loc_str = first_exon + second_exon + third_exon;
    string prot_str = "";
    CSeqTranslator::Translate(loc_str, prot_str);
    if (NStr::EndsWith(prot_str, "*")) {
        prot_str = prot_str.substr(0, prot_str.length() - 1);
    }
    pseq->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set(prot_str);
    pseq->SetSeq().SetInst().SetLength(prot_str.length());

    prot->SetLocation().SetInt().SetTo(prot_str.length() - 1);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "ShortExon", 
                              "Internal coding region exon is too short"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ExtraProteinFeature)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet ();
    CRef<CSeq_entry> pseq = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat> second_prot = AddProtFeat(pseq);
    second_prot->SetData().SetProt().SetName().front() = "different name";
    second_prot->SetLocation().SetInt().SetFrom(1);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "ExtraProteinFeature", 
                              "Protein sequence has multiple unprocessed protein features"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "ExtraProteinFeature", 
                              "Protein sequence has multiple unprocessed protein features"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_FixFormatDate)
{
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("1000"), "");
    BOOST_CHECK_EQUAL(CSubSource::GetCollectionDateProblem("1000"), "Collection_date format is not in DD-Mmm-YYYY format");

    //ISO dates are fine as they are
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2014-08-10T12:23:30Z"), "2014-08-10T12:23:30Z");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2014-08-10T12:23Z"), "2014-08-10T12:23Z");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2014-08-10T12Z"), "2014-08-10T12Z");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2014-08-10T12+00:00"), "2014-08-10T12+00:00");

    bool bad_format = false;
    bool in_future = false;
    CSubSource::IsCorrectDateFormat("collection date: Nov-2010 and Dec-2012", bad_format, in_future);
    BOOST_CHECK_EQUAL(true, bad_format);
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("collection date: Nov-2010 and Dec-2012"), "");

    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("20-12-2014"), "20-Dec-2014");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Dec-12-2014"), "12-Dec-2014");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Sep-11"), "Sep-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("missing"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("n/a"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("10-Apr-93"), "10-Apr-1993");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("1-Apr"), "Apr-2001");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("#Date"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("05122011"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("08-Mar"), "Mar-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("08022011"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("1-May"), "May-2001");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("10-Apr"), "Apr-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("10-Dec"), "Dec-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("10-May"), "May-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("10-Nov"), "Nov-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("10022011"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("10082010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("1082009"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("11-Sep"), "Sep-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("12-Apr"), "Apr-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("12-Aug"), "Aug-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("12-Dec"), "Dec-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("12-Feb"), "Feb-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("12-Jun"), "Jun-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("12-Nov"), "Nov-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("12-Oct"), "Oct-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("13072010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("14-Apr-97"), "14-Apr-1997");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("14092010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("14122011"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("15/05/98"), "15-May-1998");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("15072010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("15082010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("17-Mar-96"), "17-Mar-1996");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("17062011"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("19-Jul-99"), "19-Jul-1999");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("19-Sep-97"), "19-Sep-1997");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("19012012"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2-Aug"), "Aug-2002");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2-Jan-98"), "02-Jan-1998");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("20-Jun-91"), "20-Jun-1991");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("20009-04-14"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("20072010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("20082010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("20090415"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("202008-01-26"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("202008-01-27"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("202008-08-25"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("21-Mar-96"), "21-Mar-1996");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2209"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("23-Oct-94"), "23-Oct-1994");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("25-Apr-20010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("25-Jun-99"), "25-Jun-1999");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("25012012"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("26-Apr-20010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("26-Feb-51"), "26-Feb-1951");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("27072010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("29-Apr-20010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("29-May-98"), "29-May-1998");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("29-Sep-94"), "29-Sep-1994");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("3-Jan"), "Jan-2003");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("3-Mar-93"), "03-Mar-1993");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("3082010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("31082010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39259"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39517"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39681"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39762"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39846"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39855"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39873"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39898"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39903"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39910"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39917"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39926"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39980"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("39982"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("4-Feb"), "Feb-2004");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40010"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40035"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40057"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40070"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40087"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40093"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40313"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40359"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40360"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40361"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40367"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40368"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40370"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40379"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40428"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("40995"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Oct-20006"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Sep"), "Sep-2006");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("7-Dec"), "Dec-2007");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("8-Jul"), "Jul-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("8-Sep-99"), "08-Sep-1999");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("9-Jul"), "Jul-2009");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("9-Jul-84"), "09-Jul-1984");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("9-Jun"), "Jun-2009");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("9-Sep"), "Sep-2009");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Apr-01"), "Apr-2001");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Apr-10"), "Apr-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Aug-05"), "Aug-2005");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Aug-08"), "Aug-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Aug-12"), "Aug-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("August 13"), "Aug-2013");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("August 27"), "Aug-1927");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Dec-05"), "Dec-2005");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Dec-12"), "Dec-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Dec-98"), "Dec-1998");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Feb-12"), "Feb-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Feb-13"), "Feb-2013");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jan-06"), "Jan-2006");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jan-13"), "Jan-2013");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jan-96"), "Jan-1996");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jul-04"), "Jul-2004");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jul-08"), "Jul-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("July 23"), "Jul-1923");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("July 9"), "Jul-2009");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jun-05"), "Jun-2005");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jun-08"), "Jun-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jun-10"), "Jun-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jun-12"), "Jun-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jun-67"), "Jun-1967");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jun-80"), "Jun-1980");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("June 11"), "Jun-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("June 25"), "Jun-1925");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Mar-02"), "Mar-2002");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Mar-05"), "Mar-2005");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Mar-09"), "Mar-2009");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Mar-10"), "Mar-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Mar-11"), "Mar-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Mar-12"), "Mar-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("May 21"), "May-1921");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("May 7"), "May-2007");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("May-05"), "May-2005");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("May-08"), "May-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("May-09"), "May-2009");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("May-10"), "May-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("May-11"), "May-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Nov-10"), "Nov-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Nov-11"), "Nov-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Oct-05"), "Oct-2005");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Oct-10"), "Oct-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("October 8"), "Oct-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Sep-05"), "Sep-2005");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Sep-08"), "Sep-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Sep-09"), "Sep-2009");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Sep-12"), "Sep-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Sep-93"), "Sep-1993");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("September 10"), "Sep-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("September 24"), "Sep-1924");
    // fix leading/trailing spaces
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat(" 2010-03-01"), "2010-03-01");

    // ISO Format dates are not ambiguous
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2010-03-01"), "2010-03-01");

    // if one token is NOT zero-padded and less than 10, and the other is either
    // 10 or more or IS zero-padded, then the token that is not padded and less 
    // than 10 is the day, and the other is the year, to which we should add 2000
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Apr-04"), "06-Apr-2004");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Aug-09"), "06-Aug-2009");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Feb-08"), "06-Feb-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Jan-11"), "06-Jan-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Jun-11"), "06-Jun-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Jun-12"), "06-Jun-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-May-03"), "06-May-2003");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Nov-08"), "06-Nov-2008");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6-Oct-09"), "06-Oct-2009");


    // check for days not in month
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("31-Jun-2013"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("29-Feb-2013"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("29-Feb-2012"), "29-Feb-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("29-Feb-2000"), "29-Feb-2000");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("29-Feb-1900"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("01/01/1900"), "01-Jan-1900");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("04/04/2013"), "04-Apr-2013");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("11/11/2003"), "11-Nov-2003");

    // look for "named numbers"
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("6th July 2010"), "06-Jul-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("February 24th, 2012"), "24-Feb-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("1st December 2012"), "01-Dec-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2nd December 2012"), "02-Dec-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("3rd December 2012"), "03-Dec-2012");

    // unusual delimiters
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("July-15_2011"), "15-Jul-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("03-Aug=2011"), "03-Aug-2011");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Jul=2010"), "Jul-2010");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("30.12.1998"), "30-Dec-1998");

}


BOOST_AUTO_TEST_CASE(Test_DetectDateFormat)
{
    bool ambiguous;
    bool day_first;

    CSubSource::DetectDateFormat("1-1-2010", ambiguous, day_first);
    BOOST_CHECK_EQUAL(ambiguous, true);

    CSubSource::DetectDateFormat("1-6-2010", ambiguous, day_first);
    BOOST_CHECK_EQUAL(ambiguous, true);

    CSubSource::DetectDateFormat("7-15-2010", ambiguous, day_first);
    BOOST_CHECK_EQUAL(ambiguous, false);
    BOOST_CHECK_EQUAL(day_first, false);

    CSubSource::DetectDateFormat("2010 8 24", ambiguous, day_first);
    BOOST_CHECK_EQUAL(ambiguous, false);
    BOOST_CHECK_EQUAL(day_first, false);

    CSubSource::DetectDateFormat("31-5-2008", ambiguous, day_first);
    BOOST_CHECK_EQUAL(ambiguous, false);
    BOOST_CHECK_EQUAL(day_first, true);

}


BOOST_AUTO_TEST_CASE(Test_NewFixCountry)
{
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("United States"), "USA");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Russia, Tatarstan, Kazan"), "Russia: Tatarstan, Kazan");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Egypt: Red Sea, Ras Mohamed, Sinai"), "Egypt: Red Sea, Ras Mohamed, Sinai");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Kenya."), "Kenya");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("U.S.A."), "USA");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("\"United Kingdom: Scotland, Edinburgh\""), "United Kingdom: Scotland, Edinburgh");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("1896"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Anderson, Mesa Verde, Colorado"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Ansirabe"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Antarctic Territory Claimed by Australia"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Ari Ksatr"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Australia: south-western australia"), "Australia: south-western australia");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Auwahi, Maui"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Between Liberia and Ivory Coast"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Caroline Island, Leticia"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Catalina Island, California"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Chia-i"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Congo"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Cousin Island"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Czechoslovakia"), "Czechoslovakia");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("DE"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("France: North East France Nievre-Morvan Breuil Chenue forest"), "France: North East France Nievre-Morvan Breuil Chenue forest");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Great Britain"), "United Kingdom: Great Britain");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Greenland: Saqqaq Culture site Qeqertasussuk, north-western Greenland"), "Greenland: Saqqaq Culture site Qeqertasussuk, north-western Greenland");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Guadaloupe Island"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Hawaii"), "USA: Hawaii");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Hamoa Bay, Maui, Hawaii, USA"), "USA: Hamoa Bay, Maui, Hawaii");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Hortus Leiden, the Netherlands"), "Netherlands: Hortus Leiden");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Hortus, Leiden, the Netherlands"), "Netherlands: Hortus, Leiden");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Joffreville"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Korea"), "Korea");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Kuala Belalong, Ulu Temburong National Park"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Lake Fryxell"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Luxemburg"), "Luxembourg");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Mediterranean Sea, Spain"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Mexico. Loreto Bay, Gulf of California."), "Mexico: Loreto Bay, Gulf of California");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Meyendel, the Netherlands"), "Netherlands: Meyendel");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Mount St. Helena, California"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Nanyuki"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Netherland"), "Netherlands");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("New Guinea"), "Papua New Guinea");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("North Sea, Netherlands"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Noumea"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Roosendaal, De Moeren, the Netherlands"), "Netherlands: Roosendaal, De Moeren");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("SPAIN (orig)"), "Spain: (orig)");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("San Tome and Principe Island (1998)"), "Sao Tome and Principe: (1998)");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Scotland"), "United Kingdom: Scotland");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("USA (orig)"), "USA: (orig)");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("USA: Boqueron National Wildlife Refuge, Puerto Rico"), "USA: Boqueron National Wildlife Refuge, Puerto Rico");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("USA: hypersaline sediment collected at Bitter Lake, New Mexico"), "USA: hypersaline sediment collected at Bitter Lake, New Mexico");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Wales"), "United Kingdom: Wales");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("West Germany"), "Germany: West Germany");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("West Lobe Bonney"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Wissenkerke, Keihoogteweg, the Netherlands"), "Netherlands: Wissenkerke, Keihoogteweg");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Wolfskill Orchand, Winters, California"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("Yun Shui"), "");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("USSR: Kazakhstan, Kurtu"), "USSR: Kazakhstan, Kurtu");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("USA:"), "USA");
    BOOST_CHECK_EQUAL(CCountries::NewFixCountry("south sudan"), "South Sudan");

}


BOOST_AUTO_TEST_CASE(Fix_Structured_Voucher)
{
    //removed while issues with updating list are sorted out
    string val = "USNM<USA>:12345";
    COrgMod::FixStructuredVoucher(val, "s");
    BOOST_CHECK_EQUAL(val, "USNM<USA>:12345");

    // can't fix, needs country code
    val = "ABS<CHN>:12345";
    COrgMod::FixStructuredVoucher(val, "s");
    BOOST_CHECK_EQUAL(val, "ABS<CHN>:12345");

    // removed while structure-fixing questions are considered
    // add structure when space instead of colon
    val = "AMNH 12345";
    BOOST_CHECK_EQUAL(COrgMod::FixStructuredVoucher(val, "s"), true);
    BOOST_CHECK_EQUAL(val, "AMNH:12345");

    // add structure when letters and numbers
    val = "ABB666";
    BOOST_CHECK_EQUAL(COrgMod::FixStructuredVoucher(val, "c"), true);
    BOOST_CHECK_EQUAL(val, "ABB:666");

    // can also fix biomaterial
    val = "CNWGRGL123";
    BOOST_CHECK_EQUAL(COrgMod::FixStructuredVoucher(val, "b"), true);
    BOOST_CHECK_EQUAL(val, "CNWGRGL:123");

    // will not fix for too short code
    val = "A12345";
    BOOST_CHECK_EQUAL(COrgMod::FixStructuredVoucher(val, "s"), false);
    BOOST_CHECK_EQUAL(val, "A12345");


    // if institution code in parentheses at end of unstructured value, reorder
    // GB-6454
    val = "M.Riewe 182 (CAS)";
    BOOST_CHECK_EQUAL(COrgMod::FixStructuredVoucher(val, "s"), true);
    BOOST_CHECK_EQUAL(val, "CAS:M.Riewe 182");

    // don't fix if value in parentheses is not an institution code
    val = "L.R. Xu 0081 (WUG)";
    BOOST_CHECK_EQUAL(COrgMod::FixStructuredVoucher(val, "s"), false);
    BOOST_CHECK_EQUAL(val, "L.R. Xu 0081 (WUG)");


}


BOOST_AUTO_TEST_CASE(Test_CheckEnds)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("NNNNNNNNNNAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCCAANNNNNNNNNN");
    entry->SetSeq().SetInst().SetLength(62);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    
    EBioseqEndIsType begin_n = eBioseqEndIsType_None;
    EBioseqEndIsType begin_gap = eBioseqEndIsType_None;
    EBioseqEndIsType end_n = eBioseqEndIsType_None;
    EBioseqEndIsType end_gap = eBioseqEndIsType_None;
    bool begin_ambig = false, end_ambig = false;

    CBioseq_Handle bsh = seh.GetSeq();
    CheckBioseqEndsForNAndGap(bsh, begin_n, begin_gap, end_n, end_gap, begin_ambig, end_ambig);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_All);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_All);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_All);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_All);
    BOOST_CHECK_EQUAL(begin_ambig, true);
    BOOST_CHECK_EQUAL(end_ambig, true);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("NNNNNNNNNAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCCAANNNNNNNNN");
    entry->SetSeq().SetInst().SetLength(60);
    seh = scope.AddTopLevelSeqEntry(*entry);

    bsh = seh.GetSeq();
    CheckBioseqEndsForNAndGap(bsh, begin_n, begin_gap, end_n, end_gap, begin_ambig, end_ambig);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_Last);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_Last);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_Last);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_Last);
    BOOST_CHECK_EQUAL(begin_ambig, true);
    BOOST_CHECK_EQUAL(end_ambig, true);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCCAA");
    entry->SetSeq().SetInst().SetLength(42);
    seh = scope.AddTopLevelSeqEntry(*entry);

    bsh = seh.GetSeq();
    CheckBioseqEndsForNAndGap(bsh, begin_n, begin_gap, end_n, end_gap, begin_ambig, end_ambig);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(begin_ambig, false);
    BOOST_CHECK_EQUAL(end_ambig, false);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ANANTNNNCAAAATTGGCCAAAATTGGCCAAAANTNNCNCNA");
    entry->SetSeq().SetInst().SetLength(42);
    seh = scope.AddTopLevelSeqEntry(*entry);

    bsh = seh.GetSeq();
    CheckBioseqEndsForNAndGap(bsh, begin_n, begin_gap, end_n, end_gap, begin_ambig, end_ambig);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(begin_ambig, true);
    BOOST_CHECK_EQUAL(end_ambig, true);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("GTGTGANANTNNNCNNNNNTGGCCAAAATTGGCCAAAANTNNCNCNAGTGTG");
    entry->SetSeq().SetInst().SetLength(52);
    seh = scope.AddTopLevelSeqEntry(*entry);

    bsh = seh.GetSeq();
    CheckBioseqEndsForNAndGap(bsh, begin_n, begin_gap, end_n, end_gap, begin_ambig, end_ambig);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(begin_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(end_n, eBioseqEndIsType_None);
    BOOST_CHECK_EQUAL(begin_ambig, true);
    BOOST_CHECK_EQUAL(end_ambig, true);

}


BOOST_AUTO_TEST_CASE(Test_SQD_313)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Jatropha cf.");

    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SQD_292)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeqdesc> create_date(new CSeqdesc());
    create_date->SetCreate_date().SetStd().SetMonth(6);
    create_date->SetCreate_date().SetStd().SetDay(12);
    create_date->SetCreate_date().SetStd().SetYear(1998);
    entry->SetSet().SetDescr().Set().push_back(create_date);
    CRef<CSeqdesc> update_date(new CSeqdesc());
    update_date->SetUpdate_date().SetStd().SetMonth(6);
    update_date->SetUpdate_date().SetStd().SetDay(11);
    update_date->SetUpdate_date().SetStd().SetYear(1998);
    entry->SetSet().SetDescr().Set().push_back(update_date);

    CRef<CSeq_entry> nuc = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_id> gi_id(new CSeq_id());
    gi_id->SetGi(GI_CONST(1322283));
    nuc->SetSeq().SetId().push_front(gi_id);
    CRef<CSeq_id> accv_id(new CSeq_id("gb|U54469.1"));
    nuc->SetSeq().SetId().push_front (accv_id);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gb|U54469.1|", eDiag_Warning, "InconsistentDates",
                              "Inconsistent create_date [Jun 12, 1998] and update_date [Jun 11, 1998]"));
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Warning, "InconsistentDates",
                              "Inconsistent create_date [Jun 12, 1998] and update_date [Jun 11, 1998]"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SQD_1470)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    edit::CGenomeAssemblyComment gac1;
    gac1.SetAssemblyMethodProgram("a");
    gac1.SetAssemblyMethodVersion("1");
    gac1.SetGenomeCoverage("3x");
    gac1.SetSequencingTechnology("foo");

    CRef<CSeqdesc> sd1(new CSeqdesc());
    sd1->SetUser(*(gac1.MakeUserObject()));
    entry->SetSeq().SetDescr().Set().push_back(sd1);

    CRef<CSeqdesc> sd2(new CSeqdesc());
    sd2->SetUser(*(gac1.MakeUserObject()));
    entry->SetSeq().SetDescr().Set().push_back(sd2);
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "MultipleComments",
                              "Multiple structured comments with prefix ##Genome-Assembly-Data-START##"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SQD_1309)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::RevComp(entry);    
    CRef<CSeq_entry> nentry = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    SetTech(nentry, CMolInfo::eTech_tsa);
    unit_test_util::SetBiomol (nentry, CMolInfo::eBiomol_transcribed_RNA);
    nentry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "CDSonMinusStrandTranscribedRNA",
                              "Coding region on TSA transcribed RNA should not be on the minus strand"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    eval = validator.GetTSACDSOnMinusStrandErrors(seh);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadComment)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nentry = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);

    STANDARD_SETUP
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetComment("ambiguity in stop codon");

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "BadComment",
                              "Feature comment indicates ambiguity in stop codon but no ambiguities are present in stop codon."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    edit::AddTerminalCodeBreak(*cds, seh.GetScope());
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    nentry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTNAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    seh = scope.AddTopLevelSeqEntry(*entry);

// Error below is not expected anymore since VR-110 issue fixed:
//
//    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "UnnecessaryTranslExcept",
//                              "Unexpected transl_except * at position 9 just past end of protein"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_InvalidFuzz)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc1 = unit_test_util::AddMiscFeature(entry);
    misc1->SetLocation().SetInt().SetFuzz_from().SetLim(CInt_fuzz::eLim_tl);
    misc1->SetLocation().SetInt().SetFuzz_to().SetLim(CInt_fuzz::eLim_tl);

    CRef<CSeq_feat> misc2 = unit_test_util::AddMiscFeature(entry);
    misc2->SetLocation().SetInt().SetFrom(5);
    misc2->SetLocation().SetInt().SetFuzz_from().SetLim(CInt_fuzz::eLim_tr);
    misc2->SetLocation().SetInt().SetFuzz_to().SetLim(CInt_fuzz::eLim_tr);

    CRef<CSeq_feat> misc3 = unit_test_util::AddMiscFeature(entry);
    CRef<CSeq_id> id(new CSeq_id());
    id->Assign(*(entry->GetSeq().GetId().front()));
    CRef<CSeq_interval> int1(new CSeq_interval(*id, 0, 5));
    int1->SetFuzz_from().SetLim(CInt_fuzz::eLim_tl);
    int1->SetFuzz_to().SetLim(CInt_fuzz::eLim_tl);
    CRef<CSeq_interval> int2(new CSeq_interval(*id, 10, 15));
    int2->SetFuzz_from().SetLim(CInt_fuzz::eLim_tr);
    int2->SetFuzz_to().SetLim(CInt_fuzz::eLim_tr);

    misc3->SetLocation().SetPacked_int().Set().push_back(int1);
    misc3->SetLocation().SetPacked_int().Set().push_back(int2);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
                                "Should not specify 'space to left' for both ends of interval"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
                                "Should not specify 'space to right' for both ends of interval"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
        "Should not specify 'space to left' for both ends of interval"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
        "Should not specify 'space to right' for both ends of interval"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
        "Should not specify 'space to left' at first position of non-circular sequence"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
        "Should not specify 'space to left' at first position of non-circular sequence"));

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SQD_1532)
{
    BOOST_CHECK_EQUAL(COrgMod::IsCultureCollectionValid("50% TSB + 2mM Cr(VI)"), "Culture_collection should be structured, but is not");
}


BOOST_AUTO_TEST_CASE(Test_SexQualifiers)
{
    BOOST_CHECK_EQUAL(CSubSource::IsValidSexQualifierValue("M"), true);
    BOOST_CHECK_EQUAL(CSubSource::FixSexQualifierValue("M"), "male");
    BOOST_CHECK_EQUAL(CSubSource::IsValidSexQualifierValue("Male"), true);
    BOOST_CHECK_EQUAL(CSubSource::IsValidSexQualifierValue("male"), true);
    BOOST_CHECK_EQUAL(CSubSource::IsValidSexQualifierValue("llama"), false);
    BOOST_CHECK_EQUAL(CSubSource::IsValidSexQualifierValue("m/f"), true);
    BOOST_CHECK_EQUAL(CSubSource::IsValidSexQualifierValue("pooled males and females"), true);
    BOOST_CHECK_EQUAL(CSubSource::IsValidSexQualifierValue("pooled male and female"), true);
    BOOST_CHECK_EQUAL(CSubSource::IsValidSexQualifierValue("mixed"), true);

    BOOST_CHECK_EQUAL(CSubSource::FixSexQualifierValue("m/f"), "male and female");
    BOOST_CHECK_EQUAL(CSubSource::FixSexQualifierValue("m/f/neuter"), "male, female, and neuter");
    BOOST_CHECK_EQUAL(CSubSource::FixSexQualifierValue("male and female (pooled)"), "pooled male and female");

}


BOOST_AUTO_TEST_CASE(TEST_DisableStrainForwarding)
{
    CBioSource src;

    src.SetDisableStrainForwarding(true);
    BOOST_CHECK_EQUAL(src.GetOrg().GetOrgname().GetAttrib(), "nomodforward");
    BOOST_CHECK_EQUAL(src.GetDisableStrainForwarding(), true);
    src.SetDisableStrainForwarding(false);
    BOOST_CHECK_EQUAL(src.GetDisableStrainForwarding(), false);
}


BOOST_AUTO_TEST_CASE(Test_AllNs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("NNNNNNNNNNNNNNNNNNNNNNNNNNNNNN");
    entry->SetSeq().SetInst().SetLength(30);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Critical, "AllNs",
                              "Sequence is all Ns"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_SubSourceAutofix)
{
    CRef<CSubSource> ss(new CSubSource());

    ss->SetSubtype(CSubSource::eSubtype_country);
    ss->SetName("Maryland, USA");
    ss->AutoFix();
    BOOST_CHECK_EQUAL(ss->GetName(), "USA: Maryland");

    ss->SetSubtype(CSubSource::eSubtype_collection_date);
    ss->SetName("1-14-97");
    ss->AutoFix();
    BOOST_CHECK_EQUAL(ss->GetName(), "14-Jan-1997");

    ss->SetSubtype(CSubSource::eSubtype_lat_lon);
    ss->SetName("Lattitude: 25.790544; longitude: -80.214930");
    ss->AutoFix();
    BOOST_CHECK_EQUAL(ss->GetName(), "25.790544 N 80.214930 W");

    ss->SetSubtype(CSubSource::eSubtype_sex);
    ss->SetName("m/f/neuter");
    ss->AutoFix();
    BOOST_CHECK_EQUAL(ss->GetName(), "male, female, and neuter");

    ss->SetSubtype(CSubSource::eSubtype_altitude);
    ss->SetName("123 ft.");
    ss->AutoFix();
    BOOST_CHECK_EQUAL(ss->GetName(), "37 m");

}


BOOST_AUTO_TEST_CASE(Test_OrgModAutofix)
{
    CRef<COrgMod> om(new COrgMod());
    om->SetSubtype(COrgMod::eSubtype_strain);
    om->SetSubname("ATCC1234");
    om->AutoFix();
    BOOST_CHECK_EQUAL(om->GetSubname(), "ATCC 1234");
    om->SetSubname("DSM  567");
    om->AutoFix();
    BOOST_CHECK_EQUAL(om->GetSubname(), "DSM 567");

    om->SetSubtype(COrgMod::eSubtype_nat_host);
    om->SetSubname("human");
    om->AutoFix();
    BOOST_CHECK_EQUAL(om->GetSubname(), "Homo sapiens");
}


BOOST_AUTO_TEST_CASE(Test_RmCultureNotes)
{
    CRef<CSubSource> ss(new CSubSource());
    ss->SetSubtype(CSubSource::eSubtype_other);
    ss->SetName("a; [mixed bacterial source]; b");
    ss->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->GetName(), "a; b");
    ss->SetName("[uncultured (using species-specific primers) bacterial source]");
    ss->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->GetName(), "amplified with species-specific primers");
    ss->SetName("[BankIt_uncultured16S_wizard]; [universal primers]; [tgge]");
    ss->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->IsSetName(), false);
    ss->SetName("[BankIt_uncultured16S_wizard]; [species_specific primers]; [dgge]");
    ss->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->GetName(), "amplified with species-specific primers");

    CRef<CBioSource> src(new CBioSource());
    ss->SetName("a; [mixed bacterial source]; b");
    src->SetSubtype().push_back(ss);
    src->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->GetName(), "a; b");
    ss->SetName("[BankIt_uncultured16S_wizard]; [universal primers]; [tgge]");
    src->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(src->IsSetSubtype(), false);
}


BOOST_AUTO_TEST_CASE(Test_VR_28)
{
    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", 
                                                 eDiag_Warning, 
                                                 "MultipleSourceQualifiers",
                                                 "Multiple segment qualifiers present"));

    // Mutliple segment qualifiers
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_segment, "1");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_segment, "2");
    
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_segment, "");

    // Multiple collected_by qualifiers
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_collected_by, "Michael Hunter");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_collected_by, "Steven Fisher");
    expected_errors[0]->SetErrMsg("Multiple collected_by qualifiers present");
    
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_collected_by, "");

    // Multiple identified_by qualifiers
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_identified_by, "Michael Hunter");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_identified_by, "Steven Fisher");
    expected_errors[0]->SetErrMsg("Multiple identified_by qualifiers present");
    
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_identified_by, "");

    // Multiple collection_date qualifiers
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_collection_date, "31-May-2014");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_collection_date, "30-Apr-2014");
    expected_errors[0]->SetErrMsg("Multiple collection_date qualifiers present");
    
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_collection_date, "");

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_GP_9919)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();

    STANDARD_SETUP

    entry->SetSet().SetSeq_set().back()->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MP*K*E*N");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("GTGCCCTAAAAATAAGAGTAAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetExcept(true);
    cds->SetExcept_text("unclassified translation discrepancy");

    BOOST_CHECK_EQUAL(validator::HasStopInProtein(*cds, scope), true);
    BOOST_CHECK_EQUAL(validator::HasInternalStop(*cds, scope, false), true);

    // list of expected errors
    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "StopInProtein", "[3] termination symbols in protein sequence (gene? - fake protein name)"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "ExceptionProblem", "unclassified translation discrepancy is not a legal exception explanation"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "InternalStop", "3 internal stops (and illegal start codon). Genetic code [0]"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now suppress an error
    CRef<CSeqdesc> suppress(new CSeqdesc());
    suppress->SetUser().SetObjectType(CUser_object::eObjectType_ValidationSuppression);
    CValidErrorFormat::AddSuppression(suppress->SetUser(), eErr_SEQ_FEAT_InternalStop);
    entry->SetSet().SetDescr().Set().push_back(suppress);
    delete(expected_errors[2]);
    expected_errors.pop_back();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // suppress two errors
    CValidErrorFormat::AddSuppression(suppress->SetUser(), eErr_SEQ_FEAT_ExceptionProblem);
    delete(expected_errors[1]);
    expected_errors.pop_back();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // suppress three errors
    CValidErrorFormat::AddSuppression(suppress->SetUser(), eErr_SEQ_INST_StopInProtein);
    CLEAR_ERRORS
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(Test_RemoveLineageSourceNotes)
{
    CRef<CBioSource> bsrc(new CBioSource);
    bsrc->SetOrg().SetTaxname("Influenza A virus");
    bsrc->SetOrg().SetOrgname().SetLineage("Viruses; ssRNA negative-strand viruses; Orthomyxoviridae; Influenzavirus A");

    CRef<CSubSource> subsrc(new CSubSource(CSubSource::eSubtype_other, "Organism: viruses"));
    bsrc->SetSubtype().push_back(subsrc);
    CRef<COrgMod> mod_a(new COrgMod(COrgMod::eSubtype_strain, "virus strain"));
    CRef<COrgMod> mod_b(new COrgMod(COrgMod::eSubtype_other, "note: influenza A"));
    bsrc->SetOrg().SetOrgname().SetMod().push_back(mod_a);
    bsrc->SetOrg().SetOrgname().SetMod().push_back(mod_b);

    bool removed = bsrc->RemoveLineageSourceNotes();
    BOOST_CHECK_EQUAL(removed, false); // it won't remove the notes as there is no taxid
    bsrc->SetOrg().SetTaxId(11320);

    removed = bsrc->RemoveLineageSourceNotes();
    BOOST_CHECK_EQUAL(removed, true);
    BOOST_CHECK_EQUAL(bsrc->IsSetSubtype(), false);
    FOR_EACH_ORGMOD_ON_BIOSOURCE( orgmod, *bsrc) {
        if ((*orgmod)->IsSetSubtype()) {
            BOOST_CHECK_EQUAL((*orgmod)->GetSubtype() == COrgMod::eSubtype_other, false);
        }
    }

    CRef<COrgMod> mod_c(new COrgMod(COrgMod::eSubtype_other, "domain: unknown domain"));
    removed = bsrc->RemoveLineageSourceNotes();
    BOOST_CHECK_EQUAL(removed, false);
    FOR_EACH_ORGMOD_ON_BIOSOURCE( orgmod, *bsrc) {
        if ((*orgmod)->IsSetSubtype() && (*orgmod)->GetSubtype() != COrgMod::eSubtype_strain) {
            BOOST_CHECK_EQUAL((*orgmod)->GetSubtype() == COrgMod::eSubtype_other, true);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_GB_3714)
{
    string orig = CGb_qual::BuildExperiment("", "experiment", "");
    BOOST_CHECK_EQUAL(orig, "experiment");

    string experiment;
    string category;
    string doi;

    CGb_qual::ParseExperiment(orig, category, experiment, doi);
    BOOST_CHECK_EQUAL(category, "");
    BOOST_CHECK_EQUAL(experiment, "experiment");
    BOOST_CHECK_EQUAL(doi, "");

    orig = CGb_qual::BuildExperiment("", "experiment2", "DOI");
    BOOST_CHECK_EQUAL(orig, "experiment2[DOI]");
    CGb_qual::ParseExperiment(orig, category, experiment, doi);
    BOOST_CHECK_EQUAL(category, "");
    BOOST_CHECK_EQUAL(experiment, "experiment2");
    BOOST_CHECK_EQUAL(doi, "DOI");

    orig = CGb_qual::BuildExperiment("COORDINATES", "experiment3", "");
    BOOST_CHECK_EQUAL(orig, "COORDINATES:experiment3");
    CGb_qual::ParseExperiment(orig, category, experiment, doi);
    BOOST_CHECK_EQUAL(category, "COORDINATES");
    BOOST_CHECK_EQUAL(experiment, "experiment3");
    BOOST_CHECK_EQUAL(doi, "");

    orig = CGb_qual::BuildExperiment("EXISTENCE", "experiment4", "DOI2");
    BOOST_CHECK_EQUAL(orig, "EXISTENCE:experiment4[DOI2]");
    CGb_qual::ParseExperiment(orig, category, experiment, doi);
    BOOST_CHECK_EQUAL(category, "EXISTENCE");
    BOOST_CHECK_EQUAL(experiment, "experiment4");
    BOOST_CHECK_EQUAL(doi, "DOI2");

}


BOOST_AUTO_TEST_CASE(Test_SQD_2036)
{
    string msg = CSubSource::CheckCellLine("222", "Homo sapiens");
    BOOST_CHECK_EQUAL(msg, "The International Cell Line Authentication Committee database indicates that 222 from Homo sapiens is known to be contaminated by PA1 from Human. Please see http://iclac.org/databases/cross-contaminations/ for more information and references.");

    msg = CSubSource::CheckCellLine("223", "Homo sapiens");
    BOOST_CHECK_EQUAL(msg, "");

    msg = CSubSource::CheckCellLine("222", "Canis familiaris");
    BOOST_CHECK_EQUAL(msg, "");

    // prepare entry
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetTaxname(entry, "Cavia porcellus");
    unit_test_util::SetTaxon(entry, 0);
    unit_test_util::SetTaxon(entry, 10141);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_cell_line, "GPS-M");
    
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", 
                                                 eDiag_Warning, 
                                                 "SuspectedContaminatedCellLine",
                                                 "The International Cell Line Authentication Committee database indicates that GPS-M from Cavia porcellus is known to be contaminated by Strain L-M from Mouse. Please see http://iclac.org/databases/cross-contaminations/ for more information and references."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_VR_146)
{
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    unit_test_util::RemoveDescriptorType (e1, CSeqdesc::e_Pub);
    CRef<CSeq_entry> e2 = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::RemoveDescriptorType (e2, CSeqdesc::e_Pub);
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_phy_set);
    entry->SetSet().SetSeq_set().push_back(e1);
    entry->SetSet().SetSeq_set().push_back(e2);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", 
                                                 eDiag_Error, 
                                                 "NoPubFound", 
                                                 "No publications anywhere on this entire record."));
    expected_errors.push_back(new CExpectedError("lcl|good", 
                                                 eDiag_Info, 
                                                 "MissingPubRequirement",
                                                 "No submission citation anywhere on this entire record."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    options |= CValidator::eVal_genome_submission;
    expected_errors[1]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CLEAR_ERRORS

    unit_test_util::AddGoodPub(entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_VR_711)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> repeat_region = unit_test_util::AddMiscFeature(entry);
    repeat_region->SetData().SetImp().SetKey("repeat_region");
    repeat_region->ResetComment();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->ResetComment();

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good",
        eDiag_Warning,
        "NeedsNote",
        "A note or other qualifier is required for a misc_feature"));
    expected_errors.push_back(new CExpectedError("lcl|good",
        eDiag_Warning,
        "NeedsNote",
        "repeat_region has no qualifiers"));

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    // bump to error for -U
    options |= CValidator::eVal_genome_submission;
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[1]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // only warning for EMBL/DDBJ
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> other_acc(new CSeq_id());
    other_acc->SetEmbl().SetAccession("HE717023");
    other_acc->SetEmbl().SetVersion(1);
    entry->SetSeq().SetId().push_back(other_acc);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("emb|HE717023.1|");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[1]->SetAccession("emb|HE717023.1|");
    expected_errors[1]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_IsLocationInFrame)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CSeq_feat_Handle fh = scope->GetSeq_featHandle(*cds);
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->Assign(cds->GetLocation());
    
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_InFrame, feature::IsLocationInFrame(fh, *loc));
    loc->SetInt().SetFrom(loc->GetInt().GetFrom() + 1);
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_BadStart, feature::IsLocationInFrame(fh, *loc));
    loc->SetInt().SetFrom(loc->GetInt().GetFrom() + 1);
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_BadStart, feature::IsLocationInFrame(fh, *loc));
    loc->SetInt().SetFrom(loc->GetInt().GetFrom() + 1);
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_InFrame, feature::IsLocationInFrame(fh, *loc));
    loc->Assign(cds->GetLocation());
    loc->SetInt().SetTo(loc->GetInt().GetTo() - 1);
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_BadStop, feature::IsLocationInFrame(fh, *loc));
    loc->SetInt().SetTo(loc->GetInt().GetTo() - 1);
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_BadStop, feature::IsLocationInFrame(fh, *loc));
    loc->SetInt().SetTo(loc->GetInt().GetTo() - 1);
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_InFrame, feature::IsLocationInFrame(fh, *loc));

    loc->Assign(cds->GetLocation());
    loc->SetInt().SetFrom(loc->GetInt().GetFrom() + 1);
    loc->SetInt().SetTo(loc->GetInt().GetTo() - 1);
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_BadStartAndStop, feature::IsLocationInFrame(fh, *loc));
    loc->SetInt().SetFrom(loc->GetInt().GetFrom() + 1);
    loc->SetInt().SetTo(loc->GetInt().GetTo() - 1);
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_BadStartAndStop, feature::IsLocationInFrame(fh, *loc));

    loc->SetInt().SetFrom(cds->GetLocation().GetStop(eExtreme_Biological) + 1);
    loc->SetInt().SetTo(loc->GetInt().GetFrom() + 2);
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_NotIn, feature::IsLocationInFrame(fh, *loc));    

    CRef<CSeq_id> loc_id(new CSeq_id());
    loc_id->Assign(loc->GetInt().GetId());
    cds->SetLocation().Assign(*(unit_test_util::MakeMixLoc(loc_id)));
    loc->SetInt().SetFrom(cds->GetLocation().GetStart(eExtreme_Biological));
    loc->SetInt().SetTo(cds->GetLocation().GetStop(eExtreme_Biological));
    BOOST_CHECK_EQUAL(feature::eLocationInFrame_NotIn, feature::IsLocationInFrame(fh, *loc));
}

CRef<CTaxon3_reply> s_CreateReplyWithMessage(const string& message)
{
    CRef<CTaxon3_reply> reply(new CTaxon3_reply);
    CRef<CT3Reply> t3reply(new CT3Reply);
    t3reply->SetError().SetLevel(CT3Reply::TError::eLevel_error);
    t3reply->SetError().SetMessage(message);
    reply->SetReply().push_back(t3reply);
    return reply;
}


//removed until issues with caching and mocking service can be resolved
BOOST_AUTO_TEST_CASE(Test_Empty_Taxon_Reply)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CMockTaxon::TReplies replies;
    CRef<CTaxon3_reply> reply(new CTaxon3_reply);
    replies.push_back(reply);

    STANDARD_SETUP_WITH_MOCK_TAXON(replies);

    eval = validator.Validate(seh, options);

    expected_errors.push_back(new CExpectedError("lcl|good",
        eDiag_Error,
        "ServiceError",
        "Taxonomy service connection failure"));


    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_VR_601)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    string id_str = "ABCD123456789";
    CRef<CSeq_id> id(new CSeq_id());
    id->SetGenbank().SetAccession(id_str);

    unit_test_util::ChangeNucId(entry, id);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gb|"+id_str+"|", eDiag_Error, "InconsistentMolInfoTechnique", "WGS accession should have Mol-info.tech of wgs"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // error suppressed for TLS
    CLEAR_ERRORS
    unit_test_util::SetTech(entry->SetSet().SetSeq_set().front(), CMolInfo::eTech_targeted);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_VR_612)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    unit_test_util::SetNucProtSetProductName(entry, "This product name contains RefSeq");
    CRef<CSeqdesc> defline(new CSeqdesc());
    defline->SetTitle("This title contains RefSeq");
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    nuc->SetSeq().SetDescr().Set().push_back(defline);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "RefSeqInText", "Protein name contains 'RefSeq'"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "RefSeqInText", "Definition line contains 'RefSeq'"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_VR_616)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "yes");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "BioSourceInconsistency", "Orgmod.strain should not be 'yes'"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "NO");
    expected_errors[0]->SetErrMsg("Orgmod.strain should not be 'NO'");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "-");
    expected_errors[0]->SetErrMsg("Orgmod.strain should not be '-'");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "");
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, "microbial");
    expected_errors[0]->SetErrMsg("Orgmod.strain should not be 'microbial'");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}

BOOST_AUTO_TEST_CASE(Test_BadLocation)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();

    CRef<CSeq_feat> gene(new CSeq_feat());
    gene->SetData().SetGene().SetLocus("badguy");
    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetFrom(0);
    loc1->SetInt().SetTo(10);
    loc1->SetInt().SetId().SetLocal().SetStr("good1");    
    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetInt().SetFrom(0);
    loc2->SetInt().SetTo(10);
    loc2->SetInt().SetId().SetLocal().SetStr("good2");
    CRef<CSeq_loc> loc3(new CSeq_loc());
    loc3->SetInt().SetFrom(0);
    loc3->SetInt().SetTo(10);
    loc3->SetInt().SetId().SetLocal().SetStr("good3");

    gene->SetLocation().SetMix().Set().push_back(loc1);
    gene->SetLocation().SetMix().Set().push_back(loc2);
    gene->SetLocation().SetMix().Set().push_back(loc3);

    unit_test_util::AddFeat(gene, entry->SetSet().SetSeq_set().front());

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "BadLocation", 
                           "Feature location intervals should all be on the same sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    // error goes away if organelle small genome set
    entry->SetSet().SetClass(CBioseq_set::eClass_small_genome_set);
    // remove title, not appropriate for small genome set
    unit_test_util::RemoveDescriptorType(entry, CSeqdesc::e_Title);
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, s, entry->SetSet().SetSeq_set()) {
        unit_test_util::SetGenome(*s, CBioSource::eGenome_chloroplast);
    }
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_VR_78)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetData().SetRna().SetExt().SetName(prot->GetData().GetProt().GetName().front());
    unit_test_util::AddFeat(mrna, nuc);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(cds);
    unit_test_util::AddFeat(gene, nuc);

    STANDARD_SETUP
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetNucProtSetPartials(entry, true, false);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemMismatch5Prime",
        "gene should not be 5' complete if coding region is 5' partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemMismatch5Prime",
        "mRNA should not be 5' complete if coding region is 5' partial"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetNucProtSetPartials(entry, false, true);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemMismatch3Prime",
        "gene should not be 3' complete if coding region is 3' partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemMismatch3Prime",
        "mRNA should not be 3' complete if coding region is 3' partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemNotConsensus3Prime",
        "3' partial is not at end of sequence, gap, or consensus splice site"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblemHasStop",
        "Got stop codon, but 3'end is labeled partial"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::SetNucProtSetPartials(entry, true, true);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemMismatch5Prime",
        "gene should not be 5' complete if coding region is 5' partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemMismatch5Prime",
        "mRNA should not be 5' complete if coding region is 5' partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemMismatch3Prime",
        "gene should not be 3' complete if coding region is 3' partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemMismatch3Prime",
        "mRNA should not be 3' complete if coding region is 3' partial"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "PartialProblemNotConsensus3Prime",
        "3' partial is not at end of sequence, gap, or consensus splice site"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "PartialProblemHasStop",
        "Got stop codon, but 3'end is labeled partial"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_VR_166)
{
    string host = "Atlantic white-sided dolphin";
    string error_msg;

    BOOST_CHECK_EQUAL("Atlantic white-sided dolphin", FixSpecificHost("Atlantic white-sided dolphin"));
    BOOST_CHECK_EQUAL(true, IsSpecificHostValid("Atlantic white-sided dolphin", error_msg));


    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Atlantic white-sided dolphin");

    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(TEST_TitleNotAppropriateForSet)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);

    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good1", eDiag_Error, "TitleNotAppropriateForSet",
        "Only Pop/Phy/Mut/Eco sets should have titles"));
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_VR_664)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CUser_object> user = edit::CGenomeAssemblyComment::MakeEmptyUserObject();
    edit::CGenomeAssemblyComment::SetAssemblyMethod(*user, "x v. y");
    CRef<CUser_field> assembly_name(new CUser_field());
    assembly_name->SetLabel().SetStr("Assembly Name");
    assembly_name->SetData().SetStr("valid value");
    user->SetData().push_back(assembly_name);
    edit::CGenomeAssemblyComment::SetGenomeCoverage(*user, "2x");
    edit::CGenomeAssemblyComment::SetSequencingTechnology(*user, "z");
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().Assign(*user);
    entry->SetSeq().SetDescr().Set().push_back(desc);
  
    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    assembly_name->SetData().SetStr("not,valid");
    desc->SetUser().Assign(*user);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info,
                                 "BadStrucCommInvalidFieldValue",
                                 "Structured Comment invalid"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
                                 "BadStrucCommInvalidFieldValue",
                                 "not,valid is not a valid value for Assembly Name"));
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    assembly_name->SetData().SetStr("Ec2009C-3227");
    desc->SetUser().Assign(*user);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    assembly_name->SetData().SetStr("Anop_step_SDA-500_V1");
    desc->SetUser().Assign(*user);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_VR_478)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene(new CSeq_feat());
    gene->SetData().SetGene().SetLocus("a");
    CRef<CSeq_loc> int1(new CSeq_loc());
    int1->SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
    int1->SetInt().SetFrom(0);
    int1->SetInt().SetTo(5);
    CRef<CSeq_loc> int2(new CSeq_loc());
    int2->SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
    int2->SetInt().SetFrom(10);
    int2->SetInt().SetTo(15);
    gene->SetLocation().SetMix().Set().push_back(int1);
    gene->SetLocation().SetMix().Set().push_back(int2);
    unit_test_util::AddFeat(gene, entry);

    CRef<CSeq_feat> mobile_element(new CSeq_feat());
    mobile_element->SetData().SetImp().SetKey("mobile_element");
    mobile_element->SetLocation().SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
    mobile_element->SetLocation().SetInt().SetFrom(6);
    mobile_element->SetLocation().SetInt().SetTo(9);
    CRef<CGb_qual> qual(new CGb_qual("mobile_element_type", "superintegron"));
    mobile_element->SetQual().push_back(qual);
    unit_test_util::AddFeat(mobile_element, entry);

    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_VR_630)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("X");
    gene->SetExcept(true);
    gene->SetExcept_text("trans-splicing");

    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "BadLocation",
        "Trans-spliced feature should have multiple intervals"));
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_VR_660)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> recomb = unit_test_util::AddMiscFeature(entry);
    recomb->SetData().SetImp().SetKey("misc_recomb");
    CRef<CGb_qual> qual(new CGb_qual("recombination_class", "other"));
    recomb->SetQual().push_back(qual);

    STANDARD_SETUP

    // first check ok because recomb has comment
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // error because 'other' and no comment
    recomb->ResetComment();
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "RecombinationClassOtherNeedsNote",
        "The recombination_class 'other' is missing the required /note"));
    CheckErrors(*eval, expected_errors);

    // info because not other and not valid
    // removed per VR-770
    // qual->SetVal("not a valid recombination class");
    // expected_errors[0]->SetErrMsg("'not a valid recombination class' is not a legal value for recombination_class");
    // expected_errors[0]->SetSeverity(eDiag_Info);
    // eval = validator.Validate(seh, options);
    // CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    // no error because legal
    qual->SetVal("mitotic");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

}


void AddOrgmod(COrg_ref& org, const string& val, COrgMod::ESubtype subtype)
{
    CRef<COrgMod> om(new COrgMod(subtype, val));
    org.SetOrgname().SetMod().push_back(om);
}


void AddOrgmodDescriptor(CRef<CSeq_entry> entry, const string& val, COrgMod::ESubtype subtype)
{
    CRef<CSeqdesc> src_desc(new CSeqdesc());
    // should look up
    src_desc->SetSource().SetOrg().SetTaxname("Influenza A virus");
    AddOrgmod(src_desc->SetSource().SetOrg(), val, subtype);
    entry->SetDescr().Set().push_back(src_desc);
}

void AddOrgmodFeat(CRef<CSeq_entry> entry, const string& val, COrgMod::ESubtype subtype)
{
    CRef<CSeq_feat> src_feat = unit_test_util::AddMiscFeature(entry);
    src_feat->SetData().SetBiosrc().SetOrg().SetTaxname("Influenza virus A");
    AddOrgmod(src_feat->SetData().SetBiosrc().SetOrg(), val, subtype);
}

typedef vector< pair<string, string> > THostStringsVector;


void TestBulkSpecificHostFixList(const THostStringsVector& test_values)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    vector<CRef<COrg_ref> > original;
    vector<CRef<COrg_ref> > to_adjust;

    ITERATE(THostStringsVector, it, test_values) {
        AddOrgmodDescriptor(entry, it->first, COrgMod::eSubtype_nat_host);
        AddOrgmodFeat(entry, it->first, COrgMod::eSubtype_nat_host);
        CRef<COrg_ref> org(new COrg_ref());
        org->SetTaxname("foo");
        CRef<COrgMod> om(new COrgMod(COrgMod::eSubtype_nat_host, it->first));
        org->SetOrgname().SetMod().push_back(om);
        to_adjust.push_back(org);
        CRef<COrg_ref> cpy(new COrg_ref());
        cpy->Assign(*org);
        original.push_back(cpy);
    }
    string error_message;

    CTaxValidationAndCleanup tval;
    tval.Init(*entry);
    vector<CRef<COrg_ref> > org_rq_list = tval.GetSpecificHostLookupRequest(true);

    objects::CTaxon3 taxon3;
    taxon3.Init();
    CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(org_rq_list);
    BOOST_CHECK_EQUAL(reply->GetReply().size(), org_rq_list.size());

    BOOST_CHECK_EQUAL(tval.AdjustOrgRefsWithSpecificHostReply(org_rq_list, *reply, to_adjust, error_message), true);

    vector<CRef<COrg_ref> >::iterator org = to_adjust.begin();
    vector<CRef<COrg_ref> >::iterator cpy = original.begin();
    while (org != to_adjust.cend()) {
        const string& before = (*cpy)->GetOrgname().GetMod().front()->GetSubname();
        const string& after = (*org)->GetOrgname().GetMod().front()->GetSubname();
        THostStringsVector::const_iterator tvit = test_values.cbegin();
        while (tvit != test_values.cend() && !NStr::Equal(tvit->first, before)) {
            ++tvit;
        }

        BOOST_CHECK_EQUAL(after, tvit->second);
        ++org;
        ++cpy;
        ++tvit;
    }
}

BOOST_AUTO_TEST_CASE(Test_SQD_4354)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    THostStringsVector test_values;
    test_values.push_back(pair<string, string>("Zymomonas anaerobia", "Zymomonas mobilis"));
    TestBulkSpecificHostFixList(test_values);

    test_values.clear();
    test_values.push_back(pair<string, string>("Zymononas mobilis", "Zymomonas mobilis"));
    TestBulkSpecificHostFixList(test_values);
}


BOOST_AUTO_TEST_CASE(Test_BulkSpecificHostFix)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    THostStringsVector test_values;
    test_values.push_back(pair<string, string>("Homo supiens", "Homo supiens")); // non-fixable spelling problem
    test_values.push_back(pair<string, string>("HUMAN", "Homo sapiens"));
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Homo sapiens", "Homo sapiens"));
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Gallus Gallus", "Gallus gallus"));
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Conservemos nuestros", "Conservemos nuestros")); // non-fixable spelling problem
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Pinus sp.", "Pinus sp.")); // ambiguous
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Eschericia coli", "Escherichia coli")); // fixable spelling problem
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Avian", "Avian"));
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Bovine", "Bovine"));
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Pig", "Pig"));
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>(" Chicken", "Chicken")); // truncate space
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Homo sapiens; sex: female", "Homo sapiens; sex: female"));
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Atlantic white-sided dolphin", "Atlantic white-sided dolphin"));
    TestBulkSpecificHostFixList(test_values);
    test_values.push_back(pair<string, string>("Zymomonas anaerobia", "Zymomonas mobilis"));
    TestBulkSpecificHostFixList(test_values);

    vector<CRef<COrg_ref> > to_adjust;
    vector<CRef<COrg_ref> > original;

    ITERATE(THostStringsVector, it, test_values) {
        AddOrgmodDescriptor(entry, it->first, COrgMod::eSubtype_nat_host);
        AddOrgmodFeat(entry, it->first, COrgMod::eSubtype_nat_host);
        CRef<COrg_ref> org(new COrg_ref());
        org->SetTaxname("foo");
        CRef<COrgMod> om(new COrgMod(COrgMod::eSubtype_nat_host, it->first));
        org->SetOrgname().SetMod().push_back(om);
        to_adjust.push_back(org);
        CRef<COrg_ref> cpy(new COrg_ref());
        cpy->Assign(*org);
        original.push_back(cpy);
    }
    string error_message;

    CTaxValidationAndCleanup tval;
    tval.Init(*entry);
    vector<CRef<COrg_ref> > org_rq_list = tval.GetSpecificHostLookupRequest(true);
    BOOST_CHECK_EQUAL(org_rq_list.size(), test_values.size() - 3); // three homo sapiens are combined

    objects::CTaxon3 taxon3;
    taxon3.Init();
    CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(org_rq_list);

    BOOST_CHECK_EQUAL(tval.AdjustOrgRefsWithSpecificHostReply(org_rq_list, *reply, to_adjust, error_message), true);

    vector<CRef<COrg_ref> >::iterator org = to_adjust.begin();
    vector<CRef<COrg_ref> >::iterator cpy = original.begin();
    while (org != to_adjust.cend()) {
        const string& before = (*cpy)->GetOrgname().GetMod().front()->GetSubname();
        const string& after = (*org)->GetOrgname().GetMod().front()->GetSubname();
        THostStringsVector::const_iterator tvit = test_values.cbegin();
        while (tvit != test_values.cend() && !NStr::Equal(tvit->first, before)) {
            ++tvit;
        }

        BOOST_CHECK_EQUAL(after, tvit->second);
        ++org;
        ++cpy;
        ++tvit;
    }

    CRef<COrg_ref> test_src(new COrg_ref());
    AddOrgmod(*test_src, "Conservemos nuestros", COrgMod::eSubtype_nat_host); // don't change because bad
    AddOrgmod(*test_src, "Pinus sp.", COrgMod::eSubtype_nat_host); // don't change because ambivalent
    AddOrgmod(*test_src, "Eschericia coli", COrgMod::eSubtype_nat_host); // change because spelling

    to_adjust.clear();
    to_adjust.push_back(test_src);
    BOOST_CHECK_EQUAL(tval.AdjustOrgRefsWithSpecificHostReply(org_rq_list, *reply, to_adjust, error_message), true);
    COrgName::TMod::const_iterator m = test_src->GetOrgname().GetMod().begin();
    BOOST_CHECK_EQUAL((*m)->GetSubname(), "Conservemos nuestros");
    ++m;
    BOOST_CHECK_EQUAL((*m)->GetSubname(), "Pinus sp.");
    ++m;
    BOOST_CHECK_EQUAL((*m)->GetSubname(), "Escherichia coli");
    // already fixed all problems, don't fix again
    BOOST_CHECK_EQUAL(tval.AdjustOrgRefsWithSpecificHostReply(org_rq_list, *reply, to_adjust, error_message), false);
    m = test_src->GetOrgname().GetMod().begin();
    BOOST_CHECK_EQUAL((*m)->GetSubname(), "Conservemos nuestros");
    ++m;
    BOOST_CHECK_EQUAL((*m)->GetSubname(), "Pinus sp.");
    ++m;
    BOOST_CHECK_EQUAL((*m)->GetSubname(), "Escherichia coli");

    vector< CRef<COrg_ref> > original_orgs = tval.GetTaxonomyLookupRequest();
    vector< CRef<COrg_ref> > edited_orgs = tval.GetTaxonomyLookupRequest();
    CRef<CTaxon3_reply> lookup_reply = taxon3.SendOrgRefList(original_orgs);
    BOOST_CHECK_EQUAL(lookup_reply->GetReply().size(), original_orgs.size());
    BOOST_CHECK_EQUAL(tval.AdjustOrgRefsWithTaxLookupReply(*lookup_reply, edited_orgs, error_message), true);
    // second time should produce no additional changes
    BOOST_CHECK_EQUAL(tval.AdjustOrgRefsWithTaxLookupReply(*lookup_reply, edited_orgs, error_message), false);
    vector< CRef<COrg_ref> > spec_host_rq = tval.GetSpecificHostLookupRequest(true);
    CRef<CTaxon3_reply> spec_host_reply = taxon3.SendOrgRefList(spec_host_rq);
    BOOST_CHECK_EQUAL(tval.AdjustOrgRefsWithSpecificHostReply(org_rq_list, *spec_host_reply, edited_orgs, error_message), true);
    // second time should produce no additional changes
    BOOST_CHECK_EQUAL(tval.AdjustOrgRefsWithSpecificHostReply(org_rq_list, *spec_host_reply, edited_orgs, error_message), false);

    size_t num_descs = tval.NumDescs();
    size_t num_updated_descs = 0;
    for (size_t n = 0; n < num_descs; n++) {
        if (!original_orgs[n]->Equals(*(edited_orgs[n]))) {
            CConstRef<CSeqdesc> desc = tval.GetDesc(n);
            CRef<CSeqdesc> new_desc(new CSeqdesc());
            new_desc->Assign(*desc);
            new_desc->SetSource().SetOrg().Assign(*(edited_orgs[n]));
            num_updated_descs++;
        }
    }
    // we expect that all descs will be updated, because they have a recognizable taxname but none of the other data
    BOOST_CHECK_EQUAL(num_updated_descs, num_descs);

    size_t num_updated_feats = 0;
    for (size_t n = 0; n < tval.NumFeats(); n++) {
        if (!original_orgs[n + num_descs]->Equals(*edited_orgs[n + num_descs])) {
            CConstRef<CSeq_feat> feat = tval.GetFeat(n);
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*feat);
            new_feat->SetData().SetBiosrc().SetOrg().Assign(*(edited_orgs[n]));
            num_updated_feats++;
        }
    }
    // only five of the feats will be updated, because their taxnames cannot be
    // recognized, and only five of the specific hosts are altered.
    BOOST_CHECK_EQUAL(num_updated_feats, 5);
}


BOOST_AUTO_TEST_CASE(Test_BulkSpecificHostFixIncremental)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    THostStringsVector test_values;
    test_values.push_back(pair<string, string>("Homo supiens", "Homo supiens")); // non-fixable spelling problem
    test_values.push_back(pair<string, string>("HUMAN", "Homo sapiens"));
    test_values.push_back(pair<string, string>("Homo sapiens", "Homo sapiens"));
    test_values.push_back(pair<string, string>("Pinus sp.", "Pinus sp.")); // ambiguous
    test_values.push_back(pair<string, string>("Gallus Gallus", "Gallus gallus"));
    test_values.push_back(pair<string, string>("Eschericia coli", "Escherichia coli")); // fixable spelling problem
    test_values.push_back(pair<string, string>("Avian", "Avian"));
    test_values.push_back(pair<string, string>("Bovine", "Bovine"));
    test_values.push_back(pair<string, string>("Pig", "Pig"));
    test_values.push_back(pair<string, string>(" Chicken", "Chicken")); // truncate space
    test_values.push_back(pair<string, string>("Homo sapiens; sex: female", "Homo sapiens; sex: female"));
    test_values.push_back(pair<string, string>("Atlantic white-sided dolphin", "Atlantic white-sided dolphin"));

    vector<CRef<COrg_ref> > to_adjust;

    ITERATE(THostStringsVector, it, test_values) {
        AddOrgmodDescriptor(entry, it->first, COrgMod::eSubtype_nat_host);
        AddOrgmodFeat(entry, it->first, COrgMod::eSubtype_nat_host);
        CRef<COrg_ref> org(new COrg_ref());
        org->SetTaxname("foo");
        AddOrgmod(*org, it->first, COrgMod::eSubtype_nat_host);
        to_adjust.push_back(org);
    }
    string error_message;

    CTaxValidationAndCleanup tval;
    tval.Init(*entry);
    vector<CRef<COrg_ref> > spec_host_rq = tval.GetSpecificHostLookupRequest(true);
    BOOST_CHECK_EQUAL(spec_host_rq.size(), test_values.size() - 3); // two homo sapiens are combined

    objects::CTaxon3 taxon3;
    taxon3.Init();

    size_t chunk_size = 3;
    size_t i = 0;
    while (i < spec_host_rq.size()) {
        size_t len = min(chunk_size, spec_host_rq.size() - i);
        vector< CRef<COrg_ref> >  tmp_rq(spec_host_rq.begin() + i, spec_host_rq.begin() + i + len);
        CRef<CTaxon3_reply> tmp_spec_host_reply = taxon3.SendOrgRefList(tmp_rq);
        BOOST_CHECK_EQUAL(tval.IncrementalSpecificHostMapUpdate(tmp_rq, *tmp_spec_host_reply), kEmptyStr);
        i += chunk_size;
    }

    BOOST_CHECK_EQUAL(tval.IsSpecificHostMapUpdateComplete(), true);

    BOOST_CHECK_EQUAL(tval.AdjustOrgRefsForSpecificHosts(to_adjust), true);

    vector<CRef<COrg_ref> >::iterator org = to_adjust.begin();
    THostStringsVector::iterator tvit = test_values.begin();
    while (org != to_adjust.end()) {
        BOOST_CHECK_EQUAL((*org)->GetOrgname().GetMod().front()->GetSubname(), tvit->second);
        ++org;
        ++tvit;
    }

}


void AddStrainDescriptor(CSeq_entry& entry, const string& taxname, const string& strain, const string& lineage)
{
    CRef<CSeqdesc> src_desc(new CSeqdesc());
    // should look up
    src_desc->SetSource().SetOrg().SetTaxname(taxname);
    AddOrgmod(src_desc->SetSource().SetOrg(), strain, COrgMod::eSubtype_strain);
    src_desc->SetSource().SetOrg().SetOrgname().SetLineage(lineage);
    entry.SetDescr().Set().push_back(src_desc);
}


void TestOneStrain(const string& taxname, const string& strain, const string& lineage, bool expect_err)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CBioseq::TDescr::Tdata::iterator it = entry->SetSeq().SetDescr().Set().begin();
    while (it != entry->SetSeq().SetDescr().Set().end()) {
        if ((*it)->IsSource()) {
            it = entry->SetSeq().SetDescr().Set().erase(it);
        } else {
            ++it;
        }
    }
    AddStrainDescriptor(*entry, taxname, strain, lineage); // expect no report
    STANDARD_SETUP
        
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "NoTaxonID",
        "BioSource is missing taxon ID"));
    if (expect_err) {
        expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "StrainContainsTaxInfo",
            "Strain '" + strain + "' contains taxonomic name information"));
    }
    if (NStr::Equal(taxname, "Bacillus sp.") || NStr::Equal(taxname, "Acetobacter sp.")) {
        expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "OrganismIsUndefinedSpecies",
            "Organism '" + taxname + "' is undefined species and does not have a specific identifier."));
    }

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_BulkStrainIncremental)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    AddStrainDescriptor(*entry, "Gorilla gorilla", "abc", "xyz"); // expect no report
    AddStrainDescriptor(*entry, "Gorilla gorilla", "Aeromonas punctata", "xyz"); // expect a report
    AddStrainDescriptor(*entry, "Gorilla gorilla", "Klebsiella_quasipneumoniae", "xyz"); // expect a report
    AddStrainDescriptor(*entry, "Bacillus sp.", "cereus", "xyz");
    AddStrainDescriptor(*entry, "Hippopotamus amphibius", "giraffe cow", "xyz"); // no error - giraffe looks up but is not in taxname

    string error_message;

    CTaxValidationAndCleanup tval;
    tval.Init(*entry);

    vector<CRef<COrg_ref> > strain_rq = tval.GetStrainLookupRequest();
    BOOST_CHECK_EQUAL(strain_rq.size(), 9);

    objects::CTaxon3 taxon3;
    taxon3.Init();

    size_t chunk_size = 3;
    size_t i = 0;
    while (i < strain_rq.size()) {
        size_t len = min(chunk_size, strain_rq.size() - i);
        vector< CRef<COrg_ref> >  tmp_rq(strain_rq.begin() + i, strain_rq.begin() + i + len);
        CRef<CTaxon3_reply> tmp_strain_reply = taxon3.SendOrgRefList(tmp_rq);
        BOOST_CHECK_EQUAL(tval.IncrementalStrainMapUpdate(tmp_rq, *tmp_strain_reply), kEmptyStr);
        i += chunk_size;
    }

    BOOST_CHECK_EQUAL(tval.IsStrainMapUpdateComplete(), true);

    // commented out until TM-725 is resolved
    TestOneStrain("Hippopotamus amphibius", "giraffe cow", "xyz", false); // no error - giraffe looks up but is not in taxname
    TestOneStrain("Gorilla gorilla", "abc", "xyz", false);
    TestOneStrain("Gorilla gorilla", "Aeromonas punctata", "xyz", true); 
    TestOneStrain("Gorilla gorilla", "Klebsiella_quasipneumoniae", "xyz", true); 
    TestOneStrain("Bacillus sp.", "cereus", "xyz", true);

    TestOneStrain("Ralstonia phage phiRSL1", "Aeromonas punctata", "xyz", false);
    TestOneStrain("Gorilla gorilla", "Aeromonas punctata", "viroid", false);
    TestOneStrain("Acetobacter sp.", "DsW_063", "Bacteria", false);
}


BOOST_AUTO_TEST_CASE(VR_762)
{
    TestOneStrain("Cystobasidium minutum", "P22", "xyz", false);
}

BOOST_AUTO_TEST_CASE(TEST_VR_477)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    CRef<CCode_break> codebreak(new CCode_break());
    codebreak->SetLoc().SetInt().SetId().SetLocal().SetStr("nuc");
    codebreak->SetLoc().SetInt().SetFrom(24);
    codebreak->SetLoc().SetInt().SetTo(26);
    codebreak->SetLoc().SetPartialStop(true, eExtreme_Positional);
    cds->SetData().SetCdregion().SetCode_break().push_back(codebreak);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "TranslExceptIsPartial",
        "Translation exception locations should not be partial"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_VR_35)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> exon = unit_test_util::AddMiscFeature(entry);
    exon->SetData().SetImp().SetKey("exon");
    exon->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("number", "group I")));

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidNumberQualifier",
        "Number qualifiers should not contain spaces"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(TEST_VR_15)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> feat = unit_test_util::AddMiscFeature(entry);
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetFuzz_from().SetLim(CInt_fuzz::eLim_tl);
    feat->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength() - 1);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
    "Should not specify 'space to left' at first position of non-circular sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetLocation().SetInt().SetFuzz_from().SetLim(CInt_fuzz::eLim_tr);
    seh = scope.AddTopLevelSeqEntry(*entry);
    // not an error
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetLocation().SetInt().ResetFuzz_from();
    feat->SetLocation().SetInt().SetFuzz_to().SetLim(CInt_fuzz::eLim_tr);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
        "Should not specify 'space to right' at last position of non-circular sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
    //suppress if circular
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "CompleteCircleProblem", "Circular topology without complete flag set"));
    CheckErrors(*eval, expected_errors);

    // also suppress for point
    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetLocation().SetPnt().SetId().Assign(*(entry->GetSeq().GetId().front()));
    feat->SetLocation().SetPnt().SetPoint(0);
    feat->SetLocation().SetPnt().SetFuzz().SetLim(CInt_fuzz::eLim_tl);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().ResetTopology();
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
        "Should not specify 'space to left' at first position of non-circular sequence"));
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    feat->SetLocation().SetPnt().SetPoint(entry->GetSeq().GetInst().GetLength() - 1);
    feat->SetLocation().SetPnt().SetFuzz().SetLim(CInt_fuzz::eLim_tr);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidFuzz",
        "Should not specify 'space to right' at last position of non-circular sequence"));
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_VR_433)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetId().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(11);
    unit_test_util::SetTech(entry, CMolInfo::eTech_wgs);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "FarLocationExcludesFeatures",
        "Scaffold points to some but not all of gb|AY123456|, excluded portion contains features"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS

    // suppress error if RefSeq
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_00000001");
    CRef<CSeqdesc> biosample(new CSeqdesc());
    biosample->SetUser().SetType().SetStr("DBLink");
    CRef<CUser_field> f(new CUser_field());
    f->SetLabel().SetStr("BioSample");
    f->SetData().SetStr("SAME0001");
    biosample->SetUser().SetData().push_back(f);
    CRef<CUser_field> f2(new CUser_field());
    f2->SetLabel().SetStr("BioProject");
    f2->SetData().SetStrs().push_back("PRJNA12345");
    biosample->SetUser().SetData().push_back(f2);
    entry->SetSeq().SetDescr().Set().push_back(biosample);

    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_VR_708)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_chromosome, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_chromosome, "_abc");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_linkage_group, "*123");

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "BioSourceInconsistency",
        "chromosome value should start with letter or number"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "BioSourceInconsistency",
        "linkage-group value should start with letter or number"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_TM_145)
{
    string host = "Rhesus monkey";
    string error_msg;

    BOOST_CHECK_EQUAL("Rhesus monkey", FixSpecificHost("Rhesus monkey"));
    BOOST_CHECK_EQUAL(true, IsSpecificHostValid("Rhesus monkey", error_msg));


    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_nat_host, "Rhesus monkey");

    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}

#if 0
// commented out for now
BOOST_AUTO_TEST_CASE(Test_VR_723)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CBioSource> src;
    NON_CONST_ITERATE(CBioseq::TDescr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsSource()) {
            src.Reset(&((*it)->SetSource()));
        }
    }
    COrgName::C_Name& orgname = src->SetOrg().SetOrgname().SetName();
    STANDARD_SETUP

    // binomial
    orgname.SetBinomial().SetGenus("Sebaea");
    orgname.SetBinomial().SetSpecies("microphylla");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    orgname.SetBinomial().SetGenus("x");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "BioSourceInconsistency",
        "Taxname does not match orgname ('Sebaea microphylla', 'x microphylla')"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    orgname.SetBinomial().SetSpecies("y");
    expected_errors[0]->SetErrMsg("Taxname does not match orgname ('Sebaea microphylla', 'x y')");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    orgname.SetBinomial().SetSubspecies("z");
    expected_errors[0]->SetErrMsg("Taxname does not match orgname ('Sebaea microphylla', 'x y subsp. z')");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // virus
    orgname.SetVirus("x");
    expected_errors[0]->SetErrMsg("Taxname does not match orgname ('Sebaea microphylla', 'x')");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
    orgname.SetVirus("Sebaea microphylla");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // hybrid
    CRef<COrgName> org1(new COrgName());
    org1->SetName().SetBinomial().SetSpecies("z");
    org1->SetName().SetBinomial().SetGenus("x");
    CRef<COrgName> org2(new COrgName());
    org2->SetName().SetBinomial().SetGenus("y");
    org2->SetName().SetBinomial().SetSpecies("z");
    orgname.SetHybrid().Set().push_back(org1);
    orgname.SetHybrid().Set().push_back(org2);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "BioSourceInconsistency",
        "Taxname does not match orgname ('Sebaea microphylla', 'x z')"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    org2->SetName().SetBinomial().SetGenus("Sebaea");
    org2->SetName().SetBinomial().SetSpecies("microphylla");
    CLEAR_ERRORS
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // named hybrid
    orgname.SetNamedhybrid().SetGenus("Sebaea");
    orgname.SetNamedhybrid().SetSpecies("microphylla");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "BioSourceInconsistency",
        "Taxname does not match orgname ('Sebaea microphylla', 'Sebaea x microphylla')"));

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
    orgname.SetNamedhybrid().SetGenus("x");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "BioSourceInconsistency",
        "Taxname does not match orgname ('Sebaea microphylla', 'x x microphylla')"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // partial
    CRef<CTaxElement> elem1(new CTaxElement());
    elem1->SetFixed_level(CTaxElement::eFixed_level_class);
    elem1->SetName("x");
    orgname.SetPartial().Set().push_back(elem1);
    expected_errors[0]->SetErrMsg("Taxname does not match orgname ('Sebaea microphylla', 'x')");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CRef<CTaxElement> elem2(new CTaxElement());
    elem2->SetFixed_level(CTaxElement::eFixed_level_family);
    elem2->SetName("Sebaea microphylla");
    orgname.SetPartial().Set().push_back(elem2);
    CLEAR_ERRORS
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

}
#endif


BOOST_AUTO_TEST_CASE(Test_VR_728)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGeneral().SetDb("NCBIFILE");
    entry->SetSeq().SetId().front()->SetGeneral().SetTag().SetStr("x");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gnl|NCBIFILE|x", eDiag_Critical,
        "NoIdOnBioseq",
        "The only ids on this Bioseq will be stripped during ID load"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> other_id(new CSeq_id());
    other_id->SetLocal().SetStr("x");
    entry->SetSeq().SetId().push_back(other_id);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> bankit(new CSeq_id());
    bankit->SetGeneral().SetDb("BankIt");
    bankit->SetGeneral().SetTag().SetStr("x");
    entry->SetSeq().SetId().push_back(bankit);
    CRef<CSeq_feat> misc = AddMiscFeature(entry);
    misc->SetLocation().SetInt().SetId().Assign(*bankit);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|x", eDiag_Critical,
        "BadSeqIdFormat",
        "Feature locations should not use Seq-ids that will be stripped during ID load"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_VR_733)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> f = AddMiscFeature(entry);
    f->SetLocation().SetInt().SetStrand(eNa_strand_both);

    STANDARD_SETUP

    // expect no errors for misc_feat
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS

    scope.RemoveTopLevelSeqEntry(seh);
    f->SetData().SetImp().SetKey("exon");

    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "BothStrands",
        "exon may not be on both (forward) strands"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS

}



void TestOnePlasmid(const string& plasmid_name, bool expect_error)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetGenome(entry, CBioSource::eGenome_plasmid);
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_plasmid_name, plasmid_name);
    STANDARD_SETUP

    if (expect_error) {
        expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BioSourceInconsistency",
                    "Problematic plasmid/chromosome/linkage group name '" + plasmid_name + "'"));
    }
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_VR_742)
{
    TestOnePlasmid("plasmid", true);
    TestOnePlasmid("Sebaea microphylla", true);

    // these values are ok
    TestOnePlasmid("megaplasmid", false);
    TestOnePlasmid("2micron", false);
    TestOnePlasmid("psomething", false);
    TestOnePlasmid("unnamed", false);
    TestOnePlasmid("unnamed2", false);
    TestOnePlasmid("unnamed234", false);
}


BOOST_AUTO_TEST_CASE(Test_VR_751)
{
    BOOST_CHECK_EQUAL(IsLikelyTaxname("Lasiurus scindicus"), true);
    BOOST_CHECK_EQUAL(IsLikelyTaxname("Atlantic white-sided dolphin"), false);
}


BOOST_AUTO_TEST_CASE(Test_TripletEncodesStopCodon)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(entry);

    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGATAAACAGAGATATAATAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    CRef<CSeq_id> id = nuc->SetSeq().SetId().front();
    // first two "introns" are stop codons, third is not
    CRef<CSeq_loc> int1(new CSeq_loc(*id, 0, 8));
    CRef<CSeq_loc> int2(new CSeq_loc(*id, 12, 20));
    CRef<CSeq_loc> int3(new CSeq_loc(*id, 24, 44));
    CRef<CSeq_loc> int4(new CSeq_loc(*id, 48, 59));
    cds->SetLocation().SetMix().Set().push_back(int1);
    cds->SetLocation().SetMix().Set().push_back(int2);
    cds->SetLocation().SetMix().Set().push_back(int3);
    cds->SetLocation().SetMix().Set().push_back(int4);

    STANDARD_SETUP

    vector<CRef<CSeq_loc> > nonsense = CCDSTranslationProblems::GetNonsenseIntrons(*cds, scope);
    BOOST_CHECK_EQUAL(nonsense.size(), 2);
    BOOST_CHECK_EQUAL(nonsense.front()->GetInt().GetFrom(), 9);
    BOOST_CHECK_EQUAL(nonsense.front()->GetInt().GetTo(), 11);
    BOOST_CHECK_EQUAL(nonsense.back()->GetInt().GetFrom(), 21);
    BOOST_CHECK_EQUAL(nonsense.back()->GetInt().GetTo(), 23);

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Critical, "NonsenseIntron",
        "Triplet intron encodes stop codon"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Critical, "NonsenseIntron",
        "Triplet intron encodes stop codon"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "ShortExon", "Internal coding region exon is too short"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "InternalStop", "2 internal stops. Genetic code [0]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "NoStop", "Missing stop codon"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Error, "TransLen", "Given protein length [8] does not match translation length [17]"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusDonor", "Splice donor consensus (GT) not found after exon ending at position 9 of lcl|nuc"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusDonor", "Splice donor consensus (GT) not found after exon ending at position 21 of lcl|nuc"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusDonor", "Splice donor consensus (GT) not found after exon ending at position 45 of lcl|nuc"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusAcceptor", "Splice acceptor consensus (AG) not found before exon starting at position 13 of lcl|nuc"));
    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "NotSpliceConsensusAcceptor", "Splice acceptor consensus (AG) not found before exon starting at position 25 of lcl|nuc"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
    CLEAR_ERRORS
}

BOOST_AUTO_TEST_CASE(VR_758)
{
    // make protein
    CRef<objects::CBioseq> pseq(new objects::CBioseq());
    pseq->SetInst().SetMol(objects::CSeq_inst::eMol_aa);
    pseq->SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    pseq->SetInst().SetExt().SetDelta().AddLiteral("MPRK", objects::CSeq_inst::eMol_aa);
    CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(10);
    pseq->SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    pseq->SetInst().SetExt().SetDelta().AddLiteral("TEIN", objects::CSeq_inst::eMol_aa);
    pseq->SetInst().SetLength(18);

    CRef<objects::CSeq_id> pid(new objects::CSeq_id());
    pid->SetLocal().SetStr("prot");
    pseq->SetId().push_back(pid);

    CRef<objects::CSeqdesc> mpdesc(new objects::CSeqdesc());
    mpdesc->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_peptide);
    mpdesc->SetMolinfo().SetCompleteness(objects::CMolInfo::eCompleteness_complete);
    pseq->SetDescr().Set().push_back(mpdesc);

    CRef<objects::CSeq_entry> entry(new objects::CSeq_entry());
    entry->SetSeq(*pseq);

    AddGoodSource(entry);
    AddGoodPub(entry);

    CRef<objects::CSeq_feat> feat(new objects::CSeq_feat());
    feat->SetData().SetProt().SetName().push_back("fake protein name");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("prot");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(17);
    AddFeat(feat, entry);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "ProteinShouldNotHaveGaps", "Protein sequences should not have gaps"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


void CheckLocalId(const string& id, const string& badchar)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetLocal().SetStr(id);
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|" + id, eDiag_Warning, "BadSeqIdFormat",
           "Bad character '" + badchar + "' in local ID '" + id + "'"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(VR_V48)
{
    CheckLocalId("abc|def", "|");
}


BOOST_AUTO_TEST_CASE(Test_IsDateInPast)
{
    CRef<CDate> date(new CDate());
    BOOST_CHECK_EQUAL(IsDateInPast(*date), false);

    date.Reset(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
    BOOST_CHECK_EQUAL(IsDateInPast(*date), false);
    auto curr_day = date->GetStd().GetDay();
    if (curr_day < 28) {
        date->SetStd().SetDay(curr_day + 1);
        BOOST_CHECK_EQUAL(IsDateInPast(*date), false);
    }
    if (curr_day > 1) {
        date->SetStd().SetDay(curr_day - 1);
        BOOST_CHECK_EQUAL(IsDateInPast(*date), true);
    }
    date->SetStd().ResetDay();
    BOOST_CHECK_EQUAL(IsDateInPast(*date), false);

    auto curr_month = date->GetStd().GetMonth();
    if (curr_month < 11) {
        date->SetStd().SetMonth(curr_month + 1);
        BOOST_CHECK_EQUAL(IsDateInPast(*date), false);
    }
    if (curr_month != 0) {
        date->SetStd().SetMonth(curr_month - 1);
        BOOST_CHECK_EQUAL(IsDateInPast(*date), true);
    }
    date->SetStd().ResetMonth();
    BOOST_CHECK_EQUAL(IsDateInPast(*date), false);

    auto curr_year = date->GetStd().GetYear();
    date->SetStd().SetYear(curr_year + 1);
    BOOST_CHECK_EQUAL(IsDateInPast(*date), false);
    date->SetStd().SetYear(curr_year - 1);
    BOOST_CHECK_EQUAL(IsDateInPast(*date), true);
}


void AddYear(CDate& add_date)
{
    CTime t(add_date.GetStd().GetYear(), add_date.GetStd().GetMonth(), add_date.GetStd().GetDay());
    t.AddYear();
    CDate new_date(t);
    add_date.Assign(new_date);
}


void AddMonth(CDate& add_date)
{
    CTime t(add_date.GetStd().GetYear(), add_date.GetStd().GetMonth(), add_date.GetStd().GetDay());
    t.AddMonth();
    CDate new_date(t);
    add_date.Assign(new_date);
}


void AddDay(CDate& add_date)
{
    CTime t(add_date.GetStd().GetYear(), add_date.GetStd().GetMonth(), add_date.GetStd().GetDay());
    t.AddDay();
    CDate new_date(t);
    add_date.Assign(new_date);
}


BOOST_AUTO_TEST_CASE(VR_778)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    // find sub pub and other pub
    CRef<CPub> subpub(NULL);
    NON_CONST_ITERATE(CBioseq::TDescr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsPub()) {
            if ((*it)->GetPub().GetPub().Get().front()->IsSub()) {
                subpub = (*it)->SetPub().SetPub().Set().front();
            }
        }
    }

    STANDARD_SETUP

    time_t time_now = time(NULL);
    CDate today(time_now);
    CDate future(time_now);
    
    AddYear(future);
    subpub->SetSub().SetDate().Assign(future);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "BadDate",
                              "Submission citation date is in the future"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    future.Assign(today);
    AddMonth(future);
    subpub->SetSub().SetDate().Assign(future); 
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    future.Assign(today);
    AddDay(future);
    subpub->SetSub().SetDate().Assign(future);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    subpub->SetSub().SetDate().Assign(today);
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_InconsistentPseudogeneValue)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(entry);
    cds->SetData().SetCdregion();
    cds->ResetComment();
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("pseudogene", "unitary")));

    CRef<CSeq_feat> mrna = unit_test_util::AddMiscFeature(entry);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->ResetComment();
    mrna->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("pseudogene", "unitary")));

    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("x");
    gene->ResetComment();
    gene->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("pseudogene", "unitary")));

    // no errors, all features have matching pseudogene values
    STANDARD_SETUP

    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // no errors if cds has no pseudogene but mrna and gene do
    cds->ResetQual();
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // no errors if mrna and cds have no pseudogene but gene does
    mrna->ResetQual();
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // no errors if mrna has no pseudogene but cds and gene do
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("pseudogene", "unitary")));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // error if cds has pseudogene but gene does not
    gene->ResetQual();
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InconsistentPseudogeneValue",
        "CDS has pseudogene qualifier, gene does not"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // also error if mRNA has pseudogene but gene does not
    mrna->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("pseudogene", "unitary")));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InconsistentPseudogeneValue",
        "mRNA has pseudogene qualifier, gene does not"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    // different errors when pseudogene values conflict
    gene->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("pseudogene", "allelic")));
    mrna->SetQual().front()->SetVal("processed");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InconsistentPseudogeneValue",
        "Different pseudogene values on CDS (unitary) and gene (allelic)"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InconsistentPseudogeneValue",
        "Different pseudogene values on mRNA (processed) and gene (allelic)"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_InvalidPseudoQualifier)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("x");
    gene->ResetComment();
    gene->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("pseudogene", "")));

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InvalidPseudoQualifier",
        "/pseudogene value should not be empty"));
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InvalidPunctuation",
        "Qualifier other than replace has just quotation marks"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    gene->SetQual().front()->SetVal("abc");
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InvalidPseudoQualifier",
        "/pseudogene value should not be 'abc'"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_InvalidRptUnitRange)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> rpt = unit_test_util::AddMiscFeature(entry);
    rpt->SetData().SetImp().SetKey("repeat_region");
    rpt->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("rpt_unit_range", "x")));

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InvalidRptUnitRange",
        "/rpt_unit_range is not a base range"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    rpt->SetQual().front()->SetVal("a..b");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    rpt->SetQual().front()->SetVal("1..5");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_InvalidRptUnitSeqCharacters)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> rpt = unit_test_util::AddMiscFeature(entry);
    rpt->SetData().SetImp().SetKey("repeat_region");
    rpt->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("rpt_unit_seq", "x..y")));

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InvalidRptUnitSeqCharacters",
        "/rpt_unit_seq has illegal characters"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    rpt->SetQual().front()->SetVal("(atgc)");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_MismatchedAllele)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> rna = unit_test_util::AddMiscFeature(entry);
    rna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rna->SetData().SetRna().SetExt().SetName("16S ribosomal RNA");
    rna->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("allele", "x")));
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature(rna);
    unit_test_util::AddFeat(gene1, entry);
    gene1->SetData().SetGene().SetAllele("y");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "MismatchedAllele",
        "Mismatched allele qualifier on gene (y) and feature (x)"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_InvalidAlleleDuplicates)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    CRef<CSeq_feat> rna = unit_test_util::AddMiscFeature(entry);
    rna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    rna->SetData().SetRna().SetExt().SetName("16S ribosomal RNA");
    rna->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("allele", "x")));
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature(rna);
    unit_test_util::AddFeat(gene1, entry);
    gene1->SetData().SetGene().SetAllele("x");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InvalidAlleleDuplicates",
        "Redundant allele qualifier (x) on gene and feature"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_InvalidOperonMatchesGene)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> operon = unit_test_util::AddMiscFeature(entry);
    operon->SetData().SetImp().SetKey("operon");
    operon->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("operon", "x")));

    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(operon);
    unit_test_util::AddFeat(gene, entry);
    gene->SetData().SetGene().SetLocus("x");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "InvalidOperonMatchesGene",
        "Operon is same as gene - x"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_InvalidCompareRefSeqAccession)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().push_back(CRef<CSeq_id>(new CSeq_id("AY123456.1")));
    CRef<CSeq_feat> var = unit_test_util::AddMiscFeature(entry);
    var->SetData().SetImp().SetKey("variation");
    var->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("compare", "NC_000001.1")));

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error,
        "InvalidCompareRefSeqAccession",
        "RefSeq accession NC_000001.1 cannot be used for qualifier compare"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_InvalidCompareMissingVersion)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().push_back(CRef<CSeq_id>(new CSeq_id("AY123456.1")));
    CRef<CSeq_feat> var = unit_test_util::AddMiscFeature(entry);
    var->SetData().SetImp().SetKey("variation");
    var->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("compare", "NC_000001")));

    STANDARD_SETUP

        expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error,
        "InvalidCompareMissingVersion",
        "NC_000001 accession missing version for qualifier compare"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_InvalidCompareBadAccession)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetId().push_back(CRef<CSeq_id>(new CSeq_id("AY123456.1")));
    CRef<CSeq_feat> var = unit_test_util::AddMiscFeature(entry);
    var->SetData().SetImp().SetKey("variation");
    var->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("compare", "x_y")));

    STANDARD_SETUP

        expected_errors.push_back(new CExpectedError("gb|AY123456.1|", eDiag_Error,
        "InvalidCompareBadAccession",
        "x_y is not a legal accession for qualifier compare"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_RegulatoryClassOtherNeedsNote)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> reg = unit_test_util::AddMiscFeature(entry);
    reg->SetData().SetImp().SetKey("regulatory");
    CRef<CGb_qual> qual(new CGb_qual("regulatory_class", "other"));
    reg->SetQual().push_back(qual);

    STANDARD_SETUP

    // first check ok because recomb has comment
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    // error because 'other' and no comment
    reg->ResetComment();
    eval = validator.Validate(seh, options);
    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "RegulatoryClassOtherNeedsNote",
        "The regulatory_class 'other' is missing the required /note"));
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_UnparsedtRNAAnticodon)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::AddMiscFeature(entry);
    trna->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetNcbieaa('A');
    trna->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("anticodon", "other")));

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "UnparsedtRNAAnticodon",
        "Unparsed anticodon qualifier in tRNA"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_UnparsedtRNAProduct)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> trna = unit_test_util::AddMiscFeature(entry);
    trna->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetNcbieaa('A');
    trna->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("product", "other")));

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error,
        "UnparsedtRNAProduct",
        "Unparsed product qualifier in tRNA"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_rRNADoesNotHaveProduct)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> rrna = unit_test_util::AddMiscFeature(entry);
    rrna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning,
        "rRNADoesNotHaveProduct",
        "rRNA has no name"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_MobileElementInvalidQualifier)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetData().SetImp().SetKey("repeat_region");
    misc->AddQualifier("mobile_element", "foo");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "MobileElementInvalidQualifier",
        "foo is not a legal value for qualifier mobile_element"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    misc->SetQual().front()->SetVal("integron");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_InvalidReplace)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetData().SetImp().SetKey("misc_difference");
    misc->AddQualifier("replace", "123");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidReplace",
        "123 is not a legal value for qualifier replace - should only be composed of acgtmrwsykvhdbn nucleotide bases"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    misc->SetQual().front()->SetVal("aaccttgg");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> prot = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);

    misc = unit_test_util::AddMiscFeature(prot, prot->GetSeq().GetLength() - 1);
    misc->SetData().SetImp().SetKey("misc_difference");
    misc->AddQualifier("replace", "123");
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("lcl|prot", eDiag_Error, "InvalidReplace",
        "123 is not a legal value for qualifier replace - should only be composed of acdefghiklmnpqrstuvwy* amino acids"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_InvalidVariationReplace)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(entry);
    misc->SetData().SetImp().SetKey("variation");
    misc->AddQualifier("replace", "123");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Error, "InvalidVariationReplace",
        "123 is not a legal value for qualifier replace - should only be composed of acgt unambiguous nucleotide bases"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

    misc->SetQual().front()->SetVal("aaccttgg");
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_InvalidProductOnGene)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("x");
    gene->AddQualifier("product", "hypothetical protein");

    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Info, "InvalidProductOnGene",
        "A product qualifier is not used on a gene feature"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS

}


BOOST_AUTO_TEST_CASE(Test_InvalidCodonStart)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("codon_start", "z")));
    STANDARD_SETUP

    expected_errors.push_back(new CExpectedError("lcl|nuc", eDiag_Warning, "InvalidCodonStart",
        "codon_start value should be 1, 2, or 3"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}


BOOST_AUTO_TEST_CASE(Test_InconsistentBioSources_ConLocation)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    unit_test_util::SetGenome(entry, CBioSource::eGenome_apicoplast);
    CSeq_loc& l1 = entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc();
    l1.SetInt().SetId().SetGenbank().SetAccession("AY123456");
    l1.SetInt().SetFrom(0);
    l1.SetInt().SetTo(99);
    CSeq_loc& l2 = entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLoc();
    l2.SetInt().SetId().SetGenbank().SetAccession("AY123457");
    l2.SetInt().SetFrom(0);
    l2.SetInt().SetTo(99);

    entry->SetSeq().SetInst().SetLength(210);

    STANDARD_SETUP_WITH_DATABASE

    expected_errors.push_back(new CExpectedError("lcl|good", eDiag_Warning, "InconsistentBioSources_ConLocation",
        "Genome difference between parent and component"));
    eval = validator.Validate(seh, options);
    CheckErrors(*eval, expected_errors);

    CLEAR_ERRORS
}
