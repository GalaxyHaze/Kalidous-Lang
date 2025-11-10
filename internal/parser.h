#pragma once
#include <memory>

class ASTnode;
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
};
