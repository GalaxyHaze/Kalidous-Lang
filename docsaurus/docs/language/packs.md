# Packs (Tuples & Ownership)

Packs are Zith's ownership-aware tuples that track the lifecycle of multiple values.

## Basic Packs

```zith
let pair = (42, "hello");
let first = pair.0;   // 42
let second = pair.1; // "hello"
```

## Named Packs

```zith
let user = (name: "Alice", age: 30);
let name = user.name;
```

## Ownership with Packs

Packs respect ownership keywords:

```zith
let owned = (unique Resource.new(), unique Other.new());
let taken = owned;    // Both moved, original is zombie
```

## Destructuring

```zith
let (a, b) = (1, 2);
// a = 1, b = 2
```

## Pack Types

```zith
fn process(pair: (i32, string)) { ... }
fn user_info() -> (name: string, email: string) { ... }
```

## Use Cases

- Multiple return values
- Grouping related data
- Function parameters
- Pattern matching