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
		This is a superfast, super flexible, lightweight parser of UTF8 xml. 
		(This means the interface is not untuitive).
		One of the subtler design goals is to minimize template explosion. Each template method
		is as small as possible, and virtually all interesting code is in non-template methods.

		Lingo:
		Tag - The name and attributes within a <> pair. Open tags start an Element, and a close
			tag ends the Element.  Elemends ending in  /> are considered an open Tag immediately
			followed by a close Tag.
		Element - A Node started by an open Tag, and closed by a close Tag.
		Node - An Element, Whitespace, String, Comment, CDATA, or Processing Instruction.

		To begin, construct a mpd::xml::document_reader from a filename and bidirectional iterators
		for the content. Then one normally calls document_reader#read_document and passes in a 
		document_parser_t, and the method will return the fully parsed document. Alternatively,
		if all you care about is the only Element, you may call document_reader#element_document,
		passing a tag name and a element_parser_t, returning the parsed Element directly.
		
		Parsers interpret the attributes and tags for specific data types. Theoretically there
		are three parser contracts, but the majority of the time they are all implemented by
		a single object. 

		Any parser methods may call parser#throw_unexpected, parser#throw_missing, or 
		parser#throw_invalid_content on the reader parameter, if they receive an unexpected 
		parameter, are missing an expected parameter, or the content of a parameter is invalid. 
		The parser will throw the exception, containing the line number, column offset, parameters, 
		and a "callstack".

		// For processing a child Element. 
		// This can be useful for parsing things like `vector<unique_ptr<interface>>` where the xml
		// may have differing implementations inside the vector contents, while avoiding dynamic
		// dispatch.
		// It can also be useful for wrapping the parsing of an entire element in a try/catch block.
		interface child_parser_t {
			// Called right before an element contains a tag.
			// This is useful for resetting all state, between parsed elements
			// This member is optional: It uses the default implementation shown below.
			void reset()
			{ }

			// Called when an element contains a tag.
			// This method must call reader.read_element(tag_parser_t) to parse the element, and
			// then return the fully parsed element type to the parent element_parser.
			// (or as always, throw an exception)
			// This member is optional: It uses the default implementation shown below, and therefore 
			// the parser must also implement tag_parser_t.
			element_type parse_tag(tag_reader& reader, const std::string& tag_id)
			{ return reader.read_element(*this); }
		}

		// For processing a Tag
		interface tag_parser_t {
			// Called for each attribute in a tag.
			// This member is optional:  It uses the default implementation shown below, and therefore 
			// throws if an attribute is encountered.
			void parse_attribute(attribute_reader& reader, const std::string& name, std::string&& value)
			{ reader.throw_unexpected(); }
			// Called when a Tag is complete and we're about to parse children.
			// This is rarely useful, but can be handy if you do not know the most derived type until
			// after parsing all of the attributes. 
			// This member is optional: It uses the default implementation shown below, and therefore 
			// the parser must also implement element_parser_t.
			virtual element_parser_t parse_content(base_reader& reader) {
				return *this;
			}
		}
		// For processing the child Nodes of an Element.
		interface element_parser_t {
			// Called for each child Node of an Element.
			// An implementation must check the child_tag, and then call 
			// reader#read_child(child_parser_t), and process the parsed value.
			// (or as always, throw an exception)
			// If the child_tag is not expected, it should call reader.throw_unexpected()
			// This member is optional:  It uses the default implementation shown below, and therefore 
			// throws if a child node is encountered.
			void parse_child_element(element_reader& reader, const std::string& child_tag) {
				reader.throw_unexpected();
			}
			// Called when the parser is parsing a new non-element child node. 
			// The Caller should check the node_type. There are no special requirements. Overrides are
			// encouraged to move from the content parameter rather than copying. 
			// This member is optional:  It uses the default implementation shown below, and therefore
			// throws if a node besides an empty string is found.
			void parse_child_node(base_reader& reader, node_type type, std::string&& content) {
				if (type != node_type::string_node || !mpd::xml::trim(content).isEmpty())
					reader.throw_unexpected();
			}
			// This is called when the close Tag is reached, and the element is fully parsed.
			// Usually this returns a fully parsed object to the child_parser_t#parse_tag method. 
			// Presumably this should return the parsed type, but can return a builder or similar, as 
			// long as parse_tag is expecting to receive that type.
			virtual element_type end_parse(base_reader& reader) {
				return element;
			}
		}
		**/

		//Exception for if you call processor.parse_child_node from from a finalizer or readXml_attribute
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
			template<class element_parser_t> auto read_element(element_parser_t&& parser);
		protected:
			tag_reader(impl::reader& reader) :base_reader(reader) {}
		};

		class attribute_reader : public base_reader {
		public:
			attribute_reader(impl::reader& reader) :base_reader(reader) {}
		};

		class element_reader : public base_reader {
		public:
			template<class element_parser_t> auto read_child(element_parser_t&& parser);
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
		template<class tag_parser_t>
		inline auto tag_reader::read_element(tag_parser_t&& parser)
		{ return reader_->read_element(std::forward<tag_parser_t>(parser)); }
		template<class element_parser_t>
		inline auto element_reader::read_child(element_parser_t&& parser)
		{ return reader_->call_parse_tag(std::forward<element_parser_t>(parser), impl::special_{}); }

		struct IgnoredXmlParser {
			void parse_attribute(attribute_reader&, const std::string&, std::string) {}
			void parse_child_element(element_reader& reader, const std::string& ) {reader.read_child(*this); }
			void parse_child_node(base_reader&, node_type, std::string&&) { }
			std::nullptr_t end_parse(base_reader&) { return nullptr; }
		};

		template<class element_parser_t>
		struct document_root_parser {
			typedef decltype(std::declval<element_reader>().read_child(std::declval<element_parser_t>())) return_type;
			return_type begin_tag(tag_reader& reader)
			{ child.reset(); return reader.begin_tag(); }
			void parse_child_element(element_reader& reader, const std::string& tag) {
				if ((tag == child_tag_ || child_tag_==nullptr) && !child.has_value())
					child.emplace(reader.read_child(child_parser_));
				else reader.throw_unexpected("unexpected root element " + tag);
			}
			return_type end_parse(base_reader&) { return std::move(child).value(); }
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
			explicit document_reader(std::string&& source_name, forward_it begin, forward_it end)
				:reader_(std::move(source_name), std::in_place_type_t<impl::read_buf_impl<forward_it>>{}, begin, end) 
{}
			document_reader(const document_reader& nocopy) = delete;
			document_reader& operator=(const document_reader& nocopy) = delete;
			template<class document_parser_t> auto read_document(document_parser_t&& parser) 
			{ return reader_.read_contents(std::forward<document_parser_t>(parser)); }
			template<class element_parser_t> auto read_child(const char* tag, element_parser_t&& parser)
			{ return reader_.read_contents(document_root_parser(tag, std::forward<element_parser_t>(parser))); }
		private:
			impl::reader reader_;

		};

		template<class child_parser_t>
		struct parser_output {
			typedef decltype(std::declval<child_parser_t>().read_element(std::declval<tag_parser_t>())) type;
		};
	}
}