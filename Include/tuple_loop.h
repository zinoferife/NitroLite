#pragma once
#include <tuple>
#include <SQLite/sqlite3.h>
namespace nl
{
	namespace detail
	{
		template<size_t count>
		class loop
		{
		public:
			template<typename relation_t, template<typename> typename buffer_t>
			static void do_buffer_write(buffer_t<relation_t>& buffer, typename relation_t::tuple_t& tuple)
			{
				loop<count - 1>::template do_buffer_write<relation_t, buffer_t>(buffer, tuple);
				buffer.write(std::get<count>(tuple));
			}

			template<typename relation_t, template<typename> typename buffer_t>
			static auto do_buffer_read(buffer_t<relation_t>& buffer)
			{
				auto t0 = loop<count - 1>::template do_buffer_read<relation_t, buffer_t>(buffer);
				using arg_type = typename std::tuple_element_t<count, typename relation_t::tuple_t>;
				arg_type object;
				buffer.read(object);
				return std::tuple_cat(t0, std::make_tuple(object));
			}
		};

		template<>
		class loop<0>
		{
		public:
			template<typename relation_t, template<typename> typename buffer_t>
			static void do_buffer_write(buffer_t<relation_t>& buffer, typename relation_t::tuple_t& tuple)
			{
				buffer.write(std::get<0>(tuple));
			}

			template<typename relation_t, template<typename> typename buffer_t>
			static auto do_buffer_read(buffer_t<relation_t>& buffer)
			{
				using arg_type = typename std::tuple_element_t<0, typename relation_t::tuple_t>;
				arg_type object;
				buffer.read(object);
				return std::make_tuple(object);
			}
		};
	}
}
