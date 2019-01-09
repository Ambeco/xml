#include <algorithm>
#include <limits>
#include "xml_reader.hpp"

namespace mpd {
	namespace xml {
		namespace impl {
			static const std::size_t MAX_LOOKAHEAD = 100;
			static const std::size_t BUFFER_SIZE = 1042;

			const int html_escape_code_hash_size = 7;
			std::pair<int, int> html_escape_codes[html_escape_code_hash_size] = {
				{0,0},
				{2249262,8364},//euro
				{65935,'&'},//amp
				{0,0},
				{2419,'<'},//lt
				{2259,'>'},//gt
				{2642387,'"'},//quot
			};
			int deescape(const char* escape, std::size_t& char_count) { //points at character after &
				char_count = 0;
				if (escape[0] == '#') {
					++escape;
					int base = 10;
					if (escape[1] == 'x') {
						++escape;
						base = 16;
					}
					int value = 0;
					while (*escape != ';') {
						if (*escape >= '0' && *escape <= '9')
							value = value * base + (*escape - '0');
						else if (*escape >= 'a' && *escape <= 'f')
							value = value * 16 + (*escape - 'a' + 10);
						else if (*escape >= 'A' && *escape <= 'F')
							value = value * 16 + (*escape - 'A' + 10);
						else
							return -1;
						++char_count;
						++escape;
					}
					return value;
				} else {
					int hash = (*escape >= 'A' && *escape <= 'Z') ? 1 : 2;
					while (*escape != ';') {
						hash *= 32;
						if (*escape >= 'a' && *escape <= 'z')
							hash += *escape - 'a';
						else if (*escape >= 'A' && *escape <= 'Z')
							hash += *escape - 'A';
						else if (*escape >= '0' && *escape <= '8')
							hash += *escape - '0' + 26;
						else
							return -1;
						++char_count;
						++escape;
					}
					int pos = hash % html_escape_code_hash_size;
					if (html_escape_codes[pos].first == hash)
						return html_escape_codes[pos].second;
					else
						return -1;
				}
			}
			bool validate_escape_codes() {
#ifdef _DEBUG
				std::size_t cnt;
				assert(deescape("quot;", cnt) == '"');
				assert(deescape("amp;", cnt) == '&');
				assert(deescape("lt;", cnt) == '<');
				assert(deescape("gt;", cnt) == '>');
				assert(deescape("euro;", cnt) == 8364);
#endif
				return true;
			}
			static const bool html_escape_codes_validation = validate_escape_codes();

			void append_utf8(int cp, std::string& str) {
				char buffer[5] = {};
				if (cp <= 0x7F) {
					buffer[0] = (char)cp;
				} else if (cp < 0x7FF) {
					buffer[0] = 0xC0 | (char)(cp >> 6);
					buffer[1] = 0x80 | (char)(cp & 0x3F);

				} else if (cp < 0x7FFF) {
					buffer[0] = 0xC0 | (char)(cp >> 12);
					buffer[1] = 0x80 | (char)((cp >> 6) & 0x3F);
					buffer[2] = 0x80 | (char)(cp & 0x3F);
				} else {
					buffer[0] = 0xC0 | (char)(cp >> 18);
					buffer[1] = 0x80 | (char)((cp >> 12) & 0x3F);
					buffer[2] = 0x80 | (char)((cp >> 6) & 0x3F);
					buffer[3] = 0x80 | (char)(cp & 0x3F);
				}
				str.append(buffer);
			}

			bool in_range(char c, char min, char max) 
			{ return c>=min && c<=max; }
			bool is_whitespace(char c) 
			{ return c == ' ' || c == '\n' || c == '\t' || c=='\r'; }
			bool is_name_start_char(char c) 
			{ return c == ':' || in_range(c, 'A', 'Z') || c == '_' || in_range(c, 'a', 'z') || c<0 || c>127; }
			bool is_name_char(char c) 
			{ return is_name_start_char(c) || c=='-' || c=='.' || in_range(c,'0','9'); }


			std::string reader::get_parse_state_name() {
				switch (position.state) {
				case parse_state::document_begin: return "document start";
				case parse_state::after_tag_name: return position.tag_name + " open tag";
				case parse_state::after_attribute: return attribute.first + " attribute with value " + attribute.second.substr(0, 25);
				case parse_state::before_tag_finish: return position.tag_name + " content begin";
				case parse_state::after_open_tag: return position.tag_name + " details";
				case parse_state::document_end: return "document end";
				case parse_state::after_node: return get_node_type_string(node.first, position.tag_name, node.second.substr(0, 25));
				default:
					assert(false);
					return "UNKNOWN STATE[" + std::to_string((int)position.state) +"]";
				}
			}

			std::string reader::get_node_type_string(node_type type, const std::string& name, const std::string& content) {
				switch (type) {
				case node_type::cdata_node: return "cdata" + content;
				case node_type::comment_node: return "comment " + content;
				case node_type::element_node: return name + " element";
				case node_type::attribute_node: return name + "attribute ";
				case node_type::processing_node: return "processing node " + content;
				case node_type::string_node: return "string " + content;
				case node_type::whitespace_node: return "whitespace";
				default:
					assert(false);
					return "UNKNOWN NODE[" + std::to_string((int)node.first) + "]";
				}
			}

			std::string reader::get_location_for_exception() {
				return source_name_ + '(' + std::to_string(position.line) + ',' + std::to_string(position.column) + ")";
			}

			void reader::throw_unexpected(const char* details) {
				throw unexpected_node(get_location_for_exception() + ": ERROR: " + (details != nullptr ? details : "unexpected_node"));
			}
			void reader::throw_missing(node_type type, const char* name, const char* details) {
				std::string msg = get_node_type_string(type, name, "");
				if (details != nullptr) {
					msg += ": ";
					msg += details;
				}
				throw missing_node(get_location_for_exception() + ": ERROR: " + msg.c_str());
			}
			void reader::throw_invalid_content(const char* details) {
				throw invalid_content(get_location_for_exception() + ": ERROR: " + (details != nullptr ? details : "invalid_content"));
			}
			void reader::throw_invalid_xml(const char* details) {
				throw invalid_xml(get_location_for_exception() + ": ERROR: " + (details != nullptr ? details : "invalid_xml"));
			}
			void reader::throw_invalid_read_call(const char* details) {
				throw invalid_read_call_error(get_location_for_exception() + ": ERROR: " + (details != nullptr ? details : "invalid_read_call_error"));
			}
			void reader::throw_invalid_parser(const char* details) {
				throw invalid_parser_error(get_location_for_exception() + ": ERROR: " + (details != nullptr ? details : "invalid_parser_error"));
			}
			void reader::throw_malformed_xml(const char* details) {
				throw malformed_xml(get_location_for_exception() + ": ERROR: " + (details != nullptr ? details : "malformed_xml"));
			}
			void reader::throw_unexpeced_eof(const char* details) {
				throw unexpeced_eof(get_location_for_exception() + ": ERROR: " + (details != nullptr ? details : "unexpeced_eof"));
			}
			bool reader::next_attribute() {
				assert(position.state == parse_state::after_tag_name || position.state == parse_state::after_attribute);
				skip_ws();
				char peekc = peek();
				if (peekc == '/' || peekc == '>') {
					position.state = parse_state::before_tag_finish;
					return false;
				}
				read_name(attribute.first);
				skip_ws();
				affirm_next_char('=', 0, "missing = after attribute_name");
				skip_ws();
				char quote = affirm_next_char('\"', '\'', "attribute value must be wrapped in quotes");
				read_attr(quote);
				position.state = parse_state::after_attribute;
				return true;
			}
			bool reader::next_node() {
				if (at_eof()) return false;
				if (position.state == parse_state::before_tag_finish) {
					char c = peek();
					if (c == '/') {
						consume_nonws();
						affirm_next_char('>', 0, "> must immediately follow /");
						position.state = parse_state::after_node;
						return false;
					} else {
						affirm_next_char('>', 0, "> must close a tag");
						position.state = parse_state::after_open_tag;
					}
				}
				assert(position.state == parse_state::document_begin 
					|| position.state == parse_state::after_node
					|| position.state == parse_state::after_open_tag);
				char c = peek();
				if (is_whitespace(c)) {
					read_ws();
					position.state = parse_state::after_node;
				} else if (c != '<') {
					read_string();
					position.state = parse_state::after_node;
				} else {
					consume_maybe_ws();
					c = peek();
					if (is_name_start_char(c)) return read_tag_name();
					else if (c == '/') return read_close_tag();
					else if (c == '?') read_processing_instruction(); //   <?xml version="1.0"?>
					else if (peek("!--")) read_comment();
					else if (peek("![CDATA[")) read_cdata();
					else if (peek("!ATTLIST ")) read_attribute_list();
					else if (peek("!DOCTYPE ")) read_doctype();
					else if (peek("!ELEMENT ")) read_element_type();
					else if (peek("!NOTATION ")) read_notation();
					else if (peek("!% ")) read_conditional();
					else throw_malformed_xml(std::string("invalid tag start: ") + c);
					position.state = parse_state::after_node;
				}
				return true;
			};
			char reader::affirm_next_char(char c1, char c2, const char* message) {
				char c = peek();
				if (c != c1 && c != c2) throw_malformed_xml(message);
				return consume_nonws();
			}
			void reader::read_ws() {
				node.first = node_type::whitespace_node;
				node.second.clear();
				do {
					//todo optimize out per-char-append
					while(buffer_idx<buffer.size()) {
						if (!is_whitespace(buffer[buffer_idx]))
							return;
						else if (buffer[buffer_idx] == '\0') {
							consume_nonws();
							if (!at_eof()) throw_malformed_xml("cannot have null byte in xml");
							return;
						}
						node.second.append(1, consume_maybe_ws());
					}
				} while (!at_eof());
			};
			void reader::skip_ws() {
				do {
					while(buffer_idx<buffer.size()) {
						if (!is_whitespace(buffer[buffer_idx]))
							return;
						else if (buffer[buffer_idx] == '\0') {
							consume_nonws();
							if (!at_eof()) throw_malformed_xml("cannot have null byte in xml");
							return;
						}
						consume_maybe_ws();
					}
				} while (!at_eof());
			};
			void reader::read_name(std::string& out) {
				out.clear();
				char first = peek();
				if (!is_name_start_char(first)) throw_malformed_xml(first + std::string(" is not a valid char for starting a name"));
				out.append(1, consume_nonws());
				do {
					while(buffer_idx<buffer.size()) {
						if (!is_name_char(buffer[buffer_idx]))
							return;
						out.append(1, consume_nonws());
					}
				} while (!at_eof());
				throw_unexpeced_eof("while parsing name " + out.substr(0, 20));
			};
			void reader::read_attr(char quote) {
				attribute.second.clear();
				do {
					while(buffer_idx<buffer.size()) {
						char c = buffer[buffer_idx];
						if (c == quote) {
							consume_nonws();
							return;
						} else if (c == '&') {
							consume_escape(attribute.second);
						} else if (c == '<') {
							throw_invalid_content("attribute cannot contain <");
						} else 
							attribute.second.append(1, consume_maybe_ws());
					}
				} while (!at_eof());
				throw_unexpeced_eof("while parsing attribute " + attribute.first);
			};
			void reader::read_string() {
				assert(buffer[buffer_idx] != '<' && !is_whitespace(buffer[buffer_idx]));
				node.first = node_type::string_node;
				node.second.clear();
				do {
					while(buffer_idx<buffer.size()) {
						if (buffer[buffer_idx] == '&') {
							consume_nonws();
							consume_escape(node.second);
						}  else if (buffer[buffer_idx] == '<')
							return;
						else if (buffer[buffer_idx] == '\0') {
							consume_nonws();
							if (!at_eof()) throw_malformed_xml("cannot have null byte in xml");
							return;
						} else if (!is_whitespace(buffer[buffer_idx])) 
							node.second.append(1, consume_maybe_ws());
						else { //is whitespace. Is it trailing whitespace?
							std::size_t next_non_ws = peek_next_nonws_idx();
							if (buffer[next_non_ws]=='<' || is_whitespace(buffer[next_non_ws]))
								return;
							read_ws_until(next_non_ws); // it's not. Add it all and keep going
						}
					}
				} while (!at_eof());
				return;
			}
			bool reader::read_tag_name() {
				node.first = node_type::element_node;
				read_name(node.second);
				position.state = parse_state::after_tag_name;
				position.tag_name = node.second;
				return true;
			}
			bool reader::read_close_tag() {
				affirm_next_char('/', 0, "close tag must begin with /");
				node.first = node_type::element_node;
				read_name(node.second);
				affirm_next_char('>', 0, "close tag must begin with /");
				return false;
			}
			void reader::read_comment() {
				assert(peek("!--"));
				consume_nonws(3);
				node.first = node_type::comment_node;
				node.second.clear();
				do {
					while (buffer_idx < buffer.size()) {
						if (peek("-->")) {
							if (node.second[node.second.length()-1] == '-') throw_invalid_content("comment cannot contain --->");
							consume_nonws(3);
							return;
						}
						node.second.append(1, consume_maybe_ws());
					}
				} while (!at_eof());
				throw_unexpeced_eof(); //TODO pass descriptions like above
			}
			void reader::read_cdata() {
				assert(peek("![CDATA["));
				consume_nonws(8);
				node.first = node_type::string_node;
				node.second.clear();
				do {
					while (buffer_idx < buffer.size()) {
						if (peek("]]>")) {
							consume_nonws(3);
							return;
						}
						node.second.append(1, consume_maybe_ws());
					}
				} while (!at_eof());
				throw_unexpeced_eof();
			}
			void reader::read_conditional() {
				//TODO IMPLEMENT
				throw_unexpected("Unimplemented read_conditional");
			}
			void reader::read_attribute_list() {
				//TODO IMPLEMENT
				throw_unexpected("Unimplemented read_attribute_list");
			}
			void reader::read_doctype() {
				//TODO IMPLEMENT
				throw_unexpected("Unimplemented read_doctype");
			}
			void reader::read_element_type() {
				//TODO IMPLEMENT
				throw_unexpected("Unimplemented read_element_type");
			}
			void reader::read_notation() {
				//TODO IMPLEMENT
				throw_unexpected("Unimplemented read_notation");
			}
			void reader::read_processing_instruction() {
				assert(peek("?"));
				consume_nonws();
				node.first = node_type::processing_node;
				node.second.clear();
				do {
					while (buffer_idx < buffer.size()) {
						if (peek("?>")) {
							consume_nonws(2);
							return;
						}
						node.second.append(1, consume_maybe_ws());
					}
				} while (!at_eof());
				throw_unexpeced_eof("unexpected eof in processing instruction " + node.second.substr(0, 20));
			}
			void reader::read_ws_until(std::size_t to_idx) {
				assert(to_idx >= 0 && to_idx < buffer.size());
				node.first = node_type::whitespace_node;
				node.second.append(buffer.begin() + buffer_idx, buffer.begin() + to_idx);
				std::size_t newline_count = 0;
				std::size_t linefeed_count = 0;
				std::size_t lastnewline = std::numeric_limits<std::size_t>::max();
				for (std::size_t i = buffer_idx; i < to_idx; i++) {
					if (buffer[i] == '\n') {
						newline_count++;
						lastnewline = i;
						linefeed_count = 0;
					}
					else if (buffer[i] == '\r')
						linefeed_count++;
				}
				if (lastnewline == std::numeric_limits<std::size_t>::max()) {
					position.column += (to_idx - buffer_idx - linefeed_count);
				} else {
					position.line += newline_count;
					position.column = (to_idx - lastnewline - linefeed_count);
				}
				buffer_idx = to_idx;
			}
			__forceinline char reader::consume_nonws() {
				char c = buffer[buffer_idx];
				++position.column;
				++buffer_idx;
				return c;
			}
			__forceinline void reader::consume_nonws(int count) {
				position.column += count;
				buffer_idx += count;
			}
			char reader::consume_maybe_ws() {
				char c = buffer[buffer_idx];
				if (buffer[buffer_idx] == '\n') {
					position.line++;
					position.column = 0;
				} else if (buffer[buffer_idx] == '\r') {
					//do not advance file position
				} else 
					position.column++;
				++buffer_idx;
				return c;
			}
			void reader::consume_escape(std::string& out) {
				assert(buffer[buffer_idx] == '&');
				if (buffer_idx + 12 > buffer.size()) read_buffer(buffer_idx - 1);
				const char* deescape_ptr = buffer.data() + buffer_idx;
				std::size_t in_bytes = 0;
				int code_point = deescape(deescape_ptr, in_bytes);
				if (code_point == '&') { // rarely can be nested, but only once
					const char* deescape_ptr2 = buffer.data() + buffer_idx + in_bytes;
					std::size_t in_bytes2 = 0;
					code_point = deescape(deescape_ptr2, in_bytes2);
					in_bytes += in_bytes;
				}
				if (code_point == -1) {
					throw_invalid_xml(std::string(deescape_ptr, in_bytes) + " is not a recognized escape sequence");
				}
				append_utf8(code_point, out);
				position.column += in_bytes;
				buffer_idx += in_bytes;
			}
			std::size_t reader::peek_next_nonws_idx() {
				assert(is_whitespace(buffer[buffer_idx]));
				for (std::size_t i = buffer_idx + 1; i < buffer.size(); i++) {
					if (!is_whitespace(buffer[i])) return i;
				}
				// if didn't read at least MAX_LOOKAHEAD, then reload the buffer, and repeat
				if (buffer.size() - buffer_idx >= MAX_LOOKAHEAD) return buffer_idx;
				read_buffer(buffer_idx);
				for (std::size_t i = buffer_idx + 1; i < buffer.size(); i++) {
					if (!is_whitespace(buffer[i])) return i;
				}
				return buffer_idx;
			}
			__forceinline char reader::peek() {
				if (buffer_idx >= buffer.size()) {
					read_buffer(buffer_idx);
					if (buffer_idx >= buffer.size())
						throw_unexpeced_eof();
				}
				return buffer[buffer_idx];
			}
			__forceinline char reader::peek(int offset) {
				assert(buffer_idx + offset < BUFFER_SIZE);
				if (buffer_idx+ offset >= buffer.size()) {
					read_buffer(buffer_idx);
					if (buffer_idx + offset >= buffer.size())
						throw_unexpeced_eof();
				}
				return buffer[buffer_idx+ offset];
			}
			__forceinline bool reader::peek(const char* str, int len) {
				assert(buffer_idx + len < BUFFER_SIZE);
				if (buffer_idx + len >= buffer.size()) {
					read_buffer(buffer_idx);
					if (buffer_idx + len >= buffer.size())
						return false;
				}
				return strncmp(buffer.data() + buffer_idx, str, len) == 0;
			}
			bool reader::at_eof() {
				if (buffer_idx >= buffer.size()) {
					read_buffer(buffer_idx);
					return buffer.empty();
				}
				return false;
			}
			void reader::read_buffer(std::size_t keep_after_idx) {
				assert(keep_after_idx <= buffer.size());
				assert(keep_after_idx > 0 || 0==buffer.size());
				std::size_t keep_cnt = buffer.size() - keep_after_idx;
				if (keep_cnt > 0)
					std::move(buffer.begin() + keep_after_idx, buffer.end(), buffer.begin());
				buffer.resize(BUFFER_SIZE);
				std::size_t desired_read_cnt = BUFFER_SIZE - 1 - keep_cnt;
				std::size_t add_cnt = position.read_buf->read(buffer.data() + keep_after_idx, desired_read_cnt);
				buffer.resize(keep_cnt + add_cnt);
				buffer_idx = 0;
			}
		}
	}
}