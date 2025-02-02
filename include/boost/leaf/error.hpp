#ifndef BOOST_LEAF_BA049396D0D411E8B45DF7D4A759E189
#define BOOST_LEAF_BA049396D0D411E8B45DF7D4A759E189

// Copyright (c) 2018-2019 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/function_traits.hpp>
#include <boost/leaf/detail/optional.hpp>
#include <boost/leaf/detail/print.hpp>
#include <system_error>
#include <type_traits>
#include <ostream>
#include <sstream>
#include <atomic>
#include <thread>
#include <set>

#define LEAF_NEW_ERROR(...) ::boost::leaf::leaf_detail::new_error_at(__FILE__,__LINE__,__FUNCTION__).load(__VA_ARGS__)

#define LEAF_AUTO(v,r)\
	static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(r)>::type>::value, "LEAF_AUTO requires a result type");\
	auto _r_##v = r;\
	if( !_r_##v )\
		return _r_##v.error();\
	auto & v = _r_##v.value()

#define LEAF_CHECK(r)\
	{\
		static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(r)>::type>::value, "LEAF_CHECK requires a result type");\
		auto const & _r = r;\
		if( !_r )\
			return _r.error();\
	}

namespace boost { namespace leaf {

	namespace leaf_detail
	{
		template<class T> using has_value_impl = decltype( std::declval<T>().value );

		template <class T> using has_value = leaf_detail_mp11::mp_valid<has_value_impl, T>;

		template <class T>
		struct is_error_type_default
		{
			static constexpr bool value = has_value<T>::value || std::is_base_of<std::exception,T>::value;
		};

		template <>
		struct is_error_type_default<std::exception_ptr>: std::true_type
		{
		};
	}

	template <class T> struct is_e_type: leaf_detail::is_error_type_default<T> { };
	template <class T> struct is_e_type<T const>: is_e_type<T> { };
	template <class T> struct is_e_type<T const &>: is_e_type<T> { };
	template <class T> struct is_e_type<T &>: is_e_type<T> { };
	template <> struct is_e_type<std::error_code>: std::false_type { };

	////////////////////////////////////////

	struct e_source_location
	{
		char const * const file;
		int const line;
		char const * const function;

		friend std::ostream & operator<<( std::ostream & os, e_source_location const & x )
		{
			return os << leaf::type<e_source_location>() << ": " << x.file << '(' << x.line << ") in function " << x.function;
		}
	};

	template <>
	struct is_e_type<e_source_location>: std::true_type
	{
	};

	////////////////////////////////////////

	namespace leaf_detail
	{
		class slot_base
		{
			slot_base & operator=( slot_base const & ) = delete;
			slot_base( slot_base const & ) = delete;

			virtual bool slot_print( std::ostream &, int err_id ) const = 0;

		public:

			static void print( std::ostream & os, int err_id )
			{
				for( slot_base const * p = first(); p; p=p->next_ )
					if( p->slot_print(os,err_id) )
						os << std::endl;
			}

			static void reassign( int from_err_id, int to_err_id ) noexcept
			{
				assert(from_err_id);
				assert(to_err_id);
				for( slot_base * p = first(); p; p=p->next_ )
					if( p->err_id_==from_err_id )
						p->err_id_ = to_err_id;
			}

		protected:

			static slot_base * & first() noexcept
			{
				static thread_local slot_base * p = 0;
				return p;
			}

			int err_id_;
			slot_base * next_;

			slot_base() noexcept:
				err_id_(0),
				next_(0)
			{
			}

			slot_base( slot_base && x ) noexcept:
				err_id_(std::move(x.err_id_)),
				next_(0)
			{
				assert(x.next_==0);
			}

			~slot_base() noexcept
			{
				assert(next_ == 0);
			}

			void activate() noexcept
			{
				assert(next_ == 0);
				slot_base * * f = &first();
				next_ = *f;
				*f = this;
			}

			void deactivate() noexcept
			{
				slot_base * * f = &first();
				assert(*f == this);
				*f = next_;
				next_ = 0;
			}
		};
	}

	////////////////////////////////////////

	namespace leaf_detail
	{
		class e_unexpected_count
		{
		public:

			char const * (*first_type)();
			int count;

			explicit e_unexpected_count( char const * (*first_type)() ) noexcept:
				first_type(first_type),
				count(1)
			{
			}

			void print( std::ostream & os ) const
			{
				assert(first_type!=0);
				assert(count>0);
				os << "Detected ";
				if( count==1 )
					os << "1 attempt to communicate an E-object";
				else
					os << count << " attempts to communicate unexpected E-objects, the first one";
				os << " of type " << first_type() << std::endl;
			}
		};

		template <>
		struct is_error_type_default<e_unexpected_count>: std::true_type
		{
		};

		template <>
		struct diagnostic<e_unexpected_count,false,false>
		{
			static constexpr bool is_invisible = true;
			static void print( std::ostream &, e_unexpected_count const & ) noexcept
			{
			}
		};

		class e_unexpected_info
		{
			std::string s_;
			std::set<char const *(*)()> already_;

		public:

			e_unexpected_info() noexcept
			{
			}

			void reset() noexcept
			{
				s_.clear();
				already_.clear();
			}

			template <class E>
			void add( E const & e )
			{
				std::stringstream s;
				if( !leaf_detail::diagnostic<E>::is_invisible )
				{
					leaf_detail::diagnostic<E>::print(s,e);
					if( already_.insert(&type<E>).second  )
					{
						s << std::endl;
						s_ += s.str();
					}
				}
			}

			void print( std::ostream & os ) const
			{
				os << s_;
			}
		};

		template <>
		struct is_error_type_default<e_unexpected_info>: std::true_type
		{
		};

		template <>
		struct diagnostic<e_unexpected_info,false,false>
		{
			static constexpr bool is_invisible = true;
			static void print( std::ostream &, e_unexpected_info const & ) noexcept
			{
			}
		};
	}

	////////////////////////////////////////

	namespace leaf_detail
	{
		inline int & tl_unexpected_enabled_counter() noexcept
		{
			static thread_local int c;
			return c;
		}
	}

	////////////////////////////////////////

	namespace leaf_detail
	{
		template <class E>
		class slot;

		template <class E>
		slot<E> * & tl_slot_ptr() noexcept
		{
			static thread_local slot<E> * s;
			return s;
		}

		template <class E>
		class slot:
			slot_base,
			optional<E>
		{
			slot( slot const & ) = delete;
			slot & operator=( slot const & ) = delete;

			typedef optional<E> impl;
			slot<E> * * top_;
			slot<E> * prev_;
			static_assert(is_e_type<E>::value,"Not an error type");

			bool slot_print( std::ostream & os, int err_id ) const
			{
				if( !diagnostic<E>::is_invisible && *top_==this )
					if( E const * e = has_value(err_id) )
					{
						assert(err_id);
						diagnostic<decltype(*e)>::print(os, *e);
						return true;
					}
				return false;
			}

		public:

			slot() noexcept:
				top_(0)
			{
			}

			slot( slot && x ) noexcept:
				slot_base(std::move(x)),
				optional<E>(std::move(x)),
				top_(0)
			{
				assert(x.top_==0);
			}

			~slot() noexcept
			{
				assert(top_==0);
			}

			void activate() noexcept
			{
				assert(top_==0);
				slot_base::activate();
				top_ = &tl_slot_ptr<E>();
				prev_ = *top_;
				*top_ = this;
			}

			void deactivate( bool propagate_errors ) noexcept;

			E & load( int err_id, E const & e ) noexcept
			{
				assert(err_id);
				E & ret = impl::put(e);
				err_id_ = err_id;
				return ret;
			}

			E & load( int err_id, E && e ) noexcept
			{
				assert(err_id);
				E & ret = impl::put(std::forward<E>(e));
				err_id_ = err_id;
				return ret;
			}

			E const * has_value( int err_id ) const noexcept
			{
				if( err_id == err_id_ )
				{
					assert(err_id);
					if( E const * e = impl::has_value() )
						return e;
				}
				return 0;
			}

			E * has_value( int err_id ) noexcept
			{
				if( err_id == err_id_ )
				{
					assert(err_id);
					if( E * e = impl::has_value() )
						return e;
				}
				return 0;
			}

			bool print( std::ostream & os ) const
			{
				os << '[' << err_id_ << "]: ";
				if( E const * e = impl::has_value() )
				{
					diagnostic<decltype(*e)>::print(os, *e);
					os << std::endl;
				}
				else
					os << type<E>() << ": {Empty}" << std::endl;
				return true;
			}
		};

		template <class E>
		void load_unexpected_count( int err_id, E const & e ) noexcept
		{
			if( slot<e_unexpected_count> * sl = tl_slot_ptr<e_unexpected_count>() )
				if( e_unexpected_count * unx = sl->has_value(err_id) )
					++unx->count;
				else
					sl->load(err_id, e_unexpected_count(&type<E>));
		}

		template <class E>
		void load_unexpected_info( int err_id, E const & e ) noexcept
		{
			if( slot<e_unexpected_info> * sl = tl_slot_ptr<e_unexpected_info>() )
				if( e_unexpected_info * unx = sl->has_value(err_id) )
					unx->add(e);
				else
					sl->load(err_id, e_unexpected_info()).add(e);
		}

		template <class E>
		void no_expect_slot( int err_id, E const & e  ) noexcept
		{
			load_unexpected_count(err_id, e);
			load_unexpected_info(err_id, e);
		}

		template <class E>
		void slot<E>::deactivate( bool propagate_errors ) noexcept
		{
			assert(top_!=0);
			if( propagate_errors )
				if( prev_ )
				{
					if( err_id_ )
					{
						optional<E> & p = *prev_;
						p = std::move(*this);
						prev_->err_id_ = err_id_;
					}
				}
				else
				{
					int c = tl_unexpected_enabled_counter();
					assert(c>=0);
					if( c )
						if( E const * e = impl::has_value() )
							no_expect_slot(err_id_, *e);
				}
			*top_ = prev_;
			top_ = 0;
			slot_base::deactivate();
		}

		template <class E>
		int load_slot( int err_id, E && e ) noexcept
		{
			using T = typename std::decay<E>::type;
			assert(err_id);
			if( slot<T> * p = tl_slot_ptr<T>() )
				(void) p->load(err_id, std::forward<E>(e));
			else
			{
				int c = tl_unexpected_enabled_counter();
				assert(c>=0);
				if( c )
					no_expect_slot(err_id, std::forward<E>(e));
			}
			return 0;
		}

		template <class F>
		int accumulate_slot( int err_id, F && f ) noexcept
		{
			static_assert(function_traits<F>::arity==1, "Lambdas passed to accumulate must take a single e-type argument by reference");
			using E = typename std::decay<fn_arg_type<F,0>>::type;
			static_assert(is_e_type<E>::value, "Lambdas passed to accumulate must take a single e-type argument by reference");
			assert(err_id);
			if( auto sl = tl_slot_ptr<E>() )
				if( auto v = sl->has_value(err_id) )
					(void) std::forward<F>(f)(*v);
				else
					(void) std::forward<F>(f)(sl->load(err_id,E()));
			return 0;
		}

		enum class result_variant
		{
			value,
			err_id,
			ctx
		};
	} // leaf_detail

	////////////////////////////////////////

	namespace leaf_detail
	{
		namespace id_factory
		{
			inline int new_err_id() noexcept
			{
				static std::atomic<unsigned> c;
				if( unsigned id = ++c )
					return int(id);
				else
					return int(++c);
			}

			inline int & next_id_() noexcept
			{
				static thread_local int id = new_err_id();
				return id;
			}

			inline int & last_id_() noexcept
			{
				static thread_local int id = 0;
				return id;
			}
		}

		inline int new_id() noexcept
		{
			int & n = id_factory::next_id_();
			int id = id_factory::last_id_() = n;
			n = id_factory::new_err_id();
			return id;
		}

		inline int next_id() noexcept
		{
			return id_factory::next_id_();
		}

		inline int last_id() noexcept
		{
			return id_factory::last_id_();
		}
	}

	////////////////////////////////////////

	namespace leaf_detail
	{
		struct e_original_ec { std::error_code value; };

		inline std::error_category const & get_error_category() noexcept
		{
			class cat: public std::error_category
			{
				bool equivalent( int,  std::error_condition const & ) const noexcept { return false; }
				bool equivalent( std::error_code const &, int ) const noexcept { return false; }
				char const * name() const noexcept { return "LEAF error"; }
				std::string message( int condition ) const { return name(); }
			};
			static cat c;
			return c;
		}

		template <class ErrorCode>
		std::error_code import_error_code( ErrorCode && ec ) noexcept
		{
			std::error_category const & cat = leaf_detail::get_error_category();
			if( ec && &ec.category()!=&cat )
			{
				int err_id = leaf_detail::new_id();
				leaf_detail::load_slot(err_id, leaf_detail::e_original_ec{ec});
				return std::error_code(err_id, cat);
			}
			else
				return ec;
		}

		inline bool is_error_id( std::error_code const & ec ) noexcept
		{
			return &ec.category() == &leaf_detail::get_error_category();
		}
}

	////////////////////////////////////////

	class error_id: public std::error_code
	{
	public:

		error_id() noexcept = default;

		struct tag_error_id {};
		error_id( std::error_code const & ec, tag_error_id ) noexcept:
			std::error_code(ec)
		{
			assert(leaf_detail::is_error_id(ec));
		}

		error_id( std::error_code const & ec ) noexcept:
			error_code(leaf_detail::import_error_code(ec))
		{
		}

		error_id( std::error_code && ec ) noexcept:
			error_code(leaf_detail::import_error_code(std::move(ec)))
		{
		}

		error_id const & load() const noexcept
		{
			return *this;
		}

		template <class... E>
		error_id const & load( E && ... e ) const noexcept
		{
			if( int err_id = value() )
			{
				auto _ = { leaf_detail::load_slot(err_id, std::forward<E>(e))... };
				(void) _;
			}
			return *this;
		}

		error_id const & accumulate() const noexcept
		{
			return *this;
		}

		template <class... F>
		error_id const & accumulate( F && ... f ) const noexcept
		{
			if( int err_id = value() )
			{
				auto _ = { leaf_detail::accumulate_slot(err_id, std::forward<F>(f))... };
				(void) _;
			}
			return *this;
		}
	};

	namespace leaf_detail
	{
		inline error_id make_error_id( int err_id ) noexcept
		{
			assert(err_id);
			return std::error_code(err_id, get_error_category());
		}
	}

	inline error_id new_error() noexcept
	{
		return leaf_detail::make_error_id(leaf_detail::new_id());
	}

	template <class E1, class... E>
	typename std::enable_if<is_e_type<E1>::value, error_id>::type new_error( E1 && e1, E && ... e ) noexcept
	{
		return leaf_detail::make_error_id(leaf_detail::new_id()).load(std::forward<E1>(e1), std::forward<E>(e)...);
	}

	template <class E1, class... E>
	typename std::enable_if<std::is_same<std::error_code, decltype(make_error_code(std::declval<E1>()))>::value, error_id>::type new_error( E1 const & e1, E && ... e ) noexcept
	{
		return error_id(make_error_code(e1)).load(std::forward<E>(e)...);
	}

	inline error_id next_error() noexcept
	{
		return leaf_detail::make_error_id(leaf_detail::next_id());
	}

	namespace leaf_detail
	{
		template <class... E>
		error_id new_error_at( char const * file, int line, char const * function ) noexcept
		{
			assert(file&&*file);
			assert(line>0);
			assert(function&&*function);
			e_source_location sl { file, line, function }; // Temp object MSVC workaround
			return new_error(std::move(sl));
		}
	}

	////////////////////////////////////////////

	class polymorphic_context
	{
	protected:

		polymorphic_context() noexcept = default;

	public:

		virtual ~polymorphic_context() noexcept = default;
		virtual void activate() noexcept = 0;
		virtual void deactivate( bool propagate_errors ) noexcept = 0;
		virtual bool is_active() const noexcept = 0;
		virtual void print( std::ostream & ) const = 0;
		virtual std::thread::id const & thread_id() const noexcept = 0;

		std::error_code ec;
	};

	using context_ptr = std::shared_ptr<polymorphic_context>;

	////////////////////////////////////////////

	enum class on_deactivation
	{
		propagate,
		do_not_propagate,
		propagate_if_uncaught_exception
	};

	class context_activator
	{
		context_activator( context_activator const & ) = delete;
		context_activator & operator=( context_activator const & ) = delete;

		polymorphic_context & ctx_;
		on_deactivation on_deactivate_;
		bool const ctx_was_active_;

	public:

		context_activator( polymorphic_context & ctx, on_deactivation on_deactivate ) noexcept:
			ctx_(ctx),
			on_deactivate_(on_deactivate),
			ctx_was_active_(ctx_.is_active())
		{
			if( !ctx_was_active_ )
				ctx_.activate();
		}

		~context_activator() noexcept
		{
			assert(
				on_deactivate_ == on_deactivation::propagate ||
				on_deactivate_ == on_deactivation::do_not_propagate ||
				on_deactivate_ == on_deactivation::propagate_if_uncaught_exception);
			if( !ctx_was_active_ )
				if( on_deactivate_ == on_deactivation::propagate_if_uncaught_exception )
				{
					bool has_exception = std::uncaught_exception();
					ctx_.deactivate(has_exception);
					if( !has_exception )
						(void) leaf_detail::new_id();
				}
				else
					ctx_.deactivate(on_deactivate_ == on_deactivation::propagate);
		}

		void set_on_deactivate( on_deactivation on_deactivate ) noexcept
		{
			on_deactivate_ = on_deactivate;
		}
	};

	////////////////////////////////////////////

	template <class R>
	struct is_result_type: std::false_type
	{
	};

	namespace leaf_detail
	{
		template <class R, bool IsResult = is_result_type<R>::value>
		struct is_result_tag;

		template <class R>
		struct is_result_tag<R, false>
		{
		};

		template <class R>
		struct is_result_tag<R, true>
		{
		};
	}

} }

#endif
