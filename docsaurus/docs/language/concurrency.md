# Concurrency

Zith provides structured concurrency with thread safety guarantees.

## Global Variables

Static variables with ownership rules:

```zith
// Must implement Shared trait (thread-safe)
global score: share i32 = 0;

// Must implement Lent trait (atomic access)
global counter: unique i32 = 0;

// Access
global score += 10;
```

## Thread Safety

- **`global: share`** — requires `Shared` trait (explicitly thread-safe)
- **`global: unique`** — requires `Lent` trait (atomic operations)

## Spawning Threads

```zith
fn concurrent_work() {
    let handle = thread::spawn(|| {
        // Convert entity to struct for threading
        let data = @toStruct(player);
        process(data);
        data
    });
    
    let result = handle.join();  // Must join before scope ends
}
```

## Arc for Unbounded Sharing

```zith
let arc_data = Arc.new(data);
let clone1 = arc_data.clone();
let clone2 = arc_data.clone();

// All owners share, reference counted
```

## Rules

- Global variables must implement appropriate traits
- Threads must join before scope ends (structured concurrency)
- Use `@toStruct` to convert entities for threading