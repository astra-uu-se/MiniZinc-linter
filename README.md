# New About
This is a clone of Erik Rimskog's repo for his master thesis at Uppsala Univeristy. The original repo is at [Github](https://github.com/erikvader/MiniZinc-linter). The report can be found on 
[Diva](http://uu.diva-portal.org/smash/get/diva2:1591018/FULLTEXT01.pdf). 



# Old About
This is my master thesis project for a
[linter](https://en.wikipedia.org/wiki/Lint_(software)) for
[MiniZinc](https://www.minizinc.org/), project at [libminizinc](https://github.com/MiniZinc/libminizinc).

# Linting rules
Each linting rule is a separate issue tagged with `lint`. This gives
each one a unique id and space for discussions on a specific rule.
Some are labeled with `help wanted`, which means I have a question or
have something I don't understand about a rule. Feel free to add more
linting rules and to comment with opinions.

# Issue
Have something to discuss? Create an issue!

# Build instructions
First, clone with all submodules using for example `git clone --recurse-submodules`. Then, inside the cloned folder, build with:
```sh
mkdir build
cd build
cmake ..
cmake --build .
```
Build in parallel with `-j <N>` on the build step where *N* is the number of jobs.

There are some interesting flags that can be set on the generation step. For example `cmake -DCMAKE_BUILD_TYPE=Debug ..`.
Here is a table of some of them:
| Variable                        | Default | Description                                                                              |
|:--------------------------------|:--------|:-----------------------------------------------------------------------------------------|
| CMAKE_EXPORT_COMPILE_COMMANDS   | NO      | Export how each file got compiled, useful for [ccls](https://github.com/MaskRay/ccls)    |
| CMAKE_BUILD_TYPE                | Release | Whether to build with debug flags or release flags                                       |
| CMAKE_POSITION_INDEPENDENT_CODE | NO      | Whether to compile with [-fPIC](https://en.wikipedia.org/wiki/Position-independent_code) |
| LZN_FLAGS                       | NO      | Use my preferred development flags                                                       |

# Usage
The linter is run as:
```sh
./lzn some_model.mzn
```

The linter currently expects the standard library to be in the same directory as the executable.
A symlink to it can be added inside the build directory with:
```sh
ln -s ../deps/libminizinc/share .
```

# Testing
Unit tests to test all rules against small MiniZinc models are run with:
```sh
cmake --build . --target test
```
