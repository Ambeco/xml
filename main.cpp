
#include "std_xml_parsers.hpp"
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
	{
		if (name == "attr1") {
			if (attr1.has_value()) reader.throw_unexpected();
			attr1.emplace(std::atoi(value.data())); // TODO add easy helper method to reader
		} else if (name == "attr2") {
			if (attr2.has_value()) reader.throw_unexpected();
			attr2.emplace(std::move(value));
		} else
			reader.throw_unexpected();
	}
	three_parser& start_element(mpd::xml::attribute_reader& reader) {
		if (!attr1.has_value()) reader.throw_missing(mpd::xml::node_type::attribute_node, "attr1");
		if (!attr2.has_value()) reader.throw_missing(mpd::xml::node_type::attribute_node, "attr2");
		return *this;
	}
	void read_node(mpd::xml::element_reader& reader, mpd::xml::node_type type, std::string&&) {
		if (type == mpd::xml::node_type::element_node || type == mpd::xml::node_type::string_node)
			reader.throw_unexpected();
	}
	three end_element(mpd::xml::attribute_reader&) {
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
	{
		if (name == "attr1") {
			if (attr1.has_value()) reader.throw_unexpected();
			attr1.emplace(std::atoi(value.data())); // TODO add easy helper method to reader
		}
		else if (name == "attr2") {
			if (attr2.has_value()) reader.throw_unexpected();
			attr2.emplace(std::move(value));
		}
		else
			reader.throw_unexpected();
	}
	two_parser& start_element(mpd::xml::attribute_reader& reader) {
		if (!attr1.has_value()) reader.throw_missing(mpd::xml::node_type::attribute_node, "attr1");
		if (!attr2.has_value()) reader.throw_missing(mpd::xml::node_type::attribute_node, "attr2");
		return *this;
	}
	void read_node(mpd::xml::element_reader& reader, mpd::xml::node_type type, std::string&& content) {
		if (type == mpd::xml::node_type::element_node && content == "three")
			nodes.emplace_back(reader.read_element(three_parser{}));
		else if (type == mpd::xml::node_type::string_node) {
			if (content.find("_") != -1) reader.throw_invalid_content("two-class strings can't contain _");
			texts.push_back(content);
		} else if (type == mpd::xml::node_type::comment_node)
			std::cout << "two-comment: " << content << '\n';
		else if (type == mpd::xml::node_type::element_node)
			reader.throw_unexpected();
	}
	two end_element(mpd::xml::attribute_reader&) {
		return two{ *std::move(attr1), *std::move(attr2), std::move(nodes), std::move(texts) };
	}
};
struct one_parser {
	std::optional<int> attr1;
	std::optional<std::string> attr2;
	std::vector<two> nodes;

	one begin_element(mpd::xml::tag_reader& reader, const std::string&) 
	{ attr1.reset(); attr2.reset(); return reader.read_attributes(*this); }
	void read_attribute(mpd::xml::attribute_reader& reader, const std::string& name, std::string&& value)
	{
		if (name == "attr1") {
			if (attr1.has_value()) reader.throw_unexpected();
			attr1.emplace(std::atoi(value.data())); // TODO add easy helper method to reader
		}
		else if (name == "attr2") {
			if (attr2.has_value()) reader.throw_unexpected();
			attr2.emplace(std::move(value));
		}
		else
			reader.throw_unexpected();
	}
	one_parser& start_element(mpd::xml::attribute_reader& reader) {
		if (!attr1.has_value()) reader.throw_missing(mpd::xml::node_type::attribute_node, "attr1");
		if (!attr2.has_value()) reader.throw_missing(mpd::xml::node_type::attribute_node, "attr2");
		return *this;
	}
	void read_node(mpd::xml::element_reader& reader, mpd::xml::node_type type, std::string&& content) {
		if (type == mpd::xml::node_type::element_node && content == "two") {
			try {
				nodes.emplace_back(reader.read_element(two_parser{}));
			} catch (std::runtime_error& e) {
				std::cerr << "SUCCESSFULLY HANDLED ERROR: " << e.what() << '\n';
			}
		} else if (type == mpd::xml::node_type::string_node)
			std::cout << "one-str: " << content << '\n';
		else if (type == mpd::xml::node_type::element_node)
			reader.throw_unexpected();
	}
	one end_element(mpd::xml::attribute_reader&) {
		return one{ *std::move(attr1), *std::move(attr2), std::move(nodes) };
	}
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

	mpd::xml::document_reader parser("test literal", std::begin(buffer), std::end(buffer));
	one data{};
    try {
		data = parser.read_element("one", one_parser{});
    } catch(const std::exception& exc) {
        std::cerr << "INVALID READ: "<<exc.what()<<'\n';
    }

    //parser.readRoot("ted", main);

    return 0;
}
