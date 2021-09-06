#pragma once
#include "relation.h"

namespace nl
{
	//the idea of tables is to have named realtions
	//realtions by defination are just containers of tuples
	//the enum names and macro is very clumsy and static
	//this idea might be good? since the names are editable at run time
	template<typename...args>
	class vector_table : public vector_relation<args...>
	{
	public:
		using name_t = std::string;
		//0 is reserved for the table name
		using name_array = std::array<name_t, std::tuple_size_v<typename vector_relation<args...>::tuple_t> + 1>;
		
		virtual ~vector_table() {}



		template<size_t I>
		void as(const std::string&& name)
		{
			names[I + 1] = name;
		}

		void as(size_t I, const std::string&& name)
		{
			names[I + 1] = name;
		}

		size_t index(const std::string&& name)
		{
			//linear search, array is not going to be more than at least 10 
			for (size_t i = 1; i < names.size(); i++){
				if (names[i] == name)
				{
					return i;
				}
			}
		}

		void table_name(const std::string&& name)
		{
			names[0] = name;
		}

		constexpr const name_t& get_table_name() const
		{
			return names[0];
		}

		template<size_t I>
		constexpr const name_t& get_name() const
		{
			return names[I + 1];
		}

		const name_t& get_name(size_t i) const
		{
			return names[i + 1];
		}

		const name_array& get_names() const
		{
			return names;
		}

	protected:
		name_array names;

	};

}
