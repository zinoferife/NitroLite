#pragma once
#include "../pch.h"

//interface for table managers 
//these mangers load relations from the database, write relations to the database and update tables from the databse

class manager
{
public:
	virtual ~manager() {}

	virtual bool load() = 0;
	virtual bool store() = 0;
	virtual bool update() = 0;
	virtual bool del() = 0;

};