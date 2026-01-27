// ast.h
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// Forward-declare only what's needed in public API
struct Token;
struct Arena;

namespace nova::ast {

    enum class NodeType : uint8_t {
        end = 0x0,
        Literal,
        BinaryExpression,
        Identifier,
        VariableDeclaration,
        FunctionDeclaration,
        IfStatement,
        WhileStatement,
        ReturnStatement,
        Unknown = 0xFF,
    };

    // Opaque handle – clients never see internals
    struct ASTNode;
    using Node = ASTNode*;
    struct ValidationContext;
    struct CodeGenContext;
    struct OutputStream;

    // Public interface functions
    Node parse(std::span<Token> tokens, size_t token_count, Arena& arena);
    NodeType getNodeType(Node node);

    struct NodeVTable {
        void (*print)(const ASTNode*, OutputStream*, size_t depth);
        bool (*validate)(const ASTNode*, ValidationContext*);
        void (*codegen)(const ASTNode*, CodeGenContext*);
    };

    inline NodeVTable dispatchNode[256] = {};


} // namespace nova::ast