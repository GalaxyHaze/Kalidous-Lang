//
// Created by al24254 on 07/11/2025.
//

#ifndef NOVA_CORE_H
#define NOVA_CORE_H

#include "memory/file.h"
#include "parse/tokens.h"
#include "parse/tokenizer.h"
#include "parse/parser.h"
#include "utils/debug.h"


static const auto version = nova::file::readFile("config.txt");

#endif //NOVA_CORE_H