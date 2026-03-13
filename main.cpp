#include <iostream>
#include <vector>
#include <Kalidous/kalidous.h>

/*enum OP: int64_t {
    LITERAL = 0,
    ADD = 1,
    PRINT = 2,
    END = 3
};

using u64 = int64_t;

void interpret(std::vector<int64_t> &ops) {
    static const void *const instructions[] = {
        &&literal,
        &&add,
        &&print,
        &&end
    };

    uint32_t instruction_index = 0;
    uint8_t argument_index = 0;
    char usefulBuffer[64];

    void *store[8] = {nullptr};
    #define next() if (ops.size() > instruction_index) \
                        goto *instructions[ops[instruction_index++]]; \
                   else goto error;

    next();

    literal:
        store[argument_index++] = &ops[++instruction_index];
        ++instruction_index;
    next();

    add:
        *reinterpret_cast<int64_t *>(usefulBuffer) =
            *static_cast<int64_t *>(store[0]) + *static_cast<int64_t *>(store[1]);
    store[0] = reinterpret_cast<int64_t *>(usefulBuffer);
    argument_index = 0;
    next();

    print:
        std::cout << *static_cast<int64_t *>(store[0]) << '\n';
    next();

    end:
        return;
    error:
        std::cerr << "Vector ended without END instruction at index " << instruction_index << "\n";
}*/

int main(const int argc, const char **argv) {
    return kalidous_run(argc, argv);
}
