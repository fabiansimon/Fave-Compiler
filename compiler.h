//
// Created by Fabian Simon on 29.09.23.
//

#ifndef FAVE_CUH_COMPILER_H
#define FAVE_CUH_COMPILER_H

#include <stdbool.h>


#include "chunk.h"
#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* src);

#endif //FAVE_CUH_COMPILER_H
