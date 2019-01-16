#pragma once
#define _CRT_NONSTDC_NO_DEPRECATE
#include "type_erased.hpp"
#include <cassert>
#include <cctype>
#include <climits>
#include <cstring>
#include <algorithm>
#include <iterator>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER 
#define noinline(RETURN) __declspec(noinline) RETURN
#else
#define noinline(RETURN) RETURN __attribute__ ((noinline))
#endif

namespace mpd {
	static inline std::string_view ltrim(std::string_view s) {
		std::string_view::const_iterator last = std::find_if(s.begin(), s.end(), [](int ch) {
			return !std::isspace(ch);
		});
		return std::string_view(s.data(), last - s.begin());
	}
	static inline std::string_view rtrim(std::string_view s) {
		std::string_view::const_reverse_iterator first = std::find_if(s.rbegin(), s.rend(), [](int ch) {
			return std::isspace(ch);
		});
		std::size_t drop = first.base() - s.begin();
		return std::string_view(s.data() + drop, s.length() - drop);
	}
	static inline std::string_view trim(std::string_view s) {
		return ltrim(rtrim(s));
	}

	namespace xml {
		using namespace std::literals::string_literals;
		/**
		Lingo:
		Tag - The name and attributes within a <> pair. Open tags start an Element, and a close
			tag ends the Element.  Elemends ending in  /> are considered an open Tag immediately
			followed by a close Tag.
		Element - A Node started by an open Tag, and closed by a close Tag.
		Node - An Element, Whitespace, String, Comment, CDATA, or Processing Instruction.

		To keep this accurate but easy to use, whitespace around tags is considered a separate
		Node from the String contents within an Element.

		This is a superfast, lightweight parser of UTF8 xml. You create a document_reader with
		iterators containing the content, and then call readDocument(...) and pass in a Parser
		object, to parse the contents of the document. If all you care about is the only 'Node'
		in the document, you can pass a document_root_parser, and pass in the  expected tag name
		and Parser object for the child Nodes.
		A Parser passed to readDocument or readChild is a class that fullfills one
		attribute_parser_t requirements or the element_parser_t requirements, or both. The separate
		attribute_parser_t is useful for classes that whos constructors have parameters that will be
		fullfilled with attributes in the node tag, but is then mutable for processing child Nodes.
		The parser classes have their methods called, receiving the attributes and then child
		Nodes. Finally, end_element(...) is called, and the parser should return the fully parsed
		object. The methods on these parsers are detailed below. These will almost always be a
		single object togeather, but there may be a case where having them separate is useful, so
		I've left them as such.
		Any of these methods may call throw_unexpected, throw_missing, or throw_invalid_content on
		the reader parameter, if they  receive an unexpected parameter, are missing an expected
		parameter, or the content of a parameter is invalid. The parser will throw the exception,
		containing the line number, column offset, parameters, and callstack.

		// For processing a tag name. All members optional if also an attribute_parser_t.
		// This can be useful for parsing things like `vector<unique_ptr<interface>>` where the xml
		// may have differing implementations inside the vector contents.
		// It can also be useful for wrapping the parsing of an entire element in a try/catch block.
		interface attribute_tag_t {
			// Optional. Called when the parser is parsing a new tag. May be used to defer parsing
			// to a specific parser based on the tag.
			// If the parser is also a element_parser_t, this ends up being the same return type.
			element_type begin_element(tag_reader& reader, const std::string& tag_id)
			{ return reader.read_attributes(*this); }
		}
		// For processing a Tag's Attributes. All members optional if also an element_parser_t.
		interface attribute_parser_t {
			// Optional. Called or each tag. If this is not provided, but there is a parameter,
			// the parser will throw an unexpected_node_exception. Overrides are encouraged to
			// move from the name and value parameters rather than copying.
			void read_attribute(attribute_reader& reader, const std::string& name, std::string&& value)
			{ reader.throw_unexpected(); }
			// Optional IFF the Parser also fullfills element_parser_t. Called when there are no more
			// attributes. It should return the parser to use for parsing child Nodes.
			virtual element_parser_t begin_content(base_reader& reader) {
				return *this;
			}
		}
		// For processing the child Nodes of an Element.
		interface element_parser_t {
			// Optional. Called when the parser is parsing a new child element. Caller should check
			// the type and tag_name. You must either call reader.readChild(...) and pass a parser
			// for the child, call one of the reader.throw methods, or throw an exception yourself.
			// If this is not provided, but there is a child element, the parser will throw an
			// unexpected_node_exception. 
			virtual void read_child_element(element_reader& reader, const std::string& child_tag) {
				if (child_tag == "thing") YOURCODEHERE(reader.read_element(ChildParserType{});
				else reader.throw_unexpected();
			}
			// Optional. Called when the parser is parsing a new non-element child node. Caller should
			// check the type and tag_name. There are no special requirements. Overrides are
			// encouraged to move from the content parameter rather than copying. If this is not 
			// provided, but there is a non-whitespace string, the parser will throw an 
			// unexpected_node_exception. 
			virtual void read_child_node(base_reader& reader, node_type type, std::string&& content) {
				if (type == node_type::string_node) YOURCODEHERE(mpd::xml::trim(content));
			}
			// Required. This is called when the close Tag is reached. This returns the parsed
			// information to the parent Element Parser. Presumably this should return the parsed
			// type, but can return a builder or similar, as long as the parent Element parser is
			// expecting to receive that type.
			// If the parser is also a attribute_tag_t, this ends up being the same return type.
			virtual element_type end_element(base_reader& reader) {
				return element;
			}
		}

		// There are many sample Parsers in std_xml_parsers.hpp
		**/

		//Exception for if you call processor.read_child_node from from a finalizer or readXml_attribute
		struct invalid_read_call_error : public std::logic_error {
			invalid_read_call_error(const char* message) : std::logic_error(message) {}
			invalid_read_call_error(const std::string& message) : std::logic_error(message) {}
			invalid_read_call_error(std::string&& message) : std::logic_error(std::move(message)) {}
		};
		struct invalid_parser_error : public std::logic_error {
			invalid_parser_error(const char* message) : std::logic_error(message) {}
			invalid_parser_error(const std::string& message) : std::logic_error(message) {}
			invalid_parser_error(std::string&& message) : std::logic_error(std::move(message)) {}
		};
		//generic bottom level Xml Exception
		struct invalid_xml : public std::runtime_error {
			invalid_xml(const char* message) : std::runtime_error(message) {}
			invalid_xml(const std::string& message) : std::runtime_error(message) {}
			invalid_xml(std::string&& message) : std::runtime_error(std::move(message)) {}
		};
		//Exception for parser to throw when input isn't valid Xml regardless of schema or contents
		struct malformed_xml : public invalid_xml {
			malformed_xml(const char* message) : invalid_xml(message) {}
			malformed_xml(const std::string& message) : invalid_xml(message) {}
			malformed_xml(std::string&& message) : invalid_xml(std::move(message)) {}
		};
		struct duplicate_attribute : public malformed_xml {
			duplicate_attribute(const char* message) : malformed_xml(message) {}
			duplicate_attribute(const std::string& message) : malformed_xml(message) {}
			duplicate_attribute(std::string&& message) : malformed_xml(std::move(message)) {}
		};
		struct unexpeced_eof : public malformed_xml {
			unexpeced_eof(const char* message) : malformed_xml(message) {}
			unexpeced_eof(const std::string& message) : malformed_xml(message) {}
			unexpeced_eof(std::string&& message) : malformed_xml(std::move(message)) {}
		};
		//exceptions for user to throw for invalid content, including nodes or attributes
		struct invalid_content : public invalid_xml {
			invalid_content(const char* message) : invalid_xml(message) {}
			invalid_content(const std::string& message) : invalid_xml(message) {}
			invalid_content(std::string&& message) : invalid_xml(std::move(message)) {}
		};
		struct unexpected_node : public invalid_content {
			unexpected_node(const char* message) : invalid_content(message) {}
			unexpected_node(const std::string& message) : invalid_content(message) {}
			unexpected_node(std::string&& message) : invalid_content(std::move(message)) {}
		};
		struct missing_node : public invalid_content {
			missing_node(const char* message) : invalid_content(message) {}
			missing_node(const std::string& message) : invalid_content(message) {}
			missing_node(std::string&& message) : invalid_content(std::move(message)) {}
		};

		enum class node_type { attribute_node, processing_node, element_node, string_node, comment_node };

		namespace impl { class reader; }

		class base_reader {
		public:
			//Get the current Location
			std::string get_location_for_exception();
			//You can call this to throw a unexpected_node with the current line number and offset and such.
			[[noreturn]] void throw_unexpected(const char* details = nullptr);
			[[noreturn]] void throw_unexpected(const std::string& details) { throw_unexpected(details.c_str()); }
			//You can call this to throw a missing_node with the current line number and offset and such.
			[[noreturn]] void throw_missing(node_type type, const char* name, const char* details = nullptr);
			[[noreturn]] void throw_missing(const std::string& details) { throw_missing(details.c_str()); }
			//You can call this to throw a invalid_content with the current line number and offset and such.
			[[noreturn]] void throw_invalid_content(const char* details = nullptr);
			[[noreturn]] void throw_invalid_content(const std::string& details) { throw_invalid_content(details.c_str()); }
			base_reader(const base_reader&) = delete;
			base_reader& operator=(const base_reader&) = delete;
		protected:
			base_reader(impl::reader& reader) :reader_(&reader) {}
			impl::reader* reader_;
		};

		class tag_reader : public base_reader {
		public:
			template<class element_parser_t> auto read_attributes(element_parser_t&& parser);
		protected:
			tag_reader(impl::reader& reader) :base_reader(reader) {}
		};

		class attribute_reader : public base_reader {
		public:
			attribute_reader(impl::reader& reader) :base_reader(reader) {}
		};

		class element_reader : public base_reader {
		public:
			template<class element_parser_t> auto read_element(element_parser_t&& parser);
		protected:
			element_reader(impl::reader& reader) : base_reader(reader) {}
		};
	}
}
#include "xml_reader_impl.hpp"
namespace mpd {
	namespace xml {
		inline void base_reader::throw_unexpected(const char * details)
		{ reader_->throw_unexpected(details); }
		inline void base_reader::throw_missing(node_type type, const char * name, const char * details)
		{ reader_->throw_missing(type, name, details); }
		inline void base_reader::throw_invalid_content(const char * details)
		{ reader_->throw_invalid_content(details); }
		template<class attribute_parser_t>
		inline auto tag_reader::read_attributes(attribute_parser_t&& parser)
		{ return reader_->read_attributes(std::forward<attribute_parser_t>(parser)); }
		template<class element_parser_t>
		inline auto element_reader::read_element(element_parser_t&& parser)
		{ return reader_->read_element(std::forward<element_parser_t>(parser), impl::special_{}); }

		struct IgnoredXmlParser {
			void read_attribute(attribute_reader&, const std::string&, std::string) {}
			void read_child_element(element_reader& reader, const std::string& ) {reader.read_element(*this); }
			void read_child_node(base_reader&, node_type, std::string&&) { }
			std::nullptr_t end_element(base_reader&) { return nullptr; }
		};

		template<class element_parser_t>
		struct document_root_parser {
			typedef decltype(std::declval<element_reader>().read_element(std::declval<element_parser_t>())) return_type;
			return_type begin_tag(tag_reader& reader)
			{ child.reset(); return reader.begin_tag(); }
			void read_child_element(element_reader& reader, const std::string& tag) {
				if ((tag == child_tag_ || child_tag_==nullptr) && !child.has_value())
					child.emplace(reader.read_element(child_parser_));
				else reader.throw_unexpected("unexpected root element " + tag);
			}
			return_type end_element(base_reader&) { return std::move(child).value(); }
			document_root_parser(const char* child_tag, element_parser_t child_parser)
				: child_tag_(child_tag), child_parser_(child_parser) {}
			document_root_parser(const document_root_parser&) = delete;
			document_root_parser& operator=(const document_root_parser&) = delete;
		private:
			const char* child_tag_;
			element_parser_t child_parser_;
			std::optional<return_type> child;
		};

		struct document_reader {
			template<class forward_it>
			explicit document_reader(std::string&& source_name, forward_it begin, forward_it end) :reader_(std::move(source_name), std::in_place_type_t<impl::read_buf_impl<forward_it>>{}, begin, end) {}
			document_reader(const document_reader& nocopy) = delete;
			document_reader& operator=(const document_reader& nocopy) = delete;
			template<class document_parser_t> auto read_document(document_parser_t&& parser) 
			{ return reader_.read_contents(std::forward<document_parser_t>(parser)); }
			template<class element_parser_t> auto read_element(const char* tag, element_parser_t&& parser)
			{ return reader_.read_contents(document_root_parser(tag, std::forward<element_parser_t>(parser))); }
		private:
			impl::reader reader_;

		};

		template<class tag_parser_t>
		struct parser_output {
			typedef decltype(std::declval<tag_parser_t>().read_element(std::declval<attribute_parser_t>())) type;
		};
	}
}