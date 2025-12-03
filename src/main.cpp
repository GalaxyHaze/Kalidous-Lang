#include "../internal/core.h"

void* operator new(const size_t s){
    std::cerr << "Allocating memory for " << s << " bytes: " << s << '\n';
    return malloc(s);
}

int main() {
    const auto stream = nova::file::readSource();
    const auto tokens = Tokenizer::tokenize(stream);
    //static_assert(lookupToken("let") == TokenType::Let, "Token lookup failed");
    //const auto Tree = parse(tokens);
    //printTree(Tree);
    if (const auto t = lookupToken("let"); t != TokenType::Let) {
        std::cerr << "Token lookup failed for 'let'" << std::endl;
        std::abort();
    }
    printTokens(tokens);

}
