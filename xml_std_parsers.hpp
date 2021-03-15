#pragma once
#include "xml_parser_builder.hpp"
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


		namespace impl {
			template<class T>
			char char_parser(base_reader& reader, std::string_view content) {
				if (content.length() != 1) reader.throw_invalid_content("expected only a single char");
				return (T)content[0];
			}
			template<class T, class R, R F(const char*, char**, int)>
			T strtoi_parser(base_reader& reader, const std::string& content) {
				char* end = 0;
				R temp = F(content.c_str(), &end, 10);
				if (end != content.data() + content.length())
					reader.throw_invalid_content("could not parse entire input for "s + typeid(T).name());
				if (temp > std::numeric_limits<T>::max() || temp < std::numeric_limits<T>::min())
					reader.throw_invalid_content("out of range for "s + typeid(T).name());
				return static_cast<T>(temp);
			}
			template<class T, T F(const char*, char**)>
			T strtof_parser(base_reader& reader, const std::string& content) {
				char* end = 0;
				T value = static_cast<T>(F(content.c_str(), &end));
				if (end != content.data() + content.length())
					reader.throw_invalid_content("expected number for "s + typeid(T).name());
				return value;
			}
			template<class Container>
			void emplace_back(Container& container, typename Container::type&& item) { container.emplace_back(std::move(item)); }
			template<class T>
			void optional_emplace(std::optional<T>& container, T&& item) { container.emplace(std::move(item)); }
		}

		using trimmed_string_parser = mpd_xml_builder_text_only_parser(std::string, std::move<std::string&&>);
		using char_parser = mpd_xml_builder_text_only_parser(char, (impl::char_parser<char>));
		using signed_char_parser = mpd_xml_builder_text_only_parser(signed char, (impl::char_parser<signed char>));
		using unsigned_char_parser = mpd_xml_builder_text_only_parser(unsigned char, (impl::char_parser<unsigned char>));

		using byte_parser = mpd_xml_builder_text_only_parser(char, (impl::strtoi_parser<char, long, std::strtol>));
		using signed_byte_parser = mpd_xml_builder_text_only_parser(signed char, (impl::strtoi_parser<signed char, long, std::strtol>));
		using short_parser = mpd_xml_builder_text_only_parser(short, (impl::strtoi_parser<short, long, std::strtol>));
		using int_parser = mpd_xml_builder_text_only_parser(int, (impl::strtoi_parser<int, long, std::strtol>));
		using long_parser = mpd_xml_builder_text_only_parser(long, (impl::strtoi_parser<long, long, std::strtol>));
		using long_long_parser = mpd_xml_builder_text_only_parser(long long, (impl::strtoi_parser<long long, long long, std::strtoll>));

		using unsigned_byte_parser = mpd_xml_builder_text_only_parser(unsigned char, (impl::strtoi_parser<unsigned char, unsigned long, std::strtoul>));
		using unsigned_short_parser = mpd_xml_builder_text_only_parser(unsigned short, (impl::strtoi_parser<unsigned short, unsigned long, std::strtoul>));
		using unsigned_int_parser = mpd_xml_builder_text_only_parser(unsigned int, (impl::strtoi_parser<unsigned int, unsigned long, std::strtoul>));
		using unsigned_long_parser = mpd_xml_builder_text_only_parser(unsigned long, (impl::strtoi_parser<unsigned long, unsigned  long, std::strtoul>));
		using unsigned_long_long_parser = mpd_xml_builder_text_only_parser(unsigned long long, (impl::strtoi_parser<unsigned long long, unsigned long long, std::strtoull>));

		using float_parser = mpd_xml_builder_text_only_parser(float, (impl::strtof_parser<float, std::strtof>));
		using double_parser = mpd_xml_builder_text_only_parser(double, (impl::strtof_parser<double, std::strtod>));
		using long_double_parser = mpd_xml_builder_text_only_parser(long double, (impl::strtof_parser<long double, std::strtold>));

		
		template<class T, const char* tag, class element_parser_t>
		using vector_parser = builder::parser<std::vector<T>, std::tuple<mpd_xml_builder_element_repeating(tag, element_parser_t, &std::vector<T>::template emplace_back<T&&>)>>;
		template<class T, const char* tag, class element_parser_t>
		using list_parser = builder::parser<std::list<T>, std::tuple<mpd_xml_builder_element_repeating(tag, element_parser_t, &std::list<T>::template emplace_back<T&&>)>>;
		template<class T, const char* tag, class element_parser_t>
		using deque_parser = builder::parser<std::deque<T>, std::tuple<mpd_xml_builder_element_repeating(tag, element_parser_t, &std::deque<T>::template emplace_back<T&&>)>>;
		template<class T, const char* tag, class element_parser_t>
		using optional_parser = builder::parser<std::optional<T>, std::tuple<mpd_xml_builder_element_optional(tag, element_parser_t, &std::optional<T>::template emplace<T&&>)>>;

		//TODO: maps... using id as key?
	}
}