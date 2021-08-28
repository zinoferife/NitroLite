#pragma once
#include "pch.h"
#include <functional>
namespace nl {

	//an attempt for the visitor pattern without derivation, 
	//anther deadend ??
	template<typename T, typename arg_call_back, typename step_call_back, size_t...I>
	class visitor_expir
	{
	public:
		typedef std::index_sequence<I...> indx_t;
		typedef arg_call_back arg_call_back_t;
		typedef step_call_back step_call_back_t;

		template<typename visited_t>
		auto visit(visited_t visited)
		{
			
		}	
		
		template<typename visited_t>
		auto visit(visited_t visited, size_t row)
		{

		}

	protected:
		std::function<arg_call_back_t> arg_callback;
		std::function<step_call_back_t> step_callback;

	};
};

