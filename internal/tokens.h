//
// Created by dioguabo-rei-delas on 11/5/25.
//

#ifndef NOVA_TOKENS_H
#define NOVA_TOKENS_H

#include <cstdint>
typedef unsigned short u_short;

// --- Info ---
struct Info {
    size_t index{0};   // 0-based column/offset
    size_t line{1};    // human-friendly line number (start at 1)

    // Prefix increment: ++info -> increments and returns reference
    Info& operator++() noexcept {
        ++index;
        return *this;
    }

    // Postfix increment: info++ -> returns copy before increment
    Info operator++(int) noexcept {
        const Info tmp = *this;
        ++index;
        return tmp;
    }

    // Add offset
    Info& operator+=(const size_t offset) noexcept {
        index += offset;
        return *this;
    }

    [[nodiscard]] size_t get() const noexcept { return index; }

    explicit operator size_t() const noexcept { return index; }

    void newLine() noexcept {
        ++line;
        index = 0; // reset to 0 for the new line
    }

    void newLine(size_t n) noexcept {
        line += n;
        index = 0;
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
    End, If, Else, While, For, In, Arrow, PlusEqual, MinusEqual, MultiplyEqual, DivideEqual, Dot, Dots, Switch, Struct,
    Enum, Union, Family, Break, Continue, Mod, Entity, Float
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
