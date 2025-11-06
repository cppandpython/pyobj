// CREATOR 
// GitHub https://github.com/cppandpython
// NAME: Vladislav 
// SURNAME: Khudash  
// AGE: 17

// DATE: 06.11.2025
// APP: PYTHON_IN_CPP
// TYPE: LIB
// VERSION: LATEST
// PLATFORM: ANY


#pragma once


#include <Python.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include <iostream>
#include <sstream>


namespace py {


// ================== Python Initialization ==================
inline void init_python() { 
    if (!Py_IsInitialized()) Py_Initialize(); 
}

inline void exit_python() { 
    if (Py_IsInitialized()) Py_FinalizeEx(); 
}

// ================== Base PyObj Class ==================
class PyObj {
protected:
    PyObject* obj;

public:
    // Constructors
    PyObj(PyObject* o = nullptr) : obj(o) { Py_XINCREF(obj); }
    PyObj(const PyObj& other) : obj(other.obj) { Py_XINCREF(obj); }
    PyObj(PyObj&& other) noexcept : obj(other.obj) { other.obj = nullptr; }
    
    // Type conversion constructors
    PyObj(int v) : obj(PyLong_FromLong(v)) {}
    PyObj(long v) : obj(PyLong_FromLong(v)) {}
    PyObj(double v) : obj(PyFloat_FromDouble(v)) {}
    PyObj(bool v) : obj(v ? Py_True : Py_False) { Py_XINCREF(obj); }
    PyObj(const std::string& s) : obj(PyUnicode_FromString(s.c_str())) {}
    PyObj(const char* s) : obj(PyUnicode_FromString(s)) {}

    // Assignment operators
    PyObj& operator=(const PyObj& other) {
        if (this != &other) {
            Py_XDECREF(obj);
            obj = other.obj;
            Py_XINCREF(obj);
        }
        return *this;
    }

    PyObj& operator=(PyObj&& other) noexcept {
        if (this != &other) {
            Py_XDECREF(obj);
            obj = other.obj;
            other.obj = nullptr;
        }
        return *this;
    }

    virtual ~PyObj() { Py_XDECREF(obj); }
    
    // Getters
    PyObject* get_obj() const { return obj; }

    // Type checking methods
    bool is_empty() const {
        if (!obj) return true;
        if (PyUnicode_Check(obj)) return PyUnicode_GetLength(obj) == 0;
        if (PyList_Check(obj)) return PyList_Size(obj) == 0;
        if (PyDict_Check(obj)) return PyDict_Size(obj) == 0;
        if (PySet_Check(obj)) return PySet_Size(obj) == 0;
        return false;
    }

    bool is_dict() const { return obj && PyDict_Check(obj); }
    bool is_list() const { return obj && PyList_Check(obj); }
    bool is_tuple() const { return obj && PyTuple_Check(obj); }
    bool is_str() const { return obj && PyUnicode_Check(obj); }
    bool is_set() const { return obj && PySet_Check(obj); }
    bool is_sequence() const { return is_list() || is_tuple() || is_str(); }
    bool is_callable() const { return obj && PyCallable_Check(obj); }

    // Item access methods
    PyObj get_item(const PyObj& key) const {
        if (!obj) return PyObj();
        
        PyObject* result = PyDict_Check(obj) 
            ? PyDict_GetItem(obj, key.get_obj())
            : PyObject_GetItem(obj, key.get_obj());
            
        if (!result) { 
            PyErr_Clear(); 
            return PyObj(); 
        }
        
        Py_XINCREF(result);
        return PyObj(result);
    }

    PyObj get_item(long index) const {
        if (!obj) return PyObj();

        // Handle lists and tuples
        if (PyList_Check(obj) || PyTuple_Check(obj)) {
            Py_ssize_t size = PyList_Check(obj) 
                ? PyList_Size(obj) 
                : PyTuple_Size(obj);
                
            if (index < 0) index += size;
            if (index < 0 || index >= size) return PyObj();
            
            PyObject* item = PyList_Check(obj) 
                ? PyList_GetItem(obj, index) 
                : PyTuple_GetItem(obj, index);
                
            Py_XINCREF(item);
            return PyObj(item);
        }

        // Handle strings
        if (PyUnicode_Check(obj)) {
            Py_ssize_t length = PyUnicode_GetLength(obj);
            if (index < 0) index += length;
            if (index < 0 || index >= length) return PyObj();
            
            Py_UCS4 character = PyUnicode_ReadChar(obj, index);
            PyObject* temp = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &character, 1);
            PyObj result(temp); 
            Py_XDECREF(temp); 
            return result;
        }

        // Generic case
        PyObject* py_index = PyLong_FromLong(index);
        PyObject* result = PyObject_GetItem(obj, py_index);
        Py_XDECREF(py_index);
        
        if (!result) { 
            PyErr_Clear(); 
            return PyObj(); 
        }
        
        Py_XINCREF(result);
        return PyObj(result);
    }

    // Operator overloads for item access
    PyObj operator[](const PyObj& key) const { return get_item(key); }
    PyObj operator[](long index) const { return get_item(index); }

    // Item setting methods
    bool set_item(const PyObj& key, const PyObj& value) {
        if (!obj) return false;
        
        return PyDict_Check(obj) 
            ? PyDict_SetItem(obj, key.get_obj(), value.get_obj()) == 0
            : PyObject_SetItem(obj, key.get_obj(), value.get_obj()) == 0;
    }

    bool set_item(long index, const PyObj& value) {
        if (!obj) return false;
        
        if (PyList_Check(obj)) {
            Py_ssize_t size = PyList_Size(obj); 
            if (index < 0) index += size;
            if (index < 0 || index >= size) return false;
            
            Py_XINCREF(value.get_obj());
            return PyList_SetItem(obj, index, value.get_obj()) == 0;
        }
        
        PyObject* py_index = PyLong_FromLong(index);
        int success = PyObject_SetItem(obj, py_index, value.get_obj());
        Py_XDECREF(py_index);
        
        return success == 0;
    }

    // ===== UNIVERSAL operator () for any type =====
    template<typename... Args>
    PyObj operator()(Args&&... args) const {
        if (!obj || !PyCallable_Check(obj)) {
            std::cerr << "Error: Object is not callable" << std::endl;
            return PyObj();
        }

        // Converting arguments to Python objects
        std::vector<PyObject*> py_args;
        (py_args.push_back(convert_to_pyobject(std::forward<Args>(args))), ...);
        
        // Create a tuple of arguments
        PyObject* tuple = PyTuple_New(py_args.size());
        for (size_t i = 0; i < py_args.size(); i++) {
            PyTuple_SetItem(tuple, i, py_args[i]);
        }
        
        // Calling a function
        PyObject* result = PyObject_CallObject(obj, tuple);
        Py_DECREF(tuple);
        
        if (!result) {
            PyErr_Print();
            return PyObj();
        }
        return PyObj(result);
    }

    // String representation
    std::string str() const {
        if (!obj) return "None";
        
        PyObject* string_obj = PyObject_Str(obj);
        if (!string_obj) { 
            PyErr_Clear(); 
            return "<error>"; 
        }
        
        const char* c_str = PyUnicode_AsUTF8(string_obj);
        std::string result = c_str ? c_str : "";
        Py_XDECREF(string_obj);
        
        return result;
    }

    // Pretty printing
    static void pretty_print(std::ostream& os, const PyObj& obj, int indent = 0);

private:
    // Helper methods for argument conversion
    PyObject* convert_to_pyobject(const PyObj& arg) const {
        Py_XINCREF(arg.get_obj());
        return arg.get_obj();
    }
    
    PyObject* convert_to_pyobject(int arg) const {
        return PyLong_FromLong(arg);
    }
    
    PyObject* convert_to_pyobject(long arg) const {
        return PyLong_FromLong(arg);
    }
    
    PyObject* convert_to_pyobject(double arg) const {
        return PyFloat_FromDouble(arg);
    }
    
    PyObject* convert_to_pyobject(bool arg) const {
        return PyBool_FromLong(arg);
    }
    
    PyObject* convert_to_pyobject(const std::string& arg) const {
        return PyUnicode_FromString(arg.c_str());
    }
    
    PyObject* convert_to_pyobject(const char* arg) const {
        return PyUnicode_FromString(arg);
    }
};

// ================== String Class ==================
class Str : public PyObj {
public:
    // Constructors
    Str() : PyObj("") {}
    Str(const std::string& s) : PyObj(s) {}
    Str(const char* s) : PyObj(s) {}
    Str(PyObject* o) : PyObj(o) {}
    Str(const PyObj& o) : PyObj(o.get_obj()) {}

    // String manipulation methods
    Str capitalize() const { 
        PyObject* result = PyObject_CallMethod(obj, "capitalize", nullptr); 
        return Str(result); 
    }
    
    Str upper() const { 
        PyObject* result = PyObject_CallMethod(obj, "upper", nullptr); 
        return Str(result); 
    }
    
    Str lower() const { 
        PyObject* result = PyObject_CallMethod(obj, "lower", nullptr); 
        return Str(result); 
    }
    
    Str title() const { 
        PyObject* result = PyObject_CallMethod(obj, "title", nullptr); 
        return Str(result); 
    }
    
    Str swapcase() const { 
        PyObject* result = PyObject_CallMethod(obj, "swapcase", nullptr); 
        return Str(result); 
    }
    
    Str strip() const { 
        PyObject* result = PyObject_CallMethod(obj, "strip", nullptr); 
        return Str(result); 
    }
    
    Str lstrip() const { 
        PyObject* result = PyObject_CallMethod(obj, "lstrip", nullptr); 
        return Str(result); 
    }
    
    Str rstrip() const { 
        PyObject* result = PyObject_CallMethod(obj, "rstrip", nullptr); 
        return Str(result); 
    }

    // String validation methods
    bool isdigit() const { 
        PyObject* result = PyObject_CallMethod(obj, "isdigit", nullptr); 
        bool value = result && PyObject_IsTrue(result); 
        Py_XDECREF(result); 
        return value; 
    }
    
    bool isalpha() const { 
        PyObject* result = PyObject_CallMethod(obj, "isalpha", nullptr); 
        bool value = result && PyObject_IsTrue(result); 
        Py_XDECREF(result); 
        return value; 
    }
    
    bool isalnum() const { 
        PyObject* result = PyObject_CallMethod(obj, "isalnum", nullptr); 
        bool value = result && PyObject_IsTrue(result); 
        Py_XDECREF(result); 
        return value; 
    }
    
    bool isdecimal() const { 
        PyObject* result = PyObject_CallMethod(obj, "isdecimal", nullptr); 
        bool value = result && PyObject_IsTrue(result); 
        Py_XDECREF(result); 
        return value; 
    }
    
    bool isnumeric() const { 
        PyObject* result = PyObject_CallMethod(obj, "isnumeric", nullptr); 
        bool value = result && PyObject_IsTrue(result); 
        Py_XDECREF(result); 
        return value; 
    }
    
    bool istitle() const { 
        PyObject* result = PyObject_CallMethod(obj, "istitle", nullptr); 
        bool value = result && PyObject_IsTrue(result); 
        Py_XDECREF(result); 
        return value; 
    }
    
    bool isupper() const { 
        PyObject* result = PyObject_CallMethod(obj, "isupper", nullptr); 
        bool value = result && PyObject_IsTrue(result); 
        Py_XDECREF(result); 
        return value; 
    }
    
    bool islower() const { 
        PyObject* result = PyObject_CallMethod(obj, "islower", nullptr); 
        bool value = result && PyObject_IsTrue(result); 
        Py_XDECREF(result); 
        return value; 
    }

    // Search methods
    long find(const Str& substring) const { 
        PyObject* result = PyObject_CallMethod(obj, "find", "O", substring.get_obj()); 
        long value = result ? PyLong_AsLong(result) : -1; 
        Py_XDECREF(result); 
        return value; 
    }
    
    long rfind(const Str& substring) const { 
        PyObject* result = PyObject_CallMethod(obj, "rfind", "O", substring.get_obj()); 
        long value = result ? PyLong_AsLong(result) : -1; 
        Py_XDECREF(result); 
        return value; 
    }
    
    long index(const Str& substring) const { 
        PyObject* result = PyObject_CallMethod(obj, "index", "O", substring.get_obj()); 
        long value = result ? PyLong_AsLong(result) : -1; 
        Py_XDECREF(result); 
        return value; 
    }
    
    long rindex(const Str& substring) const { 
        PyObject* result = PyObject_CallMethod(obj, "rindex", "O", substring.get_obj()); 
        long value = result ? PyLong_AsLong(result) : -1; 
        Py_XDECREF(result); 
        return value; 
    }
    
    Str replace(const Str& old_str, const Str& new_str) const { 
        PyObject* result = PyObject_CallMethod(obj, "replace", "OO", old_str.get_obj(), new_str.get_obj()); 
        return Str(result); 
    }

    // Split and join methods
    std::vector<Str> split(const Str& separator = "") const {
        PyObject* result = separator.str().empty() 
            ? PyObject_CallMethod(obj, "split", nullptr) 
            : PyObject_CallMethod(obj, "split", "O", separator.get_obj());
            
        std::vector<Str> output;
        if (!result) { 
            PyErr_Clear(); 
            return output; 
        }
        
        if (PyList_Check(result)) {
            Py_ssize_t size = PyList_Size(result);
            for (Py_ssize_t i = 0; i < size; ++i) {
                PyObject* item = PyList_GetItem(result, i); 
                Py_XINCREF(item); 
                output.emplace_back(item); 
            }
        }
        
        Py_XDECREF(result);
        return output;
    }

    Str join(const std::vector<Str>& sequence) const {
        PyObject* list_obj = PyList_New(sequence.size());
        if (!list_obj) return Str();
        
        for (Py_ssize_t i = 0; i < sequence.size(); ++i) {
            Py_XINCREF(sequence[i].get_obj()); 
            PyList_SetItem(list_obj, i, sequence[i].get_obj()); 
        }
        
        PyObject* result = PyObject_CallMethod(obj, "join", "O", list_obj);
        Py_XDECREF(list_obj);
        
        return Str(result);
    }

    // Length and indexing
    long len() const { return obj ? PyUnicode_GetLength(obj) : 0; }

    Str operator[](long index) const {
        Py_ssize_t length = len(); 
        if (index < 0) index += length; 
        if (index < 0 || index >= length) return Str();
        
        Py_UCS4 character = PyUnicode_ReadChar(obj, index);
        PyObject* temp = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &character, 1);
        Str result(temp); 
        Py_XDECREF(temp); 
        return result;
    }

    // String concatenation and repetition
    Str operator+(const Str& other) const {
        PyObject* result = PyUnicode_Concat(obj, other.get_obj());
        return Str(result);
    }

    Str& operator+=(const Str& other) {
        PyObject* result = PyUnicode_Concat(obj, other.get_obj());
        if (result) { 
            Py_XDECREF(obj); 
            obj = result; 
        }
        return *this;
    }

    Str operator*(long n) const {
        PyObject* result = PySequence_Repeat(obj, n);
        return Str(result);
    }

    Str& operator*=(long n) {
        PyObject* result = PySequence_Repeat(obj, n);
        if (result) { 
            Py_XDECREF(obj); 
            obj = result; 
        }
        return *this;
    }

    // Comparison operators
    bool operator==(const Str& other) const { 
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_EQ) == 1; 
    }
    
    bool operator!=(const Str& other) const { 
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_NE) == 1; 
    }
    
    bool operator<(const Str& other) const { 
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_LT) == 1; 
    }
    
    bool operator>(const Str& other) const { 
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_GT) == 1; 
    }
    
    bool operator<=(const Str& other) const { 
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_LE) == 1; 
    }
    
    bool operator>=(const Str& other) const { 
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_GE) == 1; 
    }

    // Contains check
    bool contains(const Str& substring) const { 
        int result = PySequence_Contains(obj, substring.get_obj()); 
        return result == 1; 
    }

    // String representation
    std::string str() const {
        if (!obj) return "None";
        
        PyObject* string_obj = PyObject_Str(obj);
        if (!string_obj) return "None";
        
        const char* c_str = PyUnicode_AsUTF8(string_obj);
        std::string result = c_str ? c_str : "";
        Py_XDECREF(string_obj);
        
        return result;
    }
};

// ================== List Class ==================
class List : public PyObj {
public:
    List() : PyObj(PyList_New(0)) {}
    explicit List(PyObject* o) : PyObj(o) {}
    
    List(const std::initializer_list<PyObj>& elements)
        : PyObj(PyList_New(elements.size())) {
        if (!obj) return;
        
        size_t index = 0;
        for (const auto& value : elements) {
            Py_XINCREF(value.get_obj());
            PyList_SetItem(obj, index++, value.get_obj());
        }
    }

    // List manipulation methods
    void append(const PyObj& value) { 
        PyList_Append(obj, value.get_obj()); 
    }
    
    void extend(const List& other) {
        PyObject* result = PyObject_CallMethod(obj, "extend", "O", other.get_obj());
        Py_XDECREF(result);
    }
    
    void insert(long index, const PyObj& value) {
        PyObject* result = PyObject_CallMethod(obj, "insert", "lO", index, value.get_obj());
        Py_XDECREF(result);
    }
    
    void remove(const PyObj& value) {
        PyObject* result = PyObject_CallMethod(obj, "remove", "O", value.get_obj());
        Py_XDECREF(result);
    }
    
    PyObj pop(int index = -1) {
        if (!obj) return PyObj();
        
        Py_ssize_t size = PyList_Size(obj);
        if (index < 0) index += size;
        if (index < 0 || index >= size) return PyObj();
        
        PyObject* item = PyList_GetItem(obj, index);
        Py_XINCREF(item);
        PySequence_DelItem(obj, index);
        
        return PyObj(item);
    }
    
    void clear() {
        PyObject* result = PyObject_CallMethod(obj, "clear", nullptr);
        Py_XDECREF(result);
    }
    
    long index(const PyObj& value) const {
        PyObject* result = PyObject_CallMethod(obj, "index", "O", value.get_obj());
        long index_value = result ? PyLong_AsLong(result) : -1;
        Py_XDECREF(result);
        
        return index_value;
    }
    
    long count(const PyObj& value) const {
        PyObject* result = PyObject_CallMethod(obj, "count", "O", value.get_obj());
        long count_value = result ? PyLong_AsLong(result) : 0;
        Py_XDECREF(result);
        
        return count_value;
    }
    
    void reverse() {
        PyObject* result = PyObject_CallMethod(obj, "reverse", nullptr);
        Py_XDECREF(result);
    }
    
    void sort() {
        PyObject* result = PyObject_CallMethod(obj, "sort", nullptr);
        Py_XDECREF(result);
    }

    // Length and contains
    long len() const { return obj ? PyList_Size(obj) : 0; }

    bool contains(const PyObj& value) const {
        if (!obj) return false;
        
        int result = PySequence_Contains(obj, value.get_obj());
        if (result == -1) { 
            PyErr_Clear(); 
            return false; 
        }
        
        return result == 1;
    }

    // Indexing
    PyObj operator[](long index) const {
        if (!obj) return PyObj();
        
        Py_ssize_t size = PyList_Size(obj);
        if (index < 0) index += size;
        if (index < 0 || index >= size) return PyObj();
        
        PyObject* item = PyList_GetItem(obj, index);
        Py_XINCREF(item);
        
        return PyObj(item);
    }

    bool set(long index, const PyObj& value) {
        if (!obj) return false;
        return PyList_SetItem(obj, index, value.get_obj()) == 0;
    }

    // List operations
    List operator+(const List& other) const {
        if (!obj || !other.get_obj()) return List();
        
        PyObject* result = PySequence_Concat(obj, other.get_obj());
        return List(result);
    }

    List& operator+=(const List& other) {
        extend(other);
        return *this;
    }

    List operator*(long n) const {
        if (!obj) return List();
        
        PyObject* result = PySequence_Repeat(obj, n);
        return List(result);
    }

    List& operator*=(long n) {
        PyObject* result = PySequence_Repeat(obj, n);
        if (result) {
            Py_XDECREF(obj);
            obj = result;
        }
        return *this;
    }

    // Comparison operators
    bool operator==(const List& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_EQ) == 1;
    }
    
    bool operator!=(const List& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_NE) == 1;
    }
    
    bool operator<(const List& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_LT) == 1;
    }
    
    bool operator<=(const List& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_LE) == 1;
    }
    
    bool operator>(const List& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_GT) == 1;
    }
    
    bool operator>=(const List& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_GE) == 1;
    }

    // String representation
    std::string str() const {
        if (!obj) return "None";
        
        PyObject* string_obj = PyObject_Str(obj);
        if (!string_obj) return "None";
        
        const char* c_str = PyUnicode_AsUTF8(string_obj);
        std::string result = c_str ? c_str : "";
        Py_XDECREF(string_obj);
        
        return result;
    }
};

// ================== Tuple Class ==================
class Tuple : public PyObj {
public:
    Tuple() : PyObj(PyTuple_New(0)) {}
    explicit Tuple(PyObject* o) : PyObj(o) {}
    
    Tuple(const std::initializer_list<PyObj>& elements) {
        PyObject* tuple = PyTuple_New(elements.size());
        if (!tuple) {
            obj = nullptr;
            return;
        }
        
        size_t index = 0;
        for (auto& value : elements) {
            Py_XINCREF(value.get_obj());
            PyTuple_SetItem(tuple, index++, value.get_obj());
        }
        obj = tuple;
    }

    // Basic operations
    long len() const { return obj ? PyTuple_Size(obj) : 0; }

    PyObj operator[](long index) const {
        if (!obj) return PyObj();
        
        Py_ssize_t size = PyTuple_Size(obj);
        if (index < 0) index += size;
        if (index < 0 || index >= size) return PyObj();
        
        PyObject* item = PyTuple_GetItem(obj, index);
        Py_XINCREF(item);
        
        return PyObj(item);
    }

    long index(const PyObj& value) const {
        if (!obj) return -1;
        
        Py_ssize_t idx = PySequence_Index(obj, value.get_obj());
        if (idx == -1 && PyErr_Occurred()) { 
            PyErr_Clear(); 
            return -1; 
        }
        
        return idx;
    }

    long count(const PyObj& value) const {
        if (!obj) return 0;
        
        Py_ssize_t count_value = PySequence_Count(obj, value.get_obj());
        if (count_value == -1 && PyErr_Occurred()) { 
            PyErr_Clear(); 
            return 0; 
        }
        
        return count_value;
    }

    bool contains(const PyObj& value) const {
        if (!obj) return false;
        
        int result = PySequence_Contains(obj, value.get_obj());
        if (result == -1) { 
            PyErr_Clear(); 
            return false; 
        }
        
        return result == 1;
    }

    // Tuple operations
    Tuple operator+(const Tuple& other) const {
        if (!obj || !other.get_obj()) return Tuple();
        
        PyObject* result = PySequence_Concat(obj, other.get_obj());
        return Tuple(result);
    }

    Tuple operator*(long n) const {
        if (!obj) return Tuple();
        
        PyObject* result = PySequence_Repeat(obj, n);
        return Tuple(result);
    }

    // Comparison operators
    bool operator==(const Tuple& other) const {
        if (!obj || !other.get_obj()) return false;
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_EQ) == 1;
    }

    bool operator!=(const Tuple& other) const {
        if (!obj || !other.get_obj()) return true;
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_NE) == 1;
    }

    bool operator<(const Tuple& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_LT) == 1;
    }

    bool operator<=(const Tuple& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_LE) == 1;
    }

    bool operator>(const Tuple& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_GT) == 1;
    }

    bool operator>=(const Tuple& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_GE) == 1;
    }

    // Conversion
    List to_list() const {
        if (!obj) return List();
        
        PyObject* list = PySequence_List(obj);
        return List(list);
    }

    // String representation
    std::string str() const {
        if (!obj) return "None";
        
        PyObject* string_obj = PyObject_Str(obj);
        if (!string_obj) return "None";
        
        const char* c_str = PyUnicode_AsUTF8(string_obj);
        std::string result = c_str ? c_str : "";
        Py_XDECREF(string_obj);
        
        return result;
    }
};

// ================== Set Class ==================
class Set : public PyObj {
public:
    Set() : PyObj(PySet_New(nullptr)) {}
    explicit Set(PyObject* o) : PyObj(o) {}
    
    Set(const std::initializer_list<PyObj>& elements) 
        : PyObj(PySet_New(nullptr)) {
        if (!obj) return;
        
        for (auto& value : elements)
            PySet_Add(obj, value.get_obj());
    }

    // Set operations
    void add(const PyObj& value) { 
        PySet_Add(obj, value.get_obj()); 
    }
    
    void clear() { 
        PySet_Clear(obj); 
    }
    
    PyObj pop() { 
        return PyObj(PySet_Pop(obj)); 
    }

    // Basic properties
    long len() const { return obj ? PySet_Size(obj) : 0; }
    bool empty() const { return len() == 0; }

    bool contains(const PyObj& value) const {
        return obj && PySet_Contains(obj, value.get_obj()) == 1;
    }

    // Comparison operators
    bool operator==(const Set& other) const { 
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_EQ) == 1; 
    }
    
    bool operator!=(const Set& other) const { 
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_NE) == 1; 
    }
    
    bool operator<(const Set& other)  const { 
        return issubset(other) && !issuperset(other); 
    }
    
    bool operator>(const Set& other)  const { 
        return issuperset(other) && !issubset(other); 
    }
    
    bool operator<=(const Set& other) const { 
        return issubset(other); 
    }
    
    bool operator>=(const Set& other) const { 
        return issuperset(other); 
    }

    // Set theory operations
    Set union_with(const Set& other) const {
        PyObject* result = PyNumber_Or(obj, other.get_obj());
        return Set(result ? result : PySet_New(nullptr));
    }

    Set intersection(const Set& other) const {
        PyObject* result = PyNumber_And(obj, other.get_obj());
        return Set(result ? result : PySet_New(nullptr));
    }

    Set difference(const Set& other) const {
        PyObject* result = PyNumber_Subtract(obj, other.get_obj());
        return Set(result ? result : PySet_New(nullptr));
    }

    Set symmetric_difference(const Set& other) const {
        PyObject* result = PyNumber_Xor(obj, other.get_obj());
        return Set(result ? result : PySet_New(nullptr));
    }

    // Compound assignment operators
    Set& operator|=(const Set& other) {
        PyObject* result = PyObject_CallMethod(obj, "update", "O", other.get_obj());
        Py_XDECREF(result); 
        return *this;
    }

    Set& operator&=(const Set& other) {
        PyObject* intersection = PyNumber_And(obj, other.get_obj());
        if (intersection) {
            Py_XDECREF(obj);
            obj = intersection;
        }
        return *this;
    }
    
    Set& operator-=(const Set& other) {
        PyObject* difference = PyNumber_Subtract(obj, other.get_obj());
        if (difference) {
            Py_XDECREF(obj);
            obj = difference;
        }
        return *this;
    }
    
    Set& operator^=(const Set& other) {
        PyObject* symmetric_diff = PyNumber_Xor(obj, other.get_obj());
        if (symmetric_diff) {
            Py_XDECREF(obj);
            obj = symmetric_diff;
        }
        return *this;
    }

    // Binary set operators
    Set operator|(const Set& other) const { return union_with(other); }
    Set operator&(const Set& other) const { return intersection(other); }
    Set operator-(const Set& other) const { return difference(other); }
    Set operator^(const Set& other) const { return symmetric_difference(other); }

    // Set relations
    bool issubset(const Set& other) const {
        PyObject* result = PyObject_CallMethod(obj, "issubset", "O", other.get_obj());
        bool is_subset = result && PyObject_IsTrue(result);
        Py_XDECREF(result);
        
        return is_subset;
    }

    bool issuperset(const Set& other) const {
        PyObject* result = PyObject_CallMethod(obj, "issuperset", "O", other.get_obj());
        bool is_superset = result && PyObject_IsTrue(result);
        Py_XDECREF(result);
        
        return is_superset;
    }
};

// ================== Dict Class ==================
class Dict : public PyObj {
public:
    Dict() : PyObj(PyDict_New()) {}
    explicit Dict(PyObject* o) : PyObj(o) {}
    
    Dict(const std::initializer_list<std::pair<PyObj, PyObj>>& elements)
        : PyObj(PyDict_New()) {
        if (!obj) return;
        
        for (auto& pair : elements)
            PyDict_SetItem(obj, pair.first.get_obj(), pair.second.get_obj());
    }

    // Dictionary operations
    void add(const PyObj& key, const PyObj& value) {
        PyDict_SetItem(obj, key.get_obj(), value.get_obj());
    }

    PyObj get(const PyObj& key) const {
        PyObject* result = PyDict_GetItem(obj, key.get_obj());
        Py_XINCREF(result);
        
        return result ? PyObj(result) : PyObj();
    }

    bool contains(const PyObj& key) const {
        return PyDict_Contains(obj, key.get_obj()) == 1;
    }

    PyObj operator[](const PyObj& key) const {
        return get(key);
    }

    // Dictionary setter helper class
    class DictSetter {
        PyObject* dict;
        PyObj key;
        
    public:
        DictSetter(PyObject* d, const PyObj& k) : dict(d), key(k) {}

        void operator=(const PyObj& value) const {
            PyDict_SetItem(dict, key.get_obj(), value.get_obj());
        }

        operator PyObj() const {
            PyObject* result = PyDict_GetItem(dict, key.get_obj());
            Py_XINCREF(result);
            
            return result ? PyObj(result) : PyObj();
        }
    };

    DictSetter operator[](const PyObj& key) {
        return DictSetter(obj, key);
    }

    bool set(const PyObj& key, const PyObj& value) {
        return PyDict_SetItem(obj, key.get_obj(), value.get_obj()) == 0;
    }

    // Dictionary properties
    size_t len() const { return PyDict_Size(obj); }
    bool empty() const { return PyDict_Size(obj) == 0; }
    void clear() { PyDict_Clear(obj); }

    PyObj pop(const PyObj& key) {
        PyObject* value = PyDict_GetItem(obj, key.get_obj());
        if (!value) return PyObj();
        
        Py_XINCREF(value);
        PyDict_DelItem(obj, key.get_obj());
        
        return PyObj(value);
    }

    // Dictionary views
    List keys() const { return List(PyDict_Keys(obj)); }
    List values() const { return List(PyDict_Values(obj)); }
    List items() const { return List(PyDict_Items(obj)); }

    void update(const Dict& other) { 
        PyDict_Update(obj, other.get_obj()); 
    }

    // Comparison operators
    bool operator==(const Dict& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_EQ) == 1;
    }

    bool operator!=(const Dict& other) const {
        return PyObject_RichCompareBool(obj, other.get_obj(), Py_NE) == 1;
    }

    // Dictionary merging
    Dict operator|(const Dict& other) const {
        PyObject* merged = PyDict_Copy(obj);
        if (!merged) return Dict();
        
        PyDict_Update(merged, other.get_obj());
        return Dict(merged);
    }

    Dict& operator|=(const Dict& other) {
        PyDict_Update(obj, other.get_obj());
        return *this;
    }

    Dict operator+(const Dict& other) const {
        PyObject* merged = PyDict_Copy(obj);
        if (!merged) return Dict();
        
        PyDict_Update(merged, other.get_obj());
        return Dict(merged);
    }

    Dict& operator+=(const Dict& other) {
        PyDict_Update(obj, other.get_obj());
        return *this;
    }
};

// ================== Function Class ==================
class Function : public PyObj {
public:
    Function() : PyObj() {}
    Function(PyObject* o) : PyObj(o) {}
    Function(const PyObj& o) : PyObj(o) {}

    // ===== Universal calling method =====
    PyObj call(const std::vector<PyObj>& args = {}, const Dict& kwargs = Dict()) const {
        if (!obj || !PyCallable_Check(obj)) {
            std::cerr << "Error: Object is not callable" << std::endl;
            return PyObj();
        }

        PyObject* py_args = PyTuple_New(args.size());
        for (size_t i = 0; i < args.size(); i++) {
            Py_XINCREF(args[i].get_obj());
            PyTuple_SetItem(py_args, i, args[i].get_obj());
        }

        PyObject* py_kwargs = kwargs.get_obj();
        Py_XINCREF(py_kwargs);

        PyObject* result = PyObject_Call(obj, py_args, py_kwargs);

        Py_DECREF(py_args);
        Py_DECREF(py_kwargs);

        if (!result) {
            PyErr_Print();
            return PyObj();
        }

        return PyObj(result);
    }

    // ===== Universal Call via () =====
    template<typename... Args>
    PyObj operator()(Args&&... args) const {
        std::vector<PyObj> vec;
        Dict kwargs;

        // If the last argument is Dict, consider it kwargs
        if constexpr (sizeof...(args) > 0) {
            auto tuple_args = std::tuple<Args...>(std::forward<Args>(args)...);
            if constexpr (std::is_same_v<
                std::remove_cv_t<std::remove_reference_t<
                    decltype(std::get<sizeof...(args)-1>(tuple_args))
                >>,
                Dict
            >) {
                kwargs = std::get<sizeof...(args)-1>(tuple_args);
                vec = make_args_tuple(tuple_args, std::make_index_sequence<sizeof...(args)-1>{});
            } else {
                vec = make_args_tuple(tuple_args, std::make_index_sequence<sizeof...(args)>{});
            }
        }

        return call(vec, kwargs);
    }

    private:
    template<typename Tuple, std::size_t... I>
    std::vector<PyObj> make_args_tuple(const Tuple& t, std::index_sequence<I...>) const {
        return { PyObj(std::get<I>(t))... };
    }
};

// ================== Formatted String ==================
template<typename T>
std::pair<std::string, std::string> farg(const std::string& name, T&& value) {
    std::ostringstream os; 
    os << value; 
    return {name, os.str()};
}

template<typename... Args>
std::string fstring(const std::string& format, Args&&... args) {
    std::unordered_map<std::string, std::string> named_args;
    std::vector<std::string> positional_args;
    
    auto process_arg = [&](auto&& arg) {
        using ArgType = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<ArgType, std::pair<std::string, std::string>>) {
            named_args.emplace(arg.first, arg.second);
        } else {
            std::ostringstream os; 
            os << arg; 
            positional_args.push_back(os.str());
        }
    };
    
    (process_arg(std::forward<Args>(args)), ...);
    
    std::string output;
    output.reserve(format.size() + 32);
    size_t position_index = 0;
    
    for (size_t i = 0; i < format.size(); ++i) {
        if (format[i] == '{') {
            // Handle escaped braces
            if (i + 1 < format.size() && format[i + 1] == '{') {
                output.push_back('{');
                ++i;
                continue;
            }
            
            // Find closing brace
            size_t j = format.find('}', i + 1);
            if (j == std::string::npos) {
                output.push_back('{');
                continue;
            }
            
            std::string key = format.substr(i + 1, j - i - 1);
            
            // Handle empty key (positional) or named key
            if (key.empty()) {
                if (position_index < positional_args.size()) {
                    output += positional_args[position_index++];
                } else {
                    output += "{}";
                }
            } else {
                auto it = named_args.find(key);
                if (it != named_args.end()) {
                    output += it->second;
                } else {
                    output += "{" + key + "}";
                }
            }
            
            i = j;
        } else if (format[i] == '}') {
            // Handle escaped braces
            if (i + 1 < format.size() && format[i + 1] == '}') {
                output.push_back('}');
                ++i;
            } else {
                output.push_back('}');
            }
        } else {
            output.push_back(format[i]);
        }
    }
    
    return output;
}

// ================== Pretty Print Implementation ==================
namespace detail {
inline void pretty_print(std::ostream& os, const PyObj& obj, int indent = 0) {
    if (!obj.get_obj()) { 
        os << "None"; 
        return; 
    }
    
    PyObject* object = obj.get_obj();
    
    // Handle basic types
    if (PyBool_Check(object)) { 
        os << (object == Py_True ? "True" : "False"); 
        return; 
    }
    
    if (PyLong_Check(object)) { 
        os << PyLong_AsLong(object); 
        return; 
    }
    
    if (PyFloat_Check(object)) { 
        os << PyFloat_AsDouble(object); 
        return; 
    }
    
    if (PyUnicode_Check(object)) { 
        const char* str = PyUnicode_AsUTF8(object); 
        os << "\"" << (str ? str : "") << "\""; 
        return; 
    }

    // Handle lists
    if (PyList_Check(object)) {
        Py_ssize_t size = PyList_Size(object); 
        if (size == 0) {
            os << "[]"; 
            return;
        }
        
        os << "[\n"; 
        for (Py_ssize_t i = 0; i < size; ++i) { 
            for (int j = 0; j < indent + 4; ++j) os << ' '; 
            
            PyObject* item = PyList_GetItem(object, i); 
            Py_XINCREF(item); 
            pretty_print(os, PyObj(item), indent + 4); 
            Py_XDECREF(item); 
            
            if (i != size - 1) os << ","; 
            os << "\n"; 
        } 
        
        for (int j = 0; j < indent; ++j) os << ' '; 
        os << "]"; 
        return;
    }
    
    // Handle tuples
    if (PyTuple_Check(object)) { 
        Py_ssize_t size = PyTuple_Size(object); 
        if (size == 0) {
            os << "()"; 
            return;
        }
        
        os << "(\n"; 
        for (Py_ssize_t i = 0; i < size; ++i) { 
            for (int j = 0; j < indent + 4; ++j) os << ' '; 
            
            PyObject* item = PyTuple_GetItem(object, i); 
            Py_XINCREF(item); 
            pretty_print(os, PyObj(item), indent + 4); 
            Py_XDECREF(item); 
            
            if (i != size - 1) os << ","; 
            os << "\n"; 
        } 
        
        for (int j = 0; j < indent; ++j) os << ' '; 
        os << ")"; 
        return;
    }
    
    // Handle dictionaries
    if (PyDict_Check(object)) { 
        Py_ssize_t pos = 0; 
        PyObject *key, *value; 
        
        if (PyDict_Size(object) == 0) {
            os << "{}"; 
            return;
        }
        
        os << "{\n"; 
        bool first = true; 
        
        while (PyDict_Next(object, &pos, &key, &value)) { 
            if (!first) os << ",\n"; 
            first = false; 
            
            for (int j = 0; j < indent + 4; ++j) os << ' '; 
            
            Py_XINCREF(key); 
            pretty_print(os, PyObj(key), indent + 4); 
            Py_XDECREF(key); 
            
            os << ": "; 
            
            Py_XINCREF(value); 
            pretty_print(os, PyObj(value), indent + 4); 
            Py_XDECREF(value); 
        } 
        
        os << "\n"; 
        for (int j = 0; j < indent; ++j) os << ' '; 
        os << "}"; 
        return;
    }
    
    // Handle sets
    if (PySet_Check(object)) { 
        PyObject* iterator = PyObject_GetIter(object); 
        if (!iterator) { 
            os << "<set>"; 
            return;
        } 
        
        PyObject* item;
        bool first = true; 
        os << "{\n"; 
        
        while ((item = PyIter_Next(iterator))) { 
            if (!first) os << ",\n"; 
            first = false; 
            
            for (int j = 0; j < indent + 4; ++j) os << ' '; 
            pretty_print(os, PyObj(item), indent + 4); 
            Py_XDECREF(item);
        } 
        
        Py_XDECREF(iterator); 
        os << "\n"; 
        for (int j = 0; j < indent; ++j) os << ' '; 
        os << "}"; 
        return;
    }
    
    // Fallback: use repr
    PyObject* repr = PyObject_Repr(object); 
    if (repr) { 
        const char* str = PyUnicode_AsUTF8(repr); 
        os << (str ? str : "<repr-error>"); 
        Py_XDECREF(repr); 
    } else { 
        PyErr_Clear(); 
        os << "<PyObj>"; 
    }
}

} // end namespace detail

// ================== Output Operators ==================
inline std::ostream& operator<<(std::ostream& os, const PyObj& obj) {
    if (!obj.get_obj()) { 
        os << "None"; 
        return os; 
    }
    
    PyObject* repr = PyObject_Repr(obj.get_obj());
    if (repr) { 
        os << PyUnicode_AsUTF8(repr); 
        Py_XDECREF(repr); 
    } else { 
        PyErr_Clear(); 
        os << "<PyObj>"; 
    }
    
    return os;
}

// ================== Global Functions ==================
inline Str type(const PyObj& obj) { 
    if (!obj.get_obj()) return Str("<NoneType>"); 
    
    PyObject* type_obj = PyObject_Type(obj.get_obj()); 
    if (!type_obj) return Str("<unknown>"); 
    
    PyObject* name = PyObject_GetAttrString(type_obj, "__name__"); 
    Py_XDECREF(type_obj); 
    
    if (!name) return Str("<unknown>"); 
    
    const char* type_name = PyUnicode_AsUTF8(name); 
    Str result(type_name ? type_name : "<unknown>"); 
    Py_XDECREF(name); 
    
    return result; 
}

inline long len(const PyObj& obj) {
    if (!obj.get_obj()) return 0;
    
    if (obj.is_str()) return PyUnicode_GetLength(obj.get_obj());
    if (obj.is_list()) return PyList_Size(obj.get_obj());
    if (obj.is_tuple()) return PyTuple_Size(obj.get_obj());
    if (obj.is_set()) return PySet_Size(obj.get_obj());
    if (obj.is_dict()) return PyDict_Size(obj.get_obj());
    
    return 0;
}

inline List sorted(const PyObj& sequence) {
    if (!sequence.get_obj()) return List();
    
    PyObject* temp_list = PySequence_List(sequence.get_obj());
    if (!temp_list) { 
        PyErr_Clear(); 
        return List(); 
    }
    
    PyList_Sort(temp_list);
    List output;
    Py_ssize_t size = PyList_Size(temp_list);
    
    for (Py_ssize_t i = 0; i < size; ++i) {
        PyObject* item = PyList_GetItem(temp_list, i); 
        Py_XINCREF(item);
        PyList_Append(output.get_obj(), item); 
        Py_XDECREF(item);
    }
    
    Py_XDECREF(temp_list);
    return output;
}

inline PyObj reversed(const PyObj& obj) {
    if (!obj.get_obj()) return PyObj();
    
    if (obj.is_str()) {
        PyObject* reversed_obj = PyObject_CallMethod(obj.get_obj(), "__reversed__", nullptr);
        if (!reversed_obj) { 
            PyErr_Clear(); 
            return PyObj(); 
        }
        
        PyObject* sequence = PySequence_List(reversed_obj); 
        Py_XDECREF(reversed_obj);
        PyObject* joined = PyUnicode_Join(PyUnicode_FromString(""), sequence); 
        Py_XDECREF(sequence);
        
        return PyObj(joined);
    }
    
    if (obj.is_list() || obj.is_tuple()) {
        PyObject* reversed_obj = PyObject_CallMethod(obj.get_obj(), "__reversed__", nullptr);
        if (!reversed_obj) { 
            PyErr_Clear(); 
            return PyObj(); 
        }
        
        PyObject* sequence = PySequence_List(reversed_obj); 
        Py_XDECREF(reversed_obj);
        
        if (obj.is_tuple()) {
            PyObject* tuple = PySequence_Tuple(sequence); 
            Py_XDECREF(sequence); 
            return PyObj(tuple); 
        }
        
        return PyObj(sequence);
    }
    
    PyErr_Clear();
    return PyObj();
}

inline void PyObj::pretty_print(std::ostream& os, const PyObj& obj, int indent) { 
    detail::pretty_print(os, obj, indent); 
}

inline void print(const PyObj& obj = Str("\\n")) {
    if (obj.str() != "\\n")
        std::cout << obj << std::endl;
    else
        std::cout << std::endl;
}

inline void pprint(const PyObj& obj, int indent = 0) {
    PyObj::pretty_print(std::cout, obj, indent);
    std::cout << std::endl;
}

// ================== Utility Functions ==================
inline bool all(const List& list) { 
    for (long i = 0; i < list.len(); ++i) 
        if (!PyObject_IsTrue(list[i].get_obj())) 
            return false; 
    return true; 
}

inline bool any(const List& list) { 
    for (long i = 0; i < list.len(); ++i) 
        if (PyObject_IsTrue(list[i].get_obj())) 
            return true; 
    return false; 
}

inline List map(const PyObj& function, const List& list) { 
    List output; 
    for (long i = 0; i < list.len(); ++i) { 
        PyObject* result = PyObject_CallFunctionObjArgs(function.get_obj(), list[i].get_obj(), nullptr); 
        if (result) { 
            PyList_Append(output.get_obj(), result); 
            Py_XDECREF(result); 
        } else {
            PyErr_Clear();
        }
    } 
    return output; 
}

inline PyObj run_code(PyObject* codeObj, const std::string& source_name) {
    PyObject* globals = PyDict_New();
    if (!globals) return PyObj();
    
    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());

    PyObject* result = PyEval_EvalCode(codeObj, globals, globals);

    if (!result) {
        PyErr_Print();
        Py_DECREF(globals);
        return PyObj();
    }
    Py_DECREF(result);

    const char* system_keys[] = {
        "__builtins__", 
        "__name__", 
        "__doc__",
        "__package__", 
        "__loader__", 
        "__spec__", 
        "__annotations__"
    };

    for (const char* key : system_keys) {
        PyObject* pyKey = PyUnicode_FromString(key);
        if (pyKey && PyDict_Contains(globals, pyKey))
            PyDict_DelItem(globals, pyKey);
        Py_XDECREF(pyKey);
    }

    return PyObj(globals);
}

inline void exec(const std::string& code) { 
    PyRun_SimpleString(code.c_str()); 
}

inline PyObj eval(const std::string& code) {
    PyObject* codeObj = Py_CompileString(code.c_str(), "<string>", Py_file_input);
    if (!codeObj) {
        PyErr_Print();
        return PyObj();
    }
    PyObj result = run_code(codeObj, "<string>");
    Py_DECREF(codeObj);
    return result;
}

inline void run_file(const std::string& filename) { 
    FILE* file = fopen(filename.c_str(), "r"); 
    if (!file) { 
        std::cerr << "run_file: cannot open " << filename << "\n"; 
        return;
    } 
    
    PyRun_SimpleFile(file, filename.c_str()); 
    fclose(file); 
}

inline PyObj run_file_result(const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "r");

    if (!file) {
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, filename.c_str());
        PyErr_Print();
        return PyObj();
    }

    std::string code;

    {
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);

        if (size <= 0) {
            fclose(file);
            return PyObj();
        }

        code.resize(size);
        size_t read = fread(&code[0], 1, size, file);
        fclose(file);

        if (read != static_cast<size_t>(size)) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to read entire file");
            PyErr_Print();
            return PyObj();
        }
    }

    PyObject* codeObj = Py_CompileString(code.c_str(), filename.c_str(), Py_file_input);
    
    if (!codeObj) {
        PyErr_Print();
        return PyObj();
    }

    PyObj result = run_code(codeObj, filename);

    Py_DECREF(codeObj);
    return result;
}

// ================== JSON Functions ==================
inline bool json_dump(const PyObj& obj, const Str& filename, int indent = -1) {
    PyObject* json_module = PyImport_ImportModule("json");
    if (!json_module) { 
        PyErr_Clear(); 
        return false; 
    }

    PyObject* dump_func = PyObject_GetAttrString(json_module, "dump");
    Py_XDECREF(json_module);
    
    if (!dump_func) { 
        PyErr_Clear(); 
        return false; 
    }

    PyObject* file_obj = PyObject_CallMethod(
        PyImport_ImportModule("builtins"),
        "open", "ss", filename.str().c_str(), "w"
    );
    
    if (!file_obj) { 
        PyErr_Clear(); 
        Py_XDECREF(dump_func); 
        return false; 
    }

    PyObject* kwargs = PyDict_New();
    if (indent >= 0) {
        PyDict_SetItemString(kwargs, "indent", PyLong_FromLong(indent));
    }

    PyObject* result = PyObject_Call(dump_func, Py_BuildValue("(OO)", obj.get_obj(), file_obj), kwargs);
    Py_XDECREF(result);

    PyObject* close_result = PyObject_CallMethod(file_obj, "close", nullptr);
    Py_XDECREF(close_result);

    Py_XDECREF(file_obj);
    Py_XDECREF(kwargs);
    Py_XDECREF(dump_func);
    
    return true;
}

inline Str json_dumps(const PyObj& obj) {
    PyObject* json_module = PyImport_ImportModule("json"); 
    if (!json_module) { 
        PyErr_Clear(); 
        return Str(); 
    }

    PyObject* dumps_func = PyObject_GetAttrString(json_module, "dumps"); 
    Py_XDECREF(json_module); 
    if (!dumps_func) { 
        PyErr_Clear(); 
        return Str(); 
    }

    PyObject* result = PyObject_CallFunctionObjArgs(dumps_func, obj.get_obj(), nullptr); 
    Py_XDECREF(dumps_func);
    
    Str output(result); 
    Py_XDECREF(result); 
    return output;
}

inline PyObj json_load(const Str& filename) {
    PyObject* json_module = PyImport_ImportModule("json");
    if (!json_module) { 
        PyErr_Clear(); 
        return PyObj(); 
    }

    PyObject* load_func = PyObject_GetAttrString(json_module, "load");
    Py_XDECREF(json_module);
    if (!load_func) { 
        PyErr_Clear(); 
        return PyObj(); 
    }

    PyObject* file_obj = PyObject_CallMethod(
        PyImport_ImportModule("builtins"),
        "open", "ss", filename.str().c_str(), "r"
    );
    
    if (!file_obj) { 
        PyErr_Clear(); 
        Py_XDECREF(load_func); 
        return PyObj(); 
    }

    PyObject* result = PyObject_CallFunctionObjArgs(load_func, file_obj, nullptr);
    PyObject* close_result = PyObject_CallMethod(file_obj, "close", nullptr);
    Py_XDECREF(close_result);
    Py_XDECREF(file_obj);
    Py_XDECREF(load_func);

    return result ? PyObj(result) : PyObj();
}

inline PyObj json_loads(const Str& json_string) {
    PyObject* json_module = PyImport_ImportModule("json"); 
    if (!json_module) { 
        PyErr_Clear(); 
        return PyObj(); 
    }

    PyObject* loads_func = PyObject_GetAttrString(json_module, "loads"); 
    Py_XDECREF(json_module); 
    if (!loads_func) { 
        PyErr_Clear(); 
        return PyObj(); 
    }
    
    PyObject* result = PyObject_CallFunctionObjArgs(loads_func, json_string.get_obj(), nullptr); 
    Py_XDECREF(loads_func);
    
    return result ? PyObj(result) : PyObj();
}


} // end namespace py
