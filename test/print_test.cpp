// Copyright (c) 2018-2019 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/print.hpp>
#include "boost/core/lightweight_test.hpp"
#include <sstream>

namespace leaf = boost::leaf;

struct c0
{
	friend std::ostream & operator<<( std::ostream & os, c0 const & )
	{
		return os << "c0";
	}
};

struct c1
{
	int value;

	friend std::ostream & operator<<( std::ostream & os, c1 const & )
	{
		return os << "c1";
	}
};

struct c2
{
	int value;
};

std::ostream & operator<<( std::ostream & os, c2 const & )
{
	return os << "c2";
}

struct c3
{
	int value;
};

struct c4
{
	struct unprintable { };
	unprintable value;;
};

template <class T>
bool check( T const & x, char const * sub )
{
	using namespace leaf::leaf_detail;
	std::ostringstream s;
	diagnostic<T>::print(s,x);
	std::string q = s.str();
	return q.find(sub)!=q.npos;
}

int main()
{
	BOOST_TEST(check(c0{ },"c0"));
	BOOST_TEST(check(c1{42},"c1"));
	{
		c1 x;
		c1 & y = x;
		BOOST_TEST(check(x,"c1"));
		BOOST_TEST(check(y,"c1"));
	}
	BOOST_TEST(check(c2{42},"c2"));
	{
		c2 x = {42};
		c2 & y = x;
		BOOST_TEST(check(x,"c2"));
		BOOST_TEST(check(y,"c2"));
	}
	BOOST_TEST(check(c3{42},"c3"));
	BOOST_TEST(check(c3{42},"42"));
	{
		c3 x = {42};
		c3 & y = x;
		BOOST_TEST(check(x,"c3"));
		BOOST_TEST(check(x,"42"));
		BOOST_TEST(check(y,"c3"));
		BOOST_TEST(check(y,"42"));
	}
	BOOST_TEST(check(c4(),"c4"));
	BOOST_TEST(check(c4(),"{Non-Printable}"));
	{
		c4 x;
		c4 & y = x;
		BOOST_TEST(check(x,"c4"));
		BOOST_TEST(check(x,"{Non-Printable}"));
		BOOST_TEST(check(y,"c4"));
		BOOST_TEST(check(y,"{Non-Printable}"));
	}
	return boost::report_errors();
}
