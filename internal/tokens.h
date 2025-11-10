//
// Created by dioguabo-rei-delas on 11/5/25.
//

#ifndef NOVA_TOKENS_H
#define NOVA_TOKENS_H

#include <cstdint>

// --- Info ---
struct Info {
    u_short index{1};
    u_short line{1};

    //size_t operator++(int) { return index++; }
    Info& operator++(int) { index++; return *this; }
    [[nodiscard]] u_short get() const {
        return index;
    }
    Info& operator+=(const size_t offset) { index += offset; return *this; }
    explicit operator size_t() const { return index; }

    void newLine() {
        ++line;
        index = 1;
    }
};

// --- Token Types ---
enum class Token : uint8_t {

    String,
    Number,
    Type,
    Identifier,
    Modifier,

    Assignment,
    Equal,
    NotEqual,
    Plus,
    Minus,
    Multiply,
    Divide,

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

    Unknown // always nice to have a fallback
};

// --- Token Structure ---
struct TokenType {
    const std::string_view value{};
    const Info info{};
    const Token token = Token::Unknown;

    explicit TokenType(const Token token, const std::string_view& value, const Info info)
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
