// Copyright (c) 2018-2019 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/preload.hpp>
#include <boost/leaf/handle_exception.hpp>
#include <boost/leaf/exception.hpp>
#include "boost/core/lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int>
struct info
{
	int value;
};

void f0()
{
	auto load = leaf::defer( [] { return info<0>{0}; } );
	throw leaf::exception(std::exception(), info<2>{2} );
}

void f1()
{
	auto load = leaf::defer( [] { return info<0>{-1}; }, [] { return info<1>{1}; }, [] { return info<2>{-1}; } );
	f0();
}

void f2()
{
	try
	{
		f1();
	}
	catch( leaf::error_id const & err )
	{
		err.load( info<3>{3} );
		throw;
	}
}

int main()
{
	int r = leaf::try_catch(
		[]
		{
			f2();
			return 0;
		},
		[]( info<0> i0, info<1> i1, info<2> i2, info<3> i3 )
		{
			BOOST_TEST_EQ(i0.value, 0);
			BOOST_TEST_EQ(i1.value, 1);
			BOOST_TEST_EQ(i2.value, 2);
			BOOST_TEST_EQ(i3.value, 3);
			return 1;
		},
		[]
		{
			return 2;
		} );
	BOOST_TEST_EQ(r, 1);

	return boost::report_errors();
}
