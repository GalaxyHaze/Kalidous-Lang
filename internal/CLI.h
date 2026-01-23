#pragma once

#include <string>
#include <iostream>
#include <CLI/CLI.hpp>

namespace Nova {

class CoreInterfaceCommand {
public:
    CoreInterfaceCommand() = delete;
    CoreInterfaceCommand(const CoreInterfaceCommand&) = delete;
    CoreInterfaceCommand& operator=(const CoreInterfaceCommand&) = delete;

    // Must be called early in main()
    static void parse(const int argc, const char** argv) {
        // 🔥 EARLY FAILURE: no arguments beyond program name
        if (argc <= 1) {
            std::cerr << "Error: No input file provided.\n";
            std::cerr << "Usage: " << (argv[0] ? argv[0] : "nova") << " [OPTIONS] <input_file>\n";
            std::cerr << "Use --help for more information.\n";
            std::exit(1);
        }

        static CLI::App app{"Nova - A low-level general-purpose language"};
        static std::string input_file;
        static bool debug_mode = false;
        static bool show_version = false;

        // Define options
        app.add_option("input", input_file, "Input source file (.nv)")
           ->required()
           ->check(CLI::ExistingFile);

        app.add_flag("--debug", debug_mode, "Enable debug output");

        app.add_flag("--version", show_version, "Show version and exit");

        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            // CLI11 handles --help automatically; other errors go here
            std::exit(app.exit(e));
        }

        // Handle --version early
        if (show_version) {
            std::cout << "Nova v0.1.0 (Jan 2026)\n";
            std::exit(0);
        }

        // Store results in static members for accessors
        s_input_file = std::move(input_file);
        s_debug_mode = debug_mode;
    }

    // Accessors (safe to call after parse())
    static const std::string& input_file() { return s_input_file; }
    static bool debug() { return s_debug_mode; }

private:
    static std::string s_input_file;
    static bool s_debug_mode;
};

// Static member definitions
std::string CoreInterfaceCommand::s_input_file;
bool CoreInterfaceCommand::s_debug_mode = false;

} // namespace Nova