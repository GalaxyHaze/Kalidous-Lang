# Contexts (DSL Namespaces)

Contexts are isolated environments that define custom syntax, keywords, and operators for domain-specific languages (DSLs).

## Defining a Context

```zith
context WebApp {
    // Macros
    macro @getParam(name) {
        request.params[name]
    }
    
    // Constants
    const VERSION = "1.0";
    
    // Custom keywords
    macro async { /* async handling */ }
    
    // Tag macros (HTML-like syntax)
    tag macro div(content) {
        render("<div>", content, "</div>")
    }
}
```

## Activating a Context

```zith
context WebApp {
    // WebApp syntax is active here
    let version = VERSION;
    let param = @getParam("name");
    <div> Hello </div>
}
// Outside, WebApp syntax is not available
```

## Rules

- **One Global Context:** Only one context at module level
- **Local Contexts:** Can be opened in blocks
- **Namespace Isolation:** Contexts don't pollute global namespace
- **Composition:** Contexts can be nested

```zith
context Web {
    context Database {
        // Both Web and Database syntax available
    }
}
```

## Tag Macros

Tag macros provide HTML-like syntax for DSLs:

```zith
tag macro div(class: string, content) {
    "<div class=\"" + class + "\">" + content + "</div>"
}

let html = <div class="container">Content</div>;
```

## Use Cases

- **Web Development:** HTML, CSS, HTTP routing
- **Database:** SQL query building
- **Graphics:** Shader definitions
- **Configuration:** TOML/YAML-like syntax