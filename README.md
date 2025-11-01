# 🌟 pyobj


<br><br>


## en

<br>

pyobj - Provides C++ support for some Python features

<br>

## 🚀 Features

- ✅ Python in C++
- ⚙️ init_python, exit_python, Str, List, Tuple, Set, type, fstring, map, all, any, exec, eval, run_file, run_file_result
  
<br>

## 🧰 Installation

```bash
# Clone the repository
git clone https://github.com/cppandpython/pyobj.git

#Example
#include "pyobj.h"
#include <iostream>
using namespace std;
int main() {
    py::init_python();
    py::PyObj test = py::Dict({{"x", 10}, {"y", 20}, {"z", 30}});
    cout << test;
    cout << test["y"];
    py::exit_python();
    return 0;
#Result

}

```



<br><br><br><br>
