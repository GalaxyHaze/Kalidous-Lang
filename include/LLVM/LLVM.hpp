#pragma once

#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Host.h>
#include <llvm/Config/llvm-config.h>

#if LLVM_VERSION_MAJOR < 21
#error "Zith requires LLVM 21 or higher"
#endif

#include "ir.hpp"
#include "optimizer.hpp"
#include "codegen.hpp"

#if defined(LLVM_SUPPORTED)
#undef LLVM_SUPPORTED
#endif