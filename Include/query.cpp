#include "query.h"

using namespace nl;

query::query()
{

}

query::query(const std::string& query)
{
	mQuery << query;
}

query& query::del(const std::string_view& table)
{
	mQuery << fmt::format("DELETE FROM {} ", table);
	return (*this);
}

query& query::where(const std::string_view& condition)
{
	mQuery << fmt::format("WHERE {} ", condition);
	return (*this);
}

query& query::update(const std::string_view& table)
{
	mQuery << fmt::format("UPDATE {} ", table);
	return (*this);
}
query& query::insert(const std::string_view& table)
{
	mQuery << fmt::format("INSERT INTO {} ", table);
	return (*this);
}

query& query::create_table(const std::string_view& table)
{
	first_col = true;
	mQuery << fmt::format("CREATE TABLE IF NOT EXISTS {} ", table);
	return (*this);
}

query& query::create_view(const std::string_view& view)
{
	mQuery << fmt::format("CREATE VIEW IF NOT EXISTS {} ", view);
	return (*this);
}

query& query::create_index(const std::string_view& index, const std::string_view& table, const std::initializer_list<std::string_view>& colList)
{
	std::string ft = fmt::format("CREATE INDEX {} ON {} ", index, table);
	const size_t size = colList.size();
	if (size > 0)
	{
		ft += "(";
		for (int i = 0; i < size - 1; i++)
		{
			ft += fmt::format("{}, ", *(std::next(colList.begin(), i)));
		}
		ft += fmt::format("{}) ", *(std::next(colList.begin(), size - 1)));
	}
	mQuery << ft;
	return (*this);

}

query& query::create_table_temp(const std::string_view& tempTable)
{
	first_col = true;
	mQuery << fmt::format("CREATE TEMP TABLE {} ", tempTable);
	return (*this);
}

query& query::create_view_temp(const std::string_view& tempView)
{
	// TODO: insert return statement here
	mQuery << fmt::format("CREATE TEMP VIEW {} ", tempView);
	return (*this);
}

query& query::create_index_temp(const std::string_view& index, const std::string_view& table, const std::initializer_list<std::string_view>& colList)
{
	std::string ft = fmt::format("CREATE TEMP INDEX {} ON {} ", index, table);
	const size_t size = colList.size();
	if (size > 0)
	{
		ft += "(";
		for (int i = 0; i < size - 1; i++)
		{
			ft += fmt::format("{}, ", *(std::next(colList.begin(), i)));
		}
		ft += fmt::format("{}) ", *(std::next(colList.begin(), size - 1)));
	}
	mQuery << ft;
	return (*this);


}

query& query::inner_join(const std::initializer_list<std::string_view>& list)
{
	return (*this);
}

query& query::on(const std::initializer_list<std::string_view>& list)
{
	return (*this);
}

query& query::drop_table(const std::string_view& table)
{
	mQuery << fmt::format("DROP TABLE {}", table);
	return (*this);
}

query& query::drop_view(const std::string_view& view)
{
	mQuery << fmt::format("DROP VIEW {}", view);
	return (*this);
}

query& query::drop_trigger(const std::string_view& trigger)
{
	mQuery << fmt::format("DROP TRIGGER {}", trigger);
	return (*this);
}

query& query::and_(const std::string_view& condition)
{
	mQuery << fmt::format("AND {} ", condition);
	return (*this);
}

query& query::or_(const std::string_view& condition)
{
	mQuery << fmt::format("OR {} ", condition);
	return (*this);
}

query& query::not_(const std::string_view& condition)
{
	mQuery << fmt::format("NOT {} ", condition);
	return (*this);
}

query& nl::query::between(const std::string_view& v1, const std::string_view& v2)
{
	mQuery << fmt::format("BETWEEN {} AND {}", v1, v2);
	return (*this);
}

query& query::create_trigger(const std::string_view& trig_name, const std::string_view& table_name)
{
	//not complete, forgot how to write trigger commands 
	mQuery << fmt::format("CREATE TRIGGER {} ON {}", trig_name, table_name);
	return (*this);
}

query& query::end_col()
{
	mQuery << ") ";
	return (*this);
}

query& query::begin()
{
	mQuery << "BEGIN ";
	return (*this);
}

query& nl::query::begin_immediate()
{
	mQuery << "BEGIN IMMEDIATE ";
	return (*this);
}

query& nl::query::roll_back()
{
	mQuery << "ROLLBACK ";
	return (*this);
}

query& query::end()
{
	mQuery << "END ";
	return (*this);
}


