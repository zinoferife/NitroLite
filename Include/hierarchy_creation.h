#pragma once
#pragma once
#include "tuple_t_operations.h"
//hierarchy implentation is based on Andrei alexandrescu LOKI library implentation


namespace nl
{
	namespace detail
	{
		struct empty_type {};
		namespace private_ {
			template<class, class>
			struct hierarchy_tag;
		}
		template<class tuple_t, template<class> class unit>
		class gen_hierarchy;

		template<template<class> class unit, typename T, typename...S>
		class gen_hierarchy<std::tuple<T, S...>, unit> :
			public gen_hierarchy<private_::hierarchy_tag<std::tuple<T>, std::tuple<S...>>, unit>,
			public gen_hierarchy<std::tuple<S...>, unit>
		{
		public:
			typedef std::tuple<T, S...> type_list;
			typedef gen_hierarchy<private_::hierarchy_tag<std::tuple<T>, std::tuple<S...>>, unit> left_base;
			typedef gen_hierarchy<std::tuple<S...>, unit> right_base;
			template <typename T> struct rebind
			{
				typedef unit<T> result;
			};

		};

		template<template<class> class unit, typename T, typename...S>
		class gen_hierarchy<private_::hierarchy_tag<std::tuple<T>, std::tuple<S...>>, unit>
			: public gen_hierarchy<std::tuple<T>, unit>
		{
		};

		template <template<class> class unit, typename U>
		class gen_hierarchy<std::tuple<U>, unit> : public unit<U>
		{
			typedef unit<U> left_base;
			template<typename T> struct rebind
			{
				typedef unit<T> result;
			};
		};

		//field access
		template<class T, class H>
		inline typename H::template rebind<T>::result& field(H& obj)
		{
			return obj;
		}

		template<class T, class H>
		inline const typename H::template rebind<T>::result& field(const H& obj)
		{
			return obj;
		}

		template<class H, size_t I> struct find_helper;

		template<class H>
		struct find_helper<H, 0>
		{
			typedef std::tuple_element_t<0, typename H::type_list> element_type;
			typedef typename H::template rebind<element_type>::result unit_type;
			enum
			{
				is_const = std::is_const_v<H>
			};

			typedef const typename H::left_base const_left_base;
			typedef typename select_type<is_const, const_left_base, typename H::left_base>::type left_base;
			typedef typename select_type<is_const, const unit_type, unit_type>::type result_type;

			static result_type& do_(H& obj)
			{
				left_base& left = obj;
				return left;
			}
		};

		template<class H, size_t I>
		struct find_helper
		{
			typedef typename std::tuple_element_t<I, typename H::type_list> element_type;
			typedef typename H::template rebind<element_type>::result unit_type;
			enum
			{
				is_const = std::is_const_v<H>
			};

			typedef const typename H::right_base const_right_base;
			typedef typename select_type<is_const, const_right_base, typename H::right_base>::type right_base;
			typedef typename select_type<is_const, const unit_type, unit_type>::type result_type;

			static result_type& do_(H& obj)
			{
				right_base& base = obj;
				return find_helper<right_base, I - 1>::do_(base);
			}

		};

		template<size_t I, class H>
		typename find_helper<H, I>::result_type& field(H& obj)
		{
			return find_helper<H, I>::do_(obj);
		}

		template<class tuple_t, template<class type, class base> class unit, class root = empty_type>
		class gen_linear_hierarchy;

		template<typename T, template<class, class> class unit, class root, typename... S>
		class gen_linear_hierarchy<std::tuple<T, S...>, unit, root>
			: public unit<T, gen_linear_hierarchy<std::tuple<S...>, unit, root>>
		{
		public:
			typedef unit<T, gen_linear_hierarchy<std::tuple<S...>, unit, root>> unit_type;
			template<typename U>
			struct rebind
			{
				typedef unit<U, gen_linear_hierarchy<std::tuple<S...>, unit, root>> result;
			};
		};

		template<typename T, template<class, class> class unit, class root>
		class gen_linear_hierarchy<std::tuple<T>, unit, root>
			: public unit<T, root> {
			typedef unit<T, root> unit_type;
			
			//rebind cannot work in the sense that 
			//The unit requires a root type
			//these root types are different for each node in the chain
			//therefore rebind unit would give a different 
			//incorrect root in feild(H& obj) function
			template<typename U>
			struct rebind
			{
				typedef unit<U, root> result;
			};
		};
	}
}
