# Intrinsics

Intrinsics provide compile-time metadata and reflection accessed via the `@` operator.

## Type Information

```zith
// Size of type
let size = @sizeOf(T);

// Members of struct
for member in @members(T) {
    println("{}: {}", member.name, member.type);
}

// Is type copyable?
if @hasTrait(T, Copy) {
    // T can be bitwise-copied
}
```

## Location Information

```zith
// Current file
let file = @file();

// Current line
let line = @line();

// Current function
let fn_name = @funcName();

// Create rich panic messages
panic(@location() + ": something went wrong");
```

## Entity/Component Intrinsics

```zith
// Components of an entity type
for comp in @components(Player) {
    println("{}", comp);  // Position, Health, ...
}

// Convert entity to struct
let struct_data = @toStruct(player);

// Convert struct back to entity
let entity = @toEntity(struct_data, scene);

// Register component dynamically
@registerComponent(mut entity, NewComponent { /* ... */ });
```

## Compile-Time Evaluation

```zith
// Compile-time constants
const ARRAY_SIZE = @sizeOf([10]i32) / @sizeOf(i32);  // 10

// Must be compile-time computable
const MEM_LAYOUT = @members(MyStruct);
```

## Available Intrinsics

| Intrinsic | Description |
|----------|-------------|
| `@sizeOf(T)` | Size of type in bytes |
| `@members(T)` | Struct members |
| `@hasTrait(T, Trait)` | Check if type has trait |
| `@file()` | Current file path |
| `@line()` | Current line number |
| `@funcName()` | Current function name |
| `@location()` | Full location (file:line) |
| `@components(E)` | Entity components |
| `@toStruct(entity)` | Entity to struct |
| `@toEntity(struct, scene)` | Struct to entity |