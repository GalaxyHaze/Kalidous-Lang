# CLI Driver

Command-line interface and compilation pipeline orchestration.

## Files

```
cli/
└── CLI.cpp    # CLI driver (uses CLI11)
```

## Entry Point

```
main.cpp → zith_run() → CLI.cpp
```

## Command-Line Interface

```bash
# Build a project
zith build
zith build --release

# Run a file
zith run file.zith
zith run file.zith --args "arg1 arg2"

# Type check
zith check file.zith

# Format
zith fmt file.zith

# Start REPL
zith repl

# Generate docs
zith docs

# Clean build artifacts
zith clean

# New project
zith new project_name
```

## Project Configuration

Reads `ZithProject.toml` for project settings:

```toml
name = "myproject"
version = "0.1.0"
description = "My project"
authors = ["Author Name"]

[build]
release = false
optimize = true

[dependencies]
std = "latest"
```

## Pipeline Orchestration

The CLI orchestrates the full compilation:

```cpp
// 1. Load source or ZithProject.toml
std::string source = load_input(cli.input);

// 2. Tokenize
ZithArena arena;
ZithTokenList tokens = zith_tokenize(source.c_str(), &arena);

// 3. Parse to AST
ZithAstNode *ast = zith_parse_with_source(source.c_str(), &arena);

// 4. Generate LLVM IR
llvm::Module *mod = zith_codegen(ast, &context);

// 5. Emit executable or library
zith_emit(mod, output_path);
```

## Options

| Flag | Description |
|------|-------------|
| `-o`, `--output` | Output file/path |
| `-O<0-3>` | Optimization level |
| `-g` | Debug symbols |
| `--emit-llvm` | Output LLVM IR |
| `--target` | Target triple |

## Dependencies

- **CLI11** — Command-line argument parsing
- **LLVM** — Code generation and JIT

## Integration

- Uses `lexer/` for tokenization
- Uses `parser/` for AST generation
- Uses `include/LLVM/` for code generation
- Uses `diagnostics/` for error output

## See Also

- `docsaurus/` — Documentation generator
- `include/LLVM/` — LLVM codegen