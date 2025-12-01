//
// Created by dioguabo-rei-delas on 11/5/25.
//

#ifndef NOVA_TOKENS_H
#define NOVA_TOKENS_H

#include <cstdint>
typedef unsigned short u_short;

// --- Info ---
struct Info {
    u_short index{1};
    u_short line{1};

    //size_t operator++(int) { return index++; }
    Info& operator++(int) { index++; return *this; }
    [[nodiscard]] u_short get() const {
        return index;
    }
    Info& operator+=(const u_short offset) { index += offset; return *this; }
    explicit operator size_t() const { return index; }

    void newLine() {
        ++line;
        index = 1;
    }

    void newLine(const size_t size) {
        line += size;
        index = 1;
    }
};

// --- TokenType Types ---
enum class TokenType : uint8_t {

    String,//literal, string is not a type
    Number,//literal
    Type,//shall I do tokens for each type? I mean, the Token still store a string_view
    Identifier,
    Modifier,

    Assignment,
    Equal,
    NotEqual,
    Plus,
    Minus,
    Multiply,
    Divide,

    //infer type
    Const,
    Let,
    Auto,
    Mutable,

    GreaterThan,
    LessThan,
    GreaterThanOrEqual,
    LessThanOrEqual,

    And,
    Or,

    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,

    Comma,
    Colon,
    Semicolon,

    Unknown, // always nice to have a fallback
    Return,
    End, If, Else, While, For, In, Arrow, PlusEqual, MinusEqual, MultiplyEqual, DivideEqual, Dot, Dots, Switch, Struct, Enum, Union, Family, Break, Continue
};

// --- TokenType Structure ---
struct Token {
    const std::string_view value{};
    const Info info{};
    const TokenType token = TokenType::Unknown;

    explicit Token(const TokenType token, const std::string_view& value, const Info info)
        : value(value), info(info), token(token) {}
};

// --- Virtual Machine / IR Instructions ---
enum class Instruction : uint8_t {
    Add,
    Sub,
    Mul,
    Div,
    PointTo,
    AccessIn,
    Emplace,
    Store,
    // Future: Load, Jump, Call, Return, etc.
};

// --- Type Definitions ---
enum class Type : uint8_t {
    u8, u16, u32, u64, u128, u256,
    i8, i16, i32, i64, i128, i256,
    f8, f16, f32, f64, f128, f256,
    Boolean,
    Character,
    Byte,
    Null,
    Pointer
};

// --- Forward Declarations ---
class Symbol;

#endif // NOVA_TOKENS_H
