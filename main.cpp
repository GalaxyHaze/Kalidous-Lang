#include "Kalidous/interface/CLI.h"

int main(const int argc, char** argv) {
    return kalidous_try_parse(argc, argv);
}