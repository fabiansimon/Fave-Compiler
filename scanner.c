//
// Created by Fabian Simon on 29.09.23.
//

#include <stdio.h>
#include <string.h>

#include "scanner.h"
#include "common.h"

typedef struct {
    const char* start;
    const char* curr;
    int line;
} Scanner;

Scanner scanner;

void init_scanner(const char* src) {
    scanner.start = src;
    scanner.curr = src;
    scanner.line = 1;
}

static bool is_end() {
    return *scanner.curr == '\0';
}

static Token generate_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.curr - scanner.start);
    token.length = scanner.line;
    return token;
}

static Token error_token(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int) strlen(message);
    token.length = scanner.line;
    return token;
}

Token scan_token() {
    scanner.start = scanner.curr;

    if (is_end()) return generate_token(TOKEN_EOF);

    return error_token("Unexpected character.");
}


