# Chkoupi-lang :flag_dz:

A vibe coded Algerian programming language made on LLVM.

## Keywords

| Darija              | Meaning                    |
| ------------------- | -------------------------- |
| `achfa x = ...`     | declare variable (`let`)   |
| `ab9a_dayr x = ...` | declare constant (`const`) |
| `ektb(...)`         | print (`printf`)           |
| `a9ra(x)`           | read input (`scanf`)       |
| `idha`              | if                         |
| `ila_makanch`       | else                       |
| `ab9a_dor`          | while                      |
| `dor`               | for                        |
| `sa7`               | true                       |
| `ghalt`             | false                      |
| `fun`               | function                   |
| `arja3`             | return                     |
| `jib`               | import                     |
| `jarb`              | try                        |
| `ila_ghalt`         | except                     |
| `khyr`              | case                       |

## Types

`int`, `float`, `bool`, `string`, `void`

## Logical Operators

| Darija  | Meaning |
| ------- | ------- |
| `w`     | and     |
| `wla`   | or      |
| `machi` | not     |

## Example

```chk
achfa x : int = 10;
idha (x > 5) {
    ektb("kbir!\n");
} ila_makanch {
    ektb("sghir!\n");
}
```

## Building

### Prerequisites
- CMake 3.20+
- LLVM 17+ (with development headers)
- A C++17 compiler (MSVC / Clang / GCC)

### Windows (MSVC + LLVM via winget)
```powershell
winget install LLVM.LLVM
cmake -B build -DLLVM_DIR="C:/Program Files/LLVM/lib/cmake/llvm"
cmake --build build --config Release
```

### Linux / macOS
```bash
sudo apt install llvm-17-dev  # or brew install llvm
cmake -B build
cmake --build build
```

## Usage

```bash
# Emit LLVM IR
chkoupi examples/hello.chk --emit-ir

# Compile to object file then link
chkoupi examples/hello.chk --emit-obj hello.o
clang hello.o -o hello
./hello
```
___
**made for copium**
