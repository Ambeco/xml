#pragma once
#define _CRT_NONSTDC_NO_DEPRECATE
#include <cassert>
#include <stdexcept>
#include <string>
#include <iterator>
#include <climits>
#include <cstring>
#include <type_traits>
#include <functional>
#include <utility>
#include "type_erased.hpp"

#ifdef _MSC_VER 
#define noinline(RETURN) __declspec(noinline) RETURN
#else
#define noinline(RETURN) RETURN __attribute__ ((noinline))
#endif

namespace mpd {
	namespace xml {
		struct document_reader;
		namespace impl {
			struct general_ {};
			struct special_ : general_ {};
			template<typename T> struct identity { typedef T type; };

			struct read_buf_t {
				virtual read_buf_t* copy_construct_at(char* buffer, std::size_t buffer_size)const& = 0;
				virtual read_buf_t* move_construct_at(char* buffer, std::size_t buffer_size) & = 0;
				virtual ~read_buf_t() {};
				virtual int read(char* buffer, int count) = 0;
			};

			class reader : public tag_reader, attribute_reader, element_reader {
				enum class parse_state { document_begin, after_tag_name, after_attribute, before_tag_finish, after_open_tag, after_node, document_end };
				struct parse_pos {
					type_erased<read_buf_t> read_buf;
					std::size_t line = 0;
					std::size_t column = 0;
					parse_state state = parse_state::document_begin;
					std::string tag_name;

					template<class read_buff_t, class...Us>
					parse_pos(std::in_place_type_t<read_buff_t> name, Us&&...us) :read_buf(name, std::forward<Us>(us)...) {}
				} position;
				std::string source_name_;
				std::pair<std::string, std::string> attribute;
				std::pair<node_type, std::string> node;
				std::string buffer;
				std::size_t buffer_idx = 0;
				std::size_t escape_end_idx = 0;
			public:
				//Get the current Location
				std::string get_location_for_exception();
				//You can call this to throw a unexpected_node with the current line number and offset and such.
				[[noreturn]] void throw_unexpected(const char* details = nullptr);
				[[noreturn]] void throw_unexpected(const std::string& details) { throw_unexpected(details.c_str()); }
				//You can call this to throw a missing_node with the current line number and offset and such.
				[[noreturn]] void throw_missing(node_type type, const char* name, const char* details = nullptr);
				[[noreturn]] void throw_missing(node_type type, const char* name, const std::string& details) { throw_missing(type, name, details.c_str()); }
				//You can call this to throw a invalid_content with the current line number and offset and such.
				[[noreturn]] void throw_invalid_content(const char* details = nullptr);
				[[noreturn]] void throw_invalid_content(const std::string& details) { throw_invalid_content(details.c_str()); }
			private:
				struct post_condition {
					reader& reader_;
					parse_state desired_;
					const char* message_;
					post_condition(reader* reader, parse_state desired, const char* message)
						:reader_(*reader), desired_(desired), message_(message) {}
					~post_condition() {
						if (!std::uncaught_exceptions() && reader_.position.state != desired_)
							reader_.throw_invalid_parser(message_);
					}
				};
			public:
				template<class tag_parser_t>//, typename identity<decltype(attribute_parser_t::begin_element)>::type = 0>
				auto read_element(tag_parser_t& parser, special_) -> decltype(parser.begin_element(*this, position.tag_name))
				{
					post_condition condition(this, parse_state::after_node, "parser.begin_element must call reader.read_attributes");
					return parser.begin_element(*this, position.tag_name); 
				}
				template<class attribute_parser_t>
				auto read_element(attribute_parser_t&& parser, general_)
				{return read_attributes(std::forward<attribute_parser_t>(parser)); }
				template<class attribute_parser_t> auto read_attributes(attribute_parser_t&& parser) {
					if (position.state != parse_state::after_tag_name) throw_invalid_read_call("called read_attributes, but not at the beginning of a tag");
					parse_pos saved_pos(position);
					try {
						while (next_attribute())
							read_attribute(parser, special_{});
						return read_contents(start_element(std::forward<attribute_parser_t>(parser), special_{}));
					}
					catch (const std::exception&) {
						position = std::move(saved_pos);
						buffer.clear();
						buffer_idx = 0;
						throw;
					}
				}
			protected:
				template<class element_parser_t> auto read_contents(element_parser_t&& parser) {
					if (position.state != parse_state::document_begin
						&& position.state != parse_state::before_tag_finish
						&& position.state != parse_state::after_node)
						throw_invalid_read_call("called readDocument from invalid call location");
					parse_pos saved_pos(position);
					try {
						while (next_node()) {
							post_condition condition(this, parse_state::after_node, "parser.read_node must call reader.read_element when given a node_type::element_node");
							parser.read_node(*this, node.first, std::move(node.second));
						}
						return std::forward<element_parser_t>(parser).end_element(*this);
					}
					catch (const std::exception&) {
						position = std::move(saved_pos);
						buffer.clear();
						buffer_idx = 0;
						throw;
					}
				}

				template<class attribute_parser_t>//, typename identity<decltype(attribute_parser_t::read_attribute)>::type = 0>
				auto read_attribute(attribute_parser_t& parser, special_) -> decltype(parser.read_attribute(*this, std::move(attribute.first), std::move(attribute.second)))
				{
					post_condition condition(this, parse_state::after_attribute, "parser.read_attribute somehow did something invalid"); 
					return parser.read_attribute(*this, std::move(attribute.first), std::move(attribute.second));
				}
				template<class attribute_parser_t>
				void read_attribute(attribute_parser_t&, general_)
				{ throw_unexpected("unexpected attribute " + attribute.first); }

				template<class element_parser_t>//, typename identity<decltype(element_parser_t::start_element)>::type = 0>
				auto start_element(element_parser_t&& parser, special_) -> decltype(std::move(parser).start_element(*this))
				{ return std::move(parser).start_element(*this); }
				template<class element_parser_t>
				element_parser_t&& start_element(element_parser_t&& parser, general_)
				{ return std::forward<element_parser_t>(parser); }

				//TODO: Handle attribute namespaces.
				//TODO: Handle the xml namespace.
				friend document_reader;
				template<class read_buff_t, class...Us>
				reader(std::string&& source_name, std::in_place_type_t<read_buff_t> name, Us&&...us)
					:tag_reader(*this)
					, attribute_reader(*this)
					, element_reader(*this)
					, position(name, std::forward<Us>(us)...)
					, source_name_(std::move(source_name))
				{ }
				std::string get_parse_state_name();
				std::string get_node_type_string(node_type type, const std::string& name, const std::string& content);
				void throw_invalid_xml(const char* details = nullptr);
				void throw_invalid_xml(const std::string& details) { throw_invalid_xml(details.c_str()); }
				void throw_invalid_read_call(const char* details = nullptr);
				void throw_invalid_read_call(const std::string& details) { throw_invalid_read_call(details.c_str()); }
				void throw_invalid_parser(const char* details = nullptr);
				void throw_invalid_parser(const std::string& details) { throw_invalid_parser(details.c_str()); }
				void throw_malformed_xml(const char* details = nullptr);
				void throw_malformed_xml(const std::string& details) { throw_malformed_xml(details.c_str()); }
				void throw_unexpeced_eof(const char* details = nullptr);
				void throw_unexpeced_eof(const std::string& details) { throw_unexpeced_eof(details.c_str()); }
				noinline(bool) next_attribute();
				noinline(bool) next_node();
				char affirm_next_char(char c1, char c2, const char* message);
				void read_ws();
				void skip_ws();
				void read_name(std::string&);
				void read_attr(char quote);
				void read_string();
				bool read_tag_name();
				bool read_close_tag();
				void read_comment();
				void read_cdata();
				void read_conditional();
				void read_attribute_list();
				void read_doctype();
				void read_element_type();
				void read_notation();
				void read_processing_instruction();
				void read_ws_until(std::size_t to_idx);
				char consume_nonws();
				void consume_nonws(int count);
				char consume_maybe_ws();
				void consume_escape(std::string& out);
				std::size_t peek_next_nonws_idx();
				char peek();
				char peek(int idx);
				template<int len> bool peek(const char(&str)[len]) { return peek(str, len-1); }
				bool peek(const char* str, int len);
				bool at_eof();
				void read_buffer(std::size_t keep_after_idx);
			};

			template<class forward_it>
			class read_buf_impl : public read_buf_t {
				forward_it begin_;
				forward_it end_;
			public:
				read_buf_impl(forward_it begin, forward_it end)
					: begin_(begin), end_(end)
				{}
				virtual read_buf_impl* copy_construct_at(char* buffer, std::size_t buffer_size)const& //noexcept(noexcept(read_buf_impl{ *this }))
				{
					assert(buffer_size > sizeof(read_buf_impl)); return new(buffer)read_buf_impl(*this);
				}
				virtual read_buf_impl* move_construct_at(char* buffer, std::size_t buffer_size) & //noexcept(noexcept(read_buf_impl{ std::move(*this) }))
				{
					assert(buffer_size > sizeof(read_buf_impl)); return new(buffer)read_buf_impl(std::move(*this));
				}
				virtual ~read_buf_impl() {};
				virtual int read(char* buffer, int count) {
					int c = 0;
					while (c < count && begin_ != end_) {
						*buffer = *begin_;
						++buffer;
						++begin_;
						++c;
					}
					return c;
				}
			};
		}
	}
}