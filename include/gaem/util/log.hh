#ifndef GAEM_UTIL_LOG_HH_
#define GAEM_UTIL_LOG_HH_
#include <gaem/common.hh>
#include <fmt/core.h>
#include <fmt/std.h>
#include <fmt/ranges.h>
#include <vector>

namespace gaem {
	namespace util::log {
		class logger {
			std::FILE *file_;
			bool had_nl_ = true;
			int indent_ = 0;
			int spread_out_ = 0;
			bool buffering_ = true;
			int buffer_indent_ = 0;
			std::vector<char> buffer_;

			void puts_(strv s);
			void putc_(char c);
			void output_indent_(int indent);
			void maybe_flush_(bool always = false);

		public:
			logger(std::FILE *file) : file_(file) {}

			void indent() {
				assert(had_nl_ && "must have newline before indenting.");
				++indent_;
			}

			void dedent() {
				assert(had_nl_ && "must have newline before dedenting.");
				--indent_;
			}

			void set_buffering(bool v) { buffering_ = v; }
			bool get_buffering() { return buffering_; }
			void set_spread_out(int v) { spread_out_ = v; }
			[[nodiscard]] int get_spread_out() const { return spread_out_; }

			void flush() {
				maybe_flush_(true);
			}

			template<typename ...Ts>
			void println(fmt::format_string<Ts...> f, Ts ...ts) {
				print(f, std::forward<Ts>(ts)...);
				newline();
			}

			void newline() {
				putc_('\n');
				had_nl_ = true;
				buffer_indent_ = indent_;
			}

			template<typename ...Ts>
			void print(fmt::format_string<Ts...> f, Ts ...ts) {
				maybe_flush_();
				if(buffering_) {
					buffer_.reserve(buffer_.size() + fmt::formatted_size(f, std::forward<Ts>(ts)...));
					fmt::format_to(std::back_inserter(buffer_), f, std::forward<Ts>(ts)...);
				} else {
					fmt::print(file_, f, std::forward<Ts>(ts)...);
				}
			}
		};

		extern logger default_logger;
	}

	[[maybe_unused]] static auto &clog = util::log::default_logger;
}

namespace gaem::util {
	template<typename ...Ts>
	void print_error(fmt::format_string<Ts...> f, Ts ...ts) {
		std::fputs("\033[31mError\33[m:", stderr);
		fmt::print(stderr, f, std::forward<Ts>(ts)...);
		std::fputc('\n', stderr);
	}

	void fail();

	template<typename ...Ts>
	void fail_error(fmt::format_string<Ts...> f, Ts ...ts) {
		print_error(f, std::forward<Ts>(ts)...);
		fail();
	}
}
#endif
