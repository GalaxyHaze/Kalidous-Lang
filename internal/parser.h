#pragma once
#include <memory>
#include <utility>
#include <vector>
#include <functional>
#include "tokens.h" // assumindo que TokenType e Token vêm daqui

class ASTNode;

enum class NodeType {
    Literal,
    BinaryExpression,
    Identifier,
    VariableDeclaration,
    FunctionDeclaration,
    IfStatement,
    WhileStatement,
    ReturnStatement,
    Unknown
};

using Node = std::unique_ptr<ASTNode>;
using Evaluator = std::function<Node(std::vector<TokenType>&, size_t&)>;

class ASTNode {
    Node next = nullptr;                 // ponteiro opcional p/ próxima expressão (sequência)
    std::vector<Node> children;          // subnós (por exemplo, argumentos, corpo, etc.)
    Evaluator eval = nullptr;            // função opcional de avaliação
    NodeType type = NodeType::Unknown;
    Token token;

public:
    ASTNode() = delete;
    ASTNode(const NodeType type, Token token, Evaluator evaluator = nullptr)
        : eval(std::move(evaluator)), type(type), token(std::move(token)) {}

    // Cria e adiciona um novo filho com um token específico
    ASTNode& addChild(Node child) {
        children.push_back(std::move(child));
        return *this;
    }

    // Liga esta expressão à próxima (útil para encadeamentos)
    ASTNode& addExpression(Node node) {
        next = std::move(node);
        return *this;
    }

    // Getters simples
    [[nodiscard]] const std::vector<Node>& getChildren() const noexcept { return children; }
    [[nodiscard]] NodeType getType() const noexcept { return type; }
    [[nodiscard]] const auto& getToken() const noexcept { return token; }
    [[nodiscard]] const auto& getNext() const noexcept { return next; }

    // Caso queiras processar/avaliar o nó
    Node evaluate(std::vector<TokenType>& tokens, size_t& pos) const {
        if (eval) return eval(tokens, pos);
        return nullptr;
    }

    //static

};

inline void printTree(const Node &node, const size_t depth = 0) {
    if (!node) return;
    for (int i = 0; i < depth; ++i) std::cout << "  ";
    std::cout << "Node(" << static_cast<int>(node->getType()) << ")\n";
    for (const auto& child : node->getChildren()) printTree(child, depth + 1);
    printTree(node->getNext(), depth);

}

// Parser simples: cria uma AST básica percorrendo os tokens
inline Node parse(const std::vector<Token>& tokens) {
    size_t position = 0;
    auto root = std::make_unique<ASTNode>(NodeType::Unknown, Token{TokenType::Unknown, " ", Info{}});

    while (position < tokens.size()) {
        // Exemplo: cria literal simples
        if (auto& tok = tokens[position]; tok.token == TokenType::Number || tok.token == TokenType::String) {
            root->addChild(std::make_unique<ASTNode>(NodeType::Literal, tok));
        }
        // Exemplo: identificadores (variáveis)
        else if (tok.token == TokenType::Identifier) {
            root->addChild(std::make_unique<ASTNode>(NodeType::Identifier, tok));
        }

        position++;
    }

    return root;
}