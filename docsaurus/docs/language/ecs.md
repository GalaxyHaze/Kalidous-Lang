# ECS & Scenes

Zith has native support for data-oriented programming through Entity Component System (ECS).

## Components

Plain Old Data (POD) with optional inline functions:

```zith
component Position {
    x: f32,
    y: f32
}

component Velocity {
    dx: f32,
    dy: f32
}

component Health {
    hp: u32,
    max_hp: u32,
    
    fn take_damage(self: lend, damage: u32) {
        self.hp = if self.hp > damage {
            self.hp - damage
        } else {
            0
        };
    }
}
```

## Entities (Archetypes)

Container of components with strong typing:

```zith
entity Player {
    Position,
    Velocity,
    Health,
    Inventory
}

entity Projectile {
    Position,
    Velocity
}

// Access
let player: Player = /* ... */;
player.Health.take_damage(10);
player.Position.x += 1.0;
```

### Dynamic Entities

For runtime component addition:

```zith
entity mut GameEntity {
    // Dynamic components
}

let entity: GameEntity = scene.create_entity();
entity.add_component(Position { x: 0, y: 0 });
entity.add_component(Health { hp: 100, max_hp: 100 });
```

## Scenes (Memory Regions)

Scenes are global memory regions that organize entities:

```zith
scene GameWorld(require: 100 entities) {
    // Allocate space for ~100 entities
    
    // Memory policy when full
    policy: Terminate   // kill oldest (default)
    policy: Grow      // dynamically grow
    policy: Circular  // FIFO buffer
}
```

### Scene Organization

```zith
scene MainMenu {
    entity MenuButton { /* ... */ }
    entity TextDisplay { /* ... */ }
}

scene GameLevel {
    entity Player { /* ... */ }
    entity Enemy { /* ... */ }
}

// Transition between scenes
transition(MainMenu, GameLevel);
```

## Threading

Convert entities to structs for threading:

```zith
let struct_data = @toStruct(player);

thread::spawn(|| {
    process(struct_data);
    
    // Reconstruct if needed
    let reconstructed = @toEntity(struct_data, scene);
});
```

## Rules

- **Scenes are global:** Cannot be local/scoped
- **Memory contiguity:** Compiler allocates contiguously
- **Type safety:** Components checked at compile-time
- **Dynamic entities:** `mut` allows runtime components