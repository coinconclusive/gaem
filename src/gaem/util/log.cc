#include <gaem/util/log.hh>

static constexpr const char *style_gray_ = "\033[90m";
static constexpr const char *style_none_ = "\033[m";
static constexpr const char *bar_string_ = "│ " ; // "| ";
static constexpr const char *val_string_ = "├╴ "; // "|- ";
static constexpr const char *end_string_ = "╰╴ "; // "`- ";

namespace gaem::util::log {
	void logger::puts_(strv s) {
		if(buffering_)
			buffer_.insert(buffer_.end(), s.begin(), s.end());
		else std::fputs(s.data(), file_);
	}

	void logger::putc_(char c) {
		if(buffering_) buffer_.push_back(c);
		else std::fputc(c, file_);
	}

	void logger::output_indent_(int indent) {
		std::fputs(style_gray_, file_);
		for(int i = 0; i < indent; ++i)
			std::fputs(bar_string_, file_);
		std::fputs(style_none_, file_);
	}

	void logger::maybe_flush_(bool always) {
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
	logger default_logger { stderr };
}

namespace gaem::util {
	void fail() {
		exit(1);
	}
}
