#include <gaem/common.hh>
#include <gaem/gfx/shader/compiler.hh>
#include <gaem/util/fs.hh>
#include <unordered_map>
#include <string>
#include <sstream>

namespace gaem::gfx::emsl {
	enum token_type : char {
		tt_eof,
		tt_id,
		tt_tag,
		tt_int,
		tt_float,
		tt_str
	};

	static const char *token_type_names[] = {
		"eof",
		"id",
		"tag",
		"int",
		"float",
		"str",
	};

	struct location {
		int line;
		int col;
	};

	struct token {
		char type;
		std::string value;
		location where;
	};

	void tokenize(strv code, std::vector<token> &out) {
		size_t i = 0;
		location loc { 0, 0 };

		const auto eat = [&](int n = 1) -> char {
			char c;
			for(int j = 0; j < n; ++j) {
				if(i >= code.size()) return EOF;
				c = code[i++];
				if (c == '\n') { loc.col = 0; ++loc.line; }
				else { ++loc.col; }
			}
			return c;
		};

		const auto peek = [&](int n = 0) -> char {
			if(i + n >= code.size()) return EOF;
			return code[i + n];
		};

		while(i < code.size()) {
			fmt::print("@{}, peek: {}\n", i, peek());
			while(std::isspace(peek())) eat();
			if(peek(0) == '/' && peek(1) == '*') {
				while(peek(0) != '*' || peek(1) != '/') eat();
				while(std::isspace(peek())) eat();
			}
			location start_loc = loc;
			if(peek() == EOF) {
				break;
			} else if(std::isalpha(peek()) || peek() == '_') {
				fmt::print("id\n");
				std::string value { 1, eat() };
				while(std::isalnum(peek()) || peek() == '_') {
					value.push_back(eat());
				}
				fmt::print("id doine\n");
				out.push_back(token { tt_id, value, start_loc });
			} else if(std::isdigit(peek())) {
				std::string value { 1, eat() };
				while(std::isdigit(peek()) || peek() == '_')
					if(peek() != '_') value.push_back(eat());
					else eat();

				token_type tt = tt_int;

				if(peek() == '.') {
					value.push_back(eat());
					tt = tt_float;
					while(std::isdigit(peek()) || peek() == '_')
						if(peek() != '_') value.push_back(eat());
						else eat();
				}

				out.push_back(token { tt, value, start_loc });
			} else if(peek() == '#') {
				std::string value { };
				eat();
				while(std::isalnum(peek()) || peek() == '_')
					value.push_back(eat());
				out.push_back(token { tt_tag, value, start_loc });
			} else {
				out.push_back(token { eat(), std::string(), start_loc });
			}
		}
		out.push_back(token { tt_eof, std::string(), loc });
	}

	void print_token(FILE *fout, const token &tok, const char *filename) {
		if(tok.type >= ' ') {
			if(std::isprint(tok.type)) fmt::print(fout, "'{}'", tok.type);
			else fmt::print(fout, "'{}'", (int)tok.type);
		} else {
			fmt::print(fout, "{}: '{}'", token_type_names[(int)tok.type], tok.value);
		}
		fmt::print(" @ {}:{}:{}\n", filename, tok.where.line + 1, tok.where.col + 1);
	}

	shader_module compile_module(const stdfs::path &path) {
		shader_module mod;
		
		auto code = util::fs::read_file(path);
		code.push_back('\0');

		std::vector<token> toks;
		tokenize(strv { code.begin(), code.end() }, toks);

		code.clear(); // hopefully free memory

		for(const auto &tok : toks) print_token(stderr, tok, path.c_str());

		return mod;
	}
}
