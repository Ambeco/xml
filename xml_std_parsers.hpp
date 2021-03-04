#pragma once
#include "xml_reader.hpp"
#include <limits>
#include <list>
#include <optional>
#include <queue>

namespace mpd {
	namespace xml {
		struct untrimmed_string_parser {
			protected: std::optional<std::string> value;
			public:
				void reset()
				{ value.reset(); }
				void parse_child_node(base_reader& reader, node_type type, std::string&& content) {
					if (type == node_type::string_node && !value.has_value())
						value.emplace(std::move(content));
					else reader.throw_unexpected();
				}
				std::string&& end_parse(base_reader& reader) {
					if (!value.has_value()) reader.throw_missing(node_type::string_node, "value");
					return std::move(value.value());
				}
		};

		template<class T, class s_to_v_f>
		class simple_string_parser {
		protected: 
			std::optional<T> value;
			s_to_v_f s_to_v;
		public:
			void reset()
			{ value.reset(); }
			void parse_child_node(base_reader& reader, node_type type, std::string&& content) {
				if (type == node_type::string_node && !value.has_value())
					value.emplace(s_to_v(reader, trim(content)));
				else reader.throw_unexpected();
			}
			T&& end_parse(base_reader& reader) {
				if (!value.has_value()) reader.throw_missing(node_type::string_node, "value");
				return std::move(value.value());
			}
		};
		namespace impl {
			struct trimmed_string_parser_f {
				std::string operator()(base_reader&, std::string_view content) {return std::string(content);}
			};
			template<class T>
			struct char_parser_f {
				char operator()(base_reader& reader, std::string_view content) {
					if (content.length() != 1) reader.throw_invalid_content("expected only a single char");
					return (T)content[0];
				}
			};
			template<class T, class R, R F(const char*, char**, int)>
			struct strtoi_parser {
				char operator()(base_reader& reader, std::string_view content) {
					char* end = 0;
					R temp = F(content.c_str(), &end, 10);
					if (end != content.data() + content.length())
						reader.throw_invalid_content("could not parse entire input for "s + typeid(T).name);
					if (temp > std::numeric_limits<T>::max() || temp < std::numeric_limits<T>::min())
						reader.throw_invalid_content("out of range for "s + typeid(T).name);
					return static_cast<T>(temp);
				}
			};
			template<class T, T F(const char*, char**)>
			struct strtof_parser {
				T operator()(base_reader& reader, std::string_view content) {
					char* end = 0;
					value = static_cast<T>(F(content.c_str(), &end));
					if (end != content.data() + content.length())
						reader.throw_invalid_content("expected number for "s + typeid(T).name);
					return value;
				}
			};
		}
		typedef simple_string_parser<std::string, impl::trimmed_string_parser_f> trimmed_string_parser;
		typedef simple_string_parser<char, impl::char_parser_f<char>> char_parser;
		typedef simple_string_parser<char, impl::char_parser_f<signed char>> signed_char_parser;
		typedef simple_string_parser<char, impl::char_parser_f<unsigned char>> unsigned_char_parser;

		typedef simple_string_parser<char, impl::strtoi_parser<char, long, std::strtol>> byte_parser;
		typedef simple_string_parser<signed char, impl::strtoi_parser<signed char, long, std::strtol>> signed_byte_parser;
		typedef simple_string_parser<short, impl::strtoi_parser<short, long, std::strtol>> short_parser;
		typedef simple_string_parser<int, impl::strtoi_parser<int, long, std::strtol>> int_parser;
		typedef simple_string_parser<long, impl::strtoi_parser<long, long, std::strtol>> long_parser;
		typedef simple_string_parser<long long, impl::strtoi_parser<long long, long long, std::strtoll>> long_long_parser;
		typedef simple_string_parser<unsigned char, impl::strtoi_parser<unsigned char, long, std::strtol>> ubyte_parser;
		typedef simple_string_parser<unsigned short, impl::strtoi_parser<unsigned short, unsigned long, std::strtoul>> unsigned_short_parser;
		typedef simple_string_parser<unsigned int, impl::strtoi_parser<unsigned int, unsigned long, std::strtoul>> unsigned_int_parser;
		typedef simple_string_parser<unsigned long, impl::strtoi_parser<unsigned long, unsigned long, std::strtoul>> unsigned_long_parser;
		typedef simple_string_parser<unsigned long long, impl::strtoi_parser<unsigned long long, unsigned long long, std::strtoull>> unsigned_long_long_parser;

		typedef simple_string_parser<float, impl::strtof_parser<float, std::strtof>> float_parser;
		typedef simple_string_parser<double, impl::strtof_parser<double, std::strtod>> double_parser;
		typedef simple_string_parser<long double, impl::strtof_parser<long double, std::strtold>> long_double_parser;

		template<class container_t, class element_parser_t, class add_child_f>
		class simple_container_parser {
			std::string child_tag_;
			element_parser_t parser;
		protected:
			container_t container;
			add_child_f add_child;
		public:
			simple_container_parser(std::string child_tag) : child_tag_(std::move(child_tag)), parser() {}
			template<class...Us> explicit simple_container_parser(std::string child_tag, Us&&...vs)
				: child_tag_(std::move(child_tag)), parser(std::forward<Us>(vs)...) {}
			void reset()
			{ container.clear(); }
			void parse_child_element(element_reader& reader, const std::string& tag) {
				if (tag == child_tag_ || child_tag == nullptr_)
					add_child(reader, container, reader.read_element(parser));
				else reader.throw_unexpected();
			}
			container_t&& end_parse(base_reader& reader) { return std::move(container); }
		};

		namespace impl {
			struct back_inserter_f {
				template<class Container, class T>
				void operator()(element_reader& reader, Container& container, T&& item) {
					container.insert(container.end(), std::forward<T>(item));
				}
			};
			struct optional_inserter_f {
				template<class T, class U>
				void operator()(element_reader& reader, std::optional<T>& container, U&& item) {
					if ((bool)container) reader.throw_unexpected();
					container.emplace(std::forward<U>(value));
				}
			};
		}

		template<class T, class element_parser_t>
		struct vector_parser : public simple_container_parser<element_parser_t, std::vector<T>, impl::back_inserter_f> 
		{};
		template<class T, class element_parser_t>
		struct list_parser : public simple_container_parser<element_parser_t, std::list<T>, impl::back_inserter_f> 
		{};
		template<class T, class element_parser_t>
		struct deque_parser : public simple_container_parser<element_parser_t, std::deque<T>, impl::back_inserter_f> 
		{};
		template<class T, class element_parser_t>
		struct optional_parser : public simple_container_parser<element_parser_t, std::optional<T>, impl::optional_inserter_f> 
		{};

		//TODO: maps... using id as key?
	}
}