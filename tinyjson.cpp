#include "tinyjson.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string_view>
#include <utility>

namespace tinyjson
{
/* Utility to jump whitespace and cr/lf */
const char* skip(const char* in)
{
    while(in && *in && (unsigned char)*in <= 32)
        in++;
    return in;
}

/* Parse the input text into an unescaped cstring, and populate item. */
thread_local const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

/// escape `in` to a printable version
std::string& escape_string(const std::string_view& str, std::string* escaped)
{
    const char* ptr = nullptr;
    char *ptr2 = nullptr, *out = nullptr;
    int len = 0;
    unsigned char token;

    if(str.empty()) {
        return *escaped;
    }

    ptr = str.data();
    while((token = *ptr) && ++len) {
        if(strchr("\"\\\b\f\n\r\t", token))
            len++;
        else if(token < 32)
            len += 5;
        ptr++;
    }

    escaped->resize(len + 2);
    out = escaped->data();

    ptr2 = out;
    ptr = str.data();
    *ptr2++ = '\"';
    while(*ptr) {
        if((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\')
            *ptr2++ = *ptr++;
        else {
            *ptr2++ = '\\';
            switch(token = *ptr++) {
            case '\\':
                *ptr2++ = '\\';
                break;
            case '\"':
                *ptr2++ = '\"';
                break;
            case '\b':
                *ptr2++ = 'b';
                break;
            case '\f':
                *ptr2++ = 'f';
                break;
            case '\n':
                *ptr2++ = 'n';
                break;
            case '\r':
                *ptr2++ = 'r';
                break;
            case '\t':
                *ptr2++ = 't';
                break;
            default:
                snprintf(ptr2, 6, "u%04x", token);
                ptr2 += 6;
                break; /* escape and print */
            }
        }
    }
    *ptr2++ = '\"';
    return *escaped;
}

const char* element::parse_string(tinyjson::element* item, const char* str)
{
    const char* ptr = str + 1;
    char* ptr2;
    int len = 0;
    unsigned uc, uc2;
    if(*str != '\"') {
        return nullptr;
    } /* not a string! */

    while(*ptr != '\"' && *ptr && ++len)
        if(*ptr++ == '\\')
            ptr++; /* Skip escaped quotes. */

    char* out = (char*)malloc(len + 1);
    ptr = str + 1;
    ptr2 = out;

    while(*ptr != '\"' && *ptr) {
        if(*ptr != '\\')
            *ptr2++ = *ptr++;
        else {
            ptr++;
            switch(*ptr) {
            case 'b':
                *ptr2++ = '\b';
                break;
            case 'f':
                *ptr2++ = '\f';
                break;
            case 'n':
                *ptr2++ = '\n';
                break;
            case 'r':
                *ptr2++ = '\r';
                break;
            case 't':
                *ptr2++ = '\t';
                break;
            case 'u': /* transcode utf16 to utf8. */
                sscanf(ptr + 1, "%4x", &uc);
                ptr += 4; /* get the unicode char. */

                if((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0)
                    break; // check for invalid.

                if(uc >= 0xD800 && uc <= 0xDBFF) // UTF16 surrogate pairs.
                {
                    if(ptr[1] != '\\' || ptr[2] != 'u')
                        break; // missing second-half of surrogate.
                    sscanf(ptr + 3, "%4x", &uc2);
                    ptr += 6;
                    if(uc2 < 0xDC00 || uc2 > 0xDFFF)
                        break; // invalid second-half of surrogate.
                    uc = 0x10000 | ((uc & 0x3FF) << 10) | (uc2 & 0x3FF);
                }

                len = 4;
                if(uc < 0x80)
                    len = 1;
                else if(uc < 0x800)
                    len = 2;
                else if(uc < 0x10000)
                    len = 3;
                ptr2 += len;

                switch(len) {
                case 4:
                    *--ptr2 = ((uc | 0x80) & 0xBF);
                    uc >>= 6;
                case 3:
                    *--ptr2 = ((uc | 0x80) & 0xBF);
                    uc >>= 6;
                case 2:
                    *--ptr2 = ((uc | 0x80) & 0xBF);
                    uc >>= 6;
                case 1:
                    *--ptr2 = (uc | firstByteMark[len]);
                }
                ptr2 += len;
                break;
            default:
                *ptr2++ = *ptr;
                break;
            }
            ptr++;
        }
    }
    *ptr2 = 0;
    if(*ptr == '\"')
        ptr++;

    item->m_value.str = out;
    item->m_kind = tinyjson::element_kind::T_STRING;
    return ptr;
}

element::~element()
{
    m_elements_map.clear();
    m_children.clear();

    if(m_kind == element_kind::T_STRING && m_value.str) {
        free(m_value.str);
        m_value.str = nullptr;
    }
}

element::element() { memset(&m_value, 0, sizeof(m_value)); }
element::element(element&& other)
{
    m_kind = other.m_kind;
    m_elements_map.clear();
    m_property_name = std::move(other.m_property_name);
    m_value = other.m_value;
    if(other.m_kind == element_kind::T_STRING) {
        // ensure that no double free is occured
        other.m_value.str = nullptr;
    }
    m_children = std::move(other.m_children);
    m_elements_map = std::move(other.m_elements_map);
}

/* Parse the input text to generate a number, and populate the result into item. */
const char* element::parse_number(tinyjson::element* item, const char* num)
{
    double n = 0, sign = 1, scale = 0;
    int subscale = 0, signsubscale = 1;

    /* Could use sscanf for this? */
    if(*num == '-')
        sign = -1, num++; /* Has sign? */
    if(*num == '0')
        num++; /* is zero */
    if(*num >= '1' && *num <= '9')
        do
            n = (n * 10.0) + (*num++ - '0');
        while(*num >= '0' && *num <= '9'); /* Number? */
    if(*num == '.' && num[1] >= '0' && num[1] <= '9') {
        num++;
        do
            n = (n * 10.0) + (*num++ - '0'), scale--;
        while(*num >= '0' && *num <= '9');
    }                              /* Fractional part? */
    if(*num == 'e' || *num == 'E') /* Exponent? */
    {
        num++;
        if(*num == '+')
            num++;
        else if(*num == '-')
            signsubscale = -1, num++; /* With sign? */
        while(*num >= '0' && *num <= '9')
            subscale = (subscale * 10) + (*num++ - '0'); /* Number? */
    }

    n = sign * n * pow(10.0, (scale + subscale * signsubscale)); /* number = +/- number.fraction * 10^+/- exponent */

    item->m_kind = tinyjson::element_kind::T_NUMBER;
    item->m_value.number = n;
    return num;
}

/* Build an array from input text. */
const char* element::parse_array(tinyjson::element* item, const char* value)
{
    if(*value != '[') {
        return nullptr;
    } /* not an array! */

    item->m_kind = tinyjson::element_kind::T_ARRAY;
    value = skip(value + 1);
    if(*value == ']')
        return value + 1; /* empty array. */

    auto& child = item->append_new();

    value = skip(parse_value(&child, skip(value))); /* skip any spacing, get the value. */
    if(!value)
        return nullptr;

    while(*value == ',') {
        auto& child = item->append_new();
        value = skip(parse_value(&child, skip(value + 1)));
        if(!value)
            return nullptr; /* memory fail */
    }

    if(*value == ']')
        return value + 1; /* end of array */

    return nullptr; /* malformed. */
}

/* Build an object from the text. */
const char* element::parse_object(tinyjson::element* item, const char* value)
{
    if(*value != '{') {
        return nullptr;
    } // not an object

    item->m_kind = tinyjson::element_kind::T_OBJECT;
    value = skip(value + 1);
    if(*value == '}')
        return value + 1; // empty object

    auto& child = item->append_new();
    value = skip(parse_string(&child, skip(value)));

    if(!value) {
        return nullptr;
    }

    // parse the property name
    child.m_property_name = child.m_value.str;
    free(child.m_value.str);
    child.m_value.str = nullptr;

    if(*value != ':') {
        // parse error
        return nullptr;
    }

    // parse the property value
    value = skip(parse_value(&child, skip(value + 1))); /* skip any spacing, get the value. */
    if(!value)
        return nullptr;

    while(*value == ',') {
        auto& child = item->append_new();
        value = skip(parse_string(&child, skip(value + 1)));

        if(!value) {
            return nullptr;
        }

        child.m_property_name = child.m_value.str;
        free(child.m_value.str);
        child.m_value.str = nullptr;
        if(*value != ':') {
            // parse error
            return nullptr;
        }

        // parse the property value
        value = skip(parse_value(&child, skip(value + 1))); /* skip any spacing, get the value. */
        if(!value)
            return nullptr;
    }

    if(*value == '}')
        return value + 1; /* end of array */
    return nullptr;       /* malformed. */
}

const char* element::parse_value(tinyjson::element* item, const char* value)
{
    if(!value)
        return nullptr; /* Fail on null. */
    if(!strncmp(value, "null", 4)) {
        item->m_kind = tinyjson::element_kind::T_NULL;
        return value + 4;
    }

    if(!strncmp(value, "false", 5)) {
        item->m_kind = tinyjson::element_kind::T_FALSE;
        return value + 5;
    }

    if(!strncmp(value, "true", 4)) {
        item->m_kind = tinyjson::element_kind::T_TRUE;
        return value + 4;
    }

    if(*value == '\"') {
        return parse_string(item, value);
    }

    if(*value == '-' || (*value >= '0' && *value <= '9')) {
        return parse_number(item, value);
    }

    if(*value == '[') {
        return parse_array(item, value);
    }

    if(*value == '{') {
        return parse_object(item, value);
    }

    return nullptr; /* failure. */
}

void element::index_elements()
{
    if(!m_children.empty()) {
        m_elements_map.reserve(m_children.size());
        for(auto& child : m_children) {
            if(child.property_name()) {
                m_elements_map.emplace(std::make_pair(child.property_name(), &child));
            }
        }
    }
}

bool element::create_array(element* arr)
{
    arr->m_kind = element_kind::T_ARRAY;
    return true;
}

bool element::create_object(element* obj)
{
    obj->m_kind = element_kind::T_OBJECT;
    return true;
}

bool element::parse(const std::string& content, element* root)
{
    if(!element::parse_value(root, skip(content.c_str()))) {
        return false;
    }
    return true;
}

bool element::parse_file(const std::string& path, element* root)
{
    // read the file content
    std::string content;
    FILE* file = fopen(path.c_str(), "rb");
    // Check if there was an error.
    if(file == nullptr) {
        return false;
    }

    // Get the file length
    fseek(file, 0, SEEK_END);
    auto length = ftell(file);
    fseek(file, 0, SEEK_SET);

    content.resize(length);
    // Set the contents of the string.
    size_t bytes = fread(content.data(), sizeof(char), length, file);

    // no need for the file pointer any more, close it
    fclose(file);

    // did we read all the file?
    if(bytes != length) {
        return false;
    }

    return parse(content, root);
}

thread_local element null_element;

const element& element::operator[](const char* index) const
{
    if(m_children.empty()) {
        return null_element;
    }

    if(m_elements_map.empty()) {
        // need to index it first
        const_cast<element&>(*this).index_elements();
    }

    if(m_elements_map.count(index) == 0) {
        return null_element;
    }
    return *m_elements_map.find(index)->second;
}

element& element::operator[](const char* index)
{
    if(m_children.empty()) {
        return null_element;
    }

    if(m_elements_map.empty()) {
        // need to index it first
        const_cast<element&>(*this).index_elements();
    }

    if(m_elements_map.count(index) == 0) {
        return null_element;
    }
    auto& elem = *m_elements_map.find(index)->second;
    return elem;
}

const element& element::operator[](size_t index) const
{
    if(index >= m_children.size()) {
        return null_element;
    }
    return m_children[index];
}

element& element::operator[](size_t index)
{
    if(index >= m_children.size()) {
        return null_element;
    }
    return m_children[index];
}

element& element::add_property_internal(const std::string& name)
{
    auto& elem = append_new();
    elem.m_property_name = name;

    // add new indexed entry
    if(elem.property_name()) {
        m_elements_map.insert({ elem.property_name(), &elem });
    }
    return elem;
}

element& element::add_element(element&& elem)
{
    m_children.emplace_back(std::move(elem));
    auto& item_added = m_children.back();
    if(item_added.property_name()) {
        m_elements_map.insert({ item_added.property_name(), &item_added });
    }
    return item_added;
}

element& element::add_array(const std::string& name)
{
    element arr;
    create_array(&arr);
    arr.m_property_name = name;
    return add_element(std::move(arr));
}

element& element::add_object(const std::string& name)
{
    element obj;
    create_object(&obj);
    obj.m_property_name = name;
    return add_element(std::move(obj));
}

element& element::add_property(const std::string& name, int value)
{
    return add_property(name, static_cast<double>(value));
}

element& element::add_property(const std::string& name, long value)
{
    return add_property(name, static_cast<double>(value));
}

element& element::add_property(const std::string& name, size_t value)
{
    return add_property(name, static_cast<double>(value));
}

element& element::add_property(const std::string& name, double value)
{
    auto& elem = add_property_internal(name);
    elem.m_value.number = value;
    elem.m_kind = element_kind::T_NUMBER;
    return *this;
}

element& element::add_property(const std::string& name, const std::string& value)
{
    auto& elem = add_property_internal(name);
    elem.m_value.str = strdup(value.c_str());
    elem.m_kind = element_kind::T_STRING;
    return *this;
}

element& element::add_property(const std::string& name, const char* value)
{
    auto& elem = add_property_internal(name);
    elem.m_value.str = strdup(value);
    elem.m_kind = element_kind::T_STRING;
    return *this;
}

element& element::add_property(const std::string& name, bool b)
{
    auto& elem = add_property_internal(name);
    elem.m_value.boolean = b;
    elem.m_kind = b ? element_kind::T_TRUE : element_kind::T_FALSE;
    return *this;
}

element& element::add_property_null(const std::string& name)
{
    auto& elem = add_property_internal(name);
    elem.m_kind = element_kind::T_NULL;
    return *this;
}

element& element::add_array_item(const std::string& value)
{
    auto& elem = add_property_internal("");
    elem.m_kind = element_kind::T_STRING;
    elem.m_value.str = strdup(value.c_str());
    return *this;
}

element& element::add_array_item(const char* value)
{
    auto& elem = add_property_internal("");
    elem.m_kind = element_kind::T_STRING;
    elem.m_value.str = strdup(value);
    return *this;
}

element& element::add_array_item(double value)
{
    auto& elem = add_property_internal("");
    elem.m_kind = element_kind::T_NUMBER;
    elem.m_value.number = value;
    return *this;
}

element& element::add_array_item(bool value)
{
    auto& elem = add_property_internal("");
    elem.m_kind = value ? element_kind::T_TRUE : element_kind::T_FALSE;
    elem.m_value.boolean = value;
    return *this;
}

element& element::add_array_item(element elem) { return add_element(std::move(elem)); }

element& element::add_array_object()
{
    auto& elem = append_new();
    elem.m_kind = element_kind::T_OBJECT;
    return elem;
}

bool element::contains(const char* name) const
{
    if(m_children.empty()) {
        return false;
    }
    if(m_elements_map.empty()) {
        // need to index it first
        const_cast<element&>(*this).index_elements();
    }
    return m_elements_map.count(std::string(name)) > 0;
}

bool element::contains(const std::string& name) const { return contains(name.c_str()); }

} // namespace tinyjson
