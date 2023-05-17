#include <gaem/common.hh>
#include <gaem/gfx/shader/compiler.hh>
#include <gaem/util/fs.hh>
#include <unordered_map>
#include <string>
#include <sstream>
#include <fmt/ostream.h>
#include <fmt/std.h>

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
			while(std::isspace(peek())) eat();
			if(peek(0) == '/' && peek(1) == '*') {
				while(peek(0) != '*' || peek(1) != '/') eat();
				while(std::isspace(peek())) eat();
			}
			location start_loc = loc;
			if(peek() == EOF) {
				break;
			} else if(std::isalpha(peek()) || peek() == '_') {
				std::string value;
				while(std::isalnum(peek()) || peek() == '_') {
					value.push_back(eat());
				}
				out.push_back(token { tt_id, value, start_loc });
			} else if(std::isdigit(peek())) {
				std::string value;
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
				std::string value;
				eat();
				while(std::isalnum(peek()) || peek() == '_')
					value.push_back(eat());
				out.push_back(token { tt_tag, value, start_loc });
			} else {
				out.push_back(token { eat(), {}, start_loc });
			}
		}
		out.push_back(token { tt_eof, {}, loc });
	}

	std::string token_type_to_string(char type) {
		if(type >= ' ') {
			if(std::isprint(type)) return fmt::format("'{}'", type);
			else return fmt::format("'{}'", (int)type);
		} else {
			return fmt::format("{}", token_type_names[(int)type]);
		}
	}

	void print_token(FILE *fout, const token &tok, const char *filename) {
		fmt::print(fout, "{}", token_type_to_string(tok.type));
		if(tok.type < ' ') fmt::print(fout, ": '{}'", tok.value);
		fmt::print(fout, " @ {}:{}:{}\n", filename, tok.where.line + 1, tok.where.col + 1);
	}

	using value_type = shader_module_value_type;

	struct codegen {
		std::ostream &fout;
		int indent;
		bool last_nl = true;

		template<typename ...Args>
		void gen(fmt::format_string<Args...> f, Args &&...args) {
			// not concerned with speed and memory efficiency right now.
			std::string s = fmt::format(f, std::forward<Args>(args)...);
			if(last_nl)
				fmt::print(fout, "{:{}}", "", indent * 2);
			fout << s;
			last_nl = s.find('\n') != s.npos;
		}

		const char *value_type_to_typename(value_type t) {
			// GLSL:
			switch(t) {
			case shader_module_value_type::float1: return "float";
			case shader_module_value_type::float2: return "vec2";
			case shader_module_value_type::float3: return "vec3";
			case shader_module_value_type::float4: return "vec4";
			case shader_module_value_type::float4x4: return "mat4";
			case shader_module_value_type::sampler2D: return "sampler2D";
			case shader_module_value_type::void1: return "void";
			case shader_module_value_type::int1: return "int";
			default: return "void /* value_type_to_typename NOT IMPLEMENTED! */";
			}
		}

		void visit(struct ast_name &n);
		void visit(struct ast_int &n);
		void visit(struct ast_float &n);
		void visit(struct ast_return &n);
		void visit(struct ast_block &n);
		void visit(struct ast_access &n);
		void visit(struct ast_call &n);
		void visit(struct ast_bin &n);
		void visit(struct ast_unr &n);
		void visit(struct ast_let &n);
		void visit(struct ast_set &n);

		void generate_module(const shader_module &mod);
		void generate_field(const shader_module_field &field);
		void generate_function(const shader_module_function &func);
	};

	// renaming
	struct alpha_converter {
		strv old_name, new_name;
		bool visit(struct ast_name &n);
		bool visit(struct ast_int &n);
		bool visit(struct ast_float &n);
		bool visit(struct ast_return &n);
		bool visit(struct ast_block &n);
		bool visit(struct ast_access &n);
		bool visit(struct ast_call &n);
		bool visit(struct ast_bin &n);
		bool visit(struct ast_unr &n);
		bool visit(struct ast_let &n);
		bool visit(struct ast_set &n);

		// NB: doesn't rename fields.
		void visit_module(shader_module &mod);
	};

	struct typechecker {
		void visit(struct ast_let &n);
		void visit(struct ast_set &n);
	};

	struct ast {
		location where;
		ast(const location &where) : where(where) {}
		virtual ~ast() = default;

		virtual void accept(codegen &v) = 0;
		virtual bool accept(alpha_converter &v) = 0;
		// virtual void accept(typechecker &v) = 0;
	};

	using astp = std::unique_ptr<ast>;

	template<typename T>
	struct impl_ast : ast {
		void accept(codegen &v) final { v.visit(*(T*)this); }
		bool accept(alpha_converter &v) final { return v.visit(*(T*)this); }
		// void accept(typechecker &v) { v.visit(*(T*)this); }
		impl_ast(const location &where) : ast(where) {}
	};

	struct ast_name : impl_ast<ast_name> {
		std::string name;
		ast_name(const location &where, strv name)
			: impl_ast(where), name(name) {}
	};

	struct ast_int : impl_ast<ast_int> {
		intmax_t value;
		ast_int(const location &where, intmax_t value)
			: impl_ast(where), value(value) {}
	};

	struct ast_float : impl_ast<ast_float> {
		float value;
		ast_float(const location &where, float value)
			: impl_ast(where), value(value) {}
	};

	struct ast_access : impl_ast<ast_access> {
		astp obj;
		std::string field;
		ast_access(const location &where, astp &&obj, strv field)
			: impl_ast(where), obj(std::move(obj)), field(field) {}
	};

	struct ast_let : impl_ast<ast_let> {
		std::string name;
		value_type type;
		std::optional<astp> val;
		ast_let(const location &where, strv name, value_type type, std::optional<astp> &&val)
			: impl_ast(where), name(name), type(type), val(std::move(val)) {}
	};

	struct ast_set : impl_ast<ast_set> {
		astp var, val;
		ast_set(const location &where, astp &&var, astp &&val)
			: impl_ast(where), var(std::move(var)), val(std::move(val)) {}
	};

	struct ast_bin : impl_ast<ast_bin> {
		char op;
		astp lhs, rhs;
		ast_bin(const location &where, char op, astp &&lhs, astp &&rhs)
			: impl_ast(where), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
	};

	struct ast_unr : impl_ast<ast_unr> {
		char op;
		astp val;
		ast_unr(const location &where, char op, astp &&val)
			: impl_ast(where), op(op), val(std::move(val)) {}
	};

	struct ast_return : impl_ast<ast_return> {
		std::optional<astp> val;
		ast_return(const location &where, std::optional<astp> &&val)
			: impl_ast(where), val(std::move(val)) {}
	};

	struct ast_call : impl_ast<ast_call> {
		astp func;
		std::vector<astp> params;
		ast_call(const location &where, astp &&func, std::vector<astp> &&params)
			: impl_ast(where), func(std::move(func)), params(std::move(params)) {}
	};

	struct ast_block : impl_ast<ast_block> {
		std::vector<astp> stmts;
		ast_block(const location &where, std::vector<astp> &&stmts)
			: impl_ast(where), stmts(std::move(stmts)) {}
	};

	struct parser_exception {
		location where;
		std::string msg;
	};

	struct shader_module_source_code {
		std::unique_ptr<ast_block> ast;
		shader_module_source_code(std::unique_ptr<ast_block> &&ast)
			: ast(std::move(ast)) {}
	};

	struct parser {
		size_t i = 0;
		std::span<const token> tokens;

		const token &eat() {
			if(i >= tokens.size()) return tokens[tokens.size() - 1];
			return tokens[i++];
		}

		const token &peek(int n = 1) {
			if(i + n - 1 >= tokens.size()) return tokens[tokens.size() - 1];
			return tokens[i + n - 1];
		}

		[[noreturn]] void error(strv msg, const location &where) {
			throw parser_exception{ where, msg.data() };
		}

		[[noreturn]] void error_expected(strv msg, const location &where) {
			error("expected "s + msg.data(), where);
		}

		const token &expect_kw(strv kw) {
			const auto &t = eat();
			if(t.type != tt_id || strcmp(t.value.c_str(), kw.data()) != 0)
				error_expected("'"s + kw.data() + "' keyword", t.where);
			return t;
		}

		const token &expect(char type) {
			const auto &t = eat();
			if(t.type != type)
				error_expected(token_type_to_string(type), t.where);
			return t;
		}

		const token &expect_dont_eat(char type) {
			const auto &t = peek();
			if(t.type != type)
				error_expected(token_type_to_string(type), t.where);
			return t;
		}

		const token &expect(char type, strv msg) {
			const auto &t = eat();
			if(t.type != type)
				error_expected(token_type_to_string(type) + " "s + msg.data(), t.where);
			return t;
		}

		const token &expect_dont_eat(char type, strv msg) {
			const auto &t = peek();
			if(t.type != type)
				error_expected(token_type_to_string(type) + " "s + msg.data(), t.where);
			return t;
		}

		bool is_kw(strv kw) {
			const auto &t = peek();
			return t.type == tt_id && t.value == kw;
		}

		void parse_module_type(shader_module_type &type) {
			bool is_partial = false;
			if(is_kw("partial")) {
				is_partial = true;
				eat();
			}
			if(is_kw("fragment")) {
				type = is_partial
					? shader_module_type::partial_fragment
					: shader_module_type::entry_fragment;
			} else if(is_kw("vertex")) {
				type = is_partial
					? shader_module_type::partial_vertex
					: shader_module_type::entry_vertex;
			} else {
				error_expected("'fragment' or 'vertex' keywords", peek().where);
			}
			eat();
		}

		constexpr value_type typename_to_value_type(strv name, const location &where) {
			if(name == "float") return value_type::float1;
			if(name == "float2") return value_type::float2;
			if(name == "float3") return value_type::float3;
			if(name == "float4") return value_type::float4;
			if(name == "float4x4") return value_type::float4x4;
			if(name == "int") return value_type::int1;
			if(name == "void") return value_type::void1;
			if(name == "sampler2D") return value_type::sampler2D;
			error_expected("typename", where);
		}

		void parse_field(shader_module_field &field) {
			if(is_kw("in")) field.field_type = shader_module_field_type::input;
			else if(is_kw("out")) field.field_type = shader_module_field_type::output;
			else if(is_kw("uniform")) field.field_type = shader_module_field_type::uniform;
			else error_expected("'in', 'out' or 'uniform' keywords", peek().where);
			eat();

			if(peek().type == tt_tag) {
				field.tag = eat().value;
			} else if(peek().type == tt_int) {
				field.tag = std::stoi(eat().value);
			}

			field.name = expect(tt_id, "for field name").value;
			auto type = expect(tt_id, "for field type");
			field.type = typename_to_value_type(type.value, type.where);
		}

		astp parse_atom() {
			const auto &t = eat();
			if(t.type == tt_id)
				return std::make_unique<ast_name>(t.where, t.value);
			if(t.type == tt_int)
				return std::make_unique<ast_int>(t.where, std::stoi(t.value));
			if(t.type == tt_float)
				return std::make_unique<ast_float>(t.where, std::stof(t.value));
			if(t.type == '(') {
				astp r = parse_expr();
				expect(')', "for closing parenthesis");
				return r;
			}
			error_expected("variable, int, float or subexpression", t.where);
		}

		astp parse_prefix_suffix() {
			if(peek().type == '-') {
				const auto &t = eat();
				return std::make_unique<ast_unr>(t.where, t.type, parse_prefix_suffix());
			}
			astp r = parse_atom();
			while(peek().type == '(' || peek().type == '.') {
				if(peek().type == '.') {
					const auto &t = eat();
					const auto &field = expect(tt_id);
					r = std::make_unique<ast_access>(t.where, std::move(r), field.value);
				} else {
					std::vector<astp> params;
					const auto &t = peek();
					if(peek(2).type != ')') {
						do {
							eat();
							params.push_back(parse_expr());
						} while(peek().type == ',');
					} else eat();
					expect(')');
					r = std::make_unique<ast_call>(t.where, std::move(r), std::move(params));
				}
			}
			return r;
		}

		astp parse_product() {
			astp lhs = parse_prefix_suffix();
			while(peek().type == '*' || peek().type == '/') {
				const auto &t = eat();
				astp rhs = parse_prefix_suffix();
				lhs = std::make_unique<ast_bin>(
					t.where, t.type, std::move(lhs), std::move(rhs)
				);
			}
			return lhs;
		}

		astp parse_sum() {
			astp lhs = parse_product();
			while(peek().type == '+' || peek().type == '-') {
				const auto &t = eat();
				astp rhs = parse_product();
				lhs = std::make_unique<ast_bin>(
					t.where, t.type, std::move(lhs), std::move(rhs)
				);
			}
			return lhs;
		}

		astp parse_expr() {
			astp r = parse_sum();
			if(peek().type == '=') {
				const auto &t = eat();
				return std::make_unique<ast_set>(
					t.where, std::move(r), parse_expr()
				);
			}
			return r;
		}

		std::unique_ptr<ast_let> parse_let() {
			const auto &t = expect_kw("let");
			const auto &name = expect(tt_id, "for variable name");
			const auto &type = expect(tt_id, "for variable type");
			std::optional<astp> val;
			if(peek().type == '=') {
				eat();
				val = parse_expr();
			}
			return std::make_unique<ast_let>(
				t.where, name.value,
				typename_to_value_type(type.value, type.where),
				std::move(val)
			);
		}

		astp parse_stmt() {
			if(is_kw("let")) {
				astp r = parse_let();
				expect(';');
				return r;
			}
			if(is_kw("return")) {
				const auto &t = eat();
				std::optional<astp> value;
				if(peek().type != ';')
					value = parse_expr();
				expect(';');
				return std::make_unique<ast_return>(t.where, std::move(value));
			}
			astp r = parse_expr();
			expect(';');
			return r;
		}

		std::unique_ptr<ast_block> parse_block() {
			const auto &t = expect('{');
			std::vector<astp> stmts;
			while(peek().type != '}' && peek().type != tt_eof) {
				stmts.push_back(parse_stmt());
			}
			expect('}');
			return std::make_unique<ast_block>(t.where, std::move(stmts));
		}

		void parse_func(shader_module_function &func) {
			expect_kw("func");

			func.name = expect(tt_id).value;

			expect_dont_eat('(');
			if(peek(2).type != ')') {
				do {
					eat();
					func.params.push_back({});
					shader_module_parameter &param = func.params.back();
					param.name = expect(tt_id, "for parameter name").value;
					const auto &type = expect(tt_id, "for parameter type");
					param.type = typename_to_value_type(type.value, type.where);
					param.param_type = shader_module_parameter_type::input;
				} while(peek().type == ',');
			} else eat();
			expect(')');
			const auto &type = expect(tt_id, "for function return type");
			func.return_type = typename_to_value_type(type.value, type.where);
			func.code = new shader_module_source_code(parse_block());
		}

		void parse_toplevel(shader_module &mod) {
			if(is_kw("in") || is_kw("out") || is_kw("uniform")) {
				mod.fields.push_back({ });
				parse_field(mod.fields.back());
				expect(';');
				return;
			}
			if(is_kw("func")) {
				mod.functions.push_back({ .code = nullptr });
				parse_func(mod.functions.back());
				return;
			}
		}

		void parse_module(shader_module &mod) {
			expect_kw("module");
			mod.name = expect(tt_id, "module name").value;
			expect(';');
			expect_kw("module");
			expect_kw("type");
			parse_module_type(mod.type);
			expect(';');
			while(peek().type != tt_eof) {
				parse_toplevel(mod);
			}
		}
	};

	// void visit(struct ast_name &n);
	// void visit(struct ast_int &n);
	// void visit(struct ast_float &n);
	// void visit(struct ast_return &n);
	// void visit(struct ast_block &n);
	// void visit(struct ast_call &n);
	// void visit(struct ast_bin &n);
	// void visit(struct ast_unr &n);
	// void visit(struct ast_let &n);
	// void visit(struct ast_set &n);

	void codegen::visit(struct ast_name &n) {
		gen("{}", n.name);
	}

	void codegen::visit(struct ast_int &n) {
		gen("{}", n.value);
	}

	void codegen::visit(struct ast_float &n) {
		gen("{}", n.value);
	}

	void codegen::visit(struct ast_return &n) {
		if(n.val.has_value()) {
			gen("return ");
			n.val->get()->accept(*this);
		} else {
			gen("return");
		}
	}

	void codegen::visit(struct ast_block &n) {
		gen("{{\n"); ++indent;
		for(const auto &stmt : n.stmts) {
			stmt->accept(*this);
			gen(";\n");
		}
		--indent; gen("}}\n");
	}

	void codegen::visit(struct ast_call &n) {
		gen("(");
		n.func->accept(*this);
		gen(")(");
		if(!n.params.empty()) {
			n.params[0]->accept(*this);
			for(size_t i = 1; i < n.params.size(); ++i) {
				gen(", ");
				n.params[i]->accept(*this);
			}
		}
		gen(")");
	}

	void codegen::visit(struct ast_bin &n) {
		gen("("); n.lhs->accept(*this); gen(")");
		gen(" {} ", n.op);
		gen("("); n.rhs->accept(*this); gen(")");
	}

	void codegen::visit(struct ast_unr &n) {
		gen("{}", n.op);
		gen("("); n.val->accept(*this); gen(")");
	}

	void codegen::visit(struct ast_let &n) {
		gen("{} {}", value_type_to_typename(n.type), n.name);
		if(n.val.has_value()) {
			gen(" = "); n.val->get()->accept(*this);
		}
	}

	void codegen::visit(struct ast_set &n) {
		n.var->accept(*this);
		gen(" = ");
		n.val->accept(*this);
	}

	void codegen::visit(struct ast_access &n) {
		n.obj->accept(*this);
		gen(".{}", n.field);
	}

	void codegen::generate_module(const shader_module &mod) {
		gen("#pragma module_name {}\n", mod.name);
		gen("#pragma shader_type ");
		switch(mod.type) {
		case shader_module_type::entry_vertex: gen("entry_vertex"); break;
		case shader_module_type::partial_vertex: gen("partial_vertex"); break;
		case shader_module_type::entry_fragment: gen("entry_fragment"); break;
		case shader_module_type::partial_fragment: gen("partial_fragment"); break;
		}
		gen("\n\n");
		for(const auto &field : mod.fields) {
			generate_field(field);
		}
		for(const auto &func : mod.functions) {
			generate_function(func);
		}
	}

	void codegen::generate_field(const shader_module_field &field) {
		if(holds_alternative<int>(field.tag)) {
			gen("layout (location = {}) ", std::get<int>(field.tag));
		}

		switch(field.field_type) {
		case shader_module_field_type::input: gen("in"); break;
		case shader_module_field_type::output: gen("out"); break;
		case shader_module_field_type::uniform: gen("uniform"); break;
		}
		gen(" {} {};\n", value_type_to_typename(field.type), field.name);
	}
	
	void codegen::generate_function(const shader_module_function &func) {
		gen("{} {}(", value_type_to_typename(func.return_type), func.name);
		if(!func.params.empty()) {
			const auto generate_param = [&](const shader_module_parameter &param) {
				switch(param.param_type) {
				case shader_module_parameter_type::input: gen("in "); break;
				case shader_module_parameter_type::output: gen("out "); break;
				case shader_module_parameter_type::input_output: gen("inout "); break;
				}
				gen("{} {}", value_type_to_typename(param.type), param.name);
			};
			
			generate_param(func.params[0]);
			for(size_t i = 1; i < func.params.size(); ++i) {
				gen(", ");
				generate_param(func.params[i]);
			}
		}
		gen(") ");
		func.code->ast->accept(*this);
	}

	// bad code:
	// i really need to do something about this
	// expose ast types? nah
	shader_module_function::~shader_module_function() {
		if(code) delete code; // clunky :c
	}
	
	shader_module compile_module(const stdfs::path &path) {
		shader_module mod;
		
		auto code = util::fs::read_file(path);
		code.push_back('\0');

		std::vector<token> toks;
		tokenize(strv { code.begin(), code.end() }, toks);

		code.clear(); // hopefully free memory

		// for(const auto &tok : toks) print_token(stderr, tok, path.c_str());

		parser par { 0, { toks.begin(), toks.end() } };
		try {
			par.parse_module(mod);
		} catch(const parser_exception &e) {
			fmt::print(stderr, "{}:{}:{}: {}\n",
				path.c_str(), e.where.line + 1, e.where.col + 1, e.msg);
			throw std::runtime_error("could not compile shader module");
		}

		return mod;
	}

	bool alpha_converter::visit(struct ast_name &n) {
		if(n.name == old_name) n.name = new_name;
		return false;
	}

	bool alpha_converter::visit(struct ast_int &n) { return false; }
	bool alpha_converter::visit(struct ast_float &n) { return false; }
	bool alpha_converter::visit(struct ast_access &n) { return false; }

	bool alpha_converter::visit(struct ast_return &n) {
		if(n.val.has_value()) n.val->get()->accept(*this);
		return false;
	}

	bool alpha_converter::visit(struct ast_block &n) {
		for(auto &s : n.stmts) if(s->accept(*this)) break;
		return false;
	}

	bool alpha_converter::visit(struct ast_call &n) {
		n.func->accept(*this);
		for(auto &param : n.params) param->accept(*this);
		return false;
	}

	bool alpha_converter::visit(struct ast_bin &n) {
		n.lhs->accept(*this);
		n.rhs->accept(*this);
		return false;
	}

	bool alpha_converter::visit(struct ast_unr &n) {
		n.val->accept(*this);
		return false;
	}

	bool alpha_converter::visit(struct ast_let &n) {
		if(n.val.has_value()) n.val->get()->accept(*this);
		if(n.name == new_name) return true;
		return false;
	}

	bool alpha_converter::visit(struct ast_set &n) {
		n.var->accept(*this);
		n.val->accept(*this);
		return false;
	}

	void alpha_converter::visit_module(shader_module &mod) {
		for(auto &func : mod.functions) {
			func.code->ast->accept(*this);
		}
	}

	constexpr shader_type shader_module_type_to_shader_type(shader_module_type type) {
		switch(type) {
		case shader_module_type::entry_fragment: return shader_type::fragment;
		case shader_module_type::partial_fragment: return shader_type::fragment;
		case shader_module_type::entry_vertex: return shader_type::vertex;
		case shader_module_type::partial_vertex: return shader_type::vertex;
		}
	}

	constexpr bool is_shader_module_type_entry(shader_module_type type) {
		return type == shader_module_type::entry_fragment
		    || type == shader_module_type::entry_vertex;
	}

	void link_modules(
		std::ostream &fout,
		shader_type type,
		std::span<shader_module> modules,
		strv shader_main
	) {
		std::unordered_map<std::string, size_t> fields_by_tag;
		std::vector<shader_module_field> unique_fields;
		std::vector<std::reference_wrapper<shader_module_function>> functions;
		std::string entry_point_name;
		std::vector<std::string> partial_entry_names;

		for(auto &mod : modules) {
			if(shader_module_type_to_shader_type(mod.type) != type) {
				throw std::runtime_error("incompatible shader module '" + mod.name + "' type");
			}

			// mangle function names
			bool had_entry = false;
			for(auto &func : mod.functions) {
				bool is_entry = func.name == "entry";
				func.name = "_" + mod.name + "__" + func.name;
				functions.emplace_back(func);

				if(is_entry) {
					if(had_entry) {
						throw std::runtime_error("multiple entry points");
					}

					had_entry = true;

					if(is_shader_module_type_entry(mod.type)) {
						entry_point_name = func.name;
					} else {
						partial_entry_names.push_back(func.name);
					}
				}
			}

			if(!had_entry && is_shader_module_type_entry(mod.type)) {
				throw std::runtime_error("no entry point for non-partial shader module");
			}

			// eliminate fields with the same tag.
			for(auto &field : mod.fields) {
				std::string old_name = field.name;
				if(std::holds_alternative<std::string>(field.tag)) {
					if(!fields_by_tag.contains(std::get<std::string>(field.tag))) {
						fields_by_tag[std::get<std::string>(field.tag)] = unique_fields.size();
						field.name = "_" + std::get<std::string>(field.tag);
						unique_fields.push_back(field);
					}
				} else {
					field.name = "_" + mod.name + "__" + field.name; // mangle name
					unique_fields.push_back(field);
				}
				alpha_converter ac{old_name, field.name};
				ac.visit_module(mod); // rename fields.
			}
		}

		if(entry_point_name.empty()) { // shouldn't happen, but there for safety.
			throw std::runtime_error("no entry point");
		}

		codegen cg{fout};

		for(const auto &field : unique_fields)
			cg.generate_field(field);

		fmt::print(fout, "\nvec4 partial(vec4 i);\n");

		for(const auto &func : functions)
			cg.generate_function(func);
		
		// TODO: types...
		fmt::print(fout, "\nvec4 partial(vec4 i) {{\n");
		for(auto &name : partial_entry_names)
			fmt::print(fout, "  i = {}(i);\n", name);
		fmt::print(fout, "}}\n");
		fmt::print(fout, "void {}() {{ {}(); }}\n", shader_main, entry_point_name);
	}
}
