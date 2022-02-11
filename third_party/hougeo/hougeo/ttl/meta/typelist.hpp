//  typelist.hpp
//
//  Copyright (c) 2003 Eugene Gladyshev
//
//  Permission to copy, use, modify, sell and distribute this software
//  is granted provided this copyright notice appears in all copies.
//  This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.
//

#ifndef __ttl_typelist__hpp
#define __ttl_typelist__hpp

#include "ttl/config.hpp"
#include "ttl/data_holder.hpp"
#include "ttl/exception.hpp"
#include "ttl/macro_params.hpp"
#include "ttl/equivalent_types.hpp"
#include "ttl/meta/is_same.hpp"
#include "ttl/selector.hpp"


namespace ttl
{
namespace meta
{	
struct exception : ttl::exception
{
	exception() : ttl::exception("typelist error") {}
};
//internal implementation
namespace impl
{
	////////////////////////////////////////////////////////////
	template< TTL_TPARAMS_DEF(TTL_MAX_TYPELIST_PARAMS, empty_type) >
	struct typelist_traits
	{
		typedef typelist_traits< TTL_ARGS_S(TTL_DEC(TTL_MAX_TYPELIST_PARAMS)) > tail;

		enum
		{
			length = 1 + tail::length,
		};

		typedef typename selector< sizeof(ttl::data_holder<T1>) >=sizeof(ttl::data_holder<typename tail::largest_type>),
			T1,
			typename tail::largest_type
			>::type largest_type;
	};

	template<>
	struct typelist_traits< TTL_LIST_ITEMS(TTL_MAX_TYPELIST_PARAMS,empty_type) >
	{
		typedef empty_type tail;
		enum
		{
			length = 0,
		};
		typedef empty_type largest_type;
	};
	
	////////////////////////////////////////////////////////////
	//
	//Instantiate TTL_MAX_TYPELIST_PARAMS get<> templates
	//	template<typename T>  struct get<T, 0>
	//	{ 
	//		enum { index = 0 };			
	//		typedef typename T::type1 type; 
	//	};
	//	
	//	template<typename T>  struct get<T, 1>
	//	{ 
	//		enum { index = 1 };			
	//		typedef typename T::type2 type; 
	//	};
	//	...
	//
	
	template< typename T, int N > struct get;
	#define TTL_META_TYPELIST_GET(n, t) template<typename T>  struct get<T, TTL_CNTDEC_##n>  \
	{ enum {index = n-1}; typedef typename T::t##n type; };
	
	TTL_REPEAT( TTL_MAX_TYPELIST_PARAMS, TTL_META_TYPELIST_GET, TTL_META_TYPELIST_GET, type )
	
	#undef TTL_META_TYPELIST_GET
};

	template < TTL_TPARAMS_DEF(TTL_MAX_TYPELIST_PARAMS, empty_type) >
	struct typelist
	{
		TTL_TYPEDEFS(TTL_MAX_TYPELIST_PARAMS)
		
		typedef impl::typelist_traits< TTL_ARGS(TTL_MAX_TYPELIST_PARAMS) > list_traits;
	
		enum{ length = list_traits::length };
		typedef typename list_traits::largest_type largest_type;
	};
	
	////////////////////////////////////////////////////////////
	template < typename L >
	struct length
	{
		enum { value = L::length };
	};
	
	///////////////////////////////////////////////////////////
	template< typename L, int N, bool Ok = (N < length<L>::value) >
	struct get
	{
		typedef typename impl::get<L,N>::type type;
		enum{ index = N };
	};
	
	template< typename L, int N >
	struct get<L,N,false>
	{
		//index is out of range
	};

	////////////////////////////////////////////////////////////
	//	run-time type switch
	template <typename L, int N = 0, bool Stop=(N==length<L>::value) > struct type_switch;

	template <typename L, int N, bool Stop> 
	struct type_switch
	{
		template< typename F >
		void operator()( size_t i, F& f )
		{
			if( i == N ) 
			{
#ifdef __clang__
                            f.template operator()<typename impl::get<L,N>::type>();
#else  // __clang__
                            f.operator()<typename impl::get<L,N>::type>();
#endif // __clang__
			}
			else
			{
				type_switch<L, N+1> next;
				next(i, f);
			}
		}
	};

	template <typename L, int N> 
	struct type_switch<L, N, true>
	{
		template< typename F >
		void operator()( size_t i, F& f )
		{
			throw meta::exception();
		}
	};


	//////////////////////////////////////////////////////////////
	template< typename T, typename L, int N = 0, bool Stop=(N>=length<L>::value) >
	struct find_equivalent_type
	{
	private:
		typedef impl::get<L,N> get_type;
		
		typedef typename selector< equivalent_types<typename get_type::type, T>::value,
			get_type,
			find_equivalent_type<T,L,N+1> 
			>::type found;
			
	public:
		typedef typename found::type type;
		enum {index = found::index};
	};

	template<typename T, typename L, int N>
	struct find_equivalent_type<T, L, N, true>
	{
	};


	//////////////////////////////////////////////
	template< typename T, typename L, int N = 0, bool Stop=(N>=length<L>::value) >
	struct find
	{
	private:
		typedef impl::get<L,N> get_type;
		
		typedef typename selector< is_same<typename get_type::type, T>::value,
			get_type,
			find<T,L,N+1> 
			>::type found;
			
	public:
		typedef typename found::type type;
		enum {index = found::index};
	};

	template<typename T, typename L, int N>
	struct find<T, L, N, true>
	{
	};

};
};

#endif //__typelist__hpp
