#include "./Include/Singleton.h"
////////////////////////////////////////////////////////////////////////////////
// The Loki Library
// Copyright (c) 2001 by Andrei Alexandrescu
// This code accompanies the book:
// Alexandrescu, Andrei. "Modern C++ Design: Generic Programming and Design 
//     Patterns Applied". Copyright (c) 2001. Addison-Wesley.
// Permission to use, copy, modify, distribute and sell this software for any 
//     purpose is hereby granted without fee, provided that the above copyright 
//     notice appear in all copies and that both that copyright notice and this 
//     permission notice appear in supporting documentation.
// The author or Addison-Wesley Longman make no representations about the 
//     suitability of this software for any purpose. It is provided "as is" 
//     without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////




nl::detail::TrackerArray nl::detail::pTrackerArray = 0;
unsigned int nl::detail::elements = 0;


void nl::detail::at_exit_fn()
{
	assert(elements > 0 && pTrackerArray != 0);
	LifetimeTracker* pTop = pTrackerArray[elements - 1];
	pTrackerArray = static_cast<TrackerArray>(std::realloc(
		pTrackerArray, sizeof(*pTrackerArray) * --elements));
	delete pTop;
}
