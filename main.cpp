
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
struct three_parser {
	std::optional<int> attr1;
	std::optional<std::string> attr2;

	three begin_element(mpd::xml::tag_reader& reader, const std::string&) 
	{ attr1.reset(); attr2.reset(); return reader.read_attributes(*this); }
	void read_attribute(mpd::xml::attribute_reader& reader, const std::string& name, std::string&& value) 
	{ mpd::xml::read_attributes(reader, name, std::move(value))("attr1", attr1)("attr2", attr2); }
	three_parser& begin_content(mpd::xml::attribute_reader& reader)
	{ mpd::xml::require_attributes(reader)("attr1", attr1)("attr2", attr2); return *this; }
	three end_element(mpd::xml::base_reader&) {
		return three{ *std::move(attr1), *std::move(attr2) };
	}
};
struct two_parser {
	std::optional<int> attr1;
	std::optional<std::string> attr2;
	std::vector<three> nodes;
	std::vector<std::string> texts;

	two begin_element(mpd::xml::tag_reader& reader, const std::string&) 
	{ attr1.reset(); attr2.reset(); return reader.read_attributes(*this); }
	void read_attribute(mpd::xml::attribute_reader& reader, const std::string& name, std::string&& value)
	{ mpd::xml::read_attributes(reader, name, std::move(value))("attr1", attr1)("attr2", attr2); }
	two_parser& begin_content(mpd::xml::attribute_reader& reader)
	{ mpd::xml::require_attributes(reader)("attr1", attr1)("attr2", attr2); return *this; }
	void read_child_element(mpd::xml::element_reader& reader, const std::string& content) {
		if (content == "three") nodes.emplace_back(reader.read_element(three_parser{}));
		else reader.throw_unexpected();
	}
	void read_child_node(mpd::xml::base_reader& reader, mpd::xml::node_type type, std::string&& content) {
		if (type == mpd::xml::node_type::string_node) {
			if (content.find("_") != -1) reader.throw_invalid_content("two-class strings can't contain _");
			texts.push_back(content);
		} else if (type == mpd::xml::node_type::comment_node) std::cout << "two-comment: " << content << '\n';
		else if (type == mpd::xml::node_type::processing_node) std::cout << "two-PN: " << content << '\n';
	}
	two end_element(mpd::xml::attribute_reader&) 
	{ return two{ *std::move(attr1), *std::move(attr2), std::move(nodes), std::move(texts) }; }
};
struct one_parser {
	std::optional<int> attr1;
	std::optional<std::string> attr2;
	std::vector<two> nodes;

	one begin_element(mpd::xml::tag_reader& reader, const std::string&) 
	{ attr1.reset(); attr2.reset(); return reader.read_attributes(*this); }
	void read_attribute(mpd::xml::attribute_reader& reader, const std::string& name, std::string&& value)
	{ mpd::xml::read_attributes(reader, name, std::move(value))("attr1", attr1)("attr2", attr2); }
	one_parser& begin_content(mpd::xml::attribute_reader& reader) 
	{ mpd::xml::require_attributes(reader)("attr1", attr1)("attr2", attr2); return *this; }
	void read_child_element(mpd::xml::element_reader& reader, const std::string& content) {
		if (content == "two") {
			try { nodes.emplace_back(reader.read_element(two_parser{})); }
			catch (std::runtime_error& e) { std::cerr << "SUCCESSFULLY HANDLED ERROR: " << e.what() << '\n'; }
		} else reader.throw_unexpected();
	}
	void read_child_node(mpd::xml::element_reader&, mpd::xml::node_type type, std::string&& content) 
	{ if (type == mpd::xml::node_type::string_node) std::cout << "one-str: " << content << '\n'; }
	one end_element(mpd::xml::attribute_reader&) 
	{ return one{ *std::move(attr1), *std::move(attr2), std::move(nodes) }; }
};

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
		data = parser.read_element("one", one_parser{});
    } catch(const std::exception& exc) {
        std::cerr << "INVALID READ: "<<exc.what()<<'\n';
    }

    //parser.readRoot("ted", main);

    return 0;
}
