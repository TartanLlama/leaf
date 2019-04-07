// Copyright (c) 2018-2019 Emil Dotchevski
// Copyright (c) 2018-2019 Second Spectrum, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

namespace boost { namespace mp11 { } }
namespace boost { using namespace mp11; }
#include <boost/leaf/handle_error.hpp>
#include <boost/variant2/expected.hpp>
#include "_test_ec.hpp"
#include "boost/core/lightweight_test.hpp"

namespace leaf = boost::leaf;
using boost::variant2::expected;
using boost::variant2::unexpected_;

namespace boost { namespace leaf {
	template <class T,class E>
	struct is_result_type<expected<T,E>>: std::true_type { };
} }

template <int> struct info { int value; };

expected<int,std::error_code> f( bool succeed )
{
	if( succeed )
		return 42;
	else
		return unexpected_<std::error_code>(make_error_code(errc_a::a0));
}

expected<int,std::error_code> g( bool succeed )
{
	if( auto r = f(succeed) )
		return r;
	else
		return unexpected_<std::error_code>(leaf::error_id(r.error()).load(info<42>{42}));
}

int main()
{
	{
		expected<int,std::error_code> r = leaf::try_handle_some(
			[]
			{
				return g(true);
			} );
		BOOST_TEST(r);
		BOOST_TEST_EQ(r.value(), 42);
	}
	{
		int called = 0;
		expected<int,std::error_code> r = leaf::try_handle_some(
			[&]
			{
				auto r = g(false);
				BOOST_TEST(!r);
				auto ec = r.error();
				BOOST_TEST_EQ(ec.message(), "LEAF error");
				BOOST_TEST(!std::strcmp(ec.category().name(),"LEAF error"));
				return r;
			},
			[&]( info<42> const & x, leaf::match<leaf::condition<cond_x>, cond_x::x00> ec )
			{
				called = 1;
				BOOST_TEST_EQ(x.value, 42);
				return unexpected_<std::error_code>(ec.value());
			} );
		BOOST_TEST(!r);
		BOOST_TEST_EQ(r.error(), make_error_code(errc_a::a0));
		BOOST_TEST(called);
	}
	return boost::report_errors();
}
