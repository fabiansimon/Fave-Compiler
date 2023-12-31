//
// Created by Fabian Simon on 29.09.23.
//

#include <stdio.h>
#include <stdlib.h>
#include "string.h"

#include "compiler.h"
#include "common.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

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

typedef void (*ParseFn)(bool can_assign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
} Local;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
} Compiler;

Parser parser;
Compiler* current = NULL;
Chunk* compiling_chunk;

static Chunk* get_current_chunk() {
    return &current->function->chunk;
}

static void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
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

static bool check(TokenType type) {
    return parser.curr.type == type;
}

static bool is_match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emit_byte(uint8_t byte) {
    write_chunk(get_current_chunk(), byte, parser.prev.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    write_chunk(get_current_chunk(), byte1, parser.prev.line);
    write_chunk(get_current_chunk(), byte2, parser.prev.line);
}

static void emit_loop(int loop_start) {
    emit_byte(OP_LOOP);

    int offset = get_current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) error("Loop body too large");

    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static int emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);
    return get_current_chunk()->count-2;
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

static void patch_jump(int offset) {
    int jump = get_current_chunk()->count-offset-2; // -2 to adjust for the bytecode for the jump offset itself

    if (jump > UINT16_MAX) {
        error("Too much code to jump over");
    }

    get_current_chunk()->code[offset] = (jump >> 8) & 0xff;
    get_current_chunk()->code[offset+1] = jump & 0xff;
}

static void init_compiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = new_function();
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copy_string(parser.prev.start, parser.prev.length);
    }

    Local* local = &current->locals[current->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static ObjFunction* end_compiler() {
    emit_return();
    ObjFunction* function = current->function;

    #ifdef DEBUG_PRINT_CODE
    if (!parser.had_error)
        disassemble_chunk(get_current_chunk(),
                          function->name != NULL ? function->name->chars : "<script>");
    #endif

    current = current->enclosing;
    return function;
}

static void begin_scope() {
    current->scope_depth++;
}

static void end_scope() {
    current->scope_depth--;

    while (current->local_count > 0
            && current->locals[current->local_count-1].depth > current->scope_depth) {
        emit_byte(OP_POP);
        current->local_count--;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);

static uint8_t identifier_constant(Token* name) {
    return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

static bool identifiers_equal(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(Compiler* compiler, Token* name) {
    for (int i = compiler->local_count-1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) error("Can't read local variable in its own initializer.");
            return i;
        }
    }

    return -1;
}

static void add_local(Token name) {
    if (current->local_count == UINT8_COUNT) {
        error("Too many local variables in function");
        return;
    }

    Local* local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
}

static void declare_variable() {
    if (current->scope_depth == 0) return;

    Token* name = &parser.prev;

    for (int i = current->local_count-1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    add_local(*name);
}

static uint8_t parse_variable(const char* err_message) {
    consume(TOKEN_IDENTIFIER, err_message);

    declare_variable();
    if (current->scope_depth > 0) return 0;

    return identifier_constant(&parser.prev);
}

static void mark_as_initialized() {
    if (current->scope_depth == 0) return;
    current->locals[current->local_count-1].depth = current->scope_depth;
}

static void define_variable(uint8_t global) {
    if (current->scope_depth > 0) {
        mark_as_initialized();
        return;
    }
    emit_bytes(OP_DEFINE_GLOBAL, global);
}

static void and_(bool can_assign) {
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);

    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}

static void binary(bool can_assign) {
    TokenType operator_type = parser.prev.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precedence((Precedence) (rule->precedence+1));

    switch (operator_type) {
        case TOKEN_BANG_EQUAL:      emit_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:     emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER:         emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL:   emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:            emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:      emit_bytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:            emit_byte(OP_ADD); break;
        case TOKEN_MINUS:           emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:            emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH:           emit_byte(OP_DIVIDE); break;
        default: return; // unreachable
    }
}

static void literal(bool can_assign) {
    switch (parser.prev.type) {
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_NIL: emit_byte(OP_NIL); break;
        case TOKEN_TRUE: emit_byte(OP_TRUE); break;
        default: return; // unreachable
    }
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
    Compiler compiler;
    init_compiler(&compiler, type);
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                error_at_curr("Can't have more than 255 parameters.");
            }

            uint8_t constant = parse_variable("Expect parameter name");
            define_variable(constant);
        } while (is_match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* func = end_compiler();
    emit_bytes(OP_CONSTANT, make_constant(OBJ_VAL(func)));
}

static void function_declaration() {
    uint8_t global = parse_variable("Expect function name");
    mark_as_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
}

static void variable_declaration() {
    uint8_t global = parse_variable("Expect variable name");

    if (is_match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");
    define_variable(global);
}

static void expression_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression");
    emit_byte(OP_POP);
}

static void if_statement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after if.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after if.");

    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();

    int else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);
    emit_byte(OP_POP);

    if (is_match(TOKEN_ELSE )) statement();
    patch_jump(else_jump);
}

static void print_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void while_statement() {
    int loop_start = get_current_chunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after while.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

static void for_statement() {
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (is_match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (is_match(TOKEN_VAR)) {
        variable_declaration();
    } else {
        expression_statement();
    }

    int loop_start = get_current_chunk()->count;
    int exit_jump = -1;
    if (!is_match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition");

        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }

    if (!is_match(TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(OP_JUMP);
        int increment_start = get_current_chunk()->count;

        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }

    statement();
    emit_loop(loop_start);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }

    end_scope();
}

static void synchronize() {
    parser.panic_mode = false;
    
    while (parser.curr.type != TOKEN_EOF) {
        if (parser.prev.type == TOKEN_SEMICOLON) return;

        switch (parser.curr.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                ; // Do nothing.
        }

        advance();
    }
}

static void declaration() {
    if (is_match(TOKEN_FUN)) {
        function_declaration();
    } else if (is_match(TOKEN_VAR)) {
        variable_declaration();
    } else {
        statement();
    }
;
    if (parser.panic_mode) synchronize();
}

static void statement() {
    if (is_match(TOKEN_PRINT)) {
        print_statement();
    } else if (is_match(TOKEN_FOR)) {
        for_statement();
    } else if (is_match(TOKEN_WHILE)) {
        while_statement();
    } else if (is_match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else if (is_match(TOKEN_IF)) {
        if_statement();
    } else {
        expression_statement();
    }
}

static void grouping(bool can_assign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void number(bool can_assign) {
    double val = strtod(parser.prev.start, NULL);
    emit_constant(NUMBER_VAL(val));
}

static void or_(bool can_assign) {
    int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    int end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);

    parse_precedence(PREC_OR);
    patch_jump(end_jump);
}

static void string(bool can_assign) {
    emit_constant(OBJ_VAL(copy_string(parser.prev.start + 1, parser.prev.length - 2)));
}

static void named_variable(Token name, bool can_assign) {
    uint8_t get_op, set_op;
    int arg = resolve_local(current, &name);

    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else {
        arg = identifier_constant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && is_match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(set_op, (uint8_t) arg);
    } else {
        emit_bytes(get_op, (uint8_t) arg);
    }
}

static void variable(bool can_assign) {
    named_variable(parser.prev, can_assign);
}

static void unary(bool can_assign) {
    TokenType operator_type = parser.prev.type;

    // Compile the operand
    parse_precedence(PREC_UNARY);

    // Emit the operator instruction
    switch (operator_type) {
        case TOKEN_BANG: emit_byte(OP_NOT); break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return; // unreachable
    }
}

ParseRule rules[] = {
        [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
        [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
        [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
        [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
        [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
        [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
        [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
        [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
        [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
        [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
        [TOKEN_BANG]          = {unary,     NULL,   PREC_NONE},
        [TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
        [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},
        [TOKEN_GREATER]       = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_LESS]          = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_IDENTIFIER]    = {variable,     NULL,   PREC_NONE},
        [TOKEN_STRING]        = {string,     NULL,   PREC_NONE},
        [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
        [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
        [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
        [TOKEN_FALSE]         = {literal,     NULL,   PREC_NONE},
        [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
        [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
        [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
        [TOKEN_NIL]           = {literal,     NULL,   PREC_NONE},
        [TOKEN_OR]            = {NULL,     or_,   PREC_OR},
        [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
        [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
        [TOKEN_TRUE]          = {literal,     NULL,   PREC_NONE},
        [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
        [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
        [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = get_rule(parser.prev.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expected expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= get_rule(parser.curr.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(parser.prev.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && is_match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static ParseRule* get_rule(TokenType type) {
    return &rules[type];
}

ObjFunction* compile(const char* src) {
    init_scanner(src);

    Compiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    parser.had_error = false;
    parser.panic_mode = false;

    advance();

    while (!is_match(TOKEN_EOF)) {
        declaration();
    }

    end_compiler();

    ObjFunction* function = end_compiler();
    return parser.had_error ? NULL : function;
}

