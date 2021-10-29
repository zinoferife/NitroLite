#pragma once
#include "relation.h"
#include "table_listener.h"
#include <array>
namespace nl
{
	
	enum class notifications : size_t
	{
		add,
		add_multiple,
		remove,
		remove_multiple,
		update,
		merge,
		transform,
		evt,
		load,
		reset,
		sorted,
		visited,
		normalised,
		clear,
		error,
		max
	};


	enum class notif_error_code : size_t
	{
		invalid_id,
		improper_notification_argument,
		end_of_table_iterator,
		no_error
	};

	//the idea of tables is to have named relaltions
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
		using table_t = vector_table;
		struct notification_data
		{
			nl::notifications notif;
			nl::notif_error_code error;
			size_t column;
			union {
				size_t count;
				size_t event_type;
			};
			typename relation_t::iterator row_iterator;
			typename relation_t::variant_t column_value;
		};

		using listener_t = nl::table_listener<void, const vector_table&, const notification_data&>; //
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

		template<nl::notifications notif>
		inline listener_t& sink()
		{
			static_assert(notif < nl::notifications::max, "invalid notifications type");
			return listeners_sinks[(size_t)notif];
		}

		inline listener_t& sink(nl::notifications notif)
		{
			assert(notif < nl::notifications::max && "Invalid notification type");
			return listeners_sinks[(size_t)notif];
		}

		template<nl::notifications notif>
		inline void notify(const notification_data& data)
		{
			static_assert(notif < nl::notifications::max, "invalid notifications type");
			listeners_sinks[(size_t)notif].notify(*this, data);
		}

		inline void notify(nl::notifications notif, const notification_data& data)
		{
			assert(notif < nl::notifications::max && "Invalid notification type");
			listeners_sinks[(size_t)notif].notify(*this, data);
		}
		
	protected:
		name_array names;
		std::array<listener_t, (size_t)nl::notifications::max> listeners_sinks;
	};

}
