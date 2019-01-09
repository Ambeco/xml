#pragma once
#include <optional>
#include <list>
#include <queue>
#include "xml_reader.hpp"

namespace mpd {
	namespace xml {
		template<class T, class derived_parser_t>
		class simple_string_parser {
		protected: std::optional<T> value;
		public:
			T begin_element(attribute_reader& reader, const std::string& tag_id) 
			{ value.reset(); return reader.read_attribute(*this); }
			void read_node(element_reader& reader, node_type type, std::string&& content) {
				if (type == node_type::string_node && !value.has_value()) {
					value.set(reinterpret_cast<derived_parser_t*>(this)->stov(reader, std::move(content)));
				} else if (type == node_type::string_node || type == node_type::element_node)
					reader.throw_unexpected();
			}
			T end_element(attribute_reader& reader) {
				if (!value.has_value()) reader.throw_missing(node_type::string_node, "value");
				return value;
			}
		};

		struct string_parser : public simple_string_parser<std::string, string_parser> {
			std::string&& stov(element_reader&, std::string&& content) {
				return std::move(content);
			}
		};

		struct char_parser : public simple_string_parser<std::string, string_parser> {
			char stov(element_reader& reader, std::string&& content) {
				if (content.length() != 1) reader.throw_invalid_content("expected only a single char");
				return content[0];
			}
		};

		template<class T, class R, R F(const char*, char**, int)>
		struct strtoi_parser : public simple_string_parser<T, strtoi_parser<T,R,F>> {
			T stov(element_reader& reader, std::string&& content) {
				char* end = 0;
				R temp = F(content.c_str(), &end, 10);
				if (end != content.data() + content.length())
					reader.throw_invalid_content((std::string("could not parse entire input for ") + typeid(T).name).c_str());
				if (temp > std::limits<T>::max() || temp < std::limits<T>::min())
					reader.throw_invalid_content((std::string("out of range for ") + typeid(T).name).c_str());
				return static_cast<T>(temp);
			}
		};
		typedef strtoi_parser<short, long, std::strtol> short_parser;
		typedef strtoi_parser<int, long, std::strtol> int_parser;
		typedef strtoi_parser<long, long, std::strtol> long_parser;
		typedef strtoi_parser<long long, long long, std::strtoll> long_long_parser;
		typedef strtoi_parser<unsigned short, unsigned long, std::strtoul> unsigned_short_parser;
		typedef strtoi_parser<unsigned int, unsigned long, std::strtoul> unsigned_int_parser;
		typedef strtoi_parser<unsigned long, unsigned long, std::strtoul> unsigned_long_parser;
		typedef strtoi_parser<unsigned long long, unsigned long long, std::strtoull> unsigned_long_long_parser;

		template<class T, T F(const char*, char**)>
		struct strtof_parser {
			T stov(element_reader& reader, std::string&& content) {
				char* end = 0;
				value = static_cast<T>(T(content.c_str(), &end));
				if (end != content.data() + content.length())
					reader.throw_invalid_content((std::string("expected number for ") + typeid(T).name).c_str());
				set = true;
			}
		};
		typedef strtof_parser<float, std::strtof> float_parser;
		typedef strtof_parser<double, std::strtod> double_parser;
		typedef strtof_parser<long double, std::strtold> long_double_parser;

		template<class derived_parser_t, class element_parser_t, class container_t, class element_t>
		class simple_container_parser {
			std::string child_tag_;
			element_parser_t parser;
		protected:
			container_t container;
		public:
			simple_container_parser(std::string child_tag) : child_tag_(std::move(child_tag)), parser() {}
			template<class...Us> explicit simple_container_parser(std::string child_tag, Us&&...vs)
				: child_tag_(std::move(child_tag)), parser(std::forward<Us>(vs)...) {}
			element_t begin_element(attribute_reader& reader, const std::string& tag_id) 
			{ container.clear(); return reader.read_attributes(*this); }
			void read_node(element_reader& reader, node_type type, std::string&& content) {
				if (type == element_node && content == child_tag_)
					reinterpret_cast<element_parser_t*>(this)->add_child(reader, reader.read_element(parser));
				else if (type == string_node || type == element_node)
						reader.throw_unexpected();
			}
			container_t&& end_element(attribute_reader& reader) { return std::move(container); }
		};

		template<class element_parser_t, class T = typename parser_output<element_parser_t>::type>
		struct vector_parser : public simple_container_parser<vector_parser<element_parser_t,T>, element_parser_t, std::vector<T>, T> {
			vector_parser(std::string child_tag) : element_parser_t(std::move(child_tag)) {}
			template<class...Us> explicit vector_parser(std::string child_tag, Us&&...vs) 
				: element_parser_t(std::move(child_tag), parser(std::forward<Us>(vs)...)) {}
			template<class U> void add_child(element_reader& reader, U&& value) 
			{ container.emplace_back(std::forward<U>(value)); }
		};

		template<class element_parser_t, class T = typename parser_output<element_parser_t>::type>
		struct list_parser : public simple_container_parser<list_parser<element_parser_t, T>, element_parser_t, std::list<T>, T> {
			list_parser(std::string child_tag) : element_parser_t(std::move(child_tag)) {}
			template<class...Us> explicit list_parser(std::string child_tag, Us&&...vs)
				: element_parser_t(std::move(child_tag), parser(std::forward<Us>(vs)...)) {}
			template<class U> void add_child(element_reader& reader, U&& value)
			{ container.emplace_back(std::forward<U>(value)); }
		};

		template<class element_parser_t, class T = typename parser_output<element_parser_t>::type>
		struct deque_parser : public simple_container_parser<deque_parser<element_parser_t, T>, element_parser_t, std::deque<T>, T> {
			deque_parser(std::string child_tag) : element_parser_t(std::move(child_tag)) {}
			template<class...Us> explicit deque_parser(std::string child_tag, Us&&...vs)
				: element_parser_t(std::move(child_tag), parser(std::forward<Us>(vs)...)) {}
			template<class U> void add_child(element_reader& reader, U&& value)
			{ container.emplace_back(std::forward<U>(value)); }
		};

		template<class element_parser_t, class T = typename parser_output<element_parser_t>::type>
		struct optional_parser : public simple_container_parser<optional_parser<element_parser_t, T>, element_parser_t, std::optional<T>, T> {
			optional_parser(std::string child_tag) : element_parser_t(std::move(child_tag)) {}
			template<class...Us> explicit optional_parser(std::string child_tag, Us&&...vs)
				: element_parser_t(std::move(child_tag), parser(std::forward<Us>(vs)...)) {}
			template<class U> void add_child(element_reader& reader, U&& value)
			{ if (container.has_value()) reader.throw_unexpected(); container.emplace(std::forward<U>(value)); }
		};

		//TODO: maps... using id as key?
	}
}