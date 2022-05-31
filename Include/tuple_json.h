#pragma once
#pragma once
#include <vector>
#include <string>
#include <array>
#include <nlohmann/json.hpp>


#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <type_traits>

#include "tuple_loop.h"
#include <bitset>



//converts a jsons to a tuple and tuple to a json object
//static class, works as a monostate
//the current state of the bitset is used by any of the conversion functions
namespace nl {
	namespace js = nlohmann;

	template<typename tuple>
	class tuple_json
	{
	public:
		typedef tuple tuple_t;
		typedef std::array<std::string, std::tuple_size_v<tuple_t>> obj_name_vec_t;
		typedef std::bitset<std::tuple_size_v<tuple_t>> state_t;
		using json_t = nlohmann::json;


		template<size_t i>
		struct for_each_tuple {

			static void add_tuple_to_json(const tuple_t& t,
				js::json& json_object, const obj_name_vec_t& names,
				const state_t& state)
			{
				if (state.test(i)) {
					using arg_t = std::decay_t<std::tuple_element_t<i, tuple_t>>;
					if constexpr (std::is_same_v<arg_t, nl::uuid>) {
						json_object[names[i]] = boost::uuids::to_string(std::get<i>(t));
					}
					else if constexpr (std::is_same_v<arg_t, nl::date_time_t>) {
						auto rep = nl::to_representation(std::get<i>(t));
						json_object[names[i]] = static_cast<std::uint64_t>(rep);
					}
					else if constexpr (std::is_same_v<arg_t, nl::blob_t>) {
						//skip ?? or how do i write blob data to json
					}
					else {
						json_object[names[i]] = std::get<i>(t);
					}
				}
				for_each_tuple<i - 1>::template add_tuple_to_json(t, json_object, names,
					state);
			}

			static void add_json_to_tuple(tuple_t& t, const js::json& json_object, const obj_name_vec_t& names,
				const state_t& state) {
				if (state.test(i)) {
					using arg_t = std::decay_t<std::tuple_element_t<i, tuple_t>>;
					if constexpr (std::is_same_v<arg_t, nl::uuid>) {
						std::get<i>(t) = nl::uuid(boost::uuids::string_generator()(fmt::to_string(json_object[names[i]])));
					}
					else if constexpr (std::is_same_v<arg_t, nl::date_time_t>) {
						auto rep = nl::from_representation(json_object[names[i]]);
						std::get<i>(t) = nl::date_time_t(rep);
					}
					else if constexpr (std::conjunction_v<std::is_same<std::string, arg_t>, std::is_convertible<arg_t, std::string>>) {
						std::get<i>(t) = fmt::to_string(json_object[names[i]]);
					}
					else {
						std::get<i>(t) = json_object[names[i]];
					}
				}
				for_each_tuple<i - 1>::template add_json_to_tuple(t, json_object, 
					names, state);
			}

		};

		template<>
		struct for_each_tuple<0>
		{
			static void add_tuple_to_json(const tuple_t& t, js::json& json_object,
				const obj_name_vec_t& names, const state_t& state)
			{
				if (state.test(0)) {
					using arg_t = std::decay_t<std::tuple_element_t<0, tuple_t>>;
					if constexpr (std::is_same_v<arg_t, nl::uuid>) {
						json_object[names[0]] = boost::uuids::to_string(std::get<0>(t));
					}
					else if constexpr (std::is_same_v<arg_t, nl::date_time_t>) {
						auto rep = nl::to_representation(std::get<0>(t));
						json_object[names[0]] = static_cast<std::uint64_t>(rep);
					}
					else if constexpr (std::is_same_v<arg_t, nl::blob_t>) {
						//skip ?? or how do i write blob data to json
					}
					else {
						json_object[names[0]] = std::get<0>(t);
					}
				}
			}

			static void add_json_to_tuple(tuple_t& t, const js::json& json_object, const obj_name_vec_t& names,
				const state_t& state) {
				if (state.test(0)) {
					using arg_t = std::decay_t<std::tuple_element_t<0, tuple_t>>;
					if constexpr (std::is_same_v<arg_t, nl::uuid>) {
						std::get<0>(t) = nl::uuid(boost::uuids::string_generator()(fmt::to_string(json_object[names[0]])));
					}
					else if constexpr (std::is_same_v<arg_t, nl::date_time_t>) {
						auto rep = nl::from_representation(json_object[names[0]]);
						std::get<0>(t) = nl::date_time_t(rep);
					}
					else if constexpr (std::conjunction_v<std::is_same<std::string, arg_t>, std::is_convertible<arg_t, std::string>>) {
						std::get<0>(t) = fmt::to_string(json_object[names[0]]);
					}
					else {
						std::get<0>(t) = json_object[names[0]];
					}
				}
			}
		};


		static obj_name_vec_t& get_obj_names()
		{
			return m_obj_names;
		}

		static state_t& get_state() { return m_state; }

		static json_t convert_to_json(const tuple_t& tuple) {
			json_t json_obj;
			for_each_tuple<std::tuple_size_v<tuple_t> -1>::template add_tuple_to_json(tuple,
				json_obj, m_obj_names, m_state);
			return json_obj;

		}
		static tuple_t convert_from_json(const json_t& js) {
			tuple_t row;
			for_each_tuple<std::tuple_size_v<tuple_t> -1>::template add_json_to_tuple(row,
				js, m_obj_names, m_state);
			return row;
		}

		static void set_object_name(const std::string& name, size_t tuple_index) {
			if (m_obj_names.size() <= tuple_index) {
				return;
			}
			m_obj_names[tuple_index] = name;
		}

		static void set_state(const state_t& state) { m_state = state; }

		static void set(size_t flag, bool state = true){
			m_state.set(flag, state);
		}
		static void set_all_state() {
			m_state.set();
		}
		static void clear_all_state(){
			m_state.reset();
		}

	private:
		static obj_name_vec_t m_obj_names;
		static state_t m_state;
	};

	template<typename T>
	std::array<std::string, std::tuple_size_v<T>> tuple_json<T>::m_obj_names;

	template<typename T>
	std::bitset<std::tuple_size_v<T>> tuple_json<T>::m_state;

}
