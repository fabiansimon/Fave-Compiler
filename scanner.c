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

static char advance() {
    scanner.curr++;
    return scanner.curr[-1];
}

static bool is_match(char expected) {
    if (is_end()) return false;
    if (*scanner.curr != expected) return false;
    scanner.curr++;
    return true;
}

static char peek() {
    return *scanner.curr;
}

static char peek_next() {
    if (is_end()) return '\0';
    return scanner.curr[1];
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

static void skip_wspace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_end()) advance();
                } else {
                    return;
                }
            default:
                return;
        }
    }
}

static TokenType check_keyword(int start, int length, const char* rest, TokenType type) {
    if ((scanner.curr - scanner.start == start + length) &&
            (memcmp(scanner.start + start, rest, length) == 0)) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType get_identifier_type() {
    switch (scanner.start[0]) {
        case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.curr - scanner.start > 1)
            {
                switch (scanner.start[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(2, 1, "n", TOKEN_FOR);
                }
            }
        case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.curr - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
        case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
    }
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static Token handle_string() {
    while (peek() != '"' && !is_end()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (is_end()) return error_token("Unterminated string.");

    advance();
    return generate_token(TOKEN_STRING);
}

static Token handle_number() {
    while (is_digit(peek())) advance();

    if (peek()=='.' && is_digit(peek_next())) {
        advance();
        while (is_digit(peek())) advance();
    }

    return generate_token(TOKEN_NUMBER);
}

static Token handle_identifier() {
    while (is_alpha(peek()) || is_digit(peek())) advance();
    return generate_token(get_identifier_type());
}

Token scan_token() {
    skip_wspace();

    scanner.start = scanner.curr;

    if (is_end()) return generate_token(TOKEN_EOF);

    char c = advance();

    if (is_alpha(c)) return handle_identifier();

    if (is_digit(c)) return handle_number();

    switch (c) {
        case '(': return generate_token(TOKEN_LEFT_PAREN);
        case ')': return generate_token(TOKEN_RIGHT_PAREN);
        case '{': return generate_token(TOKEN_LEFT_BRACE);
        case '}': return generate_token(TOKEN_RIGHT_BRACE);
        case ';': return generate_token(TOKEN_SEMICOLON);
        case ',': return generate_token(TOKEN_COMMA);
        case '.': return generate_token(TOKEN_DOT);
        case '-': return generate_token(TOKEN_MINUS);
        case '+': return generate_token(TOKEN_PLUS);
        case '/': return generate_token(TOKEN_SLASH);
        case '*': return generate_token(TOKEN_STAR);
        case '!': return generate_token(is_match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return generate_token(is_match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return generate_token(is_match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return generate_token(is_match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return handle_string();

    }

    return error_token("Unexpected character.");
}


