//
// Created by Fabian Simon on 29.09.23.
//

#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "common.h"
#include "scanner.h"

typedef struct {
    Token curr;
    Token prev;
    bool had_error;
    bool panic_mode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

Parser parser;
Chunk* compiling_chunk;

static Chunk* get_current_chunk() {
    return compiling_chunk;
}

static void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Do nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(const char* message) {
    error_at(&parser.prev, message);
}

static void error_at_curr(const char* message) {
    error_at(&parser.curr, message);
}

static void advance() {
    parser.prev = parser.curr;

    for (;;) {
        parser.curr = scan_token();
        if (parser.curr.type != TOKEN_ERROR) break;

        error_at_curr(parser.curr.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.curr.type == type) {
        advance();
        return;
    }
    error_at_curr(message);
}

static void emit_byte(uint8_t byte) {
    write_chunk(get_current_chunk(), byte, parser.prev.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    write_chunk(get_current_chunk(), byte1, parser.prev.line);
    write_chunk(get_current_chunk(), byte2, parser.prev.line);
}

static void emit_return() {
    emit_byte(OP_RETURN);
}

static uint8_t make_constant(Value val) {
    int constant = add_constant(get_current_chunk(), val);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk");
        return 0;
    }

    return (uint8_t) constant;
}

static void emit_constant(Value val) {
    emit_bytes(OP_CONSTANT, make_constant(val));
}

static void end_compiler() {
    emit_return();
}

static void parse_precedence(Precedence precedence) {

}


static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void number() {
    double val = strtod(parser.prev.start, NULL);
}

static void unary() {
    TokenType operator_type = parser.prev.type;

    // Compile the operand
    parse_precedence(PREC_UNARY);

    // Emit the operator instruction
    switch (operator_type) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return; // unreachable
    }
}

bool compile(const char* src, Chunk* chunk) {
    init_scanner(src);

    compiling_chunk = chunk;

    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expected end of expression");

    end_compiler();

    return !parser.had_error;
    /*
    int line = -1;
    for (;;) {
        Token token = scan_token();
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF) break;
    }
    */
}

