
#include "xml_std_parsers.hpp"
#include "xml_attributes.hpp"
#include <iostream>
#include <new>

// simple class heiarchy
struct three {
	int attr1;
	std::string attr2;
};
struct two {
	int attr1;
	std::string attr2;
	std::vector<three> nodes;
	std::vector<std::string> texts;
};
struct one {
	int attr1;
	std::string attr2;
	std::vector<two> nodes;
};

//parsers for those classes
/*struct three_parser {
	using element_type = three;
	std::optional<int> attr1;
	std::optional<std::string> attr2;

	void reset() 
	{ attr1.reset(); }
	three_parser parse_tag(mpd::xml::tag_reader& reader, const std::string&) { return reader.read_element(*this); }
	void parse_attribute(mpd::xml::attribute_reader& reader, const std::string& name, std::string&& value) 
	{ mpd::xml::read_element(reader, name, std::move(value))("attr1", attr1)("attr2", attr2); }
	three_parser& parse_content(mpd::xml::attribute_reader& reader)
	{ mpd::xml::require_attributes(reader)("attr1", attr1)("attr2", attr2); return *this; }
	three end_parse(mpd::xml::base_reader&) {
		return three{ *std::move(attr1), *std::move(attr2) };
	}
};*/
extern const char attr1_string[] = "attr1";
extern const char attr2_string[] = "attr2";
using three_parser = mpd::xml::builder::parser<three,
	std::tuple<>, //elements
	std::tuple<	//attributes
		mpd_xml_builder_attribute(attr1_string, (mpd::xml::impl::strtoi_parser<int, long, std::strtol>), &three::attr1),
		mpd_xml_builder_attribute(attr2_string, std::move<std::string&&>, &three::attr2)
	>
>;
struct two_parser {
	using element_type = two;
	std::optional<int> attr1;
	std::optional<std::string> attr2;
	std::vector<three> nodes;
	std::vector<std::string> texts;

	void reset() 
	{ attr1.reset(); attr2.reset(); }
	two parse_tag(mpd::xml::tag_reader& reader, const std::string&) { return reader.read_element(*this); }
	void parse_attribute(mpd::xml::attribute_reader& reader, const std::string& name, std::string&& value)
	{ mpd::xml::read_element(reader, name, std::move(value))("attr1", attr1)("attr2", attr2); }
	two_parser& parse_content(mpd::xml::base_reader& reader)
	{ mpd::xml::require_attributes(reader)("attr1", attr1)("attr2", attr2); return *this; }
	void parse_child_element(mpd::xml::element_reader& reader, const std::string& content) {
		if (content == "three") nodes.emplace_back(reader.read_child(three_parser{}));
		else reader.throw_unexpected();
	}
	void parse_child_node(mpd::xml::base_reader& reader, mpd::xml::node_type type, std::string&& content) {
		if (type == mpd::xml::node_type::string_node) {
			if (content.find("_") != -1) reader.throw_invalid_content("two-class strings can't contain _");
			texts.push_back(content);
		} else if (type == mpd::xml::node_type::comment_node) std::cout << "two-comment: " << content << '\n';
		else if (type == mpd::xml::node_type::processing_node) std::cout << "two-PN: " << content << '\n';
	}
	two end_parse(mpd::xml::attribute_reader&) 
	{ return two{ *std::move(attr1), *std::move(attr2), std::move(nodes), std::move(texts) }; }
};
/*
Builder would look like this, but builder can't handle comments nor processing nodes
void add_three_to_two(two& parent, three&& child) {parent.nodes.emplace_back(std::move(child));}
void add_text_to_two(two& parent, std::string&& child) {parent.texts.emplace_back(std::move(child));}
using two_parser = mpd::xml::builder::parser<two,
	std::tuple<	//elements
		mpd_xml_builder_element_repeating("three", three_parser, add_three_to_two)
	>,
	std::tuple<	//attributes
		mpd_xml_builder_attribute(attr1_string, (mpd::xml::impl::strtoi_parser<int, long, std::strtol>), &two::attr1),
		mpd_xml_builder_attribute(attr2_string, std::move<std::string&&>, &two::attr2)
	>,
	mpd_xml_builder_text(std::move<std::string&&>, add_text_to_two) //texts
>;
*/
struct one_parser {
	using element_type = one;
	std::optional<int> attr1;
	std::optional<std::string> attr2;
	std::vector<two> nodes;

	void reset() 
	{ attr1.reset(); attr2.reset(); }
	one parse_tag(mpd::xml::tag_reader& reader, const std::string&) { return reader.read_element(*this); }
	void parse_attribute(mpd::xml::attribute_reader& reader, const std::string& name, std::string&& value)
	{ mpd::xml::read_element(reader, name, std::move(value))("attr1", attr1)("attr2", attr2); }
	one_parser& parse_content(mpd::xml::base_reader& reader) 
	{ mpd::xml::require_attributes(reader)("attr1", attr1)("attr2", attr2); return *this; }
	void parse_child_element(mpd::xml::element_reader& reader, const std::string& content) {
		if (content == "two") {
			try { nodes.emplace_back(reader.read_child(two_parser{})); }
			catch (std::runtime_error& e) { std::cerr << "SUCCESSFULLY HANDLED ERROR: " << e.what() << '\n'; }
		} else reader.throw_unexpected();
	}
	void parse_child_node(mpd::xml::base_reader&, mpd::xml::node_type type, std::string&& content) 
	{ if (type == mpd::xml::node_type::string_node) std::cout << "one-str: " << content << '\n'; }
	one end_parse(mpd::xml::attribute_reader&) 
	{ return one{ *std::move(attr1), *std::move(attr2), std::move(nodes) }; }
};
/*
Builder would look like this, but builder doesn't catch that exception
void add_two_to_one(one& parent, three&& child) {parent.nodes.emplace_back(std::move(child));}
void add_text_to_one(one& parent, std::string&& child) {parent.texts.emplace_back(std::move(child));}
using one_parser = mpd::xml::builder::parser<one,
	std::tuple<	//elements
		mpd_xml_builder_element_repeating("two", two_parser, add_two_to_one)
	>,
	std::tuple<	//attributes
		mpd_xml_builder_attribute(attr1_string, (mpd::xml::impl::strtoi_parser<int, long, std::strtol>), &one::attr1),
		mpd_xml_builder_attribute(attr2_string, std::move<std::string&&>,&one::attr2)
	>,
	mpd_xml_builder_text(std::move<std::string&&>, add_text_to_one)	//texts
>;
*/


int main() {
    char buffer[] =
        "<one attr1=\"1\" attr2=\"asdf\">\n"
        "   <two attr1=\"2\" attr2=\"asdfasdf1\">\n"
        "       <![CDATA[raw content\n"
        "       <>!\"']]]]>\n"
        "       1 Text Text Text\n"
        "       <three attr1=\"3\" attr2=\"1asdfasdfasdf\" />\n"
        "   </two>\n"
        "   <two attr1=\"4\" attr2=\"asdfasdf2\">\n"
        "       <three attr1=\"5\" attr2=\"2asdfasdfasdf\" />\n"
        "       2 Text Text Text\n"
        "       <three attr1=\"6\" attr2=\"3asdfasdfasdf\" />\n"
        "   </two>\n"
        "   <two attr1=\"7\" attr2=\"asdfasdf3\">\n"
        "       <three attr1=\"8\" attr2=\"4asdfasdfasdf\" />\n"
        "       <!--THIS IS A -COMMENT-->\n"
        "       <three attr1=\"9\" attr2=\"5asdfasdfasdf\" />\n"
        "       <?THIS IS A? PROCESSING INSTRUCTION?>\n"
        "       <three attr1=\"10\" attr2=\"6asdfasdfasdf\" />\n"
        "       3 Text Text Text\n"
        "   </two>\n"
        "   <![CDATA[raw content\n"
        "       <>!\"']]]]>\n"
        "</one>\n";

	mpd::xml::document_reader parser("test literal", std::begin(buffer), std::end(buffer)-1);
	one data{};
    try {
		data = parser.read_child("one", one_parser{});
    } catch(const std::exception& exc) {
        std::cerr << "INVALID READ: "<<exc.what()<<'\n';
    }

    //parser.readRoot("ted", main);

    return 0;
}
 