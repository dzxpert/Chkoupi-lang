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

### a7bss / kml (break / continue)

Use `a7bss` to immediately exit the nearest enclosing loop, and `kml` to skip the remainder of the current loop iteration and continue with the next iteration (including executing the update expression in `dor` loops).

```dz
// Using a7bss (break)
dor (dir i = 0; i < 10; i = i + 1) {
    idha (i == 5) {
        a7bss; // exits the loop when i is 5
    }
    ektb("i = %lld\n", i);
}

// Using kml (continue)
dir j = 0;
ab9a_dor (j < 5) {
    j = j + 1;
    idha (j == 3) {
        kml; // skips printing 3
    }
    ektb("j = %lld\n", j);
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

## Built-in Functions

### tool(x)

Returns the length of a string (`nass`) or an array (`jadwl`). The return type is `tabi3i`.

```dz
dir name = "brahim";
dir size = tool(name); // size = 6

dir numbers = [1, 2, 3, 4];
dir count = tool(numbers); // count = 4
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

## Structs (`9aleb`) & Methods (`had`)

Structs are user-defined types defined using the `9aleb` keyword. They are heap-allocated by reference. Fields can have optional default values. Methods are defined inside the struct and can access the current instance's fields using the implicit instance pointer `had`.

```dz
9aleb Nuqta {
    dir x : tabi3i = 0; // default value
    dir y : tabi3i = 0; // default value

    dalla custom_print() -> fargh {
        ektb("Nuqta: (%lld, %lld)\n", had.x, had.y);
    }
}

// Instantiation
dir p1 = Nuqta { x: 5, y: 10 };
p1.custom_print();

// Default values are applied to missing fields
dir p2 = Nuqta { x: 42 }; // y defaults to 0
p2.custom_print();
```

---

## Memory Management (`kssr`)

Since strings (`nass`), arrays (`jadwl`), and structs (`9aleb`) are heap-allocated, you can use the `kssr` keyword to manually free their memory.

```dz
dir p = Nuqta { x: 10, y: 20 };
kssr p; // frees the struct from the heap
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
