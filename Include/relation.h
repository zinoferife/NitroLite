#pragma once
#include "../pch.h"
#include "tuple_loop.h"

	/////////////////////////////////////////////////////////////
	// relation represents a frame of data from the database
	// reads and writes to the database a done via relations
	// operations can be done via relational functions or functions in classes derived from relations
	// relations can be serialized into buffers and maybe transfered across a network
	// this froms the base object of all _TBL objects in the application 
	// over engineering by ewomagram !!!!!

#include <cstdint>
#define BEGIN_COL_NAME(name)static constexpr char table_name[] = name; static constexpr const char* col_names[] =  {
#define COL_NAME(name) name,
#define END_COL_NAME() "null"};
#define IMPLEMENT_GET_COL_NAME() \
		template<size_t i> \
		constexpr const char* get_col_name() { \
		static_assert(i >= 0 && i < sizeof(col_names), "invalid index in get_col_name()"); return col_names[i]; }
#define DEFINE_UNIQUE_ID() enum {unique_id = 0 };

namespace nl {

	template<size_t...I>
	using select = std::index_sequence<I...>;

	template<typename T>
	using alloc_t = std::allocator<T>;

	template<typename ty>
	using hash_t = std::hash<ty>;

	template<typename ty>
	using key_comp_map_t = std::equal_to<ty>;

	template<typename ty>
	using key_comp_set_t = std::less<ty>;

	struct linear_relation_tag {};
	struct map_relation_tag {};


	template<class container> class relation;

	template<template<class, class>typename container, typename... val>
	class relation<container<std::tuple<val...>, alloc_t<std::tuple<val...> > > > : public container<std::tuple<val...>, alloc_t<std::tuple<val...>>>
	{
	public:

		enum rel_constants
		{
			unique_id = 0,
			col_id_start
		};

		static size_t ROWID;
		using container_t = container<std::tuple<val...>, alloc_t<std::tuple<val...>>>;
		using tuple_t = typename container_t::value_type;
		using container_tag = linear_relation_tag;


		relation() = default;
		relation(relation&) = default;
		relation(relation&&) = default;
		relation& operator=(relation&) = default;
		relation& operator=(relation&&) = default;
		virtual ~relation() {}

		auto insert(tuple_t& tuple)
		{
			return container_t::push_back(tuple);
		}

		template<size_t I>
		auto average()
		{
			double sum = 0.0;
			for (auto& tuple : *this)
			{
				sum += std::get<I>(tuple);
			}
			return (sum / container_t::size());
		}

		template<typename...T>
		void add_to_relation(T&& ... args)
		{
			static_assert(std::tuple_size_v<tuple_t> == sizeof...(args), "Incomplete argument in add_to_relation");
			container_t::emplace_back(tuple_t(std::forward<T>(args)...));
		}


		template<size_t col>
		inline typename std::tuple_element_t<col, tuple_t> get(size_t row)
		{
			tuple_t& tuple = *(std::next(container_t::begin(), row));
			return std::get<col>(tuple);
		}

		template<size_t col>
		tuple_t& find_on(typename std::tuple_element_t<col, tuple_t>& value)
		{
			auto it = std::find_if(container_t::begin(), container_t::end(), [&](tuple_t& tuple) {
				return(value == std::get<col>(tuple));
			});
			return(*it);
		}

		static tuple_t make_rel_element(const val&... values)
		{
			return std::forward_as_tuple(values...);
		}
	};

	template<template<class, class, class>typename container, typename... val>
	class relation<container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>> >> : public container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>>>
	{

	public:

		using container_t = container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>>>;
		using tuple_t = typename container_t::value_type;
		using container_tag = map_relation_tag;

		relation() = default;
		relation(relation&) = default;
		relation(relation&&) = default;
		relation& operator=(relation&) = default;
		relation& operator=(relation&&) = default;
		virtual ~relation() {}

		auto insert(tuple_t tuple)
		{
			return container_t::insert(tuple);
		}

		template<typename...T>
		void add_to_relation(T&& ... args)
		{
			static_assert(std::tuple_size_v<tuple_t> == sizeof...(args), "Incomplete argument in add_to_relation");
			container_t::insert(tuple_t(std::forward<T>(args)...));
		}


		template<size_t I>
		auto average()
		{
			double sum = 0.0;
			for (auto& tuple : *this)
			{
				sum += std::get<I>(tuple);
			}
			return (sum / container_t::size());
		}


		static tuple_t make_rel_element(val... values)
		{
			return std::forward_as_tuple(values...);
		}
	};

	template<typename...T>
	using vector_relation = relation<std::vector<std::tuple<T...>>>;
	template<typename...T>
	using list_relation = relation<std::list<std::tuple<T...>>>;
	template<typename...T>
	using set_relation = relation<std::set<std::tuple<T...>>>;

	namespace detail
	{
		//figure out a better way to do this
		template <typename T> class is_relation { public:enum { value = false }; };
		template<typename...T> class is_relation<vector_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class is_relation<vector_relation<T...>&> { public:enum { value = true }; };
		template<typename...T> class is_relation<vector_relation<T...>&&> { public:enum { value = true }; };
		template<typename...T> class is_relation<const vector_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class is_relation<const vector_relation<T...>&> { public:enum { value = true }; };
		template<typename...T> class is_relation<const vector_relation<T...>&&> { public:enum { value = true }; };

		template<typename...T> class is_relation<list_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class is_relation<list_relation<T...>&> { public:enum { value = true }; };
		template<typename...T> class is_relation<list_relation<T...>&&> { public:enum { value = true }; };
		template<typename...T> class is_relation<const list_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class is_relation<const list_relation<T...>&> { public:enum { value = true }; };
		template<typename...T> class is_relation<const list_relation<T...>&&> { public:enum { value = true }; };


		template<typename...T> class is_relation<set_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class is_relation<set_relation<T...>&> { public:enum { value = true }; };
		template<typename...T> class is_relation<set_relation<T...>&&> { public:enum { value = true }; };
		template<typename...T> class is_relation<const set_relation<T...>> { public:enum { value = true }; };
		template<typename...T> class is_relation<const set_relation<T...>&> { public:enum { value = true }; };
		template<typename...T> class is_relation<const set_relation<T...>&&> { public:enum { value = true }; };
		template<typename T, typename base_relation> class is_derived_from_relation { public:enum { value = std::is_base_of_v<base_relation, T> }; };
		template<typename T>
		constexpr bool is_relation_v = is_relation<T>::value;

		template<typename T>
		struct is_linear_relation
		{
			enum {
				value = std::is_same_v<typename T::container_tag, linear_relation_tag>
			};
		};

		template<typename T>
		struct is_map_relation
		{
			enum {
				value = std::is_same_v<typename T::container_tag, map_relation_tag>
			};
		};
		
	}


	template<typename relation>
	class relation_buffer
	{
	public:
		typedef relation relation_t;
		typedef typename relation_t::value_type tuple_t;
		typedef std::vector<std::uint8_t> buffer_t;

		relation_buffer() {}
		explicit relation_buffer(const size_t& size) : mBuffer{size} {
		
		}
		
		template<typename T, std::enable_if_t<std::is_same_v<T,std::string>, int> = 0>
		inline relation_buffer& write(T & value)
		{
			write((static_cast<std::uint32_t>(value.size())));
			std::copy(value.begin(), value.end(), std::back_insert_iterator<buffer_t>{mBuffer});
			return (*this);
		}

		template<typename T, std::enable_if_t<std::is_same_v<T, std::vector<std::uint8_t>>, int> = 0>
		inline relation_buffer& write(const T&value)
		{
			write((static_cast<std::uint32_t>(value.size())));
			std::copy(value.begin(), value.end(), std::back_insert_iterator<buffer_t>{mBuffer});
			return (*this);
		}

		template<typename T, std::enable_if_t<std::is_standard_layout_v<T> || std::is_pod_v<T>, int> = 0>
		inline relation_buffer& write(const T& value)
		{
			constexpr size_t size = sizeof(T);
			std::copy_n((buffer_t::value_type*)&value, size, std::back_insert_iterator<buffer_t>{mBuffer});
			return (*this);
		}

		inline relation_buffer& write(...)
		{
			assert(false && "Cannot serilize the object too complex or not a sqlite fundamental type that should be in a relation");
			return (*this);
		}

		template<typename T, std::enable_if_t<std::is_same_v<T, std::string>, int> = 0>
		inline relation_buffer& read(T& value)
		{
			if (is_buffer_valid())
			{
				std::uint32_t size = 0;
				read(size);
				value.resize(size);
				std::copy(cur_read_iter(), size_to_read(size), value.data());
				mReadHead += size;
			}
			return (*this);
		}

		template<typename T, std::enable_if_t<std::is_same_v<T, std::vector<std::uint8_t>>, int> = 0>
		inline relation_buffer & read(T& value)
		{
			if (is_buffer_valid())
			{
				std::uint32_t size = 0;
				read(size);
				value.resize(size);
				std::copy(cur_read_iter(), size_to_read(size), value.data());
				mReadHead += size;
			}
			return (*this);
		}

		template<typename T, std::enable_if_t<std::is_standard_layout_v<T> || std::is_pod_v<T>, int> = 0>
		inline relation_buffer & read(T& value)
		{
			if (is_buffer_valid())
			{
				std::copy(cur_read_iter(), size_to_read(sizeof(T)), (buffer_t::value_type*)(&value));
				mReadHead += sizeof(T);
				return (*this);
			}
		}

		inline relation_buffer& read(...)
		{
			assert(false && "Cannot read data into the object, object too complex or not a sqlite fundamental type that should be in a relation");
			return (*this);
		}


		constexpr bool is_buffer_valid() const { 
			//
			assert(!mBuffer.empty() && "Reading from an empty buffer");
			assert(mReadHead != mBuffer.size() && "Read head at end"); 
#ifndef _DEBUG
			if (mBuffer.empty() || mReadHead == mBuffer.size())
			{
				return false;
			}
#endif // !_DEBUG
			return true;
		}
		~relation_buffer() { mReadHead = 0; mBuffer.clear(); }
		inline const size_t get_buffer_size() const { return mBuffer.size(); }
		inline const bool read_head_at_end() const { return mReadHead == mBuffer.size(); }
		inline const buffer_t& get_buffer() const { return mBuffer; }
	private:
		inline buffer_t::const_iterator cur_read_iter() const { return std::next(mBuffer.begin(), mReadHead); }
		inline buffer_t::const_iterator size_to_read(size_t size) { return std::next(cur_read_iter(), size); }
	private:
		buffer_t mBuffer{};
		size_t mReadHead{ 0 };
	};

	template<typename relation_t, template<typename> typename buffer_t>
	void read_buffer(relation_t& rel, buffer_t<relation_t>& buffer)
	{
		if constexpr (detail::is_relation_v<relation_t>)
		{
			try {
				assert(rel.empty() && "Relation should be empty, overwrite of data already in the buffer");
				std::insert_iterator<typename relation_t::container_t> insert(rel, rel.begin());
				while (!buffer.read_head_at_end())
				{
					if constexpr (detail::is_linear_relation<relation_t>::value)
					{
						std::back_insert_iterator<typename relation_t::container_t> back_insert(rel);
						back_insert = std::forward<typename relation_t::tuple_t>(detail::loop<std::tuple_size_v<typename relation_t::tuple_t> -1>::template do_buffer_read<relation_t, buffer_t>(buffer));
					}
					else if constexpr (detail::is_map_relation<relation_t>::value)
					{
						insert = std::forward<typename relation_t::tuple_t>(detail::loop<std::tuple_size_v<typename relation_t::tuple_t> -1>::template do_buffer_read<relation_t, buffer_t>(buffer));
					}
				}
			}
			catch (std::exception& ext)
			{
				std::string what = ext.what();
			}
		}
	}

	template<typename relation_t, template<typename> typename buffer_t>
	void write_buffer(relation_t& rel, buffer_t<relation_t>& buffer)
	{
		if constexpr (detail::is_relation_v<relation_t>)
		{
			assert(!rel.empty() && "Cannot write buffer from empty relation");
			for (auto& elem : rel)
			{
				auto& _elem = const_cast<std::remove_const_t<typename relation_t::container_t::value_type>&>(elem);
				detail::loop<std::tuple_size_v<typename relation_t::tuple_t> -1>::template do_buffer_write<relation_t, buffer_t>(buffer, _elem);

			}
		}
	}


};

