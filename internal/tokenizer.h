//
// Created by al24254 on 07/11/2025.
//

#ifndef NOVA_TOKENIZER_H
#define NOVA_TOKENIZER_H
#include <string>
#include <string_view>

typedef unsigned long count_t;

struct Info {
    count_t index;
    count_t line;
    count_t column;

    count_t operator++(int) {
        ++index;
        ++column;
        return index;
    }


};

class Tokenizer {

    static bool has(Info& info, const std::string_view src) {
        return info++ < src.size();
    }

    static char peak(const Info& info, const std::string_view src, count_t offset = 1) {
        if (info.index + offset - 1 >= static_cast<count_t>(src.size())) {
            return '\0';
        }

        return src[info.index + offset - 1];
    }

    static void consume(Info& info, const count_t offset = 1) {
        info.index += offset;
        info.column += offset;
    }

public:

    static auto tokenize(std::stringstream& src) {
        std::string line;
        while (std::getline(src, line)) {

        }
    }

};

#endif //NOVA_TOKENIZER_H