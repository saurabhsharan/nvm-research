#include <string>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <streambuf>
#include <stdio.h>
#include <syscall.h>
#include "pin.H"
#include <iostream>
#include <inttypes.h>
#include <map>
#include "JSON.h"

/*
 * File JSON.cpp part of the SimpleJSON Library - http://mjpa.in/json
 *
 * Copyright (C) 2010 Mike Anchor
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * Blocks off the public constructor
 *
 * @access private
 *
 */
JSON::JSON()
{
}

/**
 * Parses a complete JSON encoded string
 * This is just a wrapper around the UNICODE Parse().
 *
 * @access public
 *
 * @param char* data The JSON text
 *
 * @return JSONValue* Returns a JSON Value representing the root, or NULL on error
 */
JSONValue *JSON::Parse(const char *data)
{
  size_t length = strlen(data) + 1;
  wchar_t *w_data = (wchar_t*)malloc(length * sizeof(wchar_t));

#if defined(WIN32) && !defined(__GNUC__)
  size_t ret_value = 0;
  if (mbstowcs_s(&ret_value, w_data, length, data, length) != 0)
  {
    free(w_data);
    return NULL;
  }
#elif defined(ANDROID)
  // mbstowcs seems to misbehave on android
  for(size_t i = 0; i<length; i++)
    w_data[i] = (wchar_t)data[i];
#else
  if (mbstowcs(w_data, data, length) == (size_t)-1)
  {
    free(w_data);
    return NULL;
  }
#endif

  JSONValue *value = JSON::Parse(w_data);
  free(w_data);
  return value;
}

/**
 * Parses a complete JSON encoded string (UNICODE input version)
 *
 * @access public
 *
 * @param wchar_t* data The JSON text
 *
 * @return JSONValue* Returns a JSON Value representing the root, or NULL on error
 */
JSONValue *JSON::Parse(const wchar_t *data)
{
  // Skip any preceding whitespace, end of data = no JSON = fail
  if (!SkipWhitespace(&data))
    return NULL;

  // We need the start of a value here now...
  JSONValue *value = JSONValue::Parse(&data);
  if (value == NULL)
    return NULL;

  // Can be white space now and should be at the end of the string then...
  if (SkipWhitespace(&data))
  {
    delete value;
    return NULL;
  }

  // We're now at the end of the string
  return value;
}

/**
 * Turns the passed in JSONValue into a JSON encode string
 *
 * @access public
 *
 * @param JSONValue* value The root value
 *
 * @return std::wstring Returns a JSON encoded string representation of the given value
 */
std::wstring JSON::Stringify(const JSONValue *value)
{
  if (value != NULL)
    return value->Stringify();
  else
    return L"";
}

/**
 * Skips over any whitespace characters (space, tab, \r or \n) defined by the JSON spec
 *
 * @access protected
 *
 * @param wchar_t** data Pointer to a wchar_t* that contains the JSON text
 *
 * @return bool Returns true if there is more data, or false if the end of the text was reached
 */
bool JSON::SkipWhitespace(const wchar_t **data)
{
  while (**data != 0 && (**data == L' ' || **data == L'\t' || **data == L'\r' || **data == L'\n'))
    (*data)++;

  return **data != 0;
}

/**
 * Extracts a JSON String as defined by the spec - "<some chars>"
 * Any escaped characters are swapped out for their unescaped values
 *
 * @access protected
 *
 * @param wchar_t** data Pointer to a wchar_t* that contains the JSON text
 * @param std::wstring& str Reference to a std::wstring to receive the extracted string
 *
 * @return bool Returns true on success, false on failure
 */
bool JSON::ExtractString(const wchar_t **data, std::wstring &str)
{
  str = L"";

  while (**data != 0)
  {
    // Save the char so we can change it if need be
    wchar_t next_char = **data;

    // Escaping something?
    if (next_char == L'\\')
    {
      // Move over the escape char
      (*data)++;

      // Deal with the escaped char
      switch (**data)
      {
        case L'"': next_char = L'"'; break;
        case L'\\': next_char = L'\\'; break;
        case L'/': next_char = L'/'; break;
        case L'b': next_char = L'\b'; break;
        case L'f': next_char = L'\f'; break;
        case L'n': next_char = L'\n'; break;
        case L'r': next_char = L'\r'; break;
        case L't': next_char = L'\t'; break;
        case L'u':
                   {
                     // We need 5 chars (4 hex + the 'u') or its not valid
                     if (!simplejson_wcsnlen(*data, 5))
                       return false;

                     // Deal with the chars
                     next_char = 0;
                     for (int i = 0; i < 4; i++)
                     {
                       // Do it first to move off the 'u' and leave us on the
                       // final hex digit as we move on by one later on
                       (*data)++;

                       next_char <<= 4;

                       // Parse the hex digit
                       if (**data >= '0' && **data <= '9')
                         next_char |= (**data - '0');
                       else if (**data >= 'A' && **data <= 'F')
                         next_char |= (10 + (**data - 'A'));
                       else if (**data >= 'a' && **data <= 'f')
                         next_char |= (10 + (**data - 'a'));
                       else
                       {
                         // Invalid hex digit = invalid JSON
                         return false;
                       }
                     }
                     break;
                   }

                   // By the spec, only the above cases are allowed
        default:
                   return false;
      }
    }

    // End of the string?
    else if (next_char == L'"')
    {
      (*data)++;
      str.reserve(); // Remove unused capacity
      return true;
    }

    // Disallowed char?
    else if (next_char < L' ' && next_char != L'\t')
    {
      // SPEC Violation: Allow tabs due to real world cases
      return false;
    }

    // Add the next char
    str += next_char;

    // Move on
    (*data)++;
  }

  // If we're here, the string ended incorrectly
  return false;
}

/**
 * Parses some text as though it is an integer
 *
 * @access protected
 *
 * @param wchar_t** data Pointer to a wchar_t* that contains the JSON text
 *
 * @return double Returns the double value of the number found
 */
double JSON::ParseInt(const wchar_t **data)
{
  double integer = 0;
  while (**data != 0 && **data >= '0' && **data <= '9')
    integer = integer * 10 + (*(*data)++ - '0');

  return integer;
}

/**
 * Parses some text as though it is a decimal
 *
 * @access protected
 *
 * @param wchar_t** data Pointer to a wchar_t* that contains the JSON text
 *
 * @return double Returns the double value of the decimal found
 */
double JSON::ParseDecimal(const wchar_t **data)
{
  double decimal = 0.0;
  double factor = 0.1;
  while (**data != 0 && **data >= '0' && **data <= '9')
  {
    int digit = (*(*data)++ - '0');
    decimal = decimal + digit * factor;
    factor *= 0.1;
  }
  return decimal;
}

/*
 * File JSONValue.cpp part of the SimpleJSON Library - http://mjpa.in/json
 *
 * Copyright (C) 2010 Mike Anchor
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <math.h>

#ifdef __MINGW32__
#define wcsncasecmp wcsnicmp
#endif

// Macros to free an array/object
#define FREE_ARRAY(x) { JSONArray::iterator iter; for (iter = x.begin(); iter != x.end(); iter++) { delete *iter; } }
#define FREE_OBJECT(x) { JSONObject::iterator iter; for (iter = x.begin(); iter != x.end(); iter++) { delete (*iter).second; } }

/**
 * Parses a JSON encoded value to a JSONValue object
 *
 * @access protected
 *
 * @param wchar_t** data Pointer to a wchar_t* that contains the data
 *
 * @return JSONValue* Returns a pointer to a JSONValue object on success, NULL on error
 */
JSONValue *JSONValue::Parse(const wchar_t **data)
{
  // Is it a string?
  if (**data == '"')
  {
    std::wstring str;
    if (!JSON::ExtractString(&(++(*data)), str))
      return NULL;
    else
      return new JSONValue(str);
  }

  // Is it a boolean?
  else if ((simplejson_wcsnlen(*data, 4) && wcsncasecmp(*data, L"true", 4) == 0) || (simplejson_wcsnlen(*data, 5) && wcsncasecmp(*data, L"false", 5) == 0))
  {
    bool value = wcsncasecmp(*data, L"true", 4) == 0;
    (*data) += value ? 4 : 5;
    return new JSONValue(value);
  }

  // Is it a null?
  else if (simplejson_wcsnlen(*data, 4) && wcsncasecmp(*data, L"null", 4) == 0)
  {
    (*data) += 4;
    return new JSONValue();
  }

  // Is it a number?
  else if (**data == L'-' || (**data >= L'0' && **data <= L'9'))
  {
    // Negative?
    bool neg = **data == L'-';
    if (neg) (*data)++;

    double number = 0.0;

    // Parse the whole part of the number - only if it wasn't 0
    if (**data == L'0')
      (*data)++;
    else if (**data >= L'1' && **data <= L'9')
      number = JSON::ParseInt(data);
    else
      return NULL;

    // Could be a decimal now...
    if (**data == '.')
    {
      (*data)++;

      // Not get any digits?
      if (!(**data >= L'0' && **data <= L'9'))
        return NULL;

      // Find the decimal and sort the decimal place out
      // Use ParseDecimal as ParseInt won't work with decimals less than 0.1
      // thanks to Javier Abadia for the report & fix
      double decimal = JSON::ParseDecimal(data);

      // Save the number
      number += decimal;
    }

    // Could be an exponent now...
    if (**data == L'E' || **data == L'e')
    {
      (*data)++;

      // Check signage of expo
      bool neg_expo = false;
      if (**data == L'-' || **data == L'+')
      {
        neg_expo = **data == L'-';
        (*data)++;
      }

      // Not get any digits?
      if (!(**data >= L'0' && **data <= L'9'))
        return NULL;

      // Sort the expo out
      double expo = JSON::ParseInt(data);
      for (double i = 0.0; i < expo; i++)
        number = neg_expo ? (number / 10.0) : (number * 10.0);
    }

    // Was it neg?
    if (neg) number *= -1;

    return new JSONValue(number);
  }

  // An object?
  else if (**data == L'{')
  {
    JSONObject object;

    (*data)++;

    while (**data != 0)
    {
      // Whitespace at the start?
      if (!JSON::SkipWhitespace(data))
      {
        FREE_OBJECT(object);
        return NULL;
      }

      // Special case - empty object
      if (object.size() == 0 && **data == L'}')
      {
        (*data)++;
        return new JSONValue(object);
      }

      // We want a string now...
      std::wstring name;
      if (!JSON::ExtractString(&(++(*data)), name))
      {
        FREE_OBJECT(object);
        return NULL;
      }

      // More whitespace?
      if (!JSON::SkipWhitespace(data))
      {
        FREE_OBJECT(object);
        return NULL;
      }

      // Need a : now
      if (*((*data)++) != L':')
      {
        FREE_OBJECT(object);
        return NULL;
      }

      // More whitespace?
      if (!JSON::SkipWhitespace(data))
      {
        FREE_OBJECT(object);
        return NULL;
      }

      // The value is here
      JSONValue *value = Parse(data);
      if (value == NULL)
      {
        FREE_OBJECT(object);
        return NULL;
      }

      // Add the name:value
      if (object.find(name) != object.end())
        delete object[name];
      object[name] = value;

      // More whitespace?
      if (!JSON::SkipWhitespace(data))
      {
        FREE_OBJECT(object);
        return NULL;
      }

      // End of object?
      if (**data == L'}')
      {
        (*data)++;
        return new JSONValue(object);
      }

      // Want a , now
      if (**data != L',')
      {
        FREE_OBJECT(object);
        return NULL;
      }

      (*data)++;
    }

    // Only here if we ran out of data
    FREE_OBJECT(object);
    return NULL;
  }

  // An array?
  else if (**data == L'[')
  {
    JSONArray array;

    (*data)++;

    while (**data != 0)
    {
      // Whitespace at the start?
      if (!JSON::SkipWhitespace(data))
      {
        FREE_ARRAY(array);
        return NULL;
      }

      // Special case - empty array
      if (array.size() == 0 && **data == L']')
      {
        (*data)++;
        return new JSONValue(array);
      }

      // Get the value
      JSONValue *value = Parse(data);
      if (value == NULL)
      {
        FREE_ARRAY(array);
        return NULL;
      }

      // Add the value
      array.push_back(value);

      // More whitespace?
      if (!JSON::SkipWhitespace(data))
      {
        FREE_ARRAY(array);
        return NULL;
      }

      // End of array?
      if (**data == L']')
      {
        (*data)++;
        return new JSONValue(array);
      }

      // Want a , now
      if (**data != L',')
      {
        FREE_ARRAY(array);
        return NULL;
      }

      (*data)++;
    }

    // Only here if we ran out of data
    FREE_ARRAY(array);
    return NULL;
  }

  // Ran out of possibilites, it's bad!
  else
  {
    return NULL;
  }
}

/**
 * Basic constructor for creating a JSON Value of type NULL
 *
 * @access public
 */
JSONValue::JSONValue(/*NULL*/)
{
  type = JSONType_Null;
}

/**
 * Basic constructor for creating a JSON Value of type String
 *
 * @access public
 *
 * @param wchar_t* m_char_value The string to use as the value
 */
JSONValue::JSONValue(const wchar_t *m_char_value)
{
  type = JSONType_String;
  string_value = new std::wstring(std::wstring(m_char_value));
}

/**
 * Basic constructor for creating a JSON Value of type String
 *
 * @access public
 *
 * @param std::wstring m_string_value The string to use as the value
 */
JSONValue::JSONValue(const std::wstring &m_string_value)
{
  type = JSONType_String;
  string_value = new std::wstring(m_string_value);
}

/**
 * Basic constructor for creating a JSON Value of type Bool
 *
 * @access public
 *
 * @param bool m_bool_value The bool to use as the value
 */
JSONValue::JSONValue(bool m_bool_value)
{
  type = JSONType_Bool;
  bool_value = m_bool_value;
}

/**
 * Basic constructor for creating a JSON Value of type Number
 *
 * @access public
 *
 * @param double m_number_value The number to use as the value
 */
JSONValue::JSONValue(double m_number_value)
{
  type = JSONType_Number;
  number_value = m_number_value;
}

/**
 * Basic constructor for creating a JSON Value of type Array
 *
 * @access public
 *
 * @param JSONArray m_array_value The JSONArray to use as the value
 */
JSONValue::JSONValue(const JSONArray &m_array_value)
{
  type = JSONType_Array;
  array_value = new JSONArray(m_array_value);
}

/**
 * Basic constructor for creating a JSON Value of type Object
 *
 * @access public
 *
 * @param JSONObject m_object_value The JSONObject to use as the value
 */
JSONValue::JSONValue(const JSONObject &m_object_value)
{
  type = JSONType_Object;
  object_value = new JSONObject(m_object_value);
}

/**
 * Copy constructor to perform a deep copy of array / object values
 *
 * @access public
 *
 * @param JSONValue m_source The source JSONValue that is being copied
 */
JSONValue::JSONValue(const JSONValue &m_source)
{
  type = m_source.type;

  switch (type)
  {
    case JSONType_String:
      string_value = new std::wstring(*m_source.string_value);
      break;

    case JSONType_Bool:
      bool_value = m_source.bool_value;
      break;

    case JSONType_Number:
      number_value = m_source.number_value;
      break;

    case JSONType_Array:
      {
        JSONArray source_array = *m_source.array_value;
        JSONArray::iterator iter;
        array_value = new JSONArray();
        for (iter = source_array.begin(); iter != source_array.end(); iter++)
          array_value->push_back(new JSONValue(**iter));
        break;
      }

    case JSONType_Object:
      {
        JSONObject source_object = *m_source.object_value;
        object_value = new JSONObject();
        JSONObject::iterator iter;
        for (iter = source_object.begin(); iter != source_object.end(); iter++)
        {
          std::wstring name = (*iter).first;
          (*object_value)[name] = new JSONValue(*((*iter).second));
        }
        break;
      }

    case JSONType_Null:
      // Nothing to do.
      break;
  }
}

/**
 * The destructor for the JSON Value object
 * Handles deleting the objects in the array or the object value
 *
 * @access public
 */
JSONValue::~JSONValue()
{
  if (type == JSONType_Array)
  {
    JSONArray::iterator iter;
    for (iter = array_value->begin(); iter != array_value->end(); iter++)
      delete *iter;
    delete array_value;
  }
  else if (type == JSONType_Object)
  {
    JSONObject::iterator iter;
    for (iter = object_value->begin(); iter != object_value->end(); iter++)
    {
      delete (*iter).second;
    }
    delete object_value;
  }
  else if (type == JSONType_String)
  {
    delete string_value;
  }
}

/**
 * Checks if the value is a NULL
 *
 * @access public
 *
 * @return bool Returns true if it is a NULL value, false otherwise
 */
bool JSONValue::IsNull() const
{
  return type == JSONType_Null;
}

/**
 * Checks if the value is a String
 *
 * @access public
 *
 * @return bool Returns true if it is a String value, false otherwise
 */
bool JSONValue::IsString() const
{
  return type == JSONType_String;
}

/**
 * Checks if the value is a Bool
 *
 * @access public
 *
 * @return bool Returns true if it is a Bool value, false otherwise
 */
bool JSONValue::IsBool() const
{
  return type == JSONType_Bool;
}

/**
 * Checks if the value is a Number
 *
 * @access public
 *
 * @return bool Returns true if it is a Number value, false otherwise
 */
bool JSONValue::IsNumber() const
{
  return type == JSONType_Number;
}

/**
 * Checks if the value is an Array
 *
 * @access public
 *
 * @return bool Returns true if it is an Array value, false otherwise
 */
bool JSONValue::IsArray() const
{
  return type == JSONType_Array;
}

/**
 * Checks if the value is an Object
 *
 * @access public
 *
 * @return bool Returns true if it is an Object value, false otherwise
 */
bool JSONValue::IsObject() const
{
  return type == JSONType_Object;
}

/**
 * Retrieves the String value of this JSONValue
 * Use IsString() before using this method.
 *
 * @access public
 *
 * @return std::wstring Returns the string value
 */
const std::wstring &JSONValue::AsString() const
{
  return (*string_value);
}

/**
 * Retrieves the Bool value of this JSONValue
 * Use IsBool() before using this method.
 *
 * @access public
 *
 * @return bool Returns the bool value
 */
bool JSONValue::AsBool() const
{
  return bool_value;
}

/**
 * Retrieves the Number value of this JSONValue
 * Use IsNumber() before using this method.
 *
 * @access public
 *
 * @return double Returns the number value
 */
double JSONValue::AsNumber() const
{
  return number_value;
}

/**
 * Retrieves the Array value of this JSONValue
 * Use IsArray() before using this method.
 *
 * @access public
 *
 * @return JSONArray Returns the array value
 */
const JSONArray &JSONValue::AsArray() const
{
  return (*array_value);
}

/**
 * Retrieves the Object value of this JSONValue
 * Use IsObject() before using this method.
 *
 * @access public
 *
 * @return JSONObject Returns the object value
 */
const JSONObject &JSONValue::AsObject() const
{
  return (*object_value);
}

/**
 * Retrieves the number of children of this JSONValue.
 * This number will be 0 or the actual number of children
 * if IsArray() or IsObject().
 *
 * @access public
 *
 * @return The number of children.
 */
std::size_t JSONValue::CountChildren() const
{
  switch (type)
  {
    case JSONType_Array:
      return array_value->size();
    case JSONType_Object:
      return object_value->size();
    default:
      return 0;
  }
}

/**
 * Checks if this JSONValue has a child at the given index.
 * Use IsArray() before using this method.
 *
 * @access public
 *
 * @return bool Returns true if the array has a value at the given index.
 */
bool JSONValue::HasChild(std::size_t index) const
{
  if (type == JSONType_Array)
  {
    return index < array_value->size();
  }
  else
  {
    return false;
  }
}

/**
 * Retrieves the child of this JSONValue at the given index.
 * Use IsArray() before using this method.
 *
 * @access public
 *
 * @return JSONValue* Returns JSONValue at the given index or NULL
 *                    if it doesn't exist.
 */
JSONValue *JSONValue::Child(std::size_t index)
{
  if (index < array_value->size())
  {
    return (*array_value)[index];
  }
  else
  {
    return NULL;
  }
}

/**
 * Checks if this JSONValue has a child at the given key.
 * Use IsObject() before using this method.
 *
 * @access public
 *
 * @return bool Returns true if the object has a value at the given key.
 */
bool JSONValue::HasChild(const wchar_t* name) const
{
  if (type == JSONType_Object)
  {
    return object_value->find(name) != object_value->end();
  }
  else
  {
    return false;
  }
}

/**
 * Retrieves the child of this JSONValue at the given key.
 * Use IsObject() before using this method.
 *
 * @access public
 *
 * @return JSONValue* Returns JSONValue for the given key in the object
 *                    or NULL if it doesn't exist.
 */
JSONValue* JSONValue::Child(const wchar_t* name)
{
  JSONObject::const_iterator it = object_value->find(name);
  if (it != object_value->end())
  {
    return it->second;
  }
  else
  {
    return NULL;
  }
}

/**
 * Retrieves the keys of the JSON Object or an empty vector
 * if this value is not an object.
 *
 * @access public
 *
 * @return std::vector<std::wstring> A vector containing the keys.
 */
std::vector<std::wstring> JSONValue::ObjectKeys() const
{
  std::vector<std::wstring> keys;

  if (type == JSONType_Object)
  {
    JSONObject::const_iterator iter = object_value->begin();
    while (iter != object_value->end())
    {
      keys.push_back(iter->first);

      iter++;
    }
  }

  return keys;
}

/**
 * Creates a JSON encoded string for the value with all necessary characters escaped
 *
 * @access public
 *
 * @param bool prettyprint Enable prettyprint
 *
 * @return std::wstring Returns the JSON string
 */
std::wstring JSONValue::Stringify(bool const prettyprint) const
{
  size_t const indentDepth = prettyprint ? 1 : 0;
  return StringifyImpl(indentDepth);
}


/**
 * Creates a JSON encoded string for the value with all necessary characters escaped
 *
 * @access private
 *
 * @param size_t indentDepth The prettyprint indentation depth (0 : no prettyprint)
 *
 * @return std::wstring Returns the JSON string
 */
std::wstring JSONValue::StringifyImpl(size_t const indentDepth) const
{
  std::wstring ret_string;
  size_t const indentDepth1 = indentDepth ? indentDepth + 1 : 0;
  std::wstring const indentStr = Indent(indentDepth);
  std::wstring const indentStr1 = Indent(indentDepth1);

  switch (type)
  {
    case JSONType_Null:
      ret_string = L"null";
      break;

    case JSONType_String:
      ret_string = StringifyString(*string_value);
      break;

    case JSONType_Bool:
      ret_string = bool_value ? L"true" : L"false";
      break;

    case JSONType_Number:
      {
        if (isinf(number_value) || isnan(number_value))
          ret_string = L"null";
        else
        {
          std::wstringstream ss;
          ss.precision(15);
          ss << number_value;
          ret_string = ss.str();
        }
        break;
      }

    case JSONType_Array:
      {
        ret_string = indentDepth ? L"[\n" + indentStr1 : L"[";
        JSONArray::const_iterator iter = array_value->begin();
        while (iter != array_value->end())
        {
          ret_string += (*iter)->StringifyImpl(indentDepth1);

          // Not at the end - add a separator
          if (++iter != array_value->end())
            ret_string += L",";
        }
        ret_string += indentDepth ? L"\n" + indentStr + L"]" : L"]";
        break;
      }

    case JSONType_Object:
      {
        ret_string = indentDepth ? L"{\n" + indentStr1 : L"{";
        JSONObject::const_iterator iter = object_value->begin();
        while (iter != object_value->end())
        {
          ret_string += StringifyString((*iter).first);
          ret_string += L":";
          ret_string += (*iter).second->StringifyImpl(indentDepth1);

          // Not at the end - add a separator
          if (++iter != object_value->end())
            ret_string += L",";
        }
        ret_string += indentDepth ? L"\n" + indentStr + L"}" : L"}";
        break;
      }
  }

  return ret_string;
}

/**
 * Creates a JSON encoded string with all required fields escaped
 * Works from http://www.ecma-internationl.org/publications/files/ECMA-ST/ECMA-262.pdf
 * Section 15.12.3.
 *
 * @access private
 *
 * @param std::wstring str The string that needs to have the characters escaped
 *
 * @return std::wstring Returns the JSON string
 */
std::wstring JSONValue::StringifyString(const std::wstring &str)
{
  std::wstring str_out = L"\"";

  std::wstring::const_iterator iter = str.begin();
  while (iter != str.end())
  {
    wchar_t chr = *iter;

    if (chr == L'"' || chr == L'\\' || chr == L'/')
    {
      str_out += L'\\';
      str_out += chr;
    }
    else if (chr == L'\b')
    {
      str_out += L"\\b";
    }
    else if (chr == L'\f')
    {
      str_out += L"\\f";
    }
    else if (chr == L'\n')
    {
      str_out += L"\\n";
    }
    else if (chr == L'\r')
    {
      str_out += L"\\r";
    }
    else if (chr == L'\t')
    {
      str_out += L"\\t";
    }
    else if (chr < L' ' || chr > 126)
    {
      str_out += L"\\u";
      for (int i = 0; i < 4; i++)
      {
        int value = (chr >> 12) & 0xf;
        if (value >= 0 && value <= 9)
          str_out += (wchar_t)('0' + value);
        else if (value >= 10 && value <= 15)
          str_out += (wchar_t)('A' + (value - 10));
        chr <<= 4;
      }
    }
    else
    {
      str_out += chr;
    }

    iter++;
  }

  str_out += L"\"";
  return str_out;
}

/**
 * Creates the indentation string for the depth given
 *
 * @access private
 *
 * @param size_t indent The prettyprint indentation depth (0 : no indentation)
 *
 * @return std::wstring Returns the string
 */
std::wstring JSONValue::Indent(size_t depth)
{
  const size_t indent_step = 2;
  depth ? --depth : 0;
  std::wstring indentStr(depth * indent_step, ' ');
  return indentStr;
}



// for some reason, the type CACHE_STATS is used in pin_cache.H even though it isn't typedef'd in that file, so we just typedef it here
typedef UINT64 CACHE_STATS;
#include "pin_cache.H"

FILE * trace;
FILE * addrs;
int firstInstr = 0;

PIN_LOCK lock;

// cat /sys/devices/system/cpu/cpu0/cache
typedef CACHE_DIRECT_MAPPED(KILO, CACHE_ALLOC::STORE_ALLOCATE) L1Cache;
typedef CACHE_ROUND_ROBIN(8192, 16, CACHE_ALLOC::STORE_ALLOCATE) L3Cache;

L1Cache *dl1cache = NULL;
L3Cache *dl3cache = NULL;

struct thread_data
{
public:
  thread_data() {}
  std::map<uint64_t, uint64_t> page_count_read_with_cache;
  std::map<uint64_t, uint64_t> page_count_read_without_cache;
  std::map<uint64_t, uint64_t> page_count_write_with_cache;
  std::map<uint64_t, uint64_t> page_count_write_without_cache;

  void record_mem_read(void *ip, void *addr, bool cache_hit) {
    uint64_t pageno = ((uint64_t)(addr)) / 4096;
    if (page_count_read_without_cache.find(pageno) == page_count_read_without_cache.end()) {
      page_count_read_without_cache[pageno] = 0;
    }
    page_count_read_without_cache[pageno]++;

    if (!cache_hit) {
      if (page_count_read_with_cache.find(pageno) == page_count_read_with_cache.end()) {
        page_count_read_with_cache[pageno] = 0;
      }
      page_count_read_with_cache[pageno]++;
    }
  }

  void record_mem_write(void *ip, void *addr, bool cache_hit) {
    uint64_t pageno = ((uint64_t)(addr)) / 4096;
    if (page_count_write_without_cache.find(pageno) == page_count_write_without_cache.end()) {
      page_count_write_without_cache[pageno] = 0;
    }
    page_count_write_without_cache[pageno]++;

    if (!cache_hit) {
      if (page_count_write_with_cache.find(pageno) == page_count_write_with_cache.end()) {
        page_count_write_with_cache[pageno] = 0;
      }
      page_count_write_with_cache[pageno]++;
    }
  }
};

TLS_KEY tls_key;

std::vector<thread_data *> all_thread_data;

VOID PrintProcAddrs()
{
  std::ifstream t("/proc/self/maps");
  std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
  // fprintf(addrs, "%zu\n%s\n", str.length(), str.c_str());
  fprintf(addrs, "%s\n\n\n", str.c_str());
}

thread_data *get_tls(THREADID threadid)
{
  return static_cast<thread_data *>(PIN_GetThreadData(tls_key, threadid));
}

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr, THREADID threadid)
{
    PIN_GetLock(&lock, 0);
    bool dl1hit = dl1cache->AccessSingleLine((ADDRINT)addr, CACHE_BASE::ACCESS_TYPE_LOAD);
    bool dl3hit = dl3cache->AccessSingleLine((ADDRINT)addr, CACHE_BASE::ACCESS_TYPE_LOAD);
    // if (std::rand() < (RAND_MAX / 3)) {
      // fprintf(trace,"M %p R %p %s %s\n", ip, addr, dl1hit ? "H" : "M", dl3hit ? "H" : "M");
    // }
    PIN_ReleaseLock(&lock);

  thread_data *td = get_tls(threadid);
  td->record_mem_read(ip, addr, dl1hit || dl3hit);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr, THREADID threadid)
{
    PIN_GetLock(&lock, 0);
    bool dl1hit = dl1cache->AccessSingleLine((ADDRINT)addr, CACHE_BASE::ACCESS_TYPE_STORE);
    bool dl3hit = dl3cache->AccessSingleLine((ADDRINT)addr, CACHE_BASE::ACCESS_TYPE_STORE);
    // if (std::rand() < (RAND_MAX / 500)) {
      // fprintf(trace,"M %p W %p %s %s\n", ip, addr, dl1hit ? "H" : "M", dl3hit ? "H" : "M");
    // }
    PIN_ReleaseLock(&lock);

  thread_data *td = get_tls(threadid);
  td->record_mem_write(ip, addr, dl1hit || dl3hit);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
  // if (firstInstr == 0) {
    // fprintf(addrs, "0x%lx %s\n", INS_Address(ins), INS_Disassemble(ins).c_str());
    // PrintProcAddrs();
    // firstInstr = 1;
  // }

  // if (std::rand() < RAND_MAX / 20) {
    // PrintProcAddrs();
    // fprintf(trace, "ADDR\n");
  // }

    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_THREAD_ID,
                IARG_END);
        }
        // Note that in some architectures a single memory operand can be
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_THREAD_ID,
                IARG_END);
        }
    }
}

// Print syscall number and arguments
VOID SysBefore(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5)
{
#if defined(TARGET_LINUX) && defined(TARGET_IA32)
  // On ia32 Linux, there are only 5 registers for passing system call arguments,
  // but mmap needs 6. For mmap on ia32, the first argument to the system call
  // is a pointer to an array of the 6 arguments
  if (num == SYS_mmap)
  {
    ADDRINT * mmapArgs = reinterpret_cast<ADDRINT *>(arg0);
    arg0 = mmapArgs[0];
    arg1 = mmapArgs[1];
    arg2 = mmapArgs[2];
    arg3 = mmapArgs[3];
    arg4 = mmapArgs[4];
    arg5 = mmapArgs[5];
  }
#endif

  // PrintProcAddrs();
  /*
  fprintf(trace,"S 0x%lx %ld 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx ",
      (unsigned long)ip,
      (long)num,
      (unsigned long)arg0,
      (unsigned long)arg1,
      (unsigned long)arg2,
      (unsigned long)arg3,
      (unsigned long)arg4,
      (unsigned long)arg5);
  */
}

VOID SyscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v)
{
  SysBefore(PIN_GetContextReg(ctxt, REG_INST_PTR),
      PIN_GetSyscallNumber(ctxt, std),
      PIN_GetSyscallArgument(ctxt, std, 0),
      PIN_GetSyscallArgument(ctxt, std, 1),
      PIN_GetSyscallArgument(ctxt, std, 2),
      PIN_GetSyscallArgument(ctxt, std, 3),
      PIN_GetSyscallArgument(ctxt, std, 4),
      PIN_GetSyscallArgument(ctxt, std, 5));
}

// Print the return value of the system call
VOID SysAfter(ADDRINT ret)
{
      // fprintf(trace,"0x%lx\n", (unsigned long)ret);
          // fflush(trace);
          PrintProcAddrs();
}


VOID SyscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v)
{
      SysAfter(PIN_GetSyscallReturn(ctxt, std));
}

VOID Fini(INT32 code, VOID *v)
{
    // fclose(addrs);
    // cout << "fini" << endl;

    std::map<uint64_t, uint64_t> aggregate_page_counts_read;
    std::map<uint64_t, uint64_t> aggregate_page_counts_write;
    std::map<uint64_t, uint64_t> aggregate_page_counts_total;

    for (size_t i = 0; i < all_thread_data.size(); i++) {
      thread_data *td = all_thread_data[i];

      std::map<uint64_t, uint64_t> &td_read = td->page_count_read_with_cache;
      std::map<uint64_t, uint64_t> &td_write = td->page_count_write_with_cache;

      for (std::map<uint64_t, uint64_t>::iterator it = td_read.begin(); it != td_read.end(); ++it) {
        uint64_t pageno = it->first;
        uint64_t count = it->second;

        if (aggregate_page_counts_read.find(pageno) == aggregate_page_counts_read.end()) {
          aggregate_page_counts_read[pageno] = 0;
        }

        if (aggregate_page_counts_total.find(pageno) == aggregate_page_counts_total.end()) {
          aggregate_page_counts_total[pageno] = 0;
        }

        aggregate_page_counts_read[pageno] += count;
        aggregate_page_counts_total[pageno] += count;
      }

      for (std::map<uint64_t, uint64_t>::iterator it = td_write.begin(); it != td_write.end(); ++it) {
        uint64_t pageno = it->first;
        uint64_t count = it->second;

        if (aggregate_page_counts_write.find(pageno) == aggregate_page_counts_write.end()) {
          aggregate_page_counts_write[pageno] = 0;
        }

        if (aggregate_page_counts_total.find(pageno) == aggregate_page_counts_total.end()) {
          aggregate_page_counts_total[pageno] = 0;
        }

        aggregate_page_counts_write[pageno] += count;
        aggregate_page_counts_total[pageno] += count;
      }
    }

    cout << endl << endl << endl << endl << endl;
    // fprintf(trace, "\n\n\n\n\n\n\n\n\n\n\n\n");

    cout << "==== READS ====" << endl;
    fprintf(trace, "## READS\n");
    for (std::map<uint64_t, uint64_t>::iterator it = aggregate_page_counts_read.begin(); it != aggregate_page_counts_read.end(); ++it) {
      cout << it->first << ": " << it->second << endl;
      fprintf(trace, "%lu: %lu\n", it->first, it->second);
    }

    cout << endl << endl;

    cout << "==== WRITES ====" << endl;
    fprintf(trace, "## WRITES\n");
    for (std::map<uint64_t, uint64_t>::iterator it = aggregate_page_counts_write.begin(); it != aggregate_page_counts_write.end(); ++it) {
      cout << it->first << ": " << it->second << endl;
      fprintf(trace, "%lu: %lu\n", it->first, it->second);
    }

    cout << endl << endl;

    cout << "==== TOTAL ====" << endl;
    fprintf(trace, "## TOTAL\n");
    for (std::map<uint64_t, uint64_t>::iterator it = aggregate_page_counts_total.begin(); it != aggregate_page_counts_total.end(); ++it) {
      cout << it->first << ": " << it->second << endl;
      fprintf(trace, "%lu: %lu\n", it->first, it->second);
    }

    fclose(trace);
}

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    PIN_GetLock(&lock, 0);

    // std::cout << "Starting thread " << threadid << endl;

    thread_data *td = new thread_data;
    PIN_SetThreadData(tls_key, td, threadid);

    all_thread_data.push_back(td);

    PIN_ReleaseLock(&lock);
}

// VOID ThreadFini(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *v)
{
    PIN_GetLock(&lock, 0);

    // std::cout << "Ending thread " << threadid << endl;

    PIN_ReleaseLock(&lock);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n"
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    PIN_InitLock(&lock);

    if (PIN_Init(argc, argv)) return Usage();

    PIN_InitLock(&lock);

    tls_key = PIN_CreateThreadDataKey(0);

    dl1cache = new L1Cache("L1 Data Cache", 64 * KILO, 64, 1);
    dl3cache = new L3Cache("L3 Unified Cache", /* size = */ 8192 * KILO, /* block size = */ 64, /* associativity = */ 16);

    trace = fopen(std::getenv("PINATRACE_OUTPUT_FILENAME"), "w");
    // addrs = fopen("pinaaddrs.out", "w");

    std::srand(std::time(0));

    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    // PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    // PIN_AddSyscallExitFunction(SyscallExit, 0);
    INS_AddInstrumentFunction(Instruction, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
