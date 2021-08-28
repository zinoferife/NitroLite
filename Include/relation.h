#pragma once
#include "../pch.h"
	/////////////////////////////////////////////////////////////
	// relation represents a frame of data from the database
	// reads and writes to the database a done via relations
	// operations can be done via relational functions or functions in classes derived from relations
	// relations can be serialized into buffers and maybe transfered across a network
	// this froms the base object of all _TBL objects in the application 

namespace nl {
	template<typename T>
	using alloc_t = std::allocator<T>;

	template<typename ty>
	using hash_t = std::hash<ty>;

	template<typename ty>
	using key_comp_map_t = std::equal_to<ty>;

	template<typename ty>
	using key_comp_set_t = std::less<ty>;




	template<class container> class relation;

	template<template<class, class>typename container, typename... val>
	class relation<container<std::tuple<val...>, alloc_t<std::tuple<val...> > > > : public container<std::tuple<val...>, alloc_t<std::tuple<val...>>>
	{
	public:
		static size_t ROWID;
		using container_t = container<std::tuple<val...>, alloc_t<std::tuple<val...>>>;
		using tuple_t = typename container_t::value_type;


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
	};

	template<template<class, class, class>typename container, typename... val>
	class relation<container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>> >> : public container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>>>
	{

	public:
		using container_t = container<std::tuple<val...>, key_comp_set_t<std::tuple<val...>>, alloc_t<std::tuple<val...>>>;
		using tuple_t = typename container_t::value_type;

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
	};

	template<typename...T>
	using vector_relation = relation<std::vector<std::tuple<T...>>>;
	template<typename...T>
	using list_relation = relation<std::list<std::tuple<T...>>>;
	template<typename...T>
	using set_relation = relation<std::set<std::tuple<T...>>>;

	namespace detail
	{
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
	}


	template<typename relation>
	class relation_buffer
	{
	public:
		typedef relation relation_t;
		typedef typename relation_t::value_type tuple_t;
		typedef std::vector<std::uint8_t> buffer_t;

		relation_buffer() {}

		template<typename T, std::enable_if_t<std::is_same_v<T,std::string>, int> = 0>
		relation_buffer& write(T & value)
		{

		}

		template<typename T, std::enable_if_t<std::is_same_v<T, std::vector<std::uint8_t>>, int> = 0>
		relation_buffer& write(const T&value)
		{

		}

		template<typename T>
		relation_buffer& write(const T& value)
		{
			static_assert(std::is_standard_layout_v<T> || std::is_pod_v<T>, "Cannot serilize complex objects");
			constexpr size_t size = sizeof(T);
			std::copy(cur_read_iter(), size_to_read(size), reinterpret_cast<const buffer_t::value_type*>(&value));
			mReadHead += sizeof(T);
		}


		template<typename T, std::enable_if_t<std::is_same_v<T, std::string>, int> = 0>
		relation_buffer& read(T&)
		{


		}



		const bool read_head_at_end() const { return mReadHead == mBuffer.size(); }
		buffer_t::const_iterator cur_read_iter() const { return std::next(mBuffer.begin(), mReadHead); }
		buffer_t::const_iterator size_to_read(size_t size) { return std::next(cur_read_iter(), size); }
		const size_t get_buffer_size() const { return mBuffer.size(); }

	private:
		buffer_t mBuffer{};
		size_t mReadHead{ 0 };
	};




};

