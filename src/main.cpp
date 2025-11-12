#include "../internal/core.h"

void* operator new(const std::size_t size){
    std::cout << "Allocating memory for " << size << " bytes" << '\n';
    const auto ptr = malloc(size);
    if (!ptr)
        throw std::bad_alloc{};
    return ptr;

}

int main() {
    const auto stream = nova::file::readSource();
    const auto tokens = Tokenizer::tokenize(stream);
    printTokens(tokens);

}
