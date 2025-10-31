#pragma once

#include <Python.h>
#include <string>
#include <vector>
#include <iostream>
#include <initializer_list>

namespace py {

// ================== PyObj ==================
class PyObj {
protected:
    PyObject* obj;
public:
    PyObj(PyObject* o = nullptr) : obj(o) { Py_XINCREF(obj); }
    PyObj(const PyObj& other) : obj(other.obj) { Py_XINCREF(obj); }
    PyObj& operator=(const PyObj& other) {
        if (this != &other) {
            Py_XDECREF(obj);
            obj = other.obj;
            Py_XINCREF(obj);
        }
        return *this;
    }

    PyObj(int v) : obj(PyLong_FromLong(v)) {}
    PyObj(long v) : obj(PyLong_FromLong(v)) {}
    PyObj(double v) : obj(PyFloat_FromDouble(v)) {}
    PyObj(bool v) : obj(v ? Py_True : Py_False) { Py_XINCREF(obj); }
    PyObj(const std::string& s) : obj(PyUnicode_FromString(s.c_str())) {}
    PyObj(const char* s) : obj(PyUnicode_FromString(s)) {}

    virtual ~PyObj() { Py_XDECREF(obj); }
    PyObject* get_obj() const { return obj; }

    bool is_empty() const {
        if (!obj) return true;
        if (PyUnicode_Check(obj)) return PyUnicode_GetLength(obj) == 0;
        if (PyList_Check(obj)) return PyList_Size(obj) == 0;
        if (PyDict_Check(obj)) return PyDict_Size(obj) == 0;
        if (PySet_Check(obj)) return PySet_Size(obj) == 0;
        return false;
    }
};

// ================== Str ==================
class Str : public PyObj {
public:
    Str() : PyObj("") {}
    Str(const std::string& s) : PyObj(s) {}
    Str(const char* s) : PyObj(s) {}
    Str(PyObject* o) : PyObj(o) {}

    Str capitalize() const { return call_method("capitalize"); }
    Str upper() const { return call_method("upper"); }
    Str lower() const { return call_method("lower"); }
    Str strip() const { return call_method("strip"); }

    Str replace(const std::string& old_val, const std::string& new_val) const {
        PyObject* r = PyObject_CallMethod(obj, (char*)"replace", (char*)"ss", old_val.c_str(), new_val.c_str());
        return Str(r ? r : PyUnicode_FromString(""));
    }

    std::vector<Str> split(const std::string& sep = "") const {
        PyObject* r = sep.empty() ? PyObject_CallMethod(obj, (char*)"split", nullptr)
                                   : PyObject_CallMethod(obj, (char*)"split", (char*)"s", sep.c_str());
        std::vector<Str> res;
        if (r && PyList_Check(r)) {
            Py_ssize_t n = PyList_Size(r);
            for (Py_ssize_t i = 0; i < n; ++i) {
                PyObject* item = PyList_GetItem(r, i);
                Py_XINCREF(item);
                res.emplace_back(item);
            }
        }
        Py_XDECREF(r);
        return res;
    }

    Str join(const std::vector<Str>& l) const {
        PyObject* pylist = PyList_New(l.size());
        for (size_t i = 0; i < l.size(); ++i) {
            Py_XINCREF(l[i].get_obj());
            PyList_SetItem(pylist, i, l[i].get_obj());
        }
        PyObject* r = PyObject_CallMethod(obj, (char*)"join", (char*)"O", pylist);
        Py_XDECREF(pylist);
        return Str(r);
    }

    long len() const { return PyUnicode_Check(obj) ? PyUnicode_GetLength(obj) : 0; }

    Str operator[](long index) const {
        if (!PyUnicode_Check(obj)) return Str();
        Py_UCS4 ch = PyUnicode_ReadChar(obj, index);
        PyObject* tmp = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &ch, 1);
        Str res(PyUnicode_AsUTF8(tmp));
        Py_XDECREF(tmp);
        return res;
    }

private:
    Str call_method(const char* name) const {
        PyObject* r = PyObject_CallMethod(obj, (char*)name, nullptr);
        return Str(r);
    }
};

// ================== List ==================
class List : public PyObj {
public:
    List() : PyObj(PyList_New(0)) {}
    List(const std::initializer_list<PyObj>& l) : PyObj(PyList_New(l.size())) {
        size_t idx = 0;
        for (const auto& v : l) {
            Py_XINCREF(v.get_obj());
            PyList_SetItem(obj, idx++, v.get_obj());
        }
    }
    List(const std::vector<PyObj>& l) : PyObj(PyList_New(l.size())) {
        for (size_t i = 0; i < l.size(); ++i) {
            Py_XINCREF(l[i].get_obj());
            PyList_SetItem(obj, i, l[i].get_obj());
        }
    }

    void append(const PyObj& val) { PyList_Append(obj, val.get_obj()); }
    PyObj pop(int index = -1) {
        Py_ssize_t n = PyList_Size(obj);
        if (index < 0) index += n;
        if (index < 0 || index >= n) return PyObj();
        PyObject* item = PyList_GetItem(obj, index);
        Py_XINCREF(item);
        PySequence_DelItem(obj, index);
        return PyObj(item);
    }
    long len() const { return PyList_Size(obj); }
    bool is_empty() const { return PyList_Size(obj) == 0; }

    PyObj operator[](long index) const {
        if (!PyList_Check(obj)) return PyObj();
        Py_ssize_t n = PyList_Size(obj);
        if (index < 0) index += n;
        if (index < 0 || index >= n) return PyObj();
        PyObject* item = PyList_GetItem(obj, index);
        Py_XINCREF(item);
        return PyObj(item);
    }

    void sort() {
        PyObject* r = PyObject_CallMethod(obj, (char*)"sort", nullptr);
        Py_XDECREF(r);
    }

    void reverse() {
        PyObject* r = PyObject_CallMethod(obj, (char*)"reverse", nullptr);
        Py_XDECREF(r);
    }
};

// ================== Tuple ==================
class Tuple : public PyObj {
public:
    Tuple() : PyObj(PyTuple_New(0)) {}
    Tuple(const std::initializer_list<PyObj>& l) : PyObj(PyTuple_New(l.size())) {
        size_t idx = 0;
        for (const auto& v : l) {
            Py_XINCREF(v.get_obj());
            PyTuple_SetItem(obj, idx++, v.get_obj());
        }
    }
    Tuple(PyObject* o) : PyObj(o) {
        if (!PyTuple_Check(o)) {
            Py_XDECREF(obj);
            obj = PyTuple_New(0);
        }
    }

    long len() const { return PyTuple_Size(obj); }
    PyObj operator[](long index) const {
        if (!PyTuple_Check(obj)) return PyObj();
        Py_ssize_t n = PyTuple_Size(obj);
        if (index < 0) index += n;
        if (index < 0 || index >= n) return PyObj();
        PyObject* item = PyTuple_GetItem(obj, index);
        Py_XINCREF(item);
        return PyObj(item);
    }
};

// ================== Dict ==================
class Dict : public PyObj {
public:
    Dict() : PyObj(PyDict_New()) {}
    Dict(const std::initializer_list<std::pair<PyObj, PyObj>>& l) : Dict() {
        for (auto& p : l) add(p.first, p.second);
    }
    Dict(PyObject* o) : PyObj(o) {
        if (!PyDict_Check(o)) {
            Py_XDECREF(obj);
            obj = PyDict_New();
        }
    }

    void add(const PyObj& key, const PyObj& val) { PyDict_SetItem(obj, key.get_obj(), val.get_obj()); }

    PyObj get(const PyObj& key) const {
        PyObject* r = PyDict_GetItem(obj, key.get_obj());
        Py_XINCREF(r);
        return r ? PyObj(r) : PyObj();
    }

    PyObj pop(const PyObj& key) {
        PyObject* r = PyDict_GetItem(obj, key.get_obj());
        if (!r) return PyObj();
        Py_XINCREF(r);
        PyDict_DelItem(obj, key.get_obj());
        return PyObj(r);
    }

    List keys() const {
        PyObject* r = PyDict_Keys(obj);
        std::vector<PyObj> v;
        if (r && PyList_Check(r)) {
            Py_ssize_t n = PyList_Size(r);
            for (Py_ssize_t i = 0; i < n; ++i) {
                PyObject* item = PyList_GetItem(r, i);
                Py_XINCREF(item);
                v.push_back(item);
            }
        }
        return List(v);
    }

    List values() const {
        PyObject* r = PyDict_Values(obj);
        std::vector<PyObj> v;
        if (r && PyList_Check(r)) {
            Py_ssize_t n = PyList_Size(r);
            for (Py_ssize_t i = 0; i < n; ++i) {
                PyObject* item = PyList_GetItem(r, i);
                Py_XINCREF(item);
                v.push_back(item);
            }
        }
        return List(v);
    }

    void clear() { PyDict_Clear(obj); }
    long len() const { return PyDict_Size(obj); }

    // перегрузки для удобства
    PyObj operator[](const PyObj& key) const { return get(key); }
    PyObj operator[](const char* key) const { return get(PyObj(key)); }
    PyObj operator[](const std::string& key) const { return get(PyObj(key)); }
};

// ================== Set ==================
class Set : public PyObj {
public:
    Set() : PyObj(PySet_New(nullptr)) {}
    Set(const std::initializer_list<PyObj>& l) : Set() {
        for (auto& v : l) add(v);
    }

    void add(const PyObj& val) { PySet_Add(obj, val.get_obj()); }
    PyObj pop() { return PyObj(PySet_Pop(obj)); }
    long len() const { return PySet_Size(obj); }
    void clear() { PySet_Clear(obj); }
    void discard(const PyObj& val) { PySet_Discard(obj, val.get_obj()); }

    bool contains(const PyObj& val) const { return PySet_Contains(obj, val.get_obj()) == 1; }
};

// ================== Python init/exit ==================
inline void init_python() { if (!Py_IsInitialized()) Py_Initialize(); }
inline void exit_python() { if (Py_IsInitialized()) Py_FinalizeEx(); }

// ================== ostream ==================
inline std::ostream& operator<<(std::ostream& os, const PyObj& obj) {
    if (!obj.get_obj()) os << "None";
    else if (PyUnicode_Check(obj.get_obj())) os << "\"" << PyUnicode_AsUTF8(obj.get_obj()) << "\"";
    else if (PyLong_Check(obj.get_obj())) os << PyLong_AsLong(obj.get_obj());
    else if (PyFloat_Check(obj.get_obj())) os << PyFloat_AsDouble(obj.get_obj());
    else if (PyList_Check(obj.get_obj())) {
        os << "[";
        Py_ssize_t n = PyList_Size(obj.get_obj());
        for (Py_ssize_t i = 0; i < n; ++i) {
            if (i != 0) os << ", ";
            os << PyObj(PyList_GetItem(obj.get_obj(), i));
        }
        os << "]";
    }
    else if (PyTuple_Check(obj.get_obj())) {
        os << "(";
        Py_ssize_t n = PyTuple_Size(obj.get_obj());
        for (Py_ssize_t i = 0; i < n; ++i) {
            if (i != 0) os << ", ";
            os << PyObj(PyTuple_GetItem(obj.get_obj(), i));
        }
        os << ")";
    }
    else if (PyDict_Check(obj.get_obj())) {
        os << "{";
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        bool first = true;
        while (PyDict_Next(obj.get_obj(), &pos, &key, &value)) {
            if (!first) os << ", "; else first = false;
            os << PyObj(key) << ": " << PyObj(value);
        }
        os << "}";
    }
    else if (PySet_Check(obj.get_obj())) {
        os << "{";
        PyObject* iter = PyObject_GetIter(obj.get_obj());
        bool first = true;
        PyObject* item;
        while ((item = PyIter_Next(iter))) {
            if (!first) os << ", "; else first = false;
            os << PyObj(item);
        }
        Py_XDECREF(iter);
        os << "}";
    }
    else os << "<PyObj>";
    return os;
}

// ================== Global functions ==================
inline bool all(const List& l) {
    for (long i = 0; i < l.len(); ++i)
        if (!PyObject_IsTrue(l[i].get_obj())) return false;
    return true;
}

inline bool any(const List& l) {
    for (long i = 0; i < l.len(); ++i)
        if (PyObject_IsTrue(l[i].get_obj())) return true;
    return false;
}

inline List map(const PyObj& func, const List& l) {
    List res;
    for (long i = 0; i < l.len(); ++i) {
        PyObject* r = PyObject_CallFunctionObjArgs(func.get_obj(), l[i].get_obj(), nullptr);
        if (r) res.append(PyObj(r));
        Py_XDECREF(r);
    }
    return res;
}

inline void exec(const std::string& code) {
    PyRun_SimpleString(code.c_str());
}

inline PyObj eval(const std::string& code) {
    PyObject* globals = PyDict_New();
    PyObject* locals  = globals;

    PyObject* result = PyRun_String(code.c_str(), Py_eval_input, globals, locals);
    if (!result) { PyErr_Print(); Py_XDECREF(globals); return PyObj(); }

    Py_XDECREF(globals);
    return PyObj(result);
}

inline void run_file(const std::string& filename) {
    FILE* f = fopen(filename.c_str(), "r");
    if (f) { PyRun_SimpleFile(f, filename.c_str()); fclose(f); }
}

inline Dict run_file_result(const std::string& filename) {
    FILE* f = fopen(filename.c_str(), "r");
    if (!f) return Dict();

    PyObject* globals = PyDict_New();
    PyObject* locals = globals;

    PyRun_File(f, filename.c_str(), Py_file_input, globals, locals);
    fclose(f);

    Dict result;
    PyObject *key, *value;
    Py_ssize_t pos = 0;
    while (PyDict_Next(globals, &pos, &key, &value)) {
        if (!PyUnicode_Check(key)) continue;
        const char* k = PyUnicode_AsUTF8(key);
        if (!k) continue;
        std::string key_str(k);
        if (key_str.empty() || key_str == "__builtins__") continue;
        result.add(key, value);
    }
    Py_XDECREF(globals);
    return result;
}

// ================== Type function ==================
inline Str type(const PyObj& obj) {
    if (!obj.get_obj()) return Str("NoneType");
    PyObject* type_obj = PyObject_Type(obj.get_obj());
    if (!type_obj) return Str("<unknown>");
    PyObject* type_name = PyObject_GetAttrString(type_obj, "__name__");
    Py_XDECREF(type_obj);
    if (!type_name) return Str("<unknown>");
    Str result(type_name);
    Py_XDECREF(type_name);
    return result;
}

} // namespace py
