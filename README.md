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
| `walo` | null/none |
| `9aleb` | struct |
| `anwa3` | enum |

## Types

| Darija | Meaning |
|---|---|
| `tabi3i` | int |
| `3ouchri` | float |
| `5iyar` | bool |
| `7arf` | char |
| `nass` | string |
| `fargh` | void |
| `jadwal[T]` | array of T |
| `ymkn[T]` | optional T |
| `9aleb Name { ... }` | struct |
| `anwa3 Name { ... }` | enum |

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

// jadwal (array)
dir nums : jadwal[tabi3i] = [10, 20, 30];
ektb("nums[1] = %lld\n", nums[1]);
nums[0] = 99;

// 9aleb (struct)
9aleb Insan {
    ism : nass;
    3omr : tabi3i;
}
dir brahim = Insan { ism: "Brahim", 3omr: 25 };
ektb("%s is %lld\n", brahim.ism, brahim.3omr);

// anwa3 (enum)
anwa3 Lon {
    7mr;
    khdhr;
    zr9;
}
dir c : tabi3i = Lon.khdhr;

// ymkn (optional)
dir x : ymkn[tabi3i] = 42;
dir y : ymkn[tabi3i] = walo;
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