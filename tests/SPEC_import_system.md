# Symbol Import and Resolution System - Technical Requirements

**Version:** 1.0  
**Status:** Specification  
**Created:** April 2026

---

## 1. Overview

This document defines the technical requirements for Zith's symbol import and resolution system. The system provides explicit, fully-qualified symbol resolution with support for duplicate detection, function overloading aliases, and multi-level module mappings.

## 2. Core Requirements

### 2.1 Fully Qualified Path Resolution

**REQ-FQ-001:** The system SHALL resolve symbols using fully qualified paths (e.g., `std.io.console.log`).

**REQ-FQ-002:** The delimiter for qualified paths SHALL be `.` for module-to-symbol references and `/` for directory-to-module references.

**REQ-FQ-003:** The system SHALL support nested module paths of arbitrary depth (e.g., `a.b.c.d.symbol`).

**REQ-FQ-004:** Resolution SHALL be case-sensitive.

**REQ-FQ-005:** The resolution process SHALL:
1. Parse the fully qualified name into path components
2. Locate the module (import) corresponding to the first component(s)
3. Traverse the module's symbol table to find the target symbol
4. Return the resolved symbol with its metadata (file location, type, visibility)

### 2.2 Module-to-File Mapping

**REQ-FM-001:** Each import (module) SHALL be mapped to exactly one defining file.

**REQ-FM-002:** The mapping SHALL support multi-level directory structures:
```
std/io/console.zith  → module "std.io.console"
std/io/file.zith     → module "std.io.file"
utils/helper.zith   → module "utils.helper"
```

**REQ-FM-003:** The system SHALL maintain a module registry that tracks:
- Module name (fully qualified)
- Source file path
- Exported symbols list
- Version (optional)

**REQ-FM-004:** Module names MUST be unique within the global registry.

### 2.3 Duplicate Symbol Detection

**REQ-DU-001:** The system SHALL detect duplicate symbol declarations within the same scope.

**REQ-DU-002:** Duplicate detection SHALL operate at the semantic analysis phase (not just parsing).

**REQ-DU-003:** The system SHALL distinguish between:
- **Invalid duplicates:** Two symbols with the same name and identical signature
- **Valid overloads:** Functions with the same name but different parameter types/counts

**REQ-DU-004:** When a duplicate is detected, the system SHALL emit:
- Error message with file location
- Line numbers of both declarations
- Suggestion for resolution (rename or use alias)

### 2.4 Function Overloading Support

**REQ-OV-001:** The system SHALL allow function overloading when semantically valid.

**REQ-OV-002:** Two functions are considered valid overloads if they differ in:
- Parameter count
- Parameter types (at least one differs)
- Return type (accompanying parameter difference)

**REQ-OV-003:** The system SHALL reject overloading if:
- Functions have identical signatures (same name, same parameter types/counts)
- Parameter types are functionally equivalent (considering const/ref)

**REQ-OV-004:** Method resolution for overloaded functions SHALL use type-based overload resolution:
```
// Valid overloads
fn add(a: int, b: int) → int
fn add(a: float, b: float) → float
fn add(a: string, b: string) → string

// Invalid - duplicate
fn add(a: int, b: int) → int  // already defined
```

**REQ-OV-005:** Overloaded functions SHALL be stored with their complete signature for resolution.

### 2.5 Alias Support

**REQ-AL-001:** The system SHALL support defining aliases for imported symbols.

**REQ-AL-002:** Aliases SHALL be defined at import time:
```
import std.io.console as io;
import std.io.console.log as print;
```

**REQ-AL-003:** The system SHALL support module aliases:
```
import std.io as io;
io.console.log("hello");  // resolves to std.io.console.log
```

**REQ-AL-004:** Aliases MUST NOT conflict with existing symbol names in the importing scope.

**REQ-AL-005:** Aliases SHALL be tracked in the symbol table with:
- Original symbol reference
- Alias name
- Source location of alias definition

---

## 3. Data Structures

### 3.1 Symbol Entry

```cpp
struct Symbol {
    std::string name;              // Symbol name (unqualified)
    std::string fully_qualified;   // Full path (e.g., "std.io.console.log")
    SymbolKind kind;              // Function, Struct, Enum, Trait, etc.
    Visibility visibility;         // Public, Protected, Private
    SourceLocation location;        // File and line
    TypeSignature signature;         // For functions: parameter types, return type
    bool is_exported;            // Whether symbol is exported
};
```

### 3.2 Module Registry

```cpp
class ModuleRegistry {
    // Maps module name → Module
    unordered_map<std::string, Module> modules_;

    // Register a new module
    bool register_module(Module module);

    // Get module by name
    Module* get_module(const std::string& name);

    // Check if module exists
    bool exists(const std::string& name) const;
};
```

### 3.3 Symbol Table

```cpp
class SymbolTable {
    // Module-level symbol storage
    struct ModuleSymbols {
        std::vector<Symbol> types;      // structs, enums, traits
        std::vector<Symbol> functions;
        std::vector<Symbol> values;   // constants, variables
    };

    // symbols_by_module[module_name] → ModuleSymbols
    unordered_map<std::string, ModuleSymbols> symbols_by_module_;

    // Add symbol to table
    bool add_symbol(const std::string& module, const Symbol& symbol);

    // Resolve symbol
    SymbolResolution resolve(const std::string& fully_qualified);

    // Detect conflicts
    std::vector<Conflict> detect_conflicts(const std::string& module);
};
```

### 3.4 Resolution Result

```cpp
class SymbolResolution {
    Symbol* symbol;              // Resolved symbol (or nullptr)
    std::string module_name;      // Module where found
    bool is_ambiguous;          // Multiple matches found
    std::vector<Symbol*> candidates;  // All candidates (for errors)
};
```

---

## 4. Error Handling

### 4.1 Error Codes

| Code | Description |
|------|-------------|
| E0001 | Module not found |
| E0002 | Symbol not found |
| E0003 | Duplicate symbol declaration |
| E0004 | Invalid overload (duplicate signature) |
| E0005 | Alias conflicts with existing symbol |
| E0006 | Circular dependency detected |
| E0007 | Symbol not exported (private access) |
| E0008 | Ambiguous resolution (multiple candidates) |

### 4.2 Error Messages

```
Error E0003: Duplicate symbol 'add' in module 'math'
  → math.zith:10:1: First declaration
  → math.zith:25:5: Second declaration

Error E0008: Ambiguous symbol 'log' - multiple candidates
  → std.io.console.log
  → std.logging.log
Consider using a fully qualified path.
```

---

## 5. Testing Requirements

### 5.1 Test Directory Structure

```
tests/
├── CMakeLists.txt
├── test_import_system.cpp       # Main test runner
├── test_modules/
│   ├── module_a.zith       # Test module A
│   ├── module_b.zith       # Test module B
│   └── sub/
│       └── module_c.zith  # Nested module
├── cases/
│   ├── test_resolution.cpp      # Fully qualified path resolution
│   ├── test_duplicates.cpp   # Duplicate detection
│   ├── test_overloading.cpp  # Function overloading
│   └── test_aliases.cpp       # Alias support
└── fixtures/
    ├── modules/            # Test fixtures
    └── expected/         # Expected outputs
```

### 5.2 Test Cases

#### 5.2.1 Resolution Tests

| ID | Description | Expected |
|----|-------------|----------|
| RES-001 | Resolve `std.io.console.log` | Symbol found, correct module |
| RES-002 | Resolve nested path `a.b.c.d` | Symbol found |
| RES-003 | Resolve non-existent path | Error E0002 |
| RES-004 | Resolve non-existent module | Error E0001 |
| RES-005 | Case sensitivity | `Console.log` ≠ `console.log` |

#### 5.2.2 Duplicate Detection Tests

| ID | Description | Expected |
|----|-------------|----------|
| DUP-001 | Duplicate function (same signature) | Error E0003 |
| DUP-002 | Duplicate struct | Error E0003 |
| DUP-003 | Duplicate in different files | Error E0003 |
| DUP-004 | Valid re-export | Success |

#### 5.2.3 Overloading Tests

| ID | Description | Expected |
|----|-------------|----------|
| OVL-001 | Different param count | Success (overload) |
| OVL-002 | Different param types | Success (overload) |
| OVL-003 | Same signature | Error E0004 |
| OVL-004 | Different return only | Error E0004 |

#### 5.2.4 Alias Tests

| ID | Description | Expected |
|----|-------------|----------|
| ALS-001 | Module alias | Resolves correctly |
| ALS-002 | Symbol alias | Resolves correctly |
| ALS-003 | Alias conflicts | Error E0005 |
| ALS-004 | Chained alias | Resolves correctly |
| ALS-005 | Alias to overloaded function | Resolves to specific overload |

---

## 6. Implementation Phases

### Phase 1: Foundation
- Module registry data structure
- Basic symbol table
- File-to-module mapping

### Phase 2: Resolution
- Fully qualified path parsing
- Symbol resolution algorithm
- Error reporting

### Phase 3: Validation
- Duplicate detection
- Overload validation
- Alias support

### Phase 4: Testing
- Test framework setup
- Comprehensive test cases
- Integration tests

---

## 7. Acceptance Criteria

1. ✅ Symbols resolve through fully qualified paths
2. ✅ Modules map correctly to source files
3. ✅ Duplicate declarations detected with clear errors
4. ✅ Valid function overloading works without conflicts
5. ✅ Invalid overloading (duplicate signature) rejected
6. ✅ Aliases work for both modules and symbols
7. ✅ All test scenarios pass
8. ✅ Error messages are clear and actionable

---

## 8. Notes

- This spec assumes a single-threaded compilation model
- Module caching may be added for performance
- Lazy loading of modules is optional for future phases
- The system should integrate with the existing `impl/parser` and `impl/import` modules