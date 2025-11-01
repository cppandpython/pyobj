#pragma once

#include <Python.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include <iostream>
#include <sstream>


namespace py {


// ================== Python init ==================
inline void init_python() { if (!Py_IsInitialized()) Py_Initialize(); }
inline void exit_python() { if (Py_IsInitialized()) Py_FinalizeEx(); }

// ================== Base PyObj ==================
class PyObj {
protected:
    PyObject* obj;

public:
    PyObj(PyObject* o = nullptr) : obj(o) { Py_XINCREF(obj); }
    PyObj(const PyObj& other) : obj(other.obj) { Py_XINCREF(obj); }
    PyObj(PyObj&& other) noexcept : obj(other.obj) { other.obj = nullptr; }

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

    bool is_dict() const { return obj && PyDict_Check(obj); }
    bool is_list() const { return obj && PyList_Check(obj); }
    bool is_tuple() const { return obj && PyTuple_Check(obj); }
    bool is_str() const { return obj && PyUnicode_Check(obj); }
    bool is_set() const { return obj && PySet_Check(obj); }
    bool is_sequence() const { return is_list() || is_tuple() || is_str(); }

    PyObj get_item(const PyObj& key) const {
        if (!obj) return PyObj();
        PyObject* r = PyDict_Check(obj) ? PyDict_GetItem(obj, key.get_obj())
                                        : PyObject_GetItem(obj, key.get_obj());
        if (!r) { PyErr_Clear(); return PyObj(); }
        Py_XINCREF(r);
        return PyObj(r);
    }

    PyObj get_item(long index) const {
        if (!obj) return PyObj();

        if (PyList_Check(obj) || PyTuple_Check(obj)) {
            Py_ssize_t n = PyList_Check(obj) ? PyList_Size(obj) : PyTuple_Size(obj);
            if (index < 0) index += n;
            if (index < 0 || index >= n) return PyObj();
            PyObject* it = PyList_Check(obj) ? PyList_GetItem(obj, index) : PyTuple_GetItem(obj, index);
            Py_XINCREF(it);
            return PyObj(it);
        }

        if (PyUnicode_Check(obj)) {
            Py_ssize_t n = PyUnicode_GetLength(obj);
            if (index < 0) index += n;
            if (index < 0 || index >= n) return PyObj();
            Py_UCS4 ch = PyUnicode_ReadChar(obj, index);
            PyObject* tmp = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &ch, 1);
            PyObj r(tmp); Py_XDECREF(tmp); return r;
        }

        PyObject* pyidx = PyLong_FromLong(index);
        PyObject* r = PyObject_GetItem(obj, pyidx);
        Py_XDECREF(pyidx);
        if (!r) { PyErr_Clear(); return PyObj(); }
        Py_XINCREF(r);
        return PyObj(r);
    }

    PyObj operator[](const PyObj& key) const { return get_item(key); }
    PyObj operator[](long index) const { return get_item(index); }

    bool set_item(const PyObj& key, const PyObj& val) {
        if (!obj) return false;
        return PyDict_Check(obj) ? PyDict_SetItem(obj, key.get_obj(), val.get_obj()) == 0
                                 : PyObject_SetItem(obj, key.get_obj(), val.get_obj()) == 0;
    }

    bool set_item(long index, const PyObj& val) {
        if (!obj) return false;
        if (PyList_Check(obj)) {
            Py_ssize_t n = PyList_Size(obj); if (index < 0) index += n;
            if (index < 0 || index >= n) return false;
            Py_XINCREF(val.get_obj());
            return PyList_SetItem(obj, index, val.get_obj()) == 0;
        }
        PyObject* pyidx = PyLong_FromLong(index);
        int ok = PyObject_SetItem(obj, pyidx, val.get_obj());
        Py_XDECREF(pyidx);
        return ok == 0;
    }

    std::string str() const {
        if (!obj) return "None";
        PyObject* r = PyObject_Str(obj);
        if (!r) { PyErr_Clear(); return "<error>"; }
        const char* c = PyUnicode_AsUTF8(r);
        std::string s = c ? c : "";
        Py_XDECREF(r);
        return s;
    }

    static void pretty_print(std::ostream& os, const PyObj& obj, int indent = 0);
};

// ================== Str ==================
class Str : public PyObj {
public:
    Str() : PyObj("") {}
    Str(const std::string& s) : PyObj(s) {}
    Str(const char* s) : PyObj(s) {}
    Str(PyObject* o) : PyObj(o) {}
    Str(const PyObj& o) : PyObj(o.get_obj()) {}

    // Basic string methods
    Str capitalize() const { PyObject* r = PyObject_CallMethod(obj,(char*)"capitalize",nullptr); return Str(r); }
    Str upper() const { PyObject* r = PyObject_CallMethod(obj,(char*)"upper",nullptr); return Str(r); }
    Str lower() const { PyObject* r = PyObject_CallMethod(obj,(char*)"lower",nullptr); return Str(r); }
    Str title() const { PyObject* r = PyObject_CallMethod(obj,(char*)"title",nullptr); return Str(r); }
    Str swapcase() const { PyObject* r = PyObject_CallMethod(obj,(char*)"swapcase",nullptr); return Str(r); }
    Str strip() const { PyObject* r = PyObject_CallMethod(obj,(char*)"strip",nullptr); return Str(r); }
    Str lstrip() const { PyObject* r = PyObject_CallMethod(obj,(char*)"lstrip",nullptr); return Str(r); }
    Str rstrip() const { PyObject* r = PyObject_CallMethod(obj,(char*)"rstrip",nullptr); return Str(r); }

    // Checks
    bool isdigit() const { PyObject* r = PyObject_CallMethod(obj,(char*)"isdigit",nullptr); bool b = r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }
    bool isalpha() const { PyObject* r = PyObject_CallMethod(obj,(char*)"isalpha",nullptr); bool b = r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }
    bool isalnum() const { PyObject* r = PyObject_CallMethod(obj,(char*)"isalnum",nullptr); bool b = r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }
    bool isdecimal() const { PyObject* r = PyObject_CallMethod(obj,(char*)"isdecimal",nullptr); bool b = r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }
    bool isnumeric() const { PyObject* r = PyObject_CallMethod(obj,(char*)"isnumeric",nullptr); bool b = r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }
    bool istitle() const { PyObject* r = PyObject_CallMethod(obj,(char*)"istitle",nullptr); bool b = r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }
    bool isupper() const { PyObject* r = PyObject_CallMethod(obj,(char*)"isupper",nullptr); bool b = r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }
    bool islower() const { PyObject* r = PyObject_CallMethod(obj,(char*)"islower",nullptr); bool b = r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }

    // Search / replace
    long find(const Str& s) const { PyObject* r = PyObject_CallMethod(obj,(char*)"find",(char*)"O",s.get_obj()); long v = r ? PyLong_AsLong(r) : -1; Py_XDECREF(r); return v; }
    long rfind(const Str& s) const { PyObject* r = PyObject_CallMethod(obj,(char*)"rfind",(char*)"O",s.get_obj()); long v = r ? PyLong_AsLong(r) : -1; Py_XDECREF(r); return v; }
    long index(const Str& s) const { PyObject* r = PyObject_CallMethod(obj,(char*)"index",(char*)"O",s.get_obj()); long v = r ? PyLong_AsLong(r) : -1; Py_XDECREF(r); return v; }
    long rindex(const Str& s) const { PyObject* r = PyObject_CallMethod(obj,(char*)"rindex",(char*)"O",s.get_obj()); long v = r ? PyLong_AsLong(r) : -1; Py_XDECREF(r); return v; }
    Str replace(const Str& old_s,const Str& new_s) const { PyObject* r = PyObject_CallMethod(obj,(char*)"replace",(char*)"OO",old_s.get_obj(),new_s.get_obj()); return Str(r); }
    std::vector<Str> split(const Str& sep="") const {
        PyObject* r = sep.str().empty() ? PyObject_CallMethod(obj,(char*)"split",nullptr) : PyObject_CallMethod(obj,(char*)"split",(char*)"O",sep.get_obj());
        std::vector<Str> out;
        if(!r){ PyErr_Clear(); return out; }
        if(PyList_Check(r)){
            Py_ssize_t n = PyList_Size(r);
            for(Py_ssize_t i=0;i<n;++i){ PyObject* it = PyList_GetItem(r,i); Py_XINCREF(it); out.push_back(Str(it)); }
        }
        Py_XDECREF(r); return out;
    }
    Str join(const std::vector<Str>& seq) const {
        PyObject* list_obj = PyList_New(seq.size());
        for(Py_ssize_t i=0;i<seq.size();++i){ Py_XINCREF(seq[i].get_obj()); PyList_SetItem(list_obj,i,seq[i].get_obj()); }
        PyObject* r = PyObject_CallMethod(obj,(char*)"join",(char*)"O",list_obj); Py_XDECREF(list_obj);
        return Str(r);
    }

    long len() const { return obj ? PyUnicode_GetLength(obj) : 0; }
    Str operator[](long index) const {
        Py_ssize_t n = len(); if(index < 0) index += n; if(index < 0 || index >= n) return Str();
        Py_UCS4 ch = PyUnicode_ReadChar(obj,index);
        PyObject* tmp = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND,&ch,1);
        Str r(tmp); Py_XDECREF(tmp); return r;
    }
};

// ================== List ==================
class List : public PyObj {
public:
    List() : PyObj(PyList_New(0)) {}
    List(const std::initializer_list<PyObj>& l) : PyObj(PyList_New(l.size())) {
        size_t i=0; for(const auto& v:l){ Py_XINCREF(v.get_obj()); PyList_SetItem(obj,i++,v.get_obj()); }
    }

    void append(const PyObj& val){ PyList_Append(obj,val.get_obj()); }
    void extend(const List& l){ PyObject* r=PyObject_CallMethod(obj,(char*)"extend",(char*)"O",l.get_obj()); Py_XDECREF(r); }
    void insert(long index,const PyObj& val){ PyObject* r=PyObject_CallMethod(obj,(char*)"insert",(char*)"lO",index,val.get_obj()); Py_XDECREF(r); }
    void remove(const PyObj& val){ PyObject* r=PyObject_CallMethod(obj,(char*)"remove",(char*)"O",val.get_obj()); Py_XDECREF(r); }
    PyObj pop(int index=-1){
        Py_ssize_t n=PyList_Size(obj); if(index<0) index+=n;
        if(index<0||index>=n) return PyObj();
        PyObject* it=PyList_GetItem(obj,index); Py_XINCREF(it);
        PySequence_DelItem(obj,index); return PyObj(it);
    }
    long index(const PyObj& val) const { PyObject* r=PyObject_CallMethod(obj,(char*)"index",(char*)"O",val.get_obj()); long v = r ? PyLong_AsLong(r) : -1; Py_XDECREF(r); return v; }
    long count(const PyObj& val) const { PyObject* r=PyObject_CallMethod(obj,(char*)"count",(char*)"O",val.get_obj()); long v = r ? PyLong_AsLong(r) : 0; Py_XDECREF(r); return v; }
    void reverse(){ PyObject* r=PyObject_CallMethod(obj,(char*)"reverse",nullptr); Py_XDECREF(r); }
    void sort(){ PyObject* r=PyObject_CallMethod(obj,(char*)"sort",nullptr); Py_XDECREF(r); }

    long len() const { return obj ? PyList_Size(obj) : 0; }
    PyObj operator[](long index) const {
        Py_ssize_t n=PyList_Size(obj); if(index<0) index+=n; if(index<0||index>=n) return PyObj();
        PyObject* it = PyList_GetItem(obj,index); Py_XINCREF(it); return PyObj(it);
    }
    bool set(long index,const PyObj& val){ return set_item(index,val); }
};

// ================== Tuple ==================
class Tuple : public PyObj {
public:
    Tuple() : PyObj(PyTuple_New(0)) {}
    Tuple(PyObject* o) : PyObj(o) {}          
    Tuple(const std::initializer_list<PyObj>& l){
        PyObject* list = PyList_New(l.size());
        size_t i = 0;
        for (auto& v : l) {
            Py_XINCREF(v.get_obj());                
            PyList_SetItem(list, i++, v.get_obj()); 
        }
        obj = PySequence_Tuple(list); 
        Py_XDECREF(list);            
    }
    
    long len() const { return obj ? PyTuple_Size(obj) : 0; }

    PyObj operator[](long index) const {
        if (!obj) return PyObj();
        Py_ssize_t n = PyTuple_Size(obj);
        if (index < 0) index += n;
        if (index < 0 || index >= n) return PyObj();
        PyObject* it = PyTuple_GetItem(obj, index);
        Py_XINCREF(it); 
        return PyObj(it);
    }

    long index(const PyObj& val) const {
        if (!obj) return -1;
        Py_ssize_t idx = PySequence_Index(obj, val.get_obj());
        if (idx == -1 && PyErr_Occurred()) { PyErr_Clear(); return -1; }
        return idx;
    }

    long count(const PyObj& val) const {
        if (!obj) return 0;
        Py_ssize_t cnt = PySequence_Count(obj, val.get_obj());
        if (cnt == -1 && PyErr_Occurred()) { PyErr_Clear(); return 0; }
        return cnt;
    }

    Tuple operator+(const Tuple& other) const {
        if (!obj || !other.get_obj()) return Tuple();
        PyObject* r = PySequence_Concat(obj, other.get_obj());
        return Tuple(r);  
    }

    Tuple operator*(long n) const {
        if (!obj) return Tuple();
        PyObject* r = PySequence_Repeat(obj, n);
        return Tuple(r);
    }

    bool contains(const PyObj& val) const {
        if (!obj) return false;
        int res = PySequence_Contains(obj, val.get_obj());
        if (res == -1) { PyErr_Clear(); return false; }
        return res == 1;
    }
};

// ================== Set ==================
class Set : public PyObj {
public:
    Set() : PyObj(PySet_New(nullptr)) {}
    Set(PyObject* o) : PyObj(o) {}
    Set(const std::initializer_list<PyObj>& l) : PyObj(PySet_New(nullptr)) {
        for(auto& v:l) PySet_Add(obj,v.get_obj());
    }

    void add(const PyObj& v){ PySet_Add(obj,v.get_obj()); }
    PyObj pop(){ return PyObj(PySet_Pop(obj)); }
    long len() const { return obj ? PySet_Size(obj) : 0; }
    bool contains(const PyObj& v) const { return obj && PySet_Contains(obj,v.get_obj())==1; }

    Set union_with(const Set& s){ PyObject* r=PyNumber_Or(obj,s.get_obj()); return Set(r?r:PySet_New(nullptr)); }
    Set intersection(const Set& s){ PyObject* r=PyNumber_And(obj,s.get_obj()); return Set(r?r:PySet_New(nullptr)); }
    Set difference(const Set& s){ PyObject* r=PyNumber_Subtract(obj,s.get_obj()); return Set(r?r:PySet_New(nullptr)); }
    bool issubset(const Set& s){ PyObject* r=PyObject_CallMethod(obj,(char*)"issubset",(char*)"O",s.get_obj()); bool b=r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }
    bool issuperset(const Set& s){ PyObject* r=PyObject_CallMethod(obj,(char*)"issuperset",(char*)"O",s.get_obj()); bool b=r && PyObject_IsTrue(r); Py_XDECREF(r); return b; }
};

// ================== Dict ==================
class Dict : public PyObj {
public:
    Dict() : PyObj(PyDict_New()) {}
    Dict(const std::initializer_list<std::pair<PyObj,PyObj>>& l) : PyObj(PyDict_New()){
        for(auto& p:l) PyDict_SetItem(obj,p.first.get_obj(),p.second.get_obj());
    }

    void add(const PyObj& key,const PyObj& val){ PyDict_SetItem(obj,key.get_obj(),val.get_obj()); }
    PyObj get(const PyObj& key) const{ PyObject* r=PyDict_GetItem(obj,key.get_obj()); Py_XINCREF(r); return r?PyObj(r):PyObj(); }
    bool contains(const PyObj& key) const { return PyDict_Contains(obj,key.get_obj())==1; }
    PyObj operator[](const PyObj& key) const{ return get(key); }
    bool set(const PyObj& key,const PyObj& val){ return set_item(key,val); }
};

// ================== Pretty Print ==================
namespace detail {
inline void pretty_print(std::ostream& os, const PyObj& obj, int indent = 0){
    if (!obj.get_obj()) { os << "None"; return; }
    PyObject* o = obj.get_obj();
    if (PyBool_Check(o)) { os << (o == Py_True ? "True" : "False"); return; }
    if (PyLong_Check(o)) { os << PyLong_AsLong(o); return; }
    if (PyFloat_Check(o)) { os << PyFloat_AsDouble(o); return; }
    if (PyUnicode_Check(o)) { const char* s = PyUnicode_AsUTF8(o); os << "\"" << (s?s:"") << "\""; return; }

    if (PyList_Check(o)) {
        Py_ssize_t n = PyList_Size(o); if(n==0){os<<"[]"; return;}
        os << "[\n"; for(Py_ssize_t i=0;i<n;++i){ for(int j=0;j<indent+4;++j) os<<' '; PyObject* it=PyList_GetItem(o,i); Py_XINCREF(it); pretty_print(os, PyObj(it), indent+4); Py_XDECREF(it); if(i!=n-1) os<<","; os<<"\n"; } for(int j=0;j<indent;++j) os<<' '; os<<"]"; return;}
    if (PyTuple_Check(o)) { Py_ssize_t n=PyTuple_Size(o); if(n==0){os<<"()"; return;} os<<"(\n"; for(Py_ssize_t i=0;i<n;++i){ for(int j=0;j<indent+4;++j) os<<' '; PyObject* it=PyTuple_GetItem(o,i); Py_XINCREF(it); pretty_print(os, PyObj(it), indent+4); Py_XDECREF(it); if(i!=n-1) os<<","; os<<"\n"; } for(int j=0;j<indent;++j) os<<' '; os<<")"; return;}
    if (PyDict_Check(o)){ Py_ssize_t pos=0; PyObject *key,*value; if(PyDict_Size(o)==0){os<<"{}"; return;} os<<"{\n"; bool first=true; while(PyDict_Next(o,&pos,&key,&value)){ if(!first) os<<",\n"; first=false; for(int j=0;j<indent+4;++j) os<<' '; Py_XINCREF(key); pretty_print(os, PyObj(key), indent+4); Py_XDECREF(key); os<<": "; Py_XINCREF(value); pretty_print(os, PyObj(value), indent+4); Py_XDECREF(value); } os<<"\n"; for(int j=0;j<indent;++j) os<<' '; os<<"}"; return;}
    if (PySet_Check(o)){ PyObject* iter=PyObject_GetIter(o); if(!iter){ os<<"<set>"; return;} PyObject* item; bool first=true; os<<"{\n"; while((item=PyIter_Next(iter))){ if(!first) os<<",\n"; first=false; for(int j=0;j<indent+4;++j) os<<' '; pretty_print(os, PyObj(item), indent+4); Py_XDECREF(item);} Py_XDECREF(iter); os<<"\n"; for(int j=0;j<indent;++j) os<<' '; os<<"}"; return;}
    PyObject* r = PyObject_Repr(o); if(r){ const char* s = PyUnicode_AsUTF8(r); os << (s?s:"<repr-error>"); Py_XDECREF(r); } else { PyErr_Clear(); os<<"<PyObj>"; }
}
} // namespace detail

inline std::ostream& operator<<(std::ostream& os, const PyObj& obj){
    if(!obj.get_obj()){ os << "None"; return os; }
    PyObject* r = PyObject_Repr(obj.get_obj());
    if(r){ os << PyUnicode_AsUTF8(r); Py_XDECREF(r); }
    else { PyErr_Clear(); os << "<invalid PyObj>"; }
    return os;
}

inline void PyObj::pretty_print(std::ostream& os,const PyObj& obj,int indent){ detail::pretty_print(os,obj,indent); }
inline void print(const PyObj& obj){ std::cout<<obj<<std::endl; }
inline void pprint(const PyObj& obj,int indent=0){ PyObj::pretty_print(std::cout,obj,indent); std::cout<<std::endl; }

// ================== Utility ==================
inline bool all(const List& l){ for(long i=0;i<l.len();++i) if(!PyObject_IsTrue(l[i].get_obj())) return false; return true; }
inline bool any(const List& l){ for(long i=0;i<l.len();++i) if(PyObject_IsTrue(l[i].get_obj())) return true; return false; }
inline List map(const PyObj& func,const List& l){ List out; for(long i=0;i<l.len();++i){ PyObject* r=PyObject_CallFunctionObjArgs(func.get_obj(),l[i].get_obj(),nullptr); if(r){ PyList_Append(out.get_obj(),r); Py_XDECREF(r);} else PyErr_Clear();} return out; }
inline void exec(const std::string& code){ PyRun_SimpleString(code.c_str()); }
inline PyObj eval(const std::string& code){ PyObject* globals=PyDict_New(); PyObject* locals=globals; PyObject* result=PyRun_String(code.c_str(),Py_eval_input,globals,locals); if(!result){ PyErr_Print(); Py_XDECREF(globals); return PyObj(); } Py_XDECREF(globals); return PyObj(result); }
inline void run_file(const std::string& filename){ FILE* f=fopen(filename.c_str(),"r"); if(!f){ std::cerr<<"run_file: cannot open "<<filename<<"\n"; return;} PyRun_SimpleFile(f,filename.c_str()); fclose(f); }
inline Dict run_file_result(const std::string& filename){ FILE* f=fopen(filename.c_str(),"r"); if(!f) return Dict(); PyObject* globals=PyDict_New(); PyObject* locals=globals; PyRun_File(f,filename.c_str(),Py_file_input,globals,locals); fclose(f); Dict result; PyObject *key,*value; Py_ssize_t pos=0; while(PyDict_Next(globals,&pos,&key,&value)){ if(!PyUnicode_Check(key)) continue; const char* k=PyUnicode_AsUTF8(key); if(!k) continue; std::string key_str(k); if(key_str.empty()||key_str=="__builtins__") continue; result.add(PyObj(key),PyObj(value)); } Py_XDECREF(globals); return result; }
inline Str type(const PyObj& o){ if(!o.get_obj()) return Str("<NoneType>"); PyObject* t=PyObject_Type(o.get_obj()); if(!t) return Str("<unknown>"); PyObject* name=PyObject_GetAttrString(t,"__name__"); Py_XDECREF(t); if(!name) return Str("<unknown>"); const char* s=PyUnicode_AsUTF8(name); Str res(s?s:"<unknown>"); Py_XDECREF(name); return res; }

// ================== fstring ==================
template<typename T>
std::pair<std::string,std::string> farg(const std::string& name,T&& value){
  std::ostringstream os; os<<value; return {name,os.str()};
}

template<typename... Args>
std::string fstring(const std::string& fmt, Args&&... args){
  std::unordered_map<std::string,std::string> named;
  std::vector<std::string> positional;
  auto process=[&](auto&& x){
    using X=std::decay_t<decltype(x)>;
    if constexpr(std::is_same_v<X,std::pair<std::string,std::string>>) named.emplace(x.first,x.second);
    else { std::ostringstream os; os<<x; positional.push_back(os.str()); }
  };
  (process(std::forward<Args>(args)),...);
  std::string out; out.reserve(fmt.size()+32); size_t pos_index=0;
  for(size_t i=0;i<fmt.size();++i){
    if(fmt[i]=='{'){
      if(i+1<fmt.size() && fmt[i+1]=='{'){ out.push_back('{'); ++i; continue; }
      size_t j=fmt.find('}',i+1); if(j==std::string::npos){ out.push_back('{'); continue; }
      std::string key=fmt.substr(i+1,j-i-1); if(key.empty()){ if(pos_index<positional.size()) out+=positional[pos_index++]; else out+="{}"; }
      else { auto it=named.find(key); if(it!=named.end()) out+=it->second; else out+="{" + key + "}"; }
      i=j;
    } else if(fmt[i]=='}'){ if(i+1<fmt.size() && fmt[i+1]=='}'){ out.push_back('}'); ++i; } else out.push_back('}'); }
    else out.push_back(fmt[i]);
  }
  return out;
}

// ================== Global sort ==================
inline long len(const PyObj& o){
    if(!o.get_obj()) return 0;
    if(o.is_str()) return PyUnicode_GetLength(o.get_obj());
    if(o.is_list()) return PyList_Size(o.get_obj());
    if(o.is_tuple()) return PyTuple_Size(o.get_obj());
    if(o.is_set()) return PySet_Size(o.get_obj());
    if(o.is_dict()) return PyDict_Size(o.get_obj());
    return 0;
}

inline List sorted(const PyObj& seq){
    if(!seq.get_obj()) return List();
    PyObject* tmp = PySequence_List(seq.get_obj());
    if(!tmp){ PyErr_Clear(); return List(); }
    PyList_Sort(tmp);
    List out;
    Py_ssize_t n = PyList_Size(tmp);
    for(Py_ssize_t i=0;i<n;++i){
        PyObject* it = PyList_GetItem(tmp,i); Py_XINCREF(it);
        PyList_Append(out.get_obj(), it); Py_XDECREF(it);
    }
    Py_XDECREF(tmp);
    return out;
}


inline PyObj reversed(const PyObj& o){
    if(!o.get_obj()) return PyObj();
    if(o.is_str()){
        PyObject* r = PyObject_CallMethod(o.get_obj(), (char*)"__reversed__", nullptr);
        if(!r){ PyErr_Clear(); return PyObj(); }
        PyObject* seq = PySequence_List(r); Py_XDECREF(r);
        PyObject* joined = PyUnicode_Join(PyUnicode_FromString(""), seq); Py_XDECREF(seq);
        return PyObj(joined);
    }
    if(o.is_list() || o.is_tuple()){
        PyObject* r = PyObject_CallMethod(o.get_obj(), (char*)"__reversed__", nullptr);
        if(!r){ PyErr_Clear(); return PyObj(); }
        PyObject* seq = PySequence_List(r); Py_XDECREF(r);
        if(o.is_tuple()){ PyObject* t = PySequence_Tuple(seq); Py_XDECREF(seq); return PyObj(t); }
        return PyObj(seq);
    }
    PyErr_Clear();
    return PyObj();
}

// ================== JSON ==================
inline PyObj json_loads(const Str& s){
    PyObject* json_mod = PyImport_ImportModule("json"); if(!json_mod){ PyErr_Clear(); return PyObj(); }
    PyObject* loads = PyObject_GetAttrString(json_mod,"loads"); Py_XDECREF(json_mod); if(!loads){ PyErr_Clear(); return PyObj(); }
    PyObject* r = PyObject_CallFunctionObjArgs(loads, s.get_obj(), nullptr); Py_XDECREF(loads);
    return r ? PyObj(r) : PyObj();
}
inline Str json_dumps(const PyObj& o){
    PyObject* json_mod = PyImport_ImportModule("json"); if(!json_mod){ PyErr_Clear(); return Str(); }
    PyObject* dumps = PyObject_GetAttrString(json_mod,"dumps"); Py_XDECREF(json_mod); if(!dumps){ PyErr_Clear(); return Str(); }
    PyObject* r = PyObject_CallFunctionObjArgs(dumps, o.get_obj(), nullptr); Py_XDECREF(dumps);
    Str res(r); Py_XDECREF(r); return res;
}


} // end namespace py
