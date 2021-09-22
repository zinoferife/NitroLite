#pragma once
#include <utility>
#include <cassert>
#include <list>

//implementation based on Molecular Musings's awesome blog 
// at: https://blog.molecular-matters.com

namespace nl {
	template<typename R, typename... Args>
	class table_listener
	{
		typedef R return_type;
		typedef void* instance_ptr;
		typedef R(*instance_function)(instance_ptr, Args...);
		typedef std::pair<instance_ptr, instance_function> listener;

		template<R(*function)(Args...)>
		static inline R function_stub(instance_ptr, Args... args)
		{
			return (function)(args...);
		}

		template<class C, R(C::*mem_function)(Args...)>
		static inline R mem_function_stub(instance_ptr instance, Args... arg)
		{
			return (static_cast<C*>(instance)->*mem_function)(arg...);
		}

	public:
		static constexpr size_t arg_count = sizeof...(Args);
		table_listener() {}
		template<R(*function)(Args...)>
		void add_listener()
		{
			m_listeners.emplace_back(std::make_pair(nullptr, &function_stub<function>));
		}

		template<class C, R(C::*mem_func)(Args...)>
		void add_listener(C* instance)
		{
			m_listeners.emplace_back(std::make_pair(instance, &mem_function_stub<C, mem_func>));
		}

		template<R(*function)(Args...)>
		void remove_listener()
		{
			std::remove_if(m_listeners.begin(), m_listeners.end(), [&](listener& listen)-> bool {
				return (listen.second == &function_stub<function>);
				});
		}
		template<class C, R(C::* mem_func)(Args...)>
		void remove_listener(C* instance)
		{
			std::remove_if(m_listeners.begin(), m_listeners.end(), [&](listener& listen)-> bool {
				return (listen.first == instance && listen.second == &mem_function_stub<C, mem_func>);
			});
		}

		void notify(Args... arg) const
		{
			if (!m_listeners.empty())
			{
				for (auto iter = m_listeners.begin(); iter != m_listeners.end(); iter++)
				{
					assert(iter->second != nullptr && "Cannot notify an empty listeneer");
					(*iter).second((*iter).first, arg...);
				}
			}
		}

		

	private:
		std::list<listener> m_listeners;
	};
}