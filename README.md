# Nova Language

> **Write once, reuse everywhere** - A modern, ECS-first programming language that combines C-family performance with beginner-friendly syntax.

## 🚀 What is Nova?

Nova is a general-purpose programming language designed for the next generation of software development. It brings the performance benefits of Entity-Component-System (ECS) architecture to mainstream programming with an intuitive, C-family inspired syntax.

### Key Features

- **🏗️ ECS by Design**: Built-in entity-component system with OOP-like interfaces
- **💾 Memory-Aware**: Compile-time memory planning with `reserve` keyword
- **🎯 Beginner Friendly**: Clean syntax with gradual learning curve
- **⚡ Performance First**: Zero-cost abstractions and predictable memory layout
- **🛡️ Safety by Default**: Immutable-by-default with explicit mutability
- **🔧 Multi-Paradigm**: Supports procedural, data-oriented, and compositional programming

## Quick Start

### Hello, World!
```nova
import std.io;

void main() public {
    print("Hello, World!");
}
```

### ECS in Action
```nova
// Define components
entity Components {
    component Position { x: f64, y: f64 };
    component Velocity { dx: f64, dy: f64 };
    component Health { current: i32, max: i32 };
};

// Create a family of behaviors
family MovingEntity requires Position, Velocity;

// Implement an entity
entity Player : MovingEntity reserve 100 {
    component Position;
    component Velocity;
    component Health { current: 100, max: 100 };
    
    public moveTo(x: f64, y: f64) -> void {
        this.Position.x = x;
        this.Position.y = y;
    }
    
    public takeDamage(amount: i32) -> Result<bool, string> {
        if amount < 0 {
            return Result.err("Damage cannot be negative");
        }
        this.Health.current = max(0, this.Health.current - amount);
        return Result.ok(this.Health.current == 0);
    }
};

// Usage
let player = Player {
    Position: { x: 0.0, y: 0.0 },
    Velocity: { dx: 1.0, dy: 0.0 },
    Health: { current: 100, max: 100 }
};
```

## 🎯 Why Nova?

### For Game Developers
```nova
// Memory-efficient entity management
entity Enemy reserve 1000 overflow resize(1.5) {
    component AI { behavior: AIBehavior, state: i32 };
    component Physics { velocity: Vector3, mass: f64 };
    
    // Compile-time memory planning
    public static preallocate() -> void {
        // Compiler reserves space for 1000 enemies upfront
    }
};
```

### For Systems Programming
```nova
// Predictable memory layout
entity NetworkPacket reserve 1024 packing(packed) {
    component Header {
        protocol: u8 align(1),
        length: u16 align(1),
        checksum: u32 align(1)
    };
};
```

### For Beginners
```nova
// Simple and readable
let numbers = [1, 2, 3, 4, 5];
var sum = 0;

for number in numbers {
    sum = sum + number;
}

print("Sum: " + toString(sum));
```

## 📚 Core Concepts

### Memory Management
Nova gives you compile-time control over memory with intelligent defaults:

```nova
// Reserve specific counts at compile time
entity Player reserve 8 overflow error { ... };      // Error if full
entity Particles reserve 1000 overflow resize { ... }; // Grow automatically
entity Audio reserve 32 overflow overwrite { ... };  // Overwrite oldest

// Compiler calculates total memory needs
// Transform: 8 + 1000 + 32 = 1040 slots
// Physics: 8 + 1000 = 1008 slots
```

### Error Handling
Choose your safety level:

```nova
// 1. Internal errors (auto-handled)
let array = [1, 2, 3];
let value = array[5]; // Handled by built-in bounds checking

// 2. Error-as-value (explicit)
public loadFile(path: string) -> Result<string, FileError> {
    let file = File.open(path);
    if !file.exists() {
        return Result.err(FileError.NOT_FOUND);
    }
    return Result.ok(file.readAll());
}
```

### Component System
```nova
// Pure data components
entity MathComponents {
    component Vector3 {
        x: f64, y: f64, z: f64;
        
        public normalized() -> Vector3 {
            let length = sqrt(x*x + y*y + z*z);
            return Vector3 { x: x/length, y: y/length, z: z/length };
        }
    };
};

// Behavior through families
family Renderable requires Transform, Material;

// Composition over inheritance
entity GameObject : Renderable, Collidable, Updatable {
    // Mix and match capabilities
};
```

## 🛠️ Installation

```bash
# Clone and build
git clone https://github.com/nova-lang/nova
cd nova
make install

# Or use package managers
brew install nova-lang
# or
cargo install nova-compiler
```

## 📖 Documentation

- [Getting Started Guide](docs/getting-started.md)
- [Language Specification](docs/specification.md)  
- [ECS Programming](docs/ecs-guide.md)
- [Standard Library](docs/stdlib.md)

## 🎯 Use Cases

### Perfect For:
- 🎮 Game development and game engines
- 🔬 Scientific simulations
- 🖥️ Systems programming
- 📱 Performance-sensitive applications
- 🏫 Teaching programming concepts

### Getting Started Templates:
```bash
nova new my-game      # Game project template
nova new my-app       # General application
nova new my-library   # Library project
```

## 🌟 Community

- [Discord](https://discord.gg/nova-lang) - Chat with the community
- [Examples](https://github.com/nova-lang/examples) - Sample projects
- [RFCs](https://github.com/nova-lang/rfcs) - Language evolution

## 📄 License

Nova is open-source under the MIT License.

---

**Ready to start?** Check out our [Interactive Tutorial](https://nova-lang.org/learn) or browse the [Examples Gallery](https://nova-lang.org/examples)!

---
*"Write once, reuse everywhere" - The Nova Philosophy*
