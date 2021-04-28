#pragma once
#include "xml_reader.hpp"

namespace mpd {
	namespace xml {


		/*
		One can always build custom parser classes, but it can feel like coding bits you shouldn't have to code.
		So alternatively, there's template magic here to create a parser with a builder-type pattern, which
		feels more configuration-y.
		*/
		namespace builder {
			namespace impl {
				template<class funcT, funcT func, class Container, class Item, std::enable_if_t<std::is_member_function_pointer_v<funcT>,bool> =true>
				auto invoke_add_item(base_reader& reader, Container& container, Item&& item) -> decltype(func(reader, container, std::move(item))) {return func(reader, container, std::move(item));}
				template<class funcT, funcT func, class Container, class Item, std::enable_if_t<std::is_member_function_pointer_v<funcT>,bool> =true>
				auto invoke_add_item(base_reader& reader, Container& container, Item&& item) -> decltype(func(container, std::move(item))) {return func(container, std::move(item));}
				template<class funcT, funcT func, class Container, class Item, std::enable_if_t<std::is_member_function_pointer_v<funcT>,bool> =true>
				auto invoke_add_item(base_reader&, Container& container, Item&& item) -> decltype((container.*func)(std::move(item))) {return (container.*func)(std::move(item));}
				template<class funcT, funcT func, class Container, class Item, std::enable_if_t<!std::is_member_function_pointer_v<funcT>,bool> =true>
				auto invoke_add_item(base_reader&, Container& container, Item&& item) -> decltype((container.*func)=std::move(item)) {return (container.*func)=std::move(item);}
				template<class funcT, funcT func>
				auto invoke_stot(base_reader& reader, std::string&& content) -> decltype(func(reader, std::move(content))) {return func(reader, std::move(content));}
				template<class funcT, funcT func>
				auto invoke_stot(base_reader&, std::string&& content) -> decltype(func(std::move(content))) {return func(std::move(content));}
			}

			template<const char* name_, class stot_t, stot_t stot, class set_attr_t, set_attr_t set_attr, bool required=true>
			struct attribute {
				bool found = false;
				void reset() {found = false;}
				const char* name() const {return name_;}
				template<class Container>
				bool parse_attribute(Container& container, attribute_reader& reader, std::string&& content)
				{ 
					if (found) reader.throw_unexpected("duplicate attribute "s + name_);
					auto attr = impl::invoke_stot<stot_t, stot>(reader, std::move(content));
					impl::invoke_add_item<set_attr_t, set_attr>(reader, container, std::move(attr));
					return true;
				}
				void end(base_reader& reader) {
					if(required && !found) return reader.throw_missing("missing "s + name_);
				}
			};
#define mpd_xml_builder_attribute(name, stot, set_attr) mpd::xml::builder::attribute<name, decltype(stot), stot, decltype(set_attr), set_attr>
			template<const char* name_, class child_parser_t, class add_child_t, add_child_t add_child, int min=0, int max=1>
			struct element {
				int found = 0;
				void reset() {found = 0;}
				const char* name() const {return name_;}
				template<class Container>
				bool parse_child_element(Container& container, element_reader& reader) {
					if (++found > max) reader.throw_unexpected("too many "s + name_);
					auto child = reader.read_child(child_parser_t{});
					impl::invoke_add_item<add_child_t, add_child>(reader, container, std::move(child));
					return true;
				}
				void end(base_reader& reader) {
					if(found < min) return reader.throw_missing("too few "s + tag_);
				}
			};
#define mpd_xml_builder_element_optional(name, child_parser_t, add_child) mpd::xml::builder::element<name, child_parser_t, decltype(add_child), add_child, 0, 1>
#define mpd_xml_builder_element_required(name, child_parser_t, add_child) mpd::xml::builder::element<name, child_parser_t, decltype(add_child), add_child, 1, 1>
#define mpd_xml_builder_element_repeating(name, child_parser_t, add_child) mpd::xml::builder::element<name, child_parser_t, decltype(add_child), add_child, 0, 0xFFFFFFFF>
			template<class s_to_t_t, s_to_t_t s_to_t, class add_text_t, add_text_t add_text>
			struct text {
				template<class Container>
				void parse_child_text(Container& container, base_reader& reader, std::string&& content) {
					auto item = impl::invoke_stot<s_to_t_t, s_to_t>(reader, std::move(content));
					impl::invoke_add_item<add_text_t, add_text>(reader, container, std::move(item));
				}
			};
#define mpd_xml_builder_text(s_to_t, add_text) mpd::xml::builder::text<decltype(s_to_t), s_to_t, decltype(add_text), add_text>

			struct no_text_parser {
				void parse_child_text(base_reader& reader, std::string&&) {
					reader.throw_unexpected();
				}
			};

			template<class T, class element_parser_tuple, class attribute_parser_tuple=std::tuple<>, class text_parser_t=no_text_parser>
			struct parser;
			template<class T, class... element_parsers_t, class... attribute_parsers_t, class text_parser_t>
			struct parser<T, std::tuple<element_parsers_t...>, std::tuple<attribute_parsers_t...>, text_parser_t> {
			private:
				T item;
				std::tuple<element_parsers_t...> element_parsers;
				std::tuple<attribute_parsers_t...> attribute_parsers;
				text_parser_t text_parser;
			public:
				void reset() { 
					item = {}; 
					(std::get<element_parsers_t>(element_parsers).reset(),...); 
					(std::get<attribute_parsers_t>(attribute_parsers).reset(),...); 
				}
				T parse_tag(tag_reader& reader, const std::string&) { return reader.read_element(*this); }
				void parse_attribute(attribute_reader& reader, const std::string& name, std::string&& value) {
					bool parsed = (
						(std::get<attribute_parsers_t>(attribute_parsers).name() == name 
							&& std::get<attribute_parsers_t>(attribute_parsers).parse_attribute(item, reader, std::move(value)))
						|| ...);
					if (!parsed) reader.throw_unexpected("unexpected attribute " + name);
				}
				parser parse_content(base_reader&) { return *this; }
				void parse_child_element(element_reader& reader, const std::string& child_tag) {
					bool parsed = (
						(std::get<element_parsers_t>(element_parsers).name() == name 
							&& std::get<element_parsers_t>(element_parsers).parse_child_element(item, reader))
						|| ...);
					if (!parsed) reader.throw_unexpected("unexpected tag " + child_tag);
				}
				void parse_child_node(base_reader& reader, node_type type, std::string&& content) {
					if (type != node_type::string_node)
						reader.throw_unexpected("unexpected node type ");
					else {
						std::string_view view = mpd::trim(content);
						if (!view.empty())
							text_parser.parse_child_text(reader, std::string(view));
					}
				}
				T&& end_parse(base_reader& reader) {
					(std::get<element_parsers_t>(element_parsers).end(reader), ...);
					return std::move(item);
				}
			};
			template<class T, class s_to_t_t, s_to_t_t s_to_t>
			struct text_only_parser {
			private:
				T item;
				bool found;
			public:
				void reset() { item = {}; found = false;}
				void parse_child_node(base_reader& reader, node_type type, std::string&& content) {
					if (type != node_type::string_node || found)
						reader.throw_unexpected();
					else {
						std::string_view view = mpd::trim(content);
						if (!view.empty()) {
							item = invoke_stot<s_to_t_t, s_to_t>(reader, std::move(content));
							found = true;
						}
					}
				}
				T&& end_parse(base_reader& reader) {
					if (!found) reader.throw_missing();
					return std::move(item);
				}
			};
#define mpd_xml_builder_text_only_parser(T, s_to_t) mpd::xml::builder::text_only_parser<T, decltype(s_to_t), s_to_t>
		}
	}
}