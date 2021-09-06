#pragma once
//singleton holder implementation based on Andrei alexandrescu implentations
//=////////////////////////////////////////////////////////////////////////////////
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

#include "../pch.h"
namespace nl
{
	namespace detail
	{
		class LifetimeTracker
		{
		public:
			LifetimeTracker(unsigned int x) : longevity_(x)
			{}

			virtual ~LifetimeTracker() = 0;

			static bool Compare(const LifetimeTracker* lhs,
				const LifetimeTracker* rhs)
			{
				return lhs->longevity_ < rhs->longevity_;
			}

		private:
			unsigned int longevity_;
		};

		// Definition required
		inline LifetimeTracker::~LifetimeTracker() {}

		// Helper data
		typedef LifetimeTracker** TrackerArray;
		extern TrackerArray pTrackerArray;
		extern unsigned int elements;

		// Concrete lifetime tracker for objects of type T
		template <typename Destroyer>
		class ConcreteLifetimeTracker : public LifetimeTracker
		{
		public:
			ConcreteLifetimeTracker(unsigned int longevity, Destroyer d)
				: LifetimeTracker(longevity)
				, destroyer_(d)
			{}

			~ConcreteLifetimeTracker()
			{
				destroyer_();
			}

		private:
			Destroyer destroyer_;
		};

		void at_exit_fn();

		template <class singleton>
		void set_longevity(unsigned int longevity)
		{
			TrackerArray pNewArray = static_cast<TrackerArray>(std::realloc(pTrackerArray, sizeof(*pTrackerArray) * (elements + 1)));
			if (!pNewArray) throw std::bad_alloc();
			pTrackerArray = pNewArray;
			LifetimeTracker* p = new ConcreteLifetimeTracker(longevity, typename singleton::destroyer{});
			TrackerArray pos = std::upper_bound(pTrackerArray, pTrackerArray + elements, p, LifetimeTracker::Compare);
			std::copy_backward(pos, pTrackerArray + elements, pTrackerArray + elements + 1);
			*pos = p;
			++elements;
			std::atexit(at_exit_fn);
		}
	};




	template<typename T>
	class singleton_holder
	{
	public:
		inline static T& instance()
		{
			if (!mInstance)
			{
				if (destoryed_)
				{
					on_dead_reference();
					destoryed_ = false;
				}
				std::call_once(m_once_flag, []() { do_create(); });
			}
			return (*mInstance);
		}

		//if first call is this it creates a singleton with argeuments
		template<typename...Args>
		inline static T& instance(Args... args)
		{
			if (!mInstance)
			{
				if (destoryed_)
				{
					on_dead_reference();
					destoryed_ = false;
				}
				std::call_once(m_once_flag, do_create, args);
			}
			return (*mInstance);
		}
		template<class singleton>
		friend void detail::set_longevity(unsigned int longevity);

	private:
		static void destory()
		{
			assert(!destoryed_ && "Singleton already destoryed");
			mInstance.release();
			destoryed_ = true;
		}

		static void on_dead_reference()
		{
			throw std::logic_error("Dead reference detected, singleton life time my need to be investigated");
		}

		static void do_create()
		{
			mInstance = std::make_unique<T>();
		}
		template<typename... Args>
		static void do_create(Args... args)
		{
			mInstance = std::make_unique<T>(args);
		}

	private:
		struct destroyer
		{
			void operator()()
			{
				singleton_holder::destory();
			}
		};


		singleton_holder() {}
		singleton_holder(singleton_holder&) = delete;
		singleton_holder& operator=(singleton_holder&) = delete;
		~singleton_holder() {}
	
	
	private:
		static std::unique_ptr<T> mInstance;
		static bool destoryed_;
		static std::once_flag m_once_flag;
	};


	//singleton data
	template<typename T>
	std::unique_ptr<T> singleton_holder<T>::mInstance{};

	template<typename T>
	bool singleton_holder<T>::destoryed_ = false;

	template<typename T>
	std::once_flag singleton_holder<T>::m_once_flag{};

};