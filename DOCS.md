# Chkoupi-lang Language Reference 🇩🇿

---

## Variables

```dz
dir x : tabi3i = 42;       // mutable
dima PI : 3ouchri = 3.14;  // constant (can't reassign)
dir name : nass = "brahim";
dir flag : 5iyar = sa7;
```

Type annotation is optional — the compiler infers from the value:
```dz
dir x = 10;    // inferred tabi3i
dir y = 3.14;  // inferred 3ouchri
```

---

## Types

| Keyword | Type | Example |
|---|---|---|
| `tabi3i` | integer (64-bit) | `42` |
| `3ouchri` | float (64-bit double) | `3.14` |
| `5iyar` | boolean | `sa7` / `ghalt` |
| `7arf` | character (8-bit) | `'a'` |
| `nass` | string (pointer) | `"salam"` |
| `fargh` | void (no return) | — |
| `jadwal[T]` | array of T | `[1, 2, 3]` |
| `9aleb` | struct (user-defined) | `9aleb Insan { ... }` |
| `anwa3` | enum (user-defined) | `anwa3 Lon { ... }` |
| `ymkn[T]` | optional T | `walo` = empty |

---

## Arrays (jadwal)

```dz
// Declaration with literal
dir nums : jadwal[tabi3i] = [10, 20, 30];

// Index access (0-based)
ektb("nums[1] = %lld\n", nums[1]);

// Index assignment
nums[0] = 99;

// Float array
dir prices : jadwal[3ouchri] = [9.99, 19.99, 29.99];
```

---

## Structs (9aleb)

```dz
// Define a struct
9aleb Insan {
    ism : nass;
    3omr : tabi3i;
}

// Create an instance
dir brahim = Insan { ism: "Brahim", 3omr: 25 };

// Field access
ektb("%s is %lld\n", brahim.ism, brahim.3omr);

// Field assignment
brahim.3omr = 26;
```

---

## Enums (anwa3)

```dz
// Define an enum
anwa3 Lon {
    7mr;
    khdhr;
    zr9;
}

// Variant access (returns integer: 0, 1, 2, ...)
dir c : tabi3i = Lon.khdhr;

// Use in conditions
idha (c == Lon.7mr) {
    ektb("7mr!\n");
}
```

---

## Optionals (ymkn)

```dz
// Optional with a value
dir x : ymkn[tabi3i] = 42;

// Optional with no value (null)
dir y : ymkn[tabi3i] = walo;
```

---

## Operators

### Arithmetic
```dz
x + y    x - y    x * y    x / y    x % y
```

### Comparison
```dz
x == y    x != y    x < y    x > y    x <= y    x >= y
```

### Logical
```dz
x w y        // and
x wla y      // or
machi x      // not
```

---

## Control Flow

### idha / idha_mknch (if / else)
```dz
idha (x > 0) {
    ektb("positive!\n");
} idha_mknch {
    ektb("machi positive\n");
}
```

### ab9a_dor (while)
```dz
dir i : tabi3i = 0;
ab9a_dor (i < 10) {
    ektb("%lld\n", i);
    i = i + 1;
}
```

### dor (for)
```dz
dor (dir i = 0; i < 5; i = i + 1) {
    ektb("i = %lld\n", i);
}
```

### bdl / khyr (switch / case)
```dz
dir code : tabi3i = 2;
bdl (code) {
    khyr 1: ektb("wahd\n");
    khyr 2: ektb("zouj\n");
    khyr 3: ektb("tlata\n");
}
```

### jarb / ila_ghalt (try / except)
```dz
jarb {
    // risky code
} ila_ghalt {
    ektb("wqe3 ghalta!\n");
}
```

---

## Functions

```dz
dalla greet(name: nass) -> fargh {
    ektb("salam %s!\n", name);
}

dalla add(a: tabi3i, b: tabi3i) -> tabi3i {
    raja3 a + b;
}

greet("brahim");
dir result = add(3, 7);
```

---

## I/O

```dz
ektb("salam!\n");              // print (printf-style)
ektb("x = %lld\n", x);        // format: %lld=int, %lf=float, %s=string

dir n : tabi3i = 0;
a9ra(n);                       // read input into variable
```

---

## Comments

```dz
// single line comment
/* multi
   line comment */
```

---

## Import

```dz
jibli "utils";    // future: link external .dz modules
```

---

## Full Example

```dz
dalla factorial(n: tabi3i) -> tabi3i {
    idha (n <= 1) {
        raja3 1;
    }
    raja3 n * factorial(n - 1);
}

dir n : tabi3i = 0;
ektb("3tini raqm: ");
a9ra(n);
ektb("%lld! = %lld\n", n, factorial(n));
```

---

## Compiler Usage

```powershell
# Run directly (JIT — default)
chkoupi.exe myfile.dz

# Print generated LLVM IR
chkoupi.exe myfile.dz --emit-ir

# Compile to object file, then link
chkoupi.exe myfile.dz --emit-obj out.o
clang out.o -o myprogram
./myprogram
```

---

## Format Rules

- Statements end with `;`
- Blocks use `{ }`
- Type annotations use `:` → `dir x : tabi3i = 5;`
- Function return type uses `->` → `dalla f() -> tabi3i { ... }`
- Source files use the `.dz` extension
