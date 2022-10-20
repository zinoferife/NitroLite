#include "Include/nl_time.h"
bool nl::hour_24 = true;
std::string nl::to_string_date(const nl::date_time_t& date_time)
{
	std::time_t time = nl::clock::to_time_t(date_time);
	return std::string(fmt::format("{:%Y-%m-%d}", fmt::localtime(time)));
}

nl::date_time_t nl::from_string_date(const std::string& date_time)
{
	std::regex r("(\\d{4})-(0?[1-9]|1[0-2])-(0?[1-9]|[1-2][0-9]|3[0-1])");
	std::smatch match;
	if (std::regex_match(date_time, match, r))
	{
		std::tm time_tm{0};
		time_tm.tm_year = std::stoi(match[1]) - 1900; // from 1900
		time_tm.tm_mon = std::stoi(match[2]) - 1; // from 0 t0 11
		time_tm.tm_mday = std::stoi(match[3]);
		return nl::clock::from_time_t(std::mktime(&time_tm));
	}
	else{
		throw std::logic_error("date_time formart is not a valid date time");
	}
	//should not reach here
	return nl::date_time_t();
}

std::string nl::to_string_time(const date_time_t& date_time)
{
	std::time_t time = nl::clock::to_time_t(date_time);
	return std::string(fmt::format("{:%R}", fmt::localtime(time)));	
}

nl::date_time_duration_t nl::time_since_midnight()
{
	auto now = nl::clock::now();

	std::time_t tnow = nl::clock::to_time_t(now);
	tm date;
	localtime_s(&date, &tnow);
	date.tm_mday += 1;
	date.tm_hour = 0;
	date.tm_min = 0;
	date.tm_sec = 0;

	auto midnight = nl::clock::from_time_t(std::mktime(&date));

	return now - midnight;
}

nl::date_time_duration_t nl::time_since_midnight(const nl::date_time_t& now)
{
	std::time_t tnow = nl::clock::to_time_t(now);
	tm date;
	localtime_s(&date, &tnow);
	date.tm_mday += 1;
	date.tm_hour = 0;
	date.tm_min = 0;
	date.tm_sec = 0;

	auto midnight = nl::clock::from_time_t(std::mktime(&date));

	return now - midnight;
}

nl::date_time_duration_t nl::time_to_midnight()
{
	auto now = nl::clock::now();

	std::time_t tnow = nl::clock::to_time_t(now);
	tm date; 
	localtime_s(&date, &tnow);
	date.tm_mday += 1;
	date.tm_hour = 0;
	date.tm_min = 0;
	date.tm_sec = 0;

	auto midnight = nl::clock::from_time_t(std::mktime(&date));

	return midnight - now;
}

nl::date_time_duration_t nl::time_to_midnight(const nl::date_time_t& now)
{
	std::time_t tnow = nl::clock::to_time_t(now);
	tm date;
	localtime_s(&date, &tnow);
	date.tm_mday += 1;
	date.tm_hour = 0;
	date.tm_min = 0;
	date.tm_sec = 0;

	auto midnight = nl::clock::from_time_t(std::mktime(&date));

	return midnight - now;
}

nl::date_time_t nl::from_string_time(const std::string& date_time)
{
	//24-hour and 12 hour clock
	std::regex r24("(0?[0-9]|1[0-9]|2[0-3]):(0?[0-9]|[1-5][0-9])");
	std::regex r12("(0?[0-9]|1[0-1]):(0?[0-9]|[1-5][0-9])");
	std::smatch match;
	if (std::regex_match(date_time, match, hour_24 ? r24 : r12)){
		std::tm time_tm{ 0 };
		time_tm.tm_hour = std::stoi(match[1]);
		time_tm.tm_min = std::stoi(match[2]);
		return nl::clock::from_time_t(std::mktime(&time_tm));
	}
	else {
		throw std::logic_error("date_time formart is not a valid date time");
	}
	//should not reach here
	return nl::date_time_t();
}

nl::clock::duration::rep nl::to_representation(const date_time_t& time)
{
	return time.time_since_epoch().count();
}

nl::date_time_t nl::from_representation(const clock::duration::rep value)
{
	return clock::time_point(clock::duration(value));
}
