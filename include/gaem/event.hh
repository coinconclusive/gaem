#ifndef GAEM_EVENT_HH_
#define GAEM_EVENT_HH_
#include <gaem/common.hh>
#include <functional>

namespace gaem {
	template<typename T>
	class event_hook;

	template<typename T>
	class event {
		friend event_hook<T>;
	public:
		template<typename ...Args> requires std::invocable<std::function<T>, Args...>
		void dispatch(Args &&...args) const {
			for(const auto &handler : handlers_)
				handler(std::forward<Args>(args)...);    
		}
	private:
		std::vector<std::function<T>> handlers_;
	};

	template<typename T>
	class event_hook {
		event<T> &event_;
	public:
		event_hook(event<T> &event) : event_(event) {}

		template<typename F>
		void add(F &&f) {
			event_.handlers_.push_back(std::move(f));
		}

		template<typename F>
		void add(const F &f) {
			event_.handlers_.push_back(f);
		}
	};
}

#endif
