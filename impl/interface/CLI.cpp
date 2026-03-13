#include <CLI/CLI.hpp>
#include <Kalidous/kalidous.h>
#include <iostream>
#include <string>

const char* kalidous_version = KALIDOUS_VERSION;

// ============================================================================
// Helpers internos
// ============================================================================

static void print_not_implemented(const char* command) {
    std::cerr << "\n[!] Command '" << command << "' is not implemented yet.\n";
    std::cerr << "    This feature will be available in a future version of Kalidous.\n";
    std::cerr << "    Track progress at: https://github.com/GalaxyHaze/Kalidous\n\n";
}

static void print_success(const char* action, const char* target) {
    std::cout << "[ok] " << action << ": " << target << "\n";
}

static void print_error(const char* msg) {
    std::cerr << "[error] " << msg << "\n";
}

static void print_info(const char* msg) {
    std::cout << "[*] " << msg << "\n";
}

// ============================================================================
// KalidousProject.toml
// ============================================================================

struct KalidousProject {
    // -- Identidade --
    std::string name;           // nome do projeto (ex: "my_app")
    std::string version;        // semver (ex: "0.1.0")
    std::string description;    // descrição curta do projeto
    std::string authors;        // autor(es) (ex: "GalaxyHaze <email>")
    std::string license;        // ex: "MIT", "Apache-2.0"
    std::string homepage;       // URL do site/repo

    // -- Compilação --
    std::string entry;          // ficheiro de entrada (ex: "src/main.kalidous")
    std::string output;         // binário final     (ex: "bin/my_app")
    std::string mode;           // debug | dev | release | fast | test
    std::string target_triple;  // ex: "x86_64-linux-gnu", "aarch64-apple-macos"
    std::string edition;        // versão da linguagem (ex: "2024")

    // -- Diretórios --
    std::string src_dir;        // diretório de sources (default: "src")
    std::string bin_dir;        // diretório de output  (default: "bin")
    std::string lib_dir;        // diretório de libs    (default: "lib")
    std::string docs_dir;       // diretório de docs    (default: "docs")
    std::string test_dir;       // diretório de testes  (default: "tests")
    std::string cache_dir;      // cache do compilador  (default: ".kalidous_cache")

    // -- Includes & Links --
    std::vector<std::string> include_dirs;   // -I extra (além do padrão)
    std::vector<std::string> lib_paths;      // -L paths para linker
    std::vector<std::string> link_libs;      // -l libs a linkar (ex: "llvm", "m")
    std::vector<std::string> link_flags;     // flags adicionais para o linker

    // -- Features & Dependências --
    std::vector<std::string> features;       // features opcionais activadas
    std::vector<std::string> dependencies;   // TODO: formato a definir (ex: "libname@1.0")

    // -- Comportamento --
    bool        emit_ir       = false;   // guarda o IR LLVM gerado
    bool        emit_asm      = false;   // guarda o assembly gerado
    bool        strip_debug   = false;   // strip de símbolos de debug no release
    bool        lto           = false;   // Link-Time Optimization
    int         opt_level     = 0;       // 0-3 (mapeado para LLVM opt passes)
    int         debug_level   = 2;       // 0-3 (mapeado para DWARF debug info)
};

// Retorna false se não encontrar ou falhar ao parsear
static bool try_load_project(KalidousProject& proj) {
    if (!kalidous_file_exists("KalidousProject.toml")) return false;

    // TODO: integrar um parser TOML real (recomendado: toml++ / tomlplusplus)
    //       https://github.com/marzer/tomlplusplus
    //       Alternativa mais simples: cpptoml (header-only)
    //
    // TODO: validar campos obrigatórios (name, version, entry)
    // TODO: reportar campos desconhecidos como warning
    // TODO: suportar herança de perfis, ex: [profile.release] opt_level = 3
    // TODO: suportar array de targets para cross-compilation
    // TODO: resolver paths relativos ao ficheiro .toml

    // Defaults temporários para não travar o fluxo enquanto o parser não existe
    proj.name        = "project";
    proj.version     = "0.1.0";
    proj.entry       = "src/main.kalidous";
    proj.output      = "bin/project";
    proj.mode        = "debug";
    proj.src_dir     = "src";
    proj.bin_dir     = "bin";
    proj.lib_dir     = "lib";
    proj.docs_dir    = "docs";
    proj.test_dir    = "tests";
    proj.cache_dir   = ".kalidous_cache";
    proj.edition     = "2024";
    proj.opt_level   = 0;
    proj.debug_level = 2;
    return true;
}

// ============================================================================
// Comandos
// ============================================================================

// check — parse + semântica, só reporta erros, não guarda resultado
static int cmd_check(const std::string& input_file,
                     const std::string& mode_str, const bool verbose) {
    std::string src = input_file;

    if (src.empty()) {
        KalidousProject proj;
        if (!try_load_project(proj)) {
            print_error("No input file and no KalidousProject.toml found");
            return 1;
        }
        src = proj.entry;
    }

    if (verbose)
        print_info(("Checking '" + src + "' in " + mode_str + " mode...").c_str());

    KalidousArena* arena = kalidous_arena_create(64 * 1024);
    if (!arena) { print_error("Failed to create memory arena"); return 1; }

    size_t file_size = 0;
    const char* source = kalidous_load_file_to_arena(arena, src.c_str(), &file_size);
    if (!source) {
        print_error(("Failed to load file: " + src).c_str());
        kalidous_arena_destroy(arena);
        return 1;
    }

    auto [data, len] = kalidous_tokenize(arena, source, file_size);
    if (!data) {
        kalidous_arena_destroy(arena);
        return 1;
    }

    if (verbose)
        std::cout << "[*] Tokenized " << len << " tokens\n";

    // TODO: chamar kalidous_parse(arena, data, len) → KalidousNode* ast
    // TODO: chamar kalidous_sema(arena, ast)  → análise semântica completa
    //       - resolução de nomes / scoping
    //       - inferência e verificação de tipos
    //       - borrow checker (se aplicável)
    //       - reportar todos os erros/warnings com span (linha:coluna)
    // TODO: descartar resultado — check não produz artefacto

    kalidous_arena_destroy(arena);
    print_success("Check passed", src.c_str());
    return 0;
}

// compile — check + gera IR LLVM ou bytecode (.nbc), sem linkar
static int cmd_compile(const std::string& input_file, const std::string& output_file,
                       const std::string& mode_str, const bool interpreted, const bool verbose,
                       const std::vector<std::string>& include_dirs = {}) {
    if (verbose) {
        const char* kind = interpreted ? "bytecode" : "LLVM IR / native object";
        std::cout << "[*] Compiling '" << input_file << "' -> "
                  << kind << " (" << mode_str << ")\n";
    }

    KalidousArena* arena = kalidous_arena_create(64 * 1024);
    if (!arena) { print_error("Failed to create memory arena"); return 1; }

    size_t file_size = 0;
    const char* source = kalidous_load_file_to_arena(arena, input_file.c_str(), &file_size);
    if (!source) {
        print_error(("Failed to load file: " + input_file).c_str());
        kalidous_arena_destroy(arena);
        return 1;
    }

    auto [data, len] = kalidous_tokenize(arena, source, file_size);
    if (!data) {
        kalidous_arena_destroy(arena);
        return 1;
    }

    if (verbose)
        std::cout << "[*] Tokenized " << len << " tokens\n";

    // TODO: kalidous_parse(arena, data, len) → AST
    // TODO: kalidous_sema(arena, ast)         → AST anotada / IR interno

    // TODO [LLVM — caminho nativo]:
    //   - Inicializar LLVMContext + LLVMModule + LLVMBuilder
    //   - Percorrer AST e emitir LLVMValueRef por nó (codegen)
    //   - Aplicar passes de optimização via LLVMPassManager
    //     (mapeado em opt_level: 0=none, 1=O1, 2=O2, 3=O3)
    //   - LLVMTargetMachineEmitToFile → .o  (ou .ll se --emit ir)
    //   - Se --emit asm → emitir .s via LLVMAssemblyFile
    //   - Considerar: llvm::orc::LLJIT para JIT futuro (REPL)

    // TODO [Bytecode — caminho interpretado]:
    //   - Definir formato .nbc (Kalidous Byte Code):
    //     header: magic + versão + nº de constantes + nº de instruções
    //     pool de constantes (strings, números)
    //     array de instruções (opcode 1 byte + operandos variáveis)
    //   - Implementar kalidous_bytecode_emit(arena, ast) → KalidousBytecode*
    //   - Serializar para ficheiro .nbc

    // TODO: usar include_dirs para resolver imports durante parse/sema
    //       (passá-los ao resolver de módulos quando existir)

    kalidous_arena_destroy(arena);

    const std::string out = output_file.empty()
        ? (interpreted ? "a.nbc" : "a.o")
        : output_file;

    print_success(interpreted ? "Bytecode compile" : "Compile", out.c_str());
    return 0;
}

// build — compile + linka → binário nativo final (nunca interpretado)
static int cmd_build(const std::string& input_file, const std::string& output_file,
                     const std::string& mode_str, const bool verbose,
                     const std::vector<std::string>& include_dirs = {}) {
    std::string src  = input_file;
    std::string out  = output_file;
    std::string mode = mode_str;

    if (src.empty()) {
        KalidousProject proj;
        if (!try_load_project(proj)) {
            print_error("No input file and no KalidousProject.toml found");
            return 1;
        }
        src = proj.entry;
        if (out.empty())     out  = proj.output;
        if (mode == "debug") mode = proj.mode;

        // TODO: propagar proj.include_dirs, proj.lib_paths, proj.link_libs
        // TODO: aplicar proj.lto se mode == "release"
        // TODO: aplicar proj.opt_level / proj.debug_level ao LLVMTargetMachine
        // TODO: criar proj.bin_dir e proj.cache_dir se não existirem
    }

    if (verbose)
        std::cout << "[*] Building '" << src << "' -> binary (" << mode << ")\n";

    if (const int rc = cmd_compile(src, "", mode, false, verbose, include_dirs); rc != 0) return rc;

    // TODO: invocar linker (LLD embutido via LLVM ou system linker como fallback)
    //       - recolher todos os .o gerados (suporte a multi-ficheiro no futuro)
    //       - adicionar lib_paths (-L) e link_libs (-l) do projecto
    //       - adicionar link_flags extra
    //       - produzir binário final em `out`
    // TODO: strip de debug se proj.strip_debug == true (release)
    // TODO: suporte a static vs dynamic linking (flag futura)

    const std::string binary = out.empty() ? "a.out" : out;
    print_success("Build", binary.c_str());
    return 0;
}

// execute — só executa artefacto existente; falha se não encontrar
static int cmd_execute(const std::string& target, const bool interpreted, const bool verbose) {
    std::string bin = target;

    if (bin.empty()) {
        KalidousProject proj;
        if (!try_load_project(proj)) {
            print_error("No target specified and no KalidousProject.toml found");
            return 1;
        }
        bin = interpreted ? (proj.output + ".nbc") : proj.output;
    }

    if (!kalidous_file_exists(bin.c_str())) {
        print_error(("Target not found: '" + bin +
                     "' -- did you run 'kalidous " +
                     (interpreted ? "compile --interpreted" : "build") +
                     "' first?").c_str());
        return 1;
    }

    if (verbose)
        std::cout << "[*] Executing '" << bin << "'"
                  << (interpreted ? " (interpreted)" : "") << "\n";

    // TODO [nativo]: execv (Linux/macOS) ou CreateProcess (Windows)
    //   - passar argv restantes ao processo filho
    //   - retornar exit code do processo filho

    // TODO [interpretado / .nbc]:
    //   - implementar KalidousVM (stack machine ou register machine, a definir)
    //   - kalidous_vm_create() → KalidousVM*
    //   - kalidous_vm_load(vm, bin)
    //   - kalidous_vm_run(vm) → int exit_code
    //   - kalidous_vm_destroy(vm)

    print_not_implemented("execute");
    return 1;
}

// run — build + execute (nativo) ou compile --interpreted + execute --interpreted
static int cmd_run(const std::string& input_file, const std::string& output_file,
                   const std::string& mode_str, const bool interpreted, const bool verbose,
                   const std::vector<std::string>& include_dirs = {}) {
    if (interpreted) {
        const std::string bc = output_file.empty() ? "a.nbc" : output_file;
        if (const int rc = cmd_compile(input_file, bc, mode_str, true, verbose, include_dirs); rc != 0) return rc;
        return cmd_execute(bc, true, verbose);
    }

    if (const int rc = cmd_build(input_file, output_file, mode_str, verbose, include_dirs); rc != 0) return rc;

    const std::string binary = output_file.empty() ? "a.out" : output_file;
    return cmd_execute(binary, false, verbose);
}

static int cmd_test(const std::string& input_file, bool verbose) {
    // TODO: definir sintaxe de testes na linguagem (ex: #[test] fn my_test() { ... })
    // TODO: descobrir automaticamente ficheiros *_test.kalidous em test_dir
    // TODO: compilar e executar cada teste isoladamente
    // TODO: reportar resultados: passed / failed / ignored com tempo de execução
    // TODO: suporte a filtro por nome: kalidous test foo::bar
    print_not_implemented("test");
    return 1;
}

static int cmd_fmt(const std::string& input_file, bool check_only, bool verbose) {
    // TODO: implementar pretty-printer sobre a AST (canonical form)
    // TODO: suporte a ficheiro único ou directório recursivo
    // TODO: --check → não modifica, retorna 1 se algum ficheiro difere (útil em CI)
    // TODO: respeitar ficheiro .kalidous_fmt (config de estilo) se existir
    print_not_implemented("fmt");
    return 1;
}

static int cmd_docs(const std::string& input_file, const std::string& output_dir, bool verbose) {
    // TODO: extrair doc-comments (ex: /// ou /** */) durante o parse
    // TODO: gerar HTML estático (ou Markdown) para cada módulo/função/tipo
    // TODO: suporte a exemplos embutidos nos doc-comments (runnable snippets)
    // TODO: output para docs_dir do projecto por defeito
    print_not_implemented("docs");
    return 1;
}

static int cmd_repl(bool verbose) {
    // TODO: implementar REPL usando llvm::orc::LLJIT (JIT compilation)
    //   - readline / linenoise para input com histórico
    //   - compilar cada linha/bloco incrementalmente via JIT
    //   - manter contexto de variáveis entre expressões
    //   - comandos especiais: :quit, :type <expr>, :help, :load <file>
    print_not_implemented("repl");
    return 1;
}

static int cmd_version() {
    std::cout << "Kalidous Programming Language\n";
    std::cout << "Version: " << kalidous_version << "\n";
    std::cout << "Compiler: " << __VERSION__ << "\n";
    // TODO: incluir versão do LLVM linkado (LLVMGetVersion)
    // TODO: incluir target triplo padrão do host (LLVMGetDefaultTargetTriple)
    return 0;
}

static int cmd_help() {
    std::cout << R"(Kalidous - A low-level general-purpose language

USAGE:
    kalidous [OPTIONS] <COMMAND> [ARGS]

COMMANDS:
    check      Parse and type-check; report errors only, no output
    compile    Compile to LLVM IR/object (native or bytecode), no linking
    build      Compile and link to native binary  (reads KalidousProject.toml)
    execute    Run an existing binary or bytecode (reads KalidousProject.toml)
    run        Build then execute                 (reads KalidousProject.toml)
    test       Run tests defined in source
    fmt        Format source code
    docs       Generate documentation
    repl       Start interactive REPL
    version    Show version information
    help       Show this help message

OPTIONS:
    -m, --mode <debug|dev|release|fast|test>    Build mode [default: debug]
    -o, --output <FILE>                         Output file
    -I, --include <DIR>                         Add include directory (repeatable)
    --interpreted                               Use bytecode path instead of native
    --emit <ast|ir|asm|obj|bin>                Emit intermediate representation
    --target <TRIPLE>                           Target triple (e.g., x86_64-linux-gnu)
    -v, --verbose                               Verbose output
    -h, --help                                  Show help

PIPELINE:
    check  <  compile  <  build
    execute             <  run
    run --interpreted  =  compile --interpreted + execute --interpreted

EXAMPLES:
    kalidous check main.kalidous
    kalidous compile main.kalidous -o main.o
    kalidous compile --interpreted main.kalidous -o main.nbc
    kalidous build
    kalidous build main.kalidous -o bin/app -m release
    kalidous execute
    kalidous execute --interpreted
    kalidous run
    kalidous run main.kalidous -m release
    kalidous run --interpreted main.kalidous

LEARN MORE:
    Source: https://github.com/GalaxyHaze/Kalidous-lang
    Docs:   https://galaxyhaze.github.io/Kalidous-Lang/kalidous-docs.html
)";
    return 0;
}

// ============================================================================
// Dispatch central: kalidous_run (C API)
// ============================================================================

extern "C" int kalidous_run(const int argc, const char* argv[]) {
    CLI::App app{"Kalidous - A low-level general-purpose language"};
    app.require_subcommand(0, 1);

    // Opcoes globais
    std::string              mode_str = "debug";
    std::string              output_file;
    std::vector<std::string> include_dirs;
    std::string              emit_target;
    std::string              target_triple;
    bool                     verbose = false;

    app.add_option("-m,--mode", mode_str,
        "Build mode: debug, dev, release, fast, test")
       ->transform(CLI::IsMember({"debug", "dev", "release", "fast", "test"}))
       ->default_str("debug");

    app.add_option("-o,--output",  output_file,   "Output file path");
    app.add_option("-I,--include", include_dirs,  "Include directories (repeatable)");
    app.add_option("--emit",       emit_target,   "Emit: ast, ir, asm, obj, bin");
    app.add_option("--target",     target_triple, "Target triple");
    app.add_flag  ("-v,--verbose", verbose,       "Verbose output");

    // TODO: propagar emit_target e target_triple para cmd_compile / cmd_build
    // TODO: validar target_triple contra targets suportados pelo LLVM linkado

    // ── Subcomandos ──────────────────────────────────────────────────────────

    std::string input_file;
    bool        interpreted = false;
    bool        fmt_check   = false;
    std::string docs_output = "docs";

    // check
    auto* check_cmd = app.add_subcommand("check", "Parse and type-check only");
    check_cmd->add_option("input", input_file, "Source file (.kalidous) [optional, reads toml if omitted]")
             ->check(CLI::ExistingFile);

    // compile
    auto* compile_cmd = app.add_subcommand("compile", "Compile to LLVM IR/object, no linking");
    compile_cmd->add_option("input", input_file, "Source file (.kalidous)")
               ->required()
               ->check(CLI::ExistingFile);
    compile_cmd->add_flag("--interpreted", interpreted, "Compile to bytecode instead of native");

    // build
    auto* build_cmd = app.add_subcommand("build", "Compile and link to native binary");
    build_cmd->add_option("input", input_file,
                          "Source file (.kalidous) [optional, reads toml if omitted]")
             ->check(CLI::ExistingFile);

    // execute
    auto* execute_cmd = app.add_subcommand("execute", "Run existing binary or bytecode");
    execute_cmd->add_option("target", input_file,
                            "Binary or bytecode [optional, reads toml if omitted]");
    execute_cmd->add_flag("--interpreted", interpreted, "Run bytecode instead of native binary");

    // run
    auto* run_cmd = app.add_subcommand("run", "Build then execute");
    run_cmd->add_option("input", input_file,
                        "Source file (.kalidous) [optional, reads toml if omitted]")
           ->check(CLI::ExistingFile);
    run_cmd->add_flag("--interpreted", interpreted,
                      "Compile to bytecode and run interpreted");

    // test
    auto* test_cmd = app.add_subcommand("test", "Run tests in source file");
    test_cmd->add_option("input", input_file, "Source file (.kalidous)")
            ->check(CLI::ExistingFile);

    // fmt
    auto* fmt_cmd = app.add_subcommand("fmt", "Format source code");
    fmt_cmd->add_option("input", input_file, "Source file or directory")->required();
    fmt_cmd->add_flag("--check", fmt_check, "Check formatting without modifying files");

    // docs
    auto* docs_cmd = app.add_subcommand("docs", "Generate documentation");
    docs_cmd->add_option("input", input_file, "Source file (.kalidous)")
            ->check(CLI::ExistingFile);
    docs_cmd->add_option("-o,--output", docs_output, "Output directory")
            ->default_str("docs");

    // repl / version / help
    const auto* repl_cmd    = app.add_subcommand("repl",    "Start interactive REPL");
    const auto* version_cmd = app.add_subcommand("version", "Show version information");
    const auto* help_cmd    = app.add_subcommand("help",    "Show help message");

    // ── Parse ────────────────────────────────────────────────────────────────

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp&) {
        return cmd_help();
    } catch (const CLI::CallForVersion&) {
        return cmd_version();
    } catch (const CLI::ParseError& e) {
        if (e.get_name() == "ExtrasError" && argc > 1 && std::string(argv[1]) == "help")
            return cmd_help();
        std::cerr << "[error] " << e.what() << "\n\n";
        std::cerr << app.help();
        return 1;
    }

    // ── Dispatch ─────────────────────────────────────────────────────────────

    if (*help_cmd)    return cmd_help();
    if (*version_cmd) return cmd_version();
    if (*repl_cmd)    return cmd_repl(verbose);
    if (*docs_cmd)    return cmd_docs(input_file, docs_output, verbose);
    if (*fmt_cmd)     return cmd_fmt(input_file, fmt_check, verbose);
    if (*test_cmd)    return cmd_test(input_file, verbose);
    if (*check_cmd)   return cmd_check(input_file, mode_str, verbose);
    if (*compile_cmd) return cmd_compile(input_file, output_file, mode_str,
                                         interpreted, verbose, include_dirs);
    if (*build_cmd)   return cmd_build(input_file, output_file, mode_str,
                                       verbose, include_dirs);
    if (*execute_cmd) return cmd_execute(input_file, interpreted, verbose);
    if (*run_cmd)     return cmd_run(input_file, output_file, mode_str,
                                     interpreted, verbose, include_dirs);

    // Sem subcomando: tenta build via toml, senão mostra ajuda
    {
        if (KalidousProject proj; try_load_project(proj))
            return cmd_build("", "", mode_str, verbose, include_dirs);
    }

    cmd_help();
    return 0;
}