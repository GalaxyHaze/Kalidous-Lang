#pragma once
#include <memory>
#include <utility>
#include <vector>
#include "tokens.h"

/*class ASTnode;
//NOTE: for now I will use std::unique_ptr, but later I will replace to another thing + an Arena Allcoator
typedef std::unique_ptr<ASTnode>(Evaluator)();

enum class ASTKind {
    Literal,
    Binary,
    Identifier
};





class ASTNode {
public:
    ASTKind kind;
    std::string_view value;
    std::vector<ASTNode*> children;

    ASTNode(ASTKind k, std::string_view v = {}) : kind(k), value(v) {}

    static ASTNode* parse(const std::vector<Token>& tokens);
};*/

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
    Unknow
};

using Node = std::unique_ptr<ASTNode>;
using Evaluator = std::function<Node(std::vector<Token>& tokens, size_t& position)>;

class ASTNode {

    Node next = nullptr;
    std::vector<Node> children;
    Evaluator eval = nullptr;
    NodeType type = NodeType::Unknow;
    Token token = Token::Unknown;

public:

    ASTNode() = default;
    ASTNode(const NodeType type, const Token token, Evaluator evaluator):
    eval(std::move(evaluator)), type(type), token(token) {}

    auto addChild(const TokenType& token) {

    }
    auto addExpression(Node node) {
        next = std::move(node);
    }
};

inline auto parse(std::vector<Token>& tokens) {

}
