#include "include/Nova/core.h"
#include "include/Nova/memory/bumpAllocator.h"
#include "include/Nova/utils/file.h"

int main(const int argc, const char** argv) {
    //Nova::CoreInterfaceCommand::parse(argc, argv);
    const nova::Arena arena;
    auto input = nova::file::readSource(arena);

}
