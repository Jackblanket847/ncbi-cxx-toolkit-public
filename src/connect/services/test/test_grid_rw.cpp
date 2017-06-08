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
 * Authors: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/test_boost.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/stream_utils.hpp>

#include <connect/ncbi_socket.hpp>

#include "../netschedule_api_submitter.cpp"

#include <random>

USING_NCBI_SCOPE;

BOOST_AUTO_TEST_SUITE(GridRwImpl)

const char kAllowedChars[] = "0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Is used to generate random strings
struct SStringGenerator
{
public:
    SStringGenerator();

    string GetString(size_t length);

    mt19937 m_Generator;

private:
    uniform_int_distribution<size_t> m_AllowedCharsRange;
};

SStringGenerator::SStringGenerator() :
    m_Generator(random_device()()),
    m_AllowedCharsRange(0, sizeof(kAllowedChars) - 2)
{
}

string SStringGenerator::GetString(size_t length)
{
    auto get_char = [&]() { return kAllowedChars[m_AllowedCharsRange(m_Generator)]; };

    string result;
    generate_n(back_inserter(result), length, get_char);
    return result;
}

NCBITEST_AUTO_INIT()
{
    // Set client IP so AppLog entries could be filtered by it
    CRequestContext& req = CDiagContext::GetRequestContext();
    unsigned addr = CSocketAPI::GetLocalHostAddress();
    req.SetClientIP(CSocketAPI::ntoa(addr));
}

#ifdef NCBI_THREADS
BOOST_AUTO_TEST_CASE(OutputCopy)
{
    const size_t kLength = 10 * 1024 * 1024;

    SStringGenerator generator;

    string source(generator.GetString(kLength));
    CStringReader reader(source);
    ostringstream writer;

    SOutputCopy copy(reader, writer);

    string dest(writer.str());
    BOOST_CHECK_EQUAL(source.size(), dest.size());
    BOOST_CHECK(source == dest);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
