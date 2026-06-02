# Chkoupi-lang 🇩🇿



A vibe coded Algerian Darija programming language — compiles to native code via LLVM.

📖 [Full Language Reference → DOCS.md](DOCS.md)

## Quick Start

```powershell
chkoupi.exe myfile.dz          # run directly (JIT)
chkoupi.exe myfile.dz --emit-ir          # print LLVM IR
chkoupi.exe myfile.dz --emit-obj out.o   # compile to object file
```

## Keywords

| Darija | Meaning |
|---|---|
| `dir x = ...` | declare variable |
| `dima x = ...` | declare constant |
| `ektb(...)` | print |
| `a9ra(x)` | read input |
| `idha` | if |
| `idha_mknch` | else |
| `ab9a_dor` | while |
| `dor` | for |
| `dalla` | function |
| `raja3` | return |
| `bdl` | switch |
| `khyr` | case |
| `jarb` | try |
| `ila_ghalt` | except |
| `jibli` | import |
| `sa7` | true |
| `ghalt` | false |

## Types

| Darija | Meaning |
|---|---|
| `tabi3i` | int |
| `3ouchri` | float |
| `5iyar` | bool |
| `7arf` | char |
| `nass` | string |
| `fargh` | void |
| `jadwl<T>` | array |

## Operators

| Darija | Meaning |
|---|---|
| `w` | and |
| `wla` | or |
| `machi` | not |

## Key Features Added

- **Arrays (`jadwl<T>`)**: Dynamic arrays on the heap. Index with `arr[i]` and check length using `tool(arr)`.
- **Strings (`nass`)**: Full string support with concatenation (`+`), comparisons (`==`, `!=`, `<`, `>`), and length via `tool(str)`.
- **Implicit Type Coercion**: Mixed `tabi3i` / `3ouchri` promotion and conversion in binary operations and assignments.
- **Function Return Inference**: Function `-> returnType` signature annotation is optional.
- **Module System (`jibli`)**: Recursive file imports and standard math library (`jibli "math";` exposing `jdr` & `qwa`).

## Example

```dz
// 1. Strings & Concatenation
dir s1 = "salam";
dir s2 = " algeria";
ektb("greet = %s (len = %lld)\n", s1 + s2, tool(s1 + s2));

// 2. Arrays
dir nums : jadwl<tabi3i> = [10, 20, 30];
nums[1] = 42;
ektb("length = %lld, nums[1] = %lld\n", tool(nums), nums[1]);

// 3. Implicit Coercion & Type Inference
dir floatVal : 3ouchri = 5;  // int promoted to double
dir result = floatVal + 2.5; // mixed arithmetic
ektb("result = %lf\n", result);

// 4. Function Return Inference
dalla compute(a: tabi3i) {
    raja3 a * 10;
}
ektb("compute = %lld\n", compute(5));
```

## Building

### Prerequisites
- CMake 3.20+
- LLVM 17+ (dev headers)
- C++17 compiler (MSVC / Clang / GCC)

### Windows
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DLLVM_DIR="D:/LLVM/lib/cmake/llvm"
cmake --build build --config Release
```

### Linux / macOS
```bash
sudo apt install llvm-dev   # or brew install llvm
cmake -B build
cmake --build build
```

---
**made as copium**