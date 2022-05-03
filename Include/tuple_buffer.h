#pragma once
#include "../pch.h"
#include "tuple_loop.h"
#include "nl_time.h"
#include <bitset>
#include <algorithm>
#include <iterator>

namespace nl {
	//class used to write a single row for updating a remote relation
	//bitset 
	template<typename tuple>
	class tuple_buffer {
	public:
		typedef std::bitset<std::tuple_size_v<typename tuple>> state_bit;
		constexpr static size_t state_size = sizeof(typename  state_bit::_Ty);
		typedef tuple tuple_t;
		typedef std::vector<uint8_t> buffer_t;
		tuple_buffer() : mDirtyBit{}, mReadHead{ 0 }, mBuffer{}, mBuffferIter(mBuffer) {}
		explicit tuple_buffer(size_t size) : mDirtyBit{}, mReadHead{ 0 }, mBuffferIter(mBuffer) {
			mBuffer.reserve(size);
		}

		//the idea here is that a single packet can contain rows from different tables
		//the ablity to create one blob_t and read or write the update data from multiple tables

		void deserialise_buffer(const nl::blob_t& buffer, typename nl::blob_t::const_iterator cIter) {
			if (buffer.empty()) return;
			int read_size = 0;
			std::copy(cIter, cIter + sizeof(int), (nl::blob_t::value_type*) & read_size);
			std::advance(cIter, sizeof(int));
			if (cIter == buffer.end()) {
				throw std::logic_error("Reached end of buffer, prematurelly");
			}
			std::copy(cIter, cIter + state_size, (nl::blob_t::value_type*) & mDirtyBit);

			std::advance(cIter, state_size);
			if (cIter == buffer.end()) {
				throw std::logic_error("Reached end of buffer, rematurelly");
			}
			std::copy(cIter, cIter + read_size, mBuffer.begin());
		}

		void seralise_buffer(nl::blob_t& buffer) {
			if (mBuffer.empty() || mDirtyBit.none()) return;
			buffer.reserve((buffer.size() + sizeof(int) + state_size + mBuffer.size()));
			size_t write_size = mBuffer.size();
			std::back_insert_iterator<nl::blob_t> iter(buffer);
			std::copy((nl::blob_t::value_type*) & write_size, sizeof(int), iter);
			std::copy((nl::blob_t::value_type*) & mDirtyBit, state_size, iter);
			std::copy(mBuffer.begin(), mBuffer.end(), iter);
		}

		void set_state(size_t state) {
			mDirtyBit.set(state, true);
		}

		template<typename... states>
		void set_state(states... s) {
			//try is_intergal instead
			static_assert(std::conjunction_v<std::is_convertible<size_t, states>...>, "State type not a size_t type");
			std::array<size_t, sizeof...(s)> sts{ size_t(s)... };
			for (auto& st : sts) {
				mDirtyBit.set(st, true);
			}
		}

		const inline state_bit& get_state() const {
			return mDirtyBit;
		}


		template<typename T,
			std::enable_if_t < std::disjunction_v<std::is_same<typename T::value_type, std::uint8_t>, 
			std::is_same<typename T::value_type, std::int8_t >>, int > = 0 >
		tuple_buffer & write(const T & value) {

			write((static_cast<std::uint32_t>(value.size())));
			std::copy(value.begin(), value.end(), mBuffferIter);
			return (*this);
		}

		template<typename T, std::enable_if_t<std::disjunction_v<std::is_standard_layout<T>, std::is_pod<T>>, int> = 0>
		tuple_buffer & write(const T & value) {
			constexpr size_t size = sizeof(T);
			std::copy_n((buffer_t::value_type*) & value, size, mBuffferIter);
			return (*this);
		}

		tuple_buffer& write(const std::string& value) {
			write(static_cast<std::uint32_t>(value.size()));
			std::copy(value.begin(), value.end(), mBuffferIter);
			return(*this);
		}

		tuple_buffer& write(const nl::uuid& uuid) {
			std::copy(uuid.begin(), uuid.end(), mBuffferIter);
			return (*this);
		}

		tuple_buffer& write(const nl::date_time_t& dt) {
			write(nl::to_representation(dt));
			return (*this);
		}

		template<typename T,
			std::enable_if_t <std::disjunction_v< std::is_same<typename T::value_type, std::uint8_t>,
			std::is_same<typename T::value_type, std::int8_t>>, int> = 0>
		tuple_buffer & read(T & value)
		{
			std::uint32_t read_size = 0;
			read(read_size);
			value.reserve(read_size);
			std::copy_n(std::next(mBuffer.begin(), mReadHead), read_size, std::back_insert_iterator<T>{value});
			mReadHead += read_size;
			return (*this);

		}
		//do for strings
		tuple_buffer& read(std::string& value) {
			std::uint32_t size = 0;
			read(size);
			value.reserve(size);
			std::copy_n(std::next(mBuffer.begin(), mReadHead), size, std::back_insert_iterator<std::string>{value});
			mReadHead += size;
			return (*this);
		}


		template<typename T, std::enable_if_t<std::disjunction_v<std::is_standard_layout<T>, std::is_pod<T>>, int> = 0>
		tuple_buffer & read(T & value)
		{
			constexpr size_t size = sizeof(T);
			std::copy_n(std::next(mBuffer.begin(), mReadHead), size, (buffer_t::value_type*) & value);
			mReadHead += size;
			return (*this);
		}

		tuple_buffer& read(nl::uuid& uuid) {
			//always 16 bytes
			std::copy_n(std::next(mBuffer.begin(), mReadHead), uuid.size(), uuid.begin());
			mReadHead += uuid.size();

			return (*this);
		}
		
		tuple_buffer& read(nl::date_time_t& dt) {
			constexpr size_t size = sizeof(nl::clock::duration::rep);
			nl::clock::duration::rep timev = 0;
			std::copy_n(std::next(mBuffer.begin(), mReadHead), size, (buffer_t::value_type*) & timev);
			dt = nl::from_representation(timev);
			return (*this);
		}

		inline bool read_head_at_end() const { std::next(mBuffer.begin(), mReadHead) == mBuffer.end(); }
		inline size_t get_buffer_size() const { return mBuffer.size(); }

		~tuple_buffer() { reset(); }

		void reset() {
			mDirtyBit.reset();
			mReadHead = 0;
			mBuffer.clear();
		}
	private:
		state_bit mDirtyBit{};
		size_t mReadHead;
		buffer_t mBuffer;
		std::back_insert_iterator<buffer_t> mBuffferIter;
	};


	//reads from the buffer into the tuple
	template<typename tuple_t>
	void read_tuple_buffer(tuple_t& tuple, tuple_buffer<tuple_t>& buffer) {
		//overides the state, that is in the tuple, 
		nl::detail::loop<std::tuple_size_v<tuple_t> -1>::template do_tbuffer_read<tuple_t, tuple_buffer>(tuple, buffer);
	}

	//writes from the tuple into the buffer
	template<typename tuple_t>
	void write_tuple_buffer(tuple_t& tuple, tuple_buffer<tuple_t>& buffer)
	{
		nl::detail::loop<std::tuple_size_v<tuple_t> -1>::template do_tbuffer_write<tuple_t, tuple_buffer>(tuple, buffer);
	}

}

