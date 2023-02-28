#ifndef JSON_LITE_HPP
#define JSON_LITE_HPP

#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tinyjson
{
#define FLATTEN_INLINE inline __attribute__((flatten))

enum class ElementKind { T_INVALID, T_TRUE, T_FALSE, T_STRING, T_NUMBER, T_OBJECT, T_ARRAY, T_NULL };

std::string& escape_string(const std::string_view& str, std::string* escaped);

union Value {
    char* str;
    double number;
    bool boolean;
};

struct Element {
private:
    /// the element's kind
    ElementKind m_kind = ElementKind::T_INVALID;
    /// if the Element has a name -> its here
    std::string m_property_name;
    /// the element's value
    Value m_value;
    /// list of all children
    std::vector<Element> m_children;
    /// provide `O(1)` access for elements by name
    std::unordered_map<std::string_view, Element*> m_elements_map;

private:
    static const char* parse_string(tinyjson::Element* item, const char* str);
    static const char* parse_number(tinyjson::Element* item, const char* num);
    static const char* parse_array(tinyjson::Element* item, const char* value);
    static const char* parse_object(tinyjson::Element* item, const char* value);
    static const char* parse_value(tinyjson::Element* item, const char* value);

    void index_elements();

private:
    /// append new item to the end of the children list and return a reference to it
    FLATTEN_INLINE Element& append_new()
    {
        m_children.emplace_back(Element{});
        return m_children.back();
    }

    FLATTEN_INLINE const char* suffix(bool is_last) const
    {
        if(is_last) {
            return "\n";
        } else {
            return ",\n";
        }
    }

    /// new property element with a given name
    /// and return it. This method does not set the value
    /// but it does add the newly added item to the index
    /// table
    Element& add_property_internal(const std::string& name);

public:
    /// construct json from string
    static bool parse(const std::string& content, Element* root);

    /// construct json from file
    static bool parse_file(const std::string& path, Element* root);

    static bool create_array(Element* arr);
    static bool create_object(Element* obj);

    Element();

    // no copy constructor is allowed, only `move`
    Element(Element& other) = delete;

    Element(Element&& other);
    ~Element();

    // Check functions
    FLATTEN_INLINE bool is_array() const { return m_kind == ElementKind::T_ARRAY; }
    FLATTEN_INLINE bool is_object() const { return m_kind == ElementKind::T_OBJECT; }
    FLATTEN_INLINE bool is_string() const { return m_kind == ElementKind::T_STRING; }
    FLATTEN_INLINE bool is_number() const { return m_kind == ElementKind::T_NUMBER; }
    FLATTEN_INLINE bool is_true() const { return m_kind == ElementKind::T_TRUE; }
    FLATTEN_INLINE bool is_false() const { return m_kind == ElementKind::T_FALSE; }
    FLATTEN_INLINE bool is_null() const { return m_kind == ElementKind::T_NULL; }
    FLATTEN_INLINE bool is_ok() const { return m_kind != ElementKind::T_INVALID; }

    // "as" methods
    // element.as<std::string>

    /// return the value as a string
    /// @param val [output]
    /// @param default_value default value to return in case of an error
    template <typename T> FLATTEN_INLINE bool as_str(T* val, const char* default_value = "") const
    {
        if(!is_string()) {
            *val = default_value;
            return false;
        }

        *val = m_value.str;
        return true;
    }

    /// return the value as a number. return the `default_value` on error
    template <typename T> FLATTEN_INLINE T&& to_str(const char* default_value = "") const
    {
        T value;
        as_str(&value, default_value);
        return std::move(value);
    }

    /// return the value as a number. return false on error
    /// @param val [output]
    /// @param default_value default value to return in case of an error
    template <typename T> FLATTEN_INLINE bool as_number(T* val, int default_value = -1) const
    {
        if(!is_number()) {
            *val = static_cast<T>(default_value);
            return false;
        }

        *val = static_cast<T>(m_value.number);
        return true;
    }

    /// return the value as a number. return the `default_value` on error
    template <typename T> FLATTEN_INLINE T to_number(int default_value = -1) const
    {
        T value;
        as_number(&value, default_value);
        return value;
    }

    /// return the value as a bool
    /// @param val [output]
    /// @param default_value default value to return in case of an error
    FLATTEN_INLINE bool as_bool(bool* val, bool default_value = false) const
    {
        switch(m_kind) {
        case ElementKind::T_FALSE:
            *val = false;
            return true;
        case ElementKind::T_TRUE:
            *val = true;
            return true;
        default:
            return false;
        }
    }

    /// return the value as a bool. return the `default_value` on error
    FLATTEN_INLINE bool to_bool(bool default_value = false) const
    {
        bool v;
        as_bool(&v, default_value);
        return v;
    }

    /// access element by name
    const Element& operator[](const char* index) const;
    Element& operator[](const char* index);

    FLATTEN_INLINE const Element& operator[](const std::string& index) const { return operator[](index.c_str()); }
    FLATTEN_INLINE Element& operator[](const std::string& index) { return operator[](index.c_str()); }

    /// access element by position
    const Element& operator[](size_t index) const;
    Element& operator[](size_t index);

    FLATTEN_INLINE const Element& operator[](int index) const { return operator[](static_cast<size_t>(index)); }
    FLATTEN_INLINE Element& operator[](int index) { return operator[](static_cast<size_t>(index)); }

    /// STL like api, so we can have `for` loops
    FLATTEN_INLINE std::vector<Element>::const_iterator begin() const { return m_children.begin(); }
    FLATTEN_INLINE std::vector<Element>::iterator begin() { return m_children.begin(); }
    FLATTEN_INLINE std::vector<Element>::const_iterator end() const { return m_children.end(); }
    FLATTEN_INLINE std::vector<Element>::iterator end() { return m_children.end(); }

    FLATTEN_INLINE std::vector<Element>::size_type size() const { return m_children.size(); }
    /// return true if this Element has no children
    FLATTEN_INLINE bool empty() const { return m_children.empty(); }
    /// delete all children
    FLATTEN_INLINE void clear()
    {
        m_children.clear();
        m_elements_map.clear();
    }
    /// return true if this Element contains a child with a given name
    FLATTEN_INLINE bool contains(const char* name) const { return m_elements_map.count(std::string_view(name)) > 0; }
    /// return true if this Element contains a child with a given name
    FLATTEN_INLINE bool contains(const std::string& name) const
    {
        return m_elements_map.count(std::string_view(name)) > 0;
    }

    // write API

    /// add new property to the `this`. return ref to `this`
    /// @return reference to `this`
    Element& add_property(const std::string& name, double value);

    /// add new property to the `this`. return ref to `this`
    /// @return reference to `this`
    Element& add_property(const std::string& name, int value);

    /// add new property to the `this`. return ref to `this`
    /// @return reference to `this`
    Element& add_property(const std::string& name, long value);

    /// add new property to the `this`. return ref to `this`
    /// @return reference to `this`
    Element& add_property(const std::string& name, size_t value);

    /// add new property to the `this`. return ref to `this`
    /// @return reference to `this`
    Element& add_property(const std::string& name, const std::string& value);

    /// add new property to the `this`. return ref to `this`
    /// @return reference to `this`
    Element& add_property(const std::string& name, const char* value);

    /// add new property to the `this`. return ref to `this`
    /// @return reference to `this`
    Element& add_property(const std::string& name, bool b);

    /// add new property to the `this`. return ref to `this`
    /// @return reference to `this`
    Element& add_element(Element&& elem);

    /// add new property to the `this`. return ref to `this`
    /// @return reference to `this`
    Element& add_property_null(const std::string& name);

    /// add new array with a given name. return the newly added Element
    /// @return the newly added object
    Element& add_array(const std::string& name);

    /// add new object with a given name. return the newly added Element
    /// @return the newly added object
    Element& add_object(const std::string& name);

    /// add an Element of type string to the array, return the array
    /// @return reference to `this`
    Element& add_array_item(const std::string& value);

    /// add an Element of type string to the array, return the array
    /// @return reference to `this`
    Element& add_array_item(const char* value);

    /// add an Element of type double to the array, return the array
    /// @return reference to `this`
    Element& add_array_item(double value);

    /// add an Element of type bool to the array, return the array
    /// @return reference to `this`
    Element& add_array_item(bool b);

    /// add an Element of type string to the array, return the array
    /// @return reference to `this`
    Element& add_array_item(Element elem);

    /// create new empty Element of type object and append it to the end of the array
    /// @return the newly added object
    Element& add_array_object();

    FLATTEN_INLINE const char* property_name() const
    {
        if(m_property_name.empty()) {
            return nullptr;
        }
        return m_property_name.c_str();
    }

    FLATTEN_INLINE void to_string(std::stringstream& ss, int depth, bool last_child) const
    {
        std::string indent(depth, ' ');
        ss << indent;
        if(property_name()) {
            ss << "\"" << property_name() << "\": ";
        }

        switch(m_kind) {
        case ElementKind::T_STRING: {
            std::string_view sv;
            as_str(&sv);
            std::string escaped_str;
            ss << escape_string(sv, &escaped_str) << suffix(last_child);
        } break;
        case ElementKind::T_NUMBER: {
            double d;
            as_number(&d);
            ss << d << suffix(last_child);
        } break;
        case ElementKind::T_TRUE: {
            ss << "true" << suffix(last_child);
        } break;
        case ElementKind::T_FALSE: {
            ss << "false" << suffix(last_child);
            ;
        } break;
        case ElementKind::T_NULL: {
            ss << "null" << suffix(last_child);
            ;
        } break;
        case ElementKind::T_OBJECT: {
            if(m_children.empty()) {
                ss << "{}" << suffix(last_child);
            }
            ss << "{\n";
            for(size_t i = 0; i < m_children.size(); ++i) {
                bool is_last = i == m_children.size() - 1;
                m_children[i].to_string(ss, depth + 1, is_last);
            }
            ss << indent << "}" << suffix(last_child);
        } break;
        case ElementKind::T_ARRAY: {
            if(m_children.empty()) {
                ss << "[]" << suffix(last_child);
            }
            ss << "[\n";
            for(size_t i = 0; i < m_children.size(); ++i) {
                bool is_last = i == m_children.size() - 1;
                m_children[i].to_string(ss, depth + 1, is_last);
            }
            ss << indent << "]" << suffix(last_child);
        } break;
        case ElementKind::T_INVALID:
            break;
        }
    }
};

FLATTEN_INLINE void to_string(const Element& root, std::stringstream& ss) { root.to_string(ss, 0, true); }

} // namespace tinyjson

#endif // JSON_LITE_HPP
