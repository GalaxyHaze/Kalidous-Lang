#include "../internal/core.h"

void* operator new(const size_t s){
    std::cerr << "Allocating memory for " << s << " bytes: " <<  '\n';
    return malloc(s);
}

int main() {
    const auto stream = nova::file::readSource();
    const auto tokens = Tokenizer::tokenize(stream);
    //const auto Tree = parse(tokens);
    //printTree(Tree);
    printTokens(tokens);

}
