#pragma once
#include <ctime>
#include <chrono>
#include <fmt/chrono.h>
#include <regex>

namespace nl
{
	extern bool hour_24;
	using clock = std::chrono::system_clock;
	using date_time_t = std::chrono::system_clock::time_point;
	using nl_date_time_durtation_t = std::chrono::system_clock::duration;
	typedef std::chrono::duration<int, std::ratio<86400>> days;
	extern std::string to_string_date(const date_time_t& date_time);
	extern std::string to_string_time(const date_time_t& date_time);
	
	//wrong-> might still be useful so left in
	extern date_time_t from_string_date(const std::string& date_time);
	extern date_time_t from_string_time(const std::string& date_time);


	//saving the clock data ass representations from epoch
	extern clock::duration::rep to_representation(const date_time_t& time);
	extern date_time_t from_representation(const clock::duration::rep value);
}