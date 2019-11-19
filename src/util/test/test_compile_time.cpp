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
 * Authors:  Sergiy Gotvyanskyy
 *
 * File Description:
 *   Test for ct::const_unordered_map
 *            ct::const_unordered_set
 *
 */

#include <ncbi_pch.hpp>

#include <util/compile_time.hpp>
//#include <util/cpubound.hpp>
//#include <util/impl/getmember.h>
#include <array>
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
        "compile time const_map Unit Test");
}

MAKE_TWOWAY_CONST_MAP(test_two_way1, ncbi::NStr::eNocase, const char*, const char*,
    {
        {"SO:0000001", "region"},
        {"SO:0000002", "sequece_secondary_structure"},
        {"SO:0000005", "satellite_DNA"},
        {"SO:0000013", "scRNA"},
        {"SO:0000035", "riboswitch"},
        {"SO:0000036", "matrix_attachment_site"},
        {"SO:0000037", "locus_control_region"},
        {"SO:0000104", "polypeptide"},
        {"SO:0000110", "sequence_feature"},
        {"SO:0000139", "ribosome_entry_site"}
    });


MAKE_TWOWAY_CONST_MAP(test_two_way2, ncbi::NStr::eNocase, const char*, int,
    {
        {"SO:0000001", 1},
        {"SO:0000002", 2},
        {"SO:0000005", 5},
        {"SO:0000013", 13},
        {"SO:0000035", 35}
    });

//test_two_way2_init

using bitset = ct::const_bitset<64 * 3, int>;
using bitset_pair = ct::const_pair<int, bitset>;

BOOST_AUTO_TEST_CASE(TestConstBitset)
{
    // this test ensures various approaches of instantiating const bitsets
//#define constexpr
    constexpr bitset t1{ 1, 10, 20, 30 };

    constexpr bitset t2{ 1, 10, 20, 30 };

    constexpr bitset t3[] = { { 5,10}, {15, 10} };
    constexpr bitset_pair t4{ 4, t1 };
    constexpr bitset_pair t5(t4);
    constexpr bitset_pair t6[] = { {4, t1}, {5, t2} };
    constexpr bitset_pair t7[] = { {4, {1, 2}}, {5, t3[0]} };

    constexpr bitset t8 = bitset::set_range(5, 12);

    BOOST_CHECK(t1.test(10));
    BOOST_CHECK(t2.test(20));
    BOOST_CHECK(!t3[1].test(11));
    BOOST_CHECK(!t4.second.test(11));
    BOOST_CHECK(t5.second.test(10));
    BOOST_CHECK(t6[1].second.test(1));
    BOOST_CHECK(t7[0].second.test(2));
    BOOST_CHECK(t8.test(5));
    BOOST_CHECK(t8.test(12));
    BOOST_CHECK(!t8.test(4));
    BOOST_CHECK(!t8.test(13));

    // array of pairs, all via aggregate initialization
    constexpr 
    bitset_pair t9[] = {
        {4,                //first
            {5, 6, 7, 8}}, //second
        {5,                //first
            {4, 5}}        //second
    };

    BOOST_CHECK(t9[1].second.test(5));
#ifdef constexpr
#undef constexpr
#endif
}

BOOST_AUTO_TEST_CASE(TestConstMap)
{
#if 1
    for (auto& rec : test_two_way1)
        std::cout << rec.first.m_hash << ":" << rec.first << ":" << rec.second << std::endl;

    auto t1 = test_two_way1.find("SO:0000001");
    BOOST_CHECK((t1 != test_two_way1.end()) && (ncbi::NStr::CompareCase(t1->second, "region") == 0));

    auto t2 = test_two_way1.find("SO:0000003");
    BOOST_CHECK(t2 == test_two_way1.end());

    auto t3 = test_two_way1_flipped.find("RegioN");
    BOOST_CHECK((t3 != test_two_way1_flipped.end()) && (ncbi::NStr::CompareNocase(t3->second, "so:0000001") == 0));

    assert(test_two_way1.size() == 10);
    BOOST_CHECK(test_two_way1_flipped.size() == 10);

    auto t4 = test_two_way2.find("SO:0000002");
    BOOST_CHECK((t4 != test_two_way2.end()) && (t4->second == 2));

    auto t5 = test_two_way2.find("SO:0000003");
    BOOST_CHECK((t5 == test_two_way2.end()));

    auto t6 = test_two_way2_flipped.find(5);
    BOOST_CHECK((t6 != test_two_way2_flipped.end()) && (ncbi::NStr::CompareNocase(t6->second, "so:0000005") == 0));
#endif
};

BOOST_AUTO_TEST_CASE(TestConstSet)
{
    MAKE_CONST_SET(ts1, ncbi::NStr::eCase, const char*,
      { "a2", "a1", "a3" });

    MAKE_CONST_SET(ts2, ncbi::NStr::eNocase, const char*,
      { "b1", "b3", "B2"});

    for (auto r : ts1)
    {
        std::cout << r << std::endl;
    }
    for (auto r : ts2)
    {
        std::cout << r << std::endl;
    }

    BOOST_CHECK(ts1.find("A1") == ts1.end());
    BOOST_CHECK(ts1.find(std::string("A1")) == ts1.end());
    BOOST_CHECK(ts1.find("a1") != ts1.end());
    BOOST_CHECK(ts1.find(std::string("a1")) != ts1.end());
    BOOST_CHECK(ts1.find("a2") != ts1.end());
    BOOST_CHECK(ts1.find("a3") != ts1.end());
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a1"), "a1") == 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a2"), "a2") == 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a3"), "a3") == 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a1"), "b1") != 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a2"), "b2") != 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a3"), "b3") != 0);

    BOOST_CHECK(ts2.find("A1") == ts2.end());
    BOOST_CHECK(ts2.find("a1") == ts2.end());
    BOOST_CHECK(ts2.find("b1") != ts2.end());
    BOOST_CHECK(ts2.find("B1") != ts2.end());
    BOOST_CHECK(ts2.find("b2") != ts2.end());
    BOOST_CHECK(ts2.find("B2") != ts2.end());
    BOOST_CHECK(ts2.find("b3") != ts2.end());
    BOOST_CHECK(ts2.find("B3") != ts2.end());

    auto b1_it = ts2.find("B1");
    BOOST_CHECK((*b1_it) == std::string("b1"));
    BOOST_CHECK((*b1_it) == std::string("B1"));
};

BOOST_AUTO_TEST_CASE(TestCRC32)
{
    using hash_type = ct::SaltedCRC32<ncbi::NStr::eCase>::type;

    constexpr hash_type good_cs = 0x38826fa7;
    constexpr hash_type good_ncs = 0xefa77b2c;
    constexpr auto hash_good_cs = ct::SaltedCRC32<ncbi::NStr::eCase>::ct("Good");
    constexpr auto hash_good_ncs = ct::SaltedCRC32<ncbi::NStr::eNocase>::ct("Good");

    auto hash_good_cs_rt = ct::SaltedCRC32<ncbi::NStr::eCase>::rt("Good", 4);
    auto hash_good_ncs_rt = ct::SaltedCRC32<ncbi::NStr::eNocase>::rt("Good", 4);

    static_assert(hash_good_cs != hash_good_ncs, "not good");
    static_assert(good_cs == hash_good_cs, "not good");
    static_assert(good_ncs == hash_good_ncs, "not good");

    std::cout << std::hex
              << "Hash values:" << std::endl
              << hash_good_cs << std::endl
              << hash_good_ncs << std::endl
              << hash_good_cs_rt << std::endl
              << hash_good_ncs_rt << std::endl
              << good_cs << std::endl
              << good_ncs << std::endl
              << std::dec
    ;

    static_assert(
        ct::SaltedCRC32<ncbi::NStr::eNocase>::ct("Good") == ct::SaltedCRC32<ncbi::NStr::eCase>::ct("good"),
        "not good");

    BOOST_CHECK(hash_good_cs  == hash_good_cs_rt);
    BOOST_CHECK(hash_good_ncs == hash_good_ncs_rt);
}

struct Implementation_via_type
{
    struct member
    {
        static const char* call()
        {
            return "member call";
        };
        const char* nscall() const
        {
            return "ns member call";
        };
    };
    struct general
    {
        static constexpr int value = 111;
        static const char* call()
        {
            return "general call";
        };
        const char* nscall() const
        {
            return "ns general call";
        };
    };
    struct sse41
    {
        static constexpr int value = 222;
        static const char* call()
        {
            return "sse41 call";
        };
        const char* nscall() const
        {
            return "ns sse41 call";
        };
    };
    struct avx
    {
        static constexpr int value = 333;
        static const char* call()
        {
            return "avx call";
        };
        const char* nscall() const
        {
            return "ns avx call";
        };
    };
};

struct Implementation_via_method
{
    static const char* member()
    {
        return "general method";
    };
    static const char* general()
    {
        return "general method";
    };
    static const char* sse41()
    {
        return "sse41 method";
    };
    static const char* avx()
    {
        return "avx";
    };
};

template<class _Impl>
struct RebindClass
{
    static std::string rebind(int a)
    {
        auto s1 = _Impl{}.nscall(); // non static access to _Impl class
        auto s2 = _Impl::call();
        return std::string("rebind class + ") + s1 + " + " + s2;
    };
};

template<class _Impl>
struct RebindDirect
{
    static std::string rebind(int a)
    {
        auto s = _Impl{}(); //call via operator()
        return std::string("rebind direct + ") + s;
    };
};

struct NothingImplemented
{
};

template<typename I>
struct empty_rebind
{
    using type = I;
};

#ifdef __CPUBOUND_HPP_INCLUDED__
BOOST_AUTO_TEST_CASE(TestCPUCaps)
{
    using Type0 = cpu_bound::TCPUSpecific<NothingImplemented, const char*>;
    using Type1 = cpu_bound::TCPUSpecific<Implementation_via_method, const char*>;
    using Type2 = cpu_bound::TCPUSpecific<Implementation_via_type, const char*>;
    using Type3 = cpu_bound::TCPUSpecificEx<RebindDirect, Implementation_via_method, std::string, int>;
    using Type4 = cpu_bound::TCPUSpecificEx<RebindClass,  Implementation_via_type, std::string, int>;

    Type0 inst0;
    Type1 inst1;
    Type2 inst2;
    Type3 inst3;
    Type4 inst4;

    //auto r = inst0();  it should assert

    std::string res1 = inst1();
    BOOST_CHECK(res1 == "sse41 method");
    std::string res2 = inst2();
    BOOST_CHECK(res2 == "sse41 call");
    std::string res3 = inst3(3);
    BOOST_CHECK(res3 == "rebind direct + sse41 method");
    std::string res4 = inst4(3);
    BOOST_CHECK(res4 == "rebind class + ns sse41 call + sse41 call");
}
#else
#if 0
BOOST_AUTO_TEST_CASE(TestCPUCaps)
{
    using T0 = GetMember<empty_rebind, NothingImplemented>;
    using T1 = GetMember<empty_rebind, Implementation_via_type>;
    using T2 = GetMember<RebindClass, Implementation_via_type>;
    using T3 = GetMember<empty_rebind, Implementation_via_method>;
    using T4 = GetMember<RebindDirect, Implementation_via_method>;

    //size_t n0 = sizeof(decltype(T0::Derived::member));
    //size_t n1 = sizeof(decltype(T1::Derived::member));
    std::cout << (T0::f == nullptr) << std::endl;
    std::cout << (T1::f == nullptr) << std::endl;
    //std::cout << (T2::f == nullptr) << std::endl;
    //std::cout << (T3::f == nullptr) << std::endl;
    //std::cout << (T4::f == nullptr) << std::endl;
}
#endif

#endif

#if 0
#include <unordered_set>

#ifdef NCBI_OS_MSWIN
#include <intrin.h>
#endif

class CTimeLapsed
{
private:
    uint64_t started = __rdtsc();
public:
    uint64_t lapsed() const
    {
        return __rdtsc() - started;
    };
};

class CTestPerformance
{
public:
    using string_set = std::set<std::string>;
    using unordered_set = std::unordered_set<std::string>;
    using hash_vector = std::vector<uint32_t>;
    using hash_t = cpu_bound::TCPUSpecific<ct::SaltedCRC32<ncbi::NStr::eCase>, uint32_t, const char*, size_t>;
    static constexpr size_t size = 16384;
    //static constexpr size_t search_size = size / 16;

    string_set   m_set;
    unordered_set m_hash_set;
    hash_vector  m_vector;
    std::vector<std::string> m_pool;
    hash_t m_hash;

    void FillData()
    {
        char buf[32];
        m_pool.reserve(size);
        m_vector.reserve(size);
        for (size_t i = 0; i < size; ++i)
        {
            size_t len = 5 + rand() % 10;
            for (size_t l = 0; l < len; ++l)
                buf[l] = '@' + rand() % 64;
            buf[len] = 0;
            m_pool.push_back(buf);
            m_set.emplace(buf);
            m_hash_set.emplace(buf);
            auto h = m_hash(buf, len);
            m_vector.push_back(h);
        }
        std::sort(m_vector.begin(), m_vector.end());
    }

    std::pair<uint64_t, uint64_t> TestMap()
    {
        CTimeLapsed lapsed;
        uint64_t sum = 0;
        for (auto it : m_pool)
        {
            auto found = m_set.find(it);
            sum += found->size();
        }

        return { sum, lapsed.lapsed() };
    }

    std::pair<uint64_t, uint64_t> TestUnordered()
    {
        CTimeLapsed lapsed;
        uint64_t sum = 0;
        for (auto it : m_pool)
        {
            auto found = m_hash_set.find(it);
            sum += found->size();
        }

        return { sum, lapsed.lapsed() };
    }

    std::pair<uint64_t, uint64_t> TestHashes()
    {
        CTimeLapsed lapsed;
        uint64_t sum = 0;
        for (auto it : m_pool)
        {
            auto h = m_hash(it.data(), it.size());
            auto found = std::lower_bound(m_vector.begin(), m_vector.end(), h);
            sum += *found;
        }

        return { sum, lapsed.lapsed() };
    }

    void RunTest()
    {
        FillData();
        auto res1 = TestMap();
        auto res2 = TestHashes();
        auto res3 = TestUnordered();

        std::cout << res1.second/1000 
            << " vs " << res2.second/1000 
            << " vs " << res3.second/1000
            << std::endl;
    }
};

BOOST_AUTO_TEST_CASE(TestPerformance)
{
    CTestPerformance test;
    test.RunTest();
}
#endif

#if 0

using table_type = ct::const_table<ncbi::NStr::ECase::eCase, const char*, int, int>;
using D01_t = table_type::MakeIndex<0, 1>;
using D10_t = table_type::MakeIndex<1, 0>;
using index01_t = D01_t::index_type;
using index10_t = D10_t::index_type;


constexpr 
table_type::row_type init_values[] = {
{ "Test1", 80, 200},
{ "Test2", 70, 300},
{ "Test3", 60, 300},
{ "Test4", 50, 300},
{ "Test5", 40, 300},
{ "Test6", 30, 300},
{ "Test7", 20, 300},
{ "Test8", 10, 300}
};

constexpr auto container01 = D01_t{}(init_values);
constexpr index01_t index01 = container01;

//alignas(std::hardware_constructive_interference_size)
constexpr auto container10 = D10_t{}(init_values);
constexpr index10_t index10 = container10;

BOOST_AUTO_TEST_CASE(TestPerformance)
{
    //std::cout << container01.first.size() << std::endl;
    //std::cout << container01.second.size() << std::endl;

    std::cout << std::hex << &container01.first << std::endl;
    std::cout << std::hex << &container10.first << std::endl;
    std::cout << container01.first[0] << std::endl;

    auto test1 = index01.at("Test3");
    auto test2 = index01.at("Test7");
    std::cout << test1 << ":" << test2 << std::endl;
    auto test3 = index10.at(60);
    std::cout << test3 << std::endl;
}

#endif

template<typename _Traits, bool remove_duplicates>
class TInsertSorter
{
public:

    template<typename _Indices, typename _Value>
    static constexpr void insert_down(_Indices& indices, size_t head, size_t tail, _Value current)
    {
        auto saved = current;
        while (head != tail)
        {
            auto prev = tail--;
            indices[prev] = indices[tail];
        }
        indices[head] = saved;
    }
    template<typename _Indices, typename _Input>
    static constexpr size_t const_lower_bound(const _Indices& indices, const _Input& input, size_t last, size_t value)
    {
        size_t _UFirst = 0;
        auto _Count = last;

        while (0 < _Count)
        {	// divide and conquer, find half that contains answer
            const auto _Count2 = _Count >> 1; // TRANSITION, VSO#433486
            const auto _UMid = _UFirst + _Count2;
            if (_Traits::compare_less(input, indices[_UMid], value))
            {	// try top half
                _UFirst = (_UMid + 1); // _Next_iter(_UMid);
                _Count -= _Count2 + 1;
            }
            else
            {
                _Count = _Count2;
            }
        }

        return _UFirst;
    }
    template<typename _Indices, typename _Input>
    static constexpr size_t insert_sort_indices(_Indices& result, const _Input& input)
    {
        auto size = result.size();
        if (size < 2)
            return size;

        // current is the first element of the unsorted part of the array
        auto current = 0;
        // the last inserted element into sorted part of the array
        auto last = current;
        result[0] = 0;
        current++;

        while (current != result.size())
        {
            if (_Traits::compare_less(input, result[last], current))
            {// optimization for presorted arrays
                result[++last] = current;
            }
            else {
                // we may exclude last element since it's already known as smaller then current
                auto fit = const_lower_bound(result, input, last, current);
                bool move_it = remove_duplicates;
                if (remove_duplicates)
                {
                    move_it = _Traits::compare_less(input, current, result[fit]);
                }
                if (move_it)
                {
                    ++last;
                    insert_down(result, fit, last, current);
                }
            }
            ++current;
        }
        if (remove_duplicates)
        {// fill the rest of the indices with maximum value
            current = last;
            while (++current != result.size())
            {
                result[current] = result[last];
            }
        }
        return 1 + last;
    }

    template <class T, std::size_t... I>
    static constexpr auto fill_array(std::index_sequence<I...>)
    {
        return ct::const_array<std::remove_cv_t<T>, sizeof...(I)>{ {(I+1000)...} };
    }

    template<typename T, size_t N>
    static constexpr auto make_indices(const T(&input)[N])
    {
        ct::const_array<size_t, N> indices{};
        //auto indices = fill_array<size_t>(std::make_index_sequence<N>{});
        auto real_size = insert_sort_indices(indices, input);
        return std::make_pair(real_size, indices);
    }

    template<typename _Input, typename _Indices, std::size_t... I>
    static constexpr auto construct(const _Input& input, const _Indices& indices, std::index_sequence<I...>) 
        -> ct::const_array<typename _Traits::value_type, sizeof...(I)>
    {
        auto real_size = indices.first;
        auto _max = indices.second[real_size - 1];
        return { { _Traits::construct(input[I < real_size ? indices.second[I] : _max]) ...} };
    }
    template<typename T, size_t N>
    constexpr auto operator()(const T(&input)[N])
    {
        auto indices = make_indices(input);
        return std::make_pair(indices.first, construct(input, indices, std::make_index_sequence<N>{}));
    }
};

BOOST_AUTO_TEST_CASE(TestSorter)
{
    using type = ct::MakeConstMap<const char*, const char*, ncbi::NStr::ECase::eNocase, ct::TwoWayMap::yes>;
    using sorter = TInsertSorter<ct::straight_sort_traits<type::init_pair_t>, true>;
    using flipped_sorter = TInsertSorter<ct::flipped_sort_traits<type::init_pair_t>, true>;
    constexpr type::init_pair_t init[] =
    {
        //  ----------------------------------------------------------------------------
        {"SO:0000001", "region"},
        {"SO:0000002", "sequece_secondary_structure"},
        {"SO:0000005", "satellite_DNA"},
        {"SO:0000013", "scRNA"},
        {"SO:0000035", "riboswitch"},
        {"SO:0000036", "matrix_attachment_site"},
        {"SO:0000037", "locus_control_region"},
        {"SO:0000104", "polypeptide"},
        {"SO:0000110", "sequence_feature"},
        {"SO:0000139", "ribosome_entry_site"},
        {"SO:0000140", "attenuator"},
        {"SO:0000141", "terminator"},
        {"SO:0000147", "exon"},
        {"SO:0000165", "enhancer"},
        {"SO:0000167", "promoter"},
        {"SO:0000172", "CAAT_signal"},
        {"SO:0000173", "GC_rich_promoter_region"},
        {"SO:0000174", "TATA_box"},
        {"SO:0000175", "minus_10_signal"},
        {"SO:0000176", "minus_35_signal"},
        {"SO:0000178", "operon"},
        {"SO:0000185", "primary_transcript"},
        {"SO:0000188", "intron"},
        {"SO:0000204", "five_prime_UTR"},
        {"SO:0000205", "three_prime_UTR"},
        {"SO:0000234", "mRNA"},
        {"SO:0000252", "rRNA"},
        {"SO:0000253", "tRNA"},
        {"SO:0000274", "snRNA"},
        {"SO:0000275", "snoRNA"},
        {"SO:0000276", "miRNA"},
        {"SO:0000286", "long_terminal_repeat"},
        {"SO:0000289", "microsatellite"},
        {"SO:0000294", "inverted_repeat"},
        {"SO:0000296", "origin_of_replication"},
        {"SO:0000297", "D_loop"},
        {"SO:0000298", "recombination_feature"},
        {"SO:0000305", "modified_DNA_base"},
        {"SO:0000313", "stem_loop"},
        {"SO:0000314", "direct_repeat"},
        {"SO:0000315", "TSS"},
        {"SO:0000316", "CDS"},
        {"SO:0000330", "conserved_region"},
        {"SO:0000331", "STS"},
        {"SO:0000336", "pseudogene"},
        {"SO:0000374", "ribozyme"},
        {"SO:0000380", "hammerhead_ribozyme"},
        {"SO:0000385", "RNase_MRP_RNA"},
        {"SO:0000386", "RNase_P_RNA"},
        {"SO:0000404", "vault_RNA"},
        {"SO:0000405", "Y_RNA"},
        {"SO:0000409", "binding_site"},
        {"SO:0000410", "protein_binding_site"},
        {"SO:0000413", "sequence_difference"},
        {"SO:0000418", "signal_peptide"},
        {"SO:0000419", "mature_protein_region"},
        {"SO:0000433", "non_LTR_retrotransposon_polymeric_tract"},
        {"SO:0000454", "rasiRNA"},
        {"SO:0000458", "D_gene_segment"},
        {"SO:0000466", "V_gene_segment"},
        {"SO:0000470", "J_gene_segment"},
        {"SO:0000478", "C_gene_segment"},
        {"SO:0000507", "pseudogenic_exon"},
        {"SO:0000516", "pseudogenic_transcript"},
        {"SO:0000551", "polyA_signal_sequence"},
        {"SO:0000553", "polyA_site"},
        {"SO:0000577", "centromere"},
        {"SO:0000584", "tmRNA"},
        {"SO:0000588", "autocatalytically_spliced_intron"},
        {"SO:0000590", "SRP_RNA"},
        {"SO:0000602", "guide_RNA"},
        {"SO:0000624", "telomere"},
        {"SO:0000625", "silencer"},
        {"SO:0000627", "insulator"},
        {"SO:0000644", "antisense_RNA"},
        {"SO:0000646", "siRNA"},
        {"SO:0000655", "ncRNA"},
        {"SO:0000657", "repeat_region"},
        {"SO:0000658", "dispersed_repeat"},
        {"SO:0000673", "transcript"},
        {"SO:0000685", "DNAsel_hypersensitive_site"},
        {"SO:0000704", "gene"},
        {"SO:0000705", "tandem_repeat"},
        {"SO:0000714", "nucleotide_motif"},
        {"SO:0000723", "iDNA"},
        {"SO:0000724", "oriT"},
        {"SO:0000725", "transit_peptide"},
        {"SO:0000730", "gap"},
        {"SO:0000777", "pseudogenic_rRNA"},
        {"SO:0000778", "pseudogenic_tRNA"},
        {"SO:0001021", "chromosome_preakpoint"},
        {"SO:0001035", "piRNA"},
        {"SO:0001037", "mobile_genetic_element"},
        {"SO:0001055", "transcriptional_cis_regulatory_region"},
        {"SO:0001059", "sequence_alteration"},
        {"SO:0001062", "propeptide"},
        {"SO:0001086", "sequence_uncertainty"},
        {"SO:0001087", "cross_link"},
        {"SO:0001088", "disulfide_bond"},
        {"SO:0001268", "recoding_stimulatory_region"},
        {"SO:0001411", "biological_region"},
        {"SO:0001484", "X_element_combinatorical_repeat"},
        {"SO:0001485", "Y_prime_element"},
        {"SO:0001496", "telomeric_repeat"},
        {"SO:0001649", "nested_repeat"},
        {"SO:0001682", "replication_regulatory_region"},
        {"SO:0001720", "epigenetically_modified_region"},
        {"SO:0001797", "centromeric_repeat"},
        {"SO:0001833", "V_region"},
        {"SO:0001835", "N_region"},
        {"SO:0001836", "S_region"},
        {"SO:0001877", "lnc_RNA"},
        {"SO:0001917", "CAGE_cluster"},
        {"SO:0002020", "boundary_element"},
        {"SO:0002072", "sequence_comparison"},
        {"SO:0002087", "pseudogenic_CDS"},
        {"SO:0002094", "non_allelic_homologous_recombination_region"},
        {"SO:0002154", "mitotic_recombination_region"},
        {"SO:0002155", "meiotic_recombination_region"},
        {"SO:0002190", "enhancer_blocking_element"},
        {"SO:0002191", "imprinting_control_region"},
        {"SO:0002205", "response_element"},
        {"SO:0005836", "regulatory_region"},
        {"SO:0005850", "primary_binding_site"},

        {"SO:0000000", ""},
        //{"SO:UNKNOWN", "replication_start_site"},
        //{"SO:UNKNOWN", "nucleotide_site"},
        //{"SO:UNKNOWN", "nucleotide_cleavage_site"},
        //{"SO:UNKNOWN", "repeat_instability_region"},
    };

#if 0
    auto sorted_indices = sorter::make_indices(init);
    for (auto rec : sorted_indices)
    {
        std::cout << rec << std::endl;
    }
#endif
    constexpr auto straight_sorted = sorter{}(init);
    constexpr auto flipped_sorted = flipped_sorter{}(init);
    static_assert(straight_sorted.first == straight_sorted.second.size(), "the array is not unique");
    static_assert(flipped_sorted.first  == flipped_sorted.second.size(), "the array is not unique");
    constexpr auto straight_map = type{}(straight_sorted.second);
    constexpr auto flipped_map = type{}(flipped_sorted.second);
    for (auto rec : init)
    {
        ncbi::CTempString s1 = rec.first;
        ncbi::CTempString s2 = rec.second;
        auto found1 = straight_map.find(s1);
        BOOST_CHECK(found1 != straight_map.end());
        auto found2 = flipped_map.find(s2);
        BOOST_CHECK(found2 != flipped_map.end());
        //std::cout << rec.first << ":" << rec.second << std::endl;
    }

    {
        constexpr int nu_numbers[] = { 100, 200, 300, 400, 500, 600 };
        using nu_sorter = TInsertSorter<ct::simple_sort_traits<int>, true>;
        constexpr auto sorted = nu_sorter{}(nu_numbers);
        static_assert(sorted.first == sorted.second.size(), "the array is not unique");
        for (auto rec : sorted.second)
        {
            std::cout << rec << std::endl;
        }
    }
    //static_assert(!remove_duplicates || real_size == N, "the array is not unique");

    {
        constexpr int nu_numbers[] = { 100, 200, 300, 200, 300, 100 };
        using nu_sorter = TInsertSorter<ct::simple_sort_traits<int>, true>;
        constexpr auto sorted = nu_sorter{}(nu_numbers);
        //static_assert(sorted.first == sorted.second.size(), "the array is not unique");
        for (auto rec : sorted.second)
        {
            std::cout << rec << std::endl;
        }
    }
}

