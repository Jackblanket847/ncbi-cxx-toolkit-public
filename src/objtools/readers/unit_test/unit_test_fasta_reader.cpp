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
* Author:  Michael Kornbluh, NCBI
*          (initial skeleton generated by script written by Pavel Ivanov)
*
* File Description:
*   Does misc tests on the CFastaReader that aren't already covered by
*   test_fasta_round_trip, etc.
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <corelib/rwstream.hpp>
#include <corelib/stream_utils.hpp>
#include <corelib/ncbimisc.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objtools/readers/fasta.hpp>

#include <objects/misc/sequence_macros.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {

    // represents inforation about one test of the CFastaReader warning system
    struct SWarningTest {

        // Each SWarningTest has one SOneWarningsInfo for each
        // possible type of warning.
        // The index into warnings_expected must equal the m_eType
        // of that one.  m_eType is also for readability of array
        struct SOneWarningsInfo {
            enum EAppears {
                // start with 1 so that 0 is invalid and we catch it
                eAppears_MustNot = 1,
                eAppears_Must
            };

            CFastaReader::CWarning::EType m_eType;
            EAppears m_eAppears;
            int      m_iLineNumExpected; // 0 if doesn't appear or doesn't matter
        };
    
        string                m_sName; // easier than array index for humans to understand
        SOneWarningsInfo      m_warnings_expected[CFastaReader::CWarning::eType_NUM];
        CFastaReader::TFlags  m_fFastaFlags;
        string                m_sInputFASTA;
    };

    const static CFastaReader::TFlags kDefaultFastaReaderFlags = 
        CFastaReader::fAssumeNuc | 
        CFastaReader::fForceType;

    // list of FASTA warning tests
    const SWarningTest fasta_warning_test_arr[] = {
        { 
            "test case of no warnings",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 }
            },
            kDefaultFastaReaderFlags, // CFastaReader flags
            "> blah \n"
            "ACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGT\n"
        },

        {
            "title too long",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_Must,    1 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 }
            },
            kDefaultFastaReaderFlags, // CFastaReader flags
            "> blah ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ ABCDEFGHIJ\n"
            "ACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGT\n"
        },

        {
            "nucs in title",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_Must,    1 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 }
            },
            kDefaultFastaReaderFlags, // CFastaReader flags
            "> blah ACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGT\n"
            "ACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGT\n"
        },

        {
            "too many ambig on first line",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_Must,    2 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 }
            },
            kDefaultFastaReaderFlags, // CFastaReader flags
            "> blah\n"
            "ACGTACGTACGTNNNNNNNNNNNNNNNTUYYYYYYYYYYYYYYYYYYYYYYYYYYTACGT\n"
        },

        {
            "invalid residue on first line",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_Must,    0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 }
            },
            kDefaultFastaReaderFlags, // CFastaReader flags
            "> blah\n"
            "ACEACGTAEEEACGTACGTACGTACGTACGTACGTACGTACGTACGT\n"
        },

        {
            "invalid residue on subsequent line",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_Must,    0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 }
            },
            kDefaultFastaReaderFlags, // CFastaReader flags
            "> blah\n"
            "ACGACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGT\n"
            "ACEACGTACGTACGTAEETACGTACGTACGTACGTACGTACGTACGT\n"
        },

        {
            "amino acids in title",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,                SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_Must,    0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 }
            },

            kDefaultFastaReaderFlags, // CFastaReader flags
            "> blah ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ\n"
        },

        {
            "trigger as many warnings as possible",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_Must, 1 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_Must, 1 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_Must, 2 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_Must, 0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_Must, 1 }
            },
            kDefaultFastaReaderFlags, // CFastaReader flags
            "> blah [topology=linear] ACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTACACGTACGTAC\n"
            "ACGACNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNGTEACGTACGTACGT\n"
        },

        {
            "invalid residue on multiple lines",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_Must,    0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 }
            },
            kDefaultFastaReaderFlags, // CFastaReader flags
            "> blah\n"
            "ACACEACGTACGTACGTEEEETACGTACGTACGTACGTACGTACGTA\n"
            "ACEACGTACGTACGTAEETACGTACGTACGTACGTACGTACGTACGT\n"
        },

        {
            "Make sure it reads modifiers if requested",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 }
            },
                // Note: non-default flags
                CFastaReader::fAddMods | kDefaultFastaReaderFlags, // CFastaReader flags
                "> blah [topology=linear]\n"
                "ACACAACGTACGTACGTAAAATACGTACGTACGTACGTACGTACGTA\n"
                "ACAACGTACGTACGTAAATACGTACGTACGTACGTACGTACGTACGT\n"
        },

        {
            "Test unexpected mods on line other than first line",

            {
                { CFastaReader::CWarning::eType_TitleTooLong,            SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_NucsInTitle,             SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_TooManyAmbigOnFirstLine, SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_InvalidResidue,          SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_AminoAcidsInTitle,       SWarningTest::SOneWarningsInfo::eAppears_MustNot, 0 },
                { CFastaReader::CWarning::eType_ModsFoundButNotExpected, SWarningTest::SOneWarningsInfo::eAppears_Must,    4 }
            },
                kDefaultFastaReaderFlags, // CFastaReader flags
                ">blah \n"
                "ACACAACGTACGTACGTAAAATACGTACGTACGTACGTACGTACGTA\n"
                "ACAACGTACGTACGTAAATACGTACGTACGTACGTACGTACGTACGT\n"
                ">blahblah [topology=linear]\n"
                "TCACAACGTACGTACGTAAAATACGTACGTACGTACGTACGTACGTA\n"
                "ACAACGTACGTACGTAAATACGTACGTACGTACGTACGTACGTACGT\n"
                ">blahblah2 \n"
                "GCACAACGTACGTACGTAAAATACGTACGTACGTACGTACGTACGTA\n"
                "ACAACGTACGTACGTAAATACGTACGTACGTACGTACGTACGTACGT\n"
        }
    };
}

// Test that the right warnings appear under the right conditions
BOOST_AUTO_TEST_CASE(TestWarnings)
{
    // IF THIS FAILS, IT MEANS YOU NEED TO UPDATE THE TESTS!
    // ( this is here because the tests may need to be adjusted if
    //   the number of possible warnings changes. )
    BOOST_REQUIRE( CFastaReader::CWarning::eType_NUM == 6 );

    // sanity test the tests
    for( size_t warn_test_idx = 0; 
        warn_test_idx < ArraySize(fasta_warning_test_arr); 
        ++warn_test_idx )
    {
        for( size_t iTestNum = 0; 
            iTestNum < CFastaReader::CWarning::eType_NUM; 
            ++iTestNum ) 
        {
            const SWarningTest::SOneWarningsInfo & one_warning_info = 
                fasta_warning_test_arr[warn_test_idx].m_warnings_expected[iTestNum];

            // index must equal the EType
            BOOST_REQUIRE( one_warning_info.m_eType == iTestNum );
            BOOST_REQUIRE( 
                one_warning_info.m_eAppears == 
                    SWarningTest::SOneWarningsInfo::eAppears_Must ||
                one_warning_info.m_eAppears == 
                    SWarningTest::SOneWarningsInfo::eAppears_MustNot );

            // expected line num should be 0 if the warning isn't supposed to appear,
            // but this isn't a show-stopper.
            BOOST_CHECK( one_warning_info.m_eAppears == 
                    SWarningTest::SOneWarningsInfo::eAppears_Must ||
                    one_warning_info.m_iLineNumExpected == 0 );
        }
    }

    for( size_t warn_test_idx = 0; 
        warn_test_idx < ArraySize(fasta_warning_test_arr); 
        ++warn_test_idx )
    {
        const SWarningTest & warning_test = fasta_warning_test_arr[warn_test_idx];

        cout << endl;
        cout << "Running test case '" << warning_test.m_sName << "'" << endl;

        // this will hold warnings found
        CRef<CFastaReader::TWarningRefVec> pWarningRefVec( 
            new CFastaReader::TWarningRefVec );

        // create fasta reader
        CStringReader fastaStringReader( warning_test.m_sInputFASTA );
        CRStream fastaRStream( &fastaStringReader );
        CFastaReader fasta_reader( fastaRStream, warning_test.m_fFastaFlags );
        fasta_reader.SetWarningOutput( pWarningRefVec );

        // do the parsing
        BOOST_CHECK_NO_THROW( fasta_reader.ReadSet() );

        // make sure exactly the warnings that were supposed to appeared
        typedef map<CFastaReader::CWarning::EType, int> TMapWarningTypeToLineNum;
        TMapWarningTypeToLineNum mapWarningTypeToLineNum;

        // load the warnings that were seen into warningsSeenFromThisTest
        ITERATE( CFastaReader::TWarningRefVec::TObjectType, 
            warning_load_it, pWarningRefVec->GetData() ) 
        {
            // check if it already exists and has a different line num
            TMapWarningTypeToLineNum::const_iterator find_iter =
                mapWarningTypeToLineNum.find((*warning_load_it)->GetType());
            BOOST_CHECK_MESSAGE(
                ( find_iter == mapWarningTypeToLineNum.end() ) || 
                    ( find_iter->second == (*warning_load_it)->GetLineNum() ), 
                "On warning check " << warning_test.m_sName << '('
                << warn_test_idx << "): "
                << "Multiple warnings on different lines of type "
                << CFastaReader::CWarning::GetStringOfType(
                (*warning_load_it)->GetType()) );
            mapWarningTypeToLineNum.insert( 
                TMapWarningTypeToLineNum::value_type(
                    (*warning_load_it)->GetType(),
                    (*warning_load_it)->GetLineNum() ) );
        }

        // make sure we have exactly the warnings we're expecting
        for( size_t warning_check_idx = 0;
            warning_check_idx < ArraySize(warning_test.m_warnings_expected);
            ++warning_check_idx )
        {
            const SWarningTest::SOneWarningsInfo & one_warning_info = 
                warning_test.m_warnings_expected[warning_check_idx];
            const CFastaReader::CWarning::EType eExpectedType = 
                one_warning_info.m_eType;
            const bool bWarningShouldAppear = ( 
                one_warning_info.m_eAppears == SWarningTest::SOneWarningsInfo::eAppears_Must );
            const int iExpectedLineNum = one_warning_info.m_iLineNumExpected;

            // check whether warning does actually appear and
            // what line it's on
            bool bWarningDoesAppear = false;
            int  iWarningLineNum    = 0;
            TMapWarningTypeToLineNum::const_iterator find_iter =
                mapWarningTypeToLineNum.find( eExpectedType ); 
            if( find_iter != mapWarningTypeToLineNum.end() ) {
                bWarningDoesAppear = true;
                iWarningLineNum    = find_iter->second;
            }

            // check if warning does appear if it's supposed to,
            // or that it doesn't appear if it isn't supposed to
            BOOST_CHECK_MESSAGE( 
                bWarningShouldAppear == bWarningDoesAppear,
                "On warning check " << warning_test.m_sName << '('
                << warn_test_idx << "): "
                << "Type " << CFastaReader::CWarning::GetStringOfType(
                eExpectedType) << ": should "
                << (bWarningShouldAppear ? "": "not ") << "appear but "
                << "it does" << (bWarningDoesAppear ? "" : " not") );

            // check that line numbers match
            if( bWarningShouldAppear && bWarningDoesAppear &&
                iExpectedLineNum > 0 ) 
            {
                BOOST_CHECK_MESSAGE(
                    iExpectedLineNum == iWarningLineNum,
                    "On warning check " << warning_test.m_sName << '('
                    << warn_test_idx << "): "
                    << "Type " << CFastaReader::CWarning::GetStringOfType(
                    eExpectedType) << ": "
                    << "Line num of warning should be " << iExpectedLineNum
                    << ", but it's " << iWarningLineNum);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(TestTitleRemovedIfEmpty)
{
    static const string kFastaWhereAllModsRemoved = 
        ">Seq1 [topology=circular]\n"
        "ACGTACGTACGTACGTACGTACGTACGTACGTACGT\n";
    CMemoryLineReader line_reader( kFastaWhereAllModsRemoved.c_str(),
        kFastaWhereAllModsRemoved.length() );
    CFastaReader fasta_reader( line_reader, CFastaReader::fAddMods );

    CRef<CSeq_entry> pSeqEntry = fasta_reader.ReadOneSeq();

    FOR_EACH_SEQDESC_ON_BIOSEQ(desc_it, pSeqEntry->GetSeq()) {
        BOOST_CHECK( ! (*desc_it)->IsTitle() );
    }
}

BOOST_AUTO_TEST_CASE(TestProteinSeqGapChar)
{
    static const string kFastaWithProtGap = 
        ">Dobi [organism=Canis familiaris] [breed=Doberman pinscher]\n"
        "MMMTGCMTGGGTMMMMGTMGTMGMMGMGMMGGCTTTTMGCCCMGMMGTMMTMCCCMTGTTTTCMGCMTTM\n"
        "GGMMMMMGGGCTGTTG\n"
        ">?unk100\n"
        "TGGMTGMCMGMMMCCTTGTTGGTCCMMMMTGCMMMCCCMGMTKGTMMGMCCMTTTTMMMMGCMTTGGGTC\n"
        "TTMGMMMTMGGGCMMCMCMGMMCMMMMMT\n"
        ">?234\n"
        "MMMMMTMMMMGCMTTMGTMGMMMTTTGTMCMGMMCTGGMMMMGGMMGGMMMMMTTTCMMMMMTTGGGCCT\n";

    CMemoryLineReader line_reader( kFastaWithProtGap.c_str(),
        kFastaWithProtGap.length() );
    CFastaReader fasta_reader( line_reader, 
        CFastaReader::fAddMods | 
        CFastaReader::fAssumeProt |  // Yes, assume prot even though you see only ACGT above
        CFastaReader::fUseIupacaa |
        CFastaReader::fNoSplit
        );

    BOOST_CHECK_NO_THROW(fasta_reader.ReadOneSeq());
}
