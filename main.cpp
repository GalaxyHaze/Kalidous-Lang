#include "Nova/interface/CLI.h"

int main(const int argc, char** argv) {
    return nova_try_parse(argc, argv);
}