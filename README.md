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
using namespace py;
using namespace std;
int main() {
    py::init_python();
    Dict d = { {"x", 10}, {"y", "s"}, {"z", 30} };
    print(type(d));
    pprint(d);
    py::exit_python();
    return 0;
}
#Result
'dict'
{
    "x": 10,
    "y": "s",
    "z": 30
}
```



<br><br><br><br>
