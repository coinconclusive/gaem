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

			static constexpr const char *style_gray_ = "\033[90m";
			static constexpr const char *style_none_ = "\033[m";
			static constexpr const char *bar_string_ = "│ " ; // "| ";
			static constexpr const char *val_string_ = "├╴ "; // "|- ";
			static constexpr const char *end_string_ = "╰╴ "; // "`- ";

			void puts_(strv s) {
				if(buffering_)
					buffer_.insert(buffer_.end(), s.begin(), s.end());
				else std::fputs(s.data(), file_);
			}

			void putc_(char c) {
				if(buffering_) buffer_.push_back(c);
				else std::fputc(c, file_);
			}

			void output_indent_(int indent) {
				std::fputs(style_gray_, file_);
				for(int i = 0; i < indent; ++i)
					std::fputs(bar_string_, file_);
				std::fputs(style_none_, file_);
			}

			void maybe_flush_(bool always = false) {
				// when not buffering, this function is called
				// to update indentation (before writing the
				// indented message).

				// when buffering, this functions is called
				// to output the stored buffer (and clear it)
				// with the corresponding indentatio. (before
				// writing the next message).

				// don't do anything of the above if called
				// when message hasn't been finished yet:
				if(!always && !had_nl_) return;

				for(int i = 0; i < spread_out_; ++i) {
					output_indent_(buffer_indent_);
					std::fputs(style_gray_, file_);
					std::fputs(bar_string_, file_);
					std::fputc('\n', file_);
					std::fputs(style_none_, file_);
				}
				output_indent_(buffer_indent_);
				std::fputs(style_gray_, file_);
				if(!buffering_) {
					// always write the end-string when not buffering
					// since we don't know the future.
					std::fputs(end_string_, file_);
				} else {
					std::fputs(
						indent_ >= buffer_indent_
							? val_string_
							: end_string_,
						file_
					);
				}
				std::fputs(style_none_, file_);
				had_nl_ = false;
				if(buffering_) {
					buffer_.push_back('\0'); // the buffer isn't null-terminated.
					std::fputs(buffer_.data(), file_);
					buffer_.clear();
					buffer_indent_ = indent_;
				}
			}
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

		static logger default_logger { stderr };
	}

	[[maybe_unused]] static auto &clog = util::log::default_logger;
}

namespace util {
	template<typename ...Ts>
	void print_error(fmt::format_string<Ts...> f, Ts ...ts) {
		std::fputs("\033[31mError\33[m:", stderr);
		fmt::print(stderr, f, std::forward<Ts>(ts)...);
		std::fputc('\n', stderr);
	}

	void fail() {
		exit(1);
	}

	template<typename ...Ts>
	void fail_error(fmt::format_string<Ts...> f, Ts ...ts) {
		print_error(f, std::forward<Ts>(ts)...);
		fail();
	}
}
#endif
