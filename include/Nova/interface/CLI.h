// nova_cli.h
#pragma once

#include <string>
#include <string_view>

namespace Nova {

    enum class BuildMode {
        debug,
        dev,
        release,
        fast,
        test
    };

    BuildMode string_to_build_mode(std::string_view mode_str);

    struct Options {
        std::string input_file;
        BuildMode mode = BuildMode::debug;
        bool show_version = false;
    };

    class CoreInterfaceCommand {
    public:
        CoreInterfaceCommand() = delete;
        CoreInterfaceCommand(const CoreInterfaceCommand&) = delete;
        CoreInterfaceCommand& operator=(const CoreInterfaceCommand&) = delete;

        static Options parse(int argc, const char** argv);
    };

} // namespace Nova