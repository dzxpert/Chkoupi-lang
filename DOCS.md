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

## Arrays (`jadwl`)

Arrays are dynamic, heap-allocated lists tracked in memory with a header specifying their length.

```dz
dir nums : jadwl<tabi3i> = [10, 20, 30]; // explicit type
dir implicitNums = [1, 2, 3];            // inferred type

// Read / Write Indexing
nums[1] = 42;
ektb("nums[1] = %lld\n", nums[1]);

// Length check via built-in tool()
ektb("length = %lld\n", tool(nums));
```

---

## Strings (`nass`)

Strings are fully-featured struct-backed types `{ ptr data, i64 length }` (unlike plain pointers).

```dz
dir s1 = "salam";
dir s2 = " algeria";

// Concatenation
dir greeting = s1 + s2; 
ektb("%s\n", greeting);

// Length checking
ektb("len = %lld\n", tool(greeting));

// Comparisons
idha (s1 == "salam") {
    ektb("salam equal sa7\n");
}
```

---

## Implicit Type Coercion

The compiler automatically promotes integers (`tabi3i`) to floats (`3ouchri`) during mixed binary arithmetic, variable assignments, and type annotations to avoid crashes.

```dz
dir floatVal : 3ouchri = 10; // 10 (int) implicitly promoted to 10.0 (double)
dir mixedResult = 5 + 3.14;  // 5 (int) promoted to float before calculation
```

---

## Function Type Inference

You can omit the explicit return type annotation (`-> returnType`) on functions. The compiler recursively analyzes return (`raja3`) statements inside the function body and automatically infers the correct return type (defaulting to `void` if no return is found).

```dz
dalla getInteger(x: tabi3i) {
    raja3 x + 10; // Inferred return type tabi3i
}
```

---

## Import (`jibli`) & Standard Library

You can recursively import other custom `.dz` files in the directory or use the bundled `"math"` standard library.

```dz
jibli "math";            // Exposes jdr (sqrt) and qwa (pow)
jibli "my_custom_file";  // Resolves and parses custom local modules

dir sqVal = jdr(16.0);    // 4.0
dir powVal = qwa(2.0, 3.0); // 8.0
```

---

## Full Example

```dz
jibli "math";

dalla factorial(n: tabi3i) {
    idha (n <= 1) {
        raja3 1;
    }
    raja3 n * factorial(n - 1);
}

dir n : tabi3i = 5;
ektb("%lld! = %lld\n", n, factorial(n));

dir hypotenuse = jdr(qwa(3.0, 2.0) + qwa(4.0, 2.0));
ektb("hypotenuse = %lf\n", hypotenuse);
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
- Function return type uses `->` or is automatically inferred
- Source files use the `.dz` extension
