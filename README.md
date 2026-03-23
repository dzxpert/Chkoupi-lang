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

## Operators

| Darija | Meaning |
|---|---|
| `w` | and |
| `wla` | or |
| `machi` | not |

## Example

```dz
dir x : tabi3i = 10;
dima PI : 3ouchri = 3.14;

idha (x > 5) {
    ektb("kbir!\n");
} idha_mknch {
    ektb("sghir!\n");
}

ab9a_dor (x > 0) {
    x = x - 1;
}

dor (dir i = 0; i < 3; i = i + 1) {
    ektb("i = %lld\n", i);
}

dalla add(a: tabi3i, b: tabi3i) -> tabi3i {
    raja3 a + b;
}

ektb("sum = %lld\n", add(3, 7));
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