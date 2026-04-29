# Ownership Keywords

Zith uses five explicit ownership keywords that enforce memory safety at compile-time through the Node Resource Model (NRM).

## The Five Keywords

### `unique` — Exclusive Ownership

Only one edge (reference) can point to a node at any time. When assigned, the original becomes a zombie.

```zith
let resource = unique ResourceHandle.new();
let other = resource;              // resource is now zombie
other.use();                      // OK
resource.use();                  // ERROR: zombie
```

**Use case:** Default ownership for most types.

### `share` — Shared Ownership

Multiple edges can point to the same node simultaneously. All owners can mutate.

```zith
let data = Data.new();
let s1 = data as share;
let s2 = s1;                     // Both valid
s1.mutate();                     // OK
s2.read();                       // OK
```

**Use case:** Multiple owners, shared mutable state. Requires `Shared` trait.

### `view` — Read-Only Borrow

Read-only access without ownership transfer. Cannot outlive the owner.

```zith
fn print_info(data: view Resource) {
    println("{}", data.name);      // OK (read)
    data.modify();                 // ERROR: immutable
}

let res = Resource.new();
print_info(view res);              // res still valid after
```

**Use case:** Efficient reading without ownership.

### `lend` — Mutable Borrow

Exclusive mutable borrow. Must be returned to the owner.

```zith
fn increment(counter: lend i32) {
    counter += 1;
}

var x = 0;
increment(lend x);               // x is borrowed
println("{}", x);                // prints 1, x is returned
```

**Use case:** Temporary exclusive mutation.

### `extension` — Hierarchical Part-Of

Child is structurally part of parent; cannot outlive parent.

```zith
struct Node<T> {
    value: T,
    next: share Node<T>?,
    prev: extension Node<T>?     // structurally part of parent
}
```

**Use case:** Self-referential structures (trees, linked lists). Eliminates need for weak pointers or unsafe.

## Summary Table

| Keyword    | Owns | Exclusive | Mutable | Thread Safe |
|------------|------|-----------|---------|-------------|
| `unique`   | Yes  | Yes       | Yes     | Yes (with Shared) |
| `share`    | Yes  | No        | Yes     | Yes (with Shared) |
| `view`     | No   | —         | No      | Yes |
| `lend`     | No   | Yes       | Yes     | Yes |
| `extension`| No   | —         | Yes     | Yes |