/*
 * Copyright 2015 Cloudius Systems
 */

/*
 * This file is part of Scylla.
 *
 * Scylla is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Scylla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Scylla.  If not, see <http://www.gnu.org/licenses/>.
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE core

#include <boost/test/unit_test.hpp>
#include "query-request.hh"
#include "schema_builder.hh"

static
bool includes_token(const schema& s, const query::partition_range& r, const dht::token& tok) {
    dht::ring_position_comparator cmp(s);

    if (r.is_wrap_around(cmp)) {
        auto sub = r.unwrap();
        return includes_token(s, sub.first, tok)
            || includes_token(s, sub.second, tok);
    }

    return !r.before(dht::ring_position(tok, dht::ring_position::token_bound::end), cmp)
        && !r.after(dht::ring_position(tok, dht::ring_position::token_bound::start), cmp);
}

BOOST_AUTO_TEST_CASE(test_range_with_positions_within_the_same_token) {
    auto s = schema_builder("ks", "cf")
        .with_column("key", bytes_type, column_kind::partition_key)
        .with_column("v", bytes_type)
        .build();

    dht::token tok = dht::global_partitioner().get_random_token();

    auto key1 = dht::decorated_key{tok,
                                   partition_key::from_single_value(*s, bytes_type->decompose(bytes("key1")))};

    auto key2 = dht::decorated_key{tok,
                                   partition_key::from_single_value(*s, bytes_type->decompose(bytes("key2")))};

    {
        auto r = query::partition_range::make(
            dht::ring_position(key2),
            dht::ring_position(key1));

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            dht::ring_position(key1),
            dht::ring_position(key2));

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.is_wrap_around(dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position(key2), false},
            {dht::ring_position(key1), false});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(!r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.contains(key2, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position(key1), false},
            {dht::ring_position(key2), false});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(!r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.contains(key2, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.is_wrap_around(dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position::starting_at(tok), false},
            {dht::ring_position(key2), true});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.is_wrap_around(dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position::ending_at(tok), false},
            {dht::ring_position(key2), true});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position::ending_at(tok), false},
            {dht::ring_position(key1), true});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position(key1), false},
            {dht::ring_position::starting_at(tok), true});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position::starting_at(tok), true},
            {dht::ring_position::ending_at(tok), true});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(!r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position::ending_at(tok), true},
            {dht::ring_position::starting_at(tok), true});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.contains(key2, dht::ring_position_comparator(*s)));
    }
}

BOOST_AUTO_TEST_CASE(test_range_with_equal_value_but_opposite_inclusiveness_is_a_full_wrap_around) {
    auto s = schema_builder("ks", "cf")
        .with_column("key", bytes_type, column_kind::partition_key)
        .with_column("v", bytes_type)
        .build();

    dht::token tok = dht::global_partitioner().get_random_token();

    auto key1 = dht::decorated_key{
        tok, partition_key::from_single_value(*s, bytes_type->decompose(bytes("key1")))};

    auto key2 = dht::decorated_key{
        tok, partition_key::from_single_value(*s, bytes_type->decompose(bytes("key2")))};

    {
        auto r = query::partition_range::make(
            {dht::ring_position::starting_at(tok), true},
            {dht::ring_position::starting_at(tok), false});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position::starting_at(tok), false},
            {dht::ring_position::starting_at(tok), true});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position::ending_at(tok), false},
            {dht::ring_position::ending_at(tok), true});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position::ending_at(tok), true},
            {dht::ring_position::ending_at(tok), false});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position(key1), true},
            {dht::ring_position(key1), false});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position(key1), false},
            {dht::ring_position(key1), true});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position(key1), false},
            {dht::ring_position(key1), false});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key2, dht::ring_position_comparator(*s)));
    }

    {
        auto r = query::partition_range::make(
            {dht::ring_position(key2), false},
            {dht::ring_position(key2), false});

        BOOST_REQUIRE(includes_token(*s, r, tok));
        BOOST_REQUIRE(r.is_wrap_around(dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(r.contains(key1, dht::ring_position_comparator(*s)));
        BOOST_REQUIRE(!r.contains(key2, dht::ring_position_comparator(*s)));
    }
}

BOOST_AUTO_TEST_CASE(test_range_contains) {
    auto cmp = [] (int i1, int i2) -> int { return i1 - i2; };

    auto check_contains = [&] (range<int> enclosing, range<int> enclosed) {
        BOOST_REQUIRE(enclosing.contains(enclosed, cmp));
        BOOST_REQUIRE(!enclosed.contains(enclosing, cmp));
    };

    BOOST_REQUIRE(range<int>({}, {}).contains(range<int>({}, {}), cmp));
    check_contains(range<int>({}, {}), range<int>({1}, {2}));
    check_contains(range<int>({}, {}), range<int>({}, {2}));
    check_contains(range<int>({}, {}), range<int>({1}, {}));
    check_contains(range<int>({}, {}), range<int>({2}, {1}));

    BOOST_REQUIRE(range<int>({}, {3}).contains(range<int>({}, {3}), cmp));
    BOOST_REQUIRE(range<int>({3}, {}).contains(range<int>({3}, {}), cmp));
    BOOST_REQUIRE(range<int>({}, {{3, false}}).contains(range<int>({}, {{3, false}}), cmp));
    BOOST_REQUIRE(range<int>({{3, false}}, {}).contains(range<int>({{3, false}}, {}), cmp));
    BOOST_REQUIRE(range<int>({1}, {3}).contains(range<int>({1}, {3}), cmp));
    BOOST_REQUIRE(range<int>({3}, {1}).contains(range<int>({3}, {1}), cmp));

    check_contains(range<int>({}, {3}), range<int>({}, {2}));
    check_contains(range<int>({}, {3}), range<int>({2}, {{3, false}}));
    BOOST_REQUIRE(!range<int>({}, {3}).contains(range<int>({}, {4}), cmp));
    BOOST_REQUIRE(!range<int>({}, {3}).contains(range<int>({2}, {{4, false}}), cmp));
    BOOST_REQUIRE(!range<int>({}, {3}).contains(range<int>({}, {}), cmp));

    check_contains(range<int>({3}, {}), range<int>({4}, {}));
    check_contains(range<int>({3}, {}), range<int>({{3, false}}, {}));
    check_contains(range<int>({3}, {}), range<int>({3}, {4}));
    check_contains(range<int>({3}, {}), range<int>({4}, {5}));
    BOOST_REQUIRE(!range<int>({3}, {}).contains(range<int>({2}, {4}), cmp));
    BOOST_REQUIRE(!range<int>({3}, {}).contains(range<int>({}, {}), cmp));

    check_contains(range<int>({}, {{3, false}}), range<int>({}, {2}));
    BOOST_REQUIRE(!range<int>({}, {{3, false}}).contains(range<int>({}, {3}), cmp));
    BOOST_REQUIRE(!range<int>({}, {{3, false}}).contains(range<int>({}, {4}), cmp));

    check_contains(range<int>({1}, {3}), range<int>({1}, {2}));
    check_contains(range<int>({1}, {3}), range<int>({1}, {1}));
    BOOST_REQUIRE(!range<int>({1}, {3}).contains(range<int>({2}, {4}), cmp));
    BOOST_REQUIRE(!range<int>({1}, {3}).contains(range<int>({0}, {1}), cmp));
    BOOST_REQUIRE(!range<int>({1}, {3}).contains(range<int>({0}, {4}), cmp));

    check_contains(range<int>({3}, {1}), range<int>({0}, {1}));
    check_contains(range<int>({3}, {1}), range<int>({3}, {4}));
    check_contains(range<int>({3}, {1}), range<int>({}, {1}));
    check_contains(range<int>({3}, {1}), range<int>({}, {{1, false}}));
    check_contains(range<int>({3}, {1}), range<int>({3}, {}));
    check_contains(range<int>({3}, {1}), range<int>({{3, false}}, {}));
    check_contains(range<int>({3}, {1}), range<int>({{3, false}}, {{1, false}}));
    check_contains(range<int>({3}, {1}), range<int>({{3, false}}, {1}));
    check_contains(range<int>({3}, {1}), range<int>({3}, {{1, false}}));
    BOOST_REQUIRE(!range<int>({3}, {1}).contains(range<int>({2}, {2}), cmp));
    BOOST_REQUIRE(!range<int>({3}, {1}).contains(range<int>({2}, {{3, false}}), cmp));
    BOOST_REQUIRE(!range<int>({3}, {1}).contains(range<int>({{1, false}}, {{3, false}}), cmp));
    BOOST_REQUIRE(!range<int>({3}, {1}).contains(range<int>({{1, false}}, {3}), cmp));
}

struct unsigned_comparator {
    int operator()(unsigned u1, unsigned u2) const {
        return (u1 > u2 ? 1 : (u1 == u2 ? 0 : -1));
    }
};

BOOST_AUTO_TEST_CASE(range_overlap_tests) {
    unsigned min = 0;
    unsigned max = std::numeric_limits<unsigned>::max();

    auto range0 = range<unsigned>::make(max, max);
    auto range1 = range<unsigned>::make(min, max);
    BOOST_REQUIRE(range0.overlap(range1, unsigned_comparator()) == true);
    BOOST_REQUIRE(range1.overlap(range1, unsigned_comparator()) == true);
    BOOST_REQUIRE(range1.overlap(range0, unsigned_comparator()) == true);

    auto range2 = range<unsigned>::make(1, max);
    BOOST_REQUIRE(range1.overlap(range2, unsigned_comparator()) == true);

    auto range3 = range<unsigned>::make(min, max-2);
    BOOST_REQUIRE(range2.overlap(range3, unsigned_comparator()) == true);

    auto range4 = range<unsigned>::make(2, 10);
    auto range5 = range<unsigned>::make(12, 20);
    auto range6 = range<unsigned>::make(22, 40);
    BOOST_REQUIRE(range4.overlap(range5, unsigned_comparator()) == false);
    BOOST_REQUIRE(range5.overlap(range4, unsigned_comparator()) == false);
    BOOST_REQUIRE(range4.overlap(range6, unsigned_comparator()) == false);

    auto range7 = range<unsigned>::make(2, 10);
    auto range8 = range<unsigned>::make(10, 20);
    auto range9 = range<unsigned>::make(min, 100);
    BOOST_REQUIRE(range7.overlap(range8, unsigned_comparator()) == true);
    BOOST_REQUIRE(range6.overlap(range8, unsigned_comparator()) == false);
    BOOST_REQUIRE(range8.overlap(range9, unsigned_comparator()) == true);

    // wrap around checks
    auto range10 = range<unsigned>::make(25, 15);
    BOOST_REQUIRE(range9.overlap(range10, unsigned_comparator()) == true);
    BOOST_REQUIRE(range10.overlap(range<unsigned>({20}, {18}), unsigned_comparator()) == true);
    auto range11 = range<unsigned>({}, {2});
    BOOST_REQUIRE(range11.overlap(range10, unsigned_comparator()) == true);
    auto range12 = range<unsigned>::make(18, 20);
    BOOST_REQUIRE(range12.overlap(range10, unsigned_comparator()) == false);

    BOOST_REQUIRE(range<unsigned>({1}, {{2, false}}).overlap(range<unsigned>({2}, {3}), unsigned_comparator()) == false);

    // open and infinite bound checks
    BOOST_REQUIRE(range<unsigned>({1}, {}).overlap(range<unsigned>({2}, {3}), unsigned_comparator()) == true);
    BOOST_REQUIRE(range<unsigned>({5}, {}).overlap(range<unsigned>({2}, {3}), unsigned_comparator()) == false);
    BOOST_REQUIRE(range<unsigned>({}, {{3, false}}).overlap(range<unsigned>({2}, {3}), unsigned_comparator()) == true);
    BOOST_REQUIRE(range<unsigned>({}, {{2, false}}).overlap(range<unsigned>({2}, {3}), unsigned_comparator()) == false);
    BOOST_REQUIRE(range<unsigned>({}, {2}).overlap(range<unsigned>({2}, {}), unsigned_comparator()) == true);
    BOOST_REQUIRE(range<unsigned>({}, {2}).overlap(range<unsigned>({3}, {4}), unsigned_comparator()) == false);

    // [3,4] and [4,5]
    BOOST_REQUIRE(range<unsigned>({3}, {4}).overlap(range<unsigned>({4}, {5}), unsigned_comparator()) == true);
    // [3,4) and [4,5]
    BOOST_REQUIRE(range<unsigned>({3}, {{4, false}}).overlap(range<unsigned>({4}, {5}), unsigned_comparator()) == false);
    // [3,4] and (4,5]
    BOOST_REQUIRE(range<unsigned>({3}, {4}).overlap(range<unsigned>({{4, false}}, {5}), unsigned_comparator()) == false);
    // [3,4) and (4,5]
    BOOST_REQUIRE(range<unsigned>({3}, {{4, false}}).overlap(range<unsigned>({{4, false}}, {5}), unsigned_comparator()) == false);
}
