#ifdef ZITH_WASM
#include <iostream>
#include <cassert>

int main() {
    std::cout << "Thank you for installing Zith." << std::endl;
    std::cout << "The WASM version is currently under development and opaque." << std::endl;
    std::cout << "I'm doing my best to bring you new features soon. Stay tuned!" << std::endl;
    return 0;
}
#else
#include "import/symbol_table.hpp"
#include <iostream>

using namespace zith::import;
int main() {
    auto& sTable = SymbolTable::instance();
    Import io("std/io/console", 1);
    io.add_type("Vector", SymbolKind::Struct, Visibility::Public);
    io.add_function("Vector.println", Visibility::Public);

    sTable.register_import(io);

    auto resolved = sTable.resolve("std/io/console/Vector.println");
    auto sym_opt = resolved.get_symbol();
    if (sym_opt) {
        std::cout << "Found: " << sym_opt->name() << "\n";
        return 0;
    }
    std::cout << "Not found\n";
    return -1;
}
#endif