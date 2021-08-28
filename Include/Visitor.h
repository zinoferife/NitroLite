#pragma once
#include "..\pch.h"
namespace nl
{
////////////////////////////////////////////////////////////////////////////////
// Visitor implementation is based on The Loki Library generic visitor 
// Copyright (c) 2001 by Andrei Alexandrescu
// This code accompanies the book:
// Alexandrescu, Andrei. "Modern C++ Design: Generic Programming and Design 
//     Patterns Applied". Copyright (c) 2001. Addison-Wesley.
// Permission to use, copy, modify, distribute and sell this software for any 
//     purpose is hereby granted without fee, provided that the above copyright 
//     notice appear in all copies and that both that copyright notice and this 
//     permission notice appear in supporting documentation.
// The author or Addison-Wesley Longman make no representations about the 
//     suitability of this software for any purpose. It is provided "as is" 
//     without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////

	class base_visitor
	{
	public:
		virtual ~base_visitor() {};
	};

	template<class T, typename R = void>
	class visitor
	{
	public:
		typedef R return_type;
		typedef T visited_type;
		
		virtual return_type visit(visited_type&) = 0;
		virtual return_type visit(visited_type&, size_t) = 0;
	};

	template<typename R, class Visited>
	struct default_catch_all
	{
		static R on_unknown_visited_type(Visited&, base_visitor&)
		{
			//return the default consturcted R type
			return R();
		}
	};

	template<typename R, class Visited>
	struct log_on_catch_all
	{
		static R on_unknown_visited_type(Visited& v, base_visitor&)
		{
			const char* type = typeid(v).name();
			//set spdlog to put in log file

			return R();

		}
	};

	template<typename R, template<typename, class> class Catch_all = default_catch_all>
	class base_visitable
	{

	public:
		typedef R return_type;
		virtual ~base_visitable(){}
		virtual R accept(base_visitor&) = 0;
		virtual R accept(base_visitor&, size_t) = 0;

	protected:
		template<class T>
		static auto acceptImpl(T& visited, base_visitor& guest)
		{
			if(visitor<T, R>* visitor_ptr = dynamic_cast<visitor<T,R>*>(&guest))
			{
				return guest->visit(visited);
			}
			return Catch_all<R, T>::on_unknown_visited_type(visited, guest);
		}
		
		template<class T>
		static auto acceptImpl(T& visited, base_visitor& guest, size_t row)
		{
			if(visitor<T, R>* visitor_ptr = dynamic_cast<visitor<T,R>*>(&guest))
			{
				return guest->visit(visited, row);
			}
			return Catch_all<R, T>::on_unknown_visited_type(visited, guest);
		}
	};

#define DEFINE_VISITABLE() \
		virtual return_type accept(::nl::base_visitor& guest) \
		{return acceptImpl(*this, guest); }
#define DEFINE_VISITABLE_ROW() \
		virtual return_type accept(::nl::base_visitor& guest, size_t row) \
		{return acceptImpl(*this, guest, row); }


	template<typename T, typename R, typename call_back, size_t... I>
	class concrete_visitor : public base_visitor, public visitor<T, R>
	{
	public:
		typedef std::index_sequence<I...> indx_t;
		typedef call_back call_back_t;

		concrete_visitor(call_back_t callback) :
			mCall_back(callback)
		{}

		virtual R visit(T& visited) override
		{
			static_assert(std::tuple_size_v<typename T::tuple_t> == sizeof...(I), "index is out of bounds");
			for (auto& tuple : visited)
			{
				 mCall_back(std::get<I>(tuple)...);
			}
			return R();
		}


		virtual R visit(T& visited, size_t row) override
		{
			static_assert(std::tuple_size_v<typename T::tuple_t> == sizeof...(I), "index is out of bounds");
			typename T::tuple_t& value = *(std::next(visited.begin(), row));
			return mCall_back(std::get<I>(value)...);
		}

	private:
		call_back_t mCall_back;

	};
};
