#pragma once
#pragma once
#include "../pch.h"
#include "tuple_loop.h"
namespace nl
{
	//todo: remove this template from this, not neccessary anymore
	template<typename relation>
	class relation_buffer
	{
	public:
		typedef relation relation_t;
		typedef typename relation_t::value_type tuple_t;
		typedef std::vector<std::uint8_t> buffer_t;

		relation_buffer() {}
		explicit relation_buffer(const size_t& size) : mBuffer{ size } {

		}

		template<typename T, std::enable_if_t<std::is_same_v<T, std::string>, int> = 0>
		inline relation_buffer & write(T & value)
		{
			write((static_cast<std::uint32_t>(value.size())));
			std::copy(value.begin(), value.end(), std::back_insert_iterator<buffer_t>{mBuffer});
			return (*this);
		}

		template<typename T, std::enable_if_t<std::is_same_v<T, std::vector<std::uint8_t>>, int> = 0>
		inline relation_buffer & write(const T & value)
		{
			write((static_cast<std::uint32_t>(value.size())));
			std::copy(value.begin(), value.end(), std::back_insert_iterator<buffer_t>{mBuffer});
			return (*this);
		}

		template<typename T, std::enable_if_t<std::is_standard_layout_v<T> || std::is_pod_v<T>, int> = 0>
		inline relation_buffer & write(const T & value)
		{
			constexpr size_t size = sizeof(T);
			std::copy_n((buffer_t::value_type*) & value, size, std::back_insert_iterator<buffer_t>{mBuffer});
			return (*this);
		}
		template<typename T, template<class, class> class container, std::enable_if_t<std::is_standard_layout_v<T> || std::is_pod_v<T>, int> = 0>
		inline relation_buffer & write(container<T, alloc_t<T>> & value)
		{
			std::uint32_t size = value.size();
			write(size);
			for (auto& i : value)
			{
				write(i);
			}
			return (*this);
		}

		inline relation_buffer& write(...)
		{
			assert(false && "Cannot serilize the object too complex or not a sqlite fundamental type that should be in a relation");
			return (*this);
		}
		//TODO: handle reading different formats of strings
		template<typename T, std::enable_if_t<std::is_same_v<T, std::string>, int> = 0>
		inline relation_buffer & read(T & value)
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
		inline relation_buffer & read(T & value)
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
		inline relation_buffer & read(T & value)
		{
			if (is_buffer_valid())
			{
				std::copy(cur_read_iter(), size_to_read(sizeof(T)), (buffer_t::value_type*)(&value));
				mReadHead += sizeof(T);
				return (*this);
			}
		}

		//todo: container should actually just be a vector type 
		template<typename T, template<class, class> class container, std::enable_if_t<std::is_standard_layout_v<T> || std::is_pod_v<T>, int> = 0>
		inline relation_buffer & read(container<T, alloc_t<T>> & value)
		{
			if (is_buffer_valid())
			{
				std::uint32_t size = 0;
				read(size);
				for (int i = 0; i < size; i++)
				{
					T val;
					read(val);
					value.push_back(val);
				}
			}
			return(*this);
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
		inline void reset_read_head() { mReadHead = 0; }
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
		if constexpr (detail::has_base_relation<relation_t>::value || detail::is_relation_v<relation_t>)
		{
				assert(rel.empty() && "Relation should be empty, overwrite of data already in the buffer");
				std::insert_iterator<typename relation_t::container_t> insert(rel, rel.begin());
				while (!buffer.read_head_at_end())
				{
					if constexpr (detail::is_linear_relation<relation_t>::value)
					{
						std::back_insert_iterator<typename relation_t::container_t> back_insert(rel);
						back_insert = std::forward<typename relation_t::tuple_t>(detail::loop<relation_t::column_count -1>::template do_buffer_read<relation_t, buffer_t>(buffer));
					}
					else if constexpr (detail::is_map_relation<relation_t>::value)
					{
						insert = std::forward<typename relation_t::tuple_t>(detail::loop<relation_t::column_count- 1>::template do_buffer_read<relation_t, buffer_t>(buffer));
					}
				}
			
		}
	}
	template<typename relation_t, template<typename> typename buffer_t>
	void write_buffer(relation_t& rel, buffer_t<relation_t>& buffer)
	{
		if constexpr (detail::has_base_relation<relation_t>::value || detail::is_relation_v<relation_t>)
		{
			assert(!rel.empty() && "Cannot write buffer from empty relation");
			for (auto& elem : rel)
			{
				auto& _elem = const_cast<std::remove_const_t<typename relation_t::container_t::value_type>&>(elem);
				detail::loop<relation_t::column_count -1>::template do_buffer_write<relation_t, buffer_t>(buffer, _elem);

			}
		}
	}
};