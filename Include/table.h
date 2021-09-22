#pragma once
#include "relation.h"
#include "table_listener.h"
#include <array>
namespace nl
{
	enum class notifications
	{
		add,
		remove,
		update,
		merge,
		transform
	};

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
		using name_array = std::array<name_t, std::tuple_size_v<typename vector_relation<args...>::tuple_t> +1 > ;
		using relation_t  = vector_relation<args...>;
		using listener_t = nl::table_listener<void, notifications, const vector_table&, const size_t&>;
		using update_listener_t = nl::table_listener<void, notifications, const vector_table&, size_t, const size_t&>;
		using table_t = vector_table;


		vector_table() {}
		explicit vector_table(size_t size) : vector_relation<args...>{ size } {}
		virtual ~vector_table() {}


		template<size_t I>
		inline void as(const std::string&& name)
		{
			names[I + 1] = name;
		}

		inline void as(size_t I, const std::string&& name)
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

		inline void table_name(const std::string&& name)
		{
			names[0] = name;
		}

		inline constexpr const name_t& get_table_name() const
		{
			return names[0];
		}

		template<size_t I>
		inline constexpr const name_t& get_name() const
		{
			return names[I + 1];
		}

		inline const name_t& get_name(size_t i) const
		{
			return names[i + 1];
		}

		inline const name_array& get_names() const
		{
			return names;
		}

		inline listener_t& sink()
		{
			return listeners;
		}

		inline update_listener_t& update_sink()
		{
			return update_listeners;
		}

		void notify(notifications notif, const size_t& row_index)
		{
			listeners.notify(notif, *this, row_index);
		}
		
		void notify(notifications notif,
			size_t column, const size_t& row_index)
		{
			update_listeners.notify(notif, *this, column, row_index);
		}

	protected:
		name_array names;
		listener_t listeners;
		update_listener_t update_listeners;

	};

}
