#pragma once
#include "xml_reader.hpp"
#include <limits>
#include <optional>

namespace mpd {
	namespace xml {
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<std::string>& attribute, const std::string& found_attribute, std::string&& value)
		{
			if (found_attribute != desired_attribute) return false;
			if (attribute.has_value()) reader.throw_unexpected("duplicate attribute "s + desired_attribute);
			attribute.emplace(std::move(value));
			return true;
		}

		template<class R, R F(const char*, char**, int), class T>
		bool try_read_int_attribute_helper(attribute_reader& reader, const char* desired_attribute, std::optional<T>& attribute, const std::string& found_attribute, std::string&& value) {
			if (found_attribute != desired_attribute) return false;
			if (attribute.has_value()) reader.throw_unexpected("duplicate attribute "s + desired_attribute);
			char* end = 0;
			R temp = F(value.c_str(), &end, 10);
			if (end != value.data() + value.length())
				reader.throw_invalid_content("could not parse entire input for "s + desired_attribute);
			if (temp > std::numeric_limits<T>::max() || temp < std::numeric_limits<T>::min())
				reader.throw_invalid_content("out of range for "s + desired_attribute);
			attribute.emplace(static_cast<T>(temp));
			return true;
		}
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<char>& att, const std::string& found_attribute, std::string&& value) 
		{ return try_read_int_attribute_helper<long, std::strtol>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<signed char>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<long, std::strtol>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<short>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<long, std::strtol>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<int>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<long, std::strtol>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<long>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<long, std::strtol>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<long long>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<long long, std::strtoll>(reader, desired_attribute, att, found_attribute, std::move(value)); }

		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<unsigned char>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<unsigned long, std::strtoul>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<unsigned short>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<unsigned long, std::strtoul>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<unsigned int>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<unsigned long, std::strtoul>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<unsigned long>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<unsigned long, std::strtoul>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<unsigned long long>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_int_attribute_helper<unsigned long long, std::strtoull>(reader, desired_attribute, att, found_attribute, std::move(value)); }

		template<class T, T F(const char*, char**)>
		bool try_read_float_attribute_helper(attribute_reader& reader, const char* desired_attribute, std::optional<T>& attribute, const std::string& found_attribute, std::string&& value) {
			if (found_attribute != desired_attribute) return false;
			if (attribute.has_value()) reader.throw_unexpected("duplicate attribute "s + desired_attribute);
			char* end = 0;
			T temp = F(value.c_str(), &end);
			if (end != value.data() + value.length())
				reader.throw_invalid_content("could not parse entire input for "s + desired_attribute);
			attribute.emplace(temp);
			return true;
		}
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<float>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_float_attribute_helper<float, std::strtof>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<double>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_float_attribute_helper<double, std::strtod>(reader, desired_attribute, att, found_attribute, std::move(value)); }
		bool try_parse_attribute(attribute_reader& reader, const char* desired_attribute, std::optional<long double>& att, const std::string& found_attribute, std::string&& value)
		{ return try_read_float_attribute_helper<long double, std::strtold>(reader, desired_attribute, att, found_attribute, std::move(value)); }

		struct read_element {
			attribute_reader& reader_;
			const std::string& found_attribute_;
			std::string&& value_;
			bool done;
			read_element(attribute_reader& reader, const std::string& found_attribute, std::string&& value) 
				:reader_(reader), found_attribute_(found_attribute), value_(std::move(value)), done(false) {}
			template<class T>
			read_element& operator()(const char* desired_attribute, std::optional<T>& attribute)
			{ done = done || try_parse_attribute(reader_, desired_attribute, attribute, found_attribute_, std::move(value_)); return *this; }
			~read_element() { if (!done && !std::uncaught_exceptions()) reader_.throw_unexpected("unexpected attribute "s + found_attribute_); }
		};

		struct require_attributes {
			attribute_reader& reader_;
			require_attributes(attribute_reader& reader) :reader_(reader) {}
			template<class T>
			require_attributes& operator()(const char* name, const std::optional<T>& attribute)
			{ if (!attribute.has_value()) reader_.throw_missing(mpd::xml::node_type::attribute_node, name); return *this;}
		};
	}
}