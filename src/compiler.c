#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

#include "common.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "lib/debug.h"
#endif

typedef struct {
    token_t current;
    token_t previous;
    bool    had_error;
    bool    panic_mode;
} _parser_t;

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

typedef void (*parse_func)();

typedef struct {
    parse_func prefix;
    parse_func infix;
    Precedence precedence;
} parse_rule_t;

_parser_t _parser;
chunk_t*  _compiling_chunk;

static chunk_t* _current_chunk() {
    return _compiling_chunk;
}

static void _error_at(token_t* token, const char* message) {
    if (_parser.panic_mode)
        return;
    
    _parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } 
    else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } 
    else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    _parser.had_error = true;
}

static void _error(const char* message) {
    _error_at(&_parser.previous, message);
}

static void _error_at_current(const char* message) {
    _error_at(&_parser.current, message);
}

static void _advance() {
    _parser.previous = _parser.current;

    for (;;) {
        _parser.current = l_scan_token();
        if (_parser.current.type != TOKEN_ERROR) 
            break;

        _error_at_current(_parser.current.start);
    }
}

static void _consume(TokenType type, const char* message) {
    if (_parser.current.type == type) {
        _advance();
        return;
    }

    _error_at_current(message);
}

static void _emit_byte(uint8_t byte) {
    l_write_chunk(_current_chunk(), byte, _parser.previous.line);
}

static void _emit_bytes(uint8_t byte1, uint8_t byte2) {
    _emit_byte(byte1);
    _emit_byte(byte2);
}

static void _emit_return() {
    _emit_byte(OP_RETURN);
}

static uint8_t _make_constant(Value value) {
    int constant = l_add_constant(_current_chunk(), value);
    if (constant > UINT8_MAX) {
        _error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void _emit_constant(Value value) {
    _emit_bytes(OP_CONSTANT, _make_constant(value));
}

static void _end_compiler() {
    _emit_return();
#ifdef DEBUG_PRINT_CODE
    if (!_parser.had_error) {
        l_dissassemble_chunk(_current_chunk(), "code");
    }
#endif
}

static void          _expression();
static parse_rule_t* _get_rule(TokenType type);
static void          _parse_precedence(Precedence precedence);

static void _binary() {
    TokenType operatorType = _parser.previous.type;
    parse_rule_t* rule = _get_rule(operatorType);
    _parse_precedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_PLUS:  
            _emit_byte(OP_ADD); 
            break;
        case TOKEN_MINUS: 
            _emit_byte(OP_SUBTRACT); 
            break;
        case TOKEN_STAR:  
            _emit_byte(OP_MULTIPLY); 
            break;
        case TOKEN_SLASH: 
            _emit_byte(OP_DIVIDE); 
            break;
        default: 
            return; // Unreachable.
    }
}

static void _grouping() {
    _expression();
    _consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void _number() {
    double value = strtod(_parser.previous.start, NULL);
    _emit_constant(value);
}

static void _unary() {
    TokenType operatorType = _parser.previous.type;

    // Compile the operand.
    _parse_precedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_MINUS: 
            _emit_byte(OP_NEGATE);
            break;
        default: 
            return; // Unreachable.
    }
}

parse_rule_t rules[] = {
    [TOKEN_LEFT_PAREN]    = {_grouping, NULL,   PREC_NONE},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {_unary,   _binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     _binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     _binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     _binary, PREC_FACTOR},
    [TOKEN_BANG]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_GREATER]       = {NULL,     NULL,   PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LESS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {_number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void _parse_precedence(Precedence precedence) {
    _advance();

    parse_func prefixRule = _get_rule(_parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        _error("Expect expression.");
        return;
    }

    prefixRule();

    while (precedence <= _get_rule(_parser.current.type)->precedence) {
        _advance();
        parse_func infixRule = _get_rule(_parser.previous.type)->infix;
        infixRule();
    }
}

static parse_rule_t* _get_rule(TokenType type) {
  return &rules[type];
}

static void _expression() {
    _parse_precedence(PREC_ASSIGNMENT);
}



bool l_compile(const char* source, chunk_t *chunk) {
    l_init_scanner(source);
    _compiling_chunk = chunk;

    _parser.had_error = false;
    _parser.panic_mode = false;

    _advance();
    _expression();
    _consume(TOKEN_EOF, "Expect end of expression.");
    _end_compiler();

    return !_parser.had_error;

    // int line = -1;
    // for (;;) {
    //     token_t token = l_scan_token();
    //     if (token.line != line) {
    //         printf("%4d ", token.line);
    //         line = token.line;
    //     } else {
    //         printf("   | ");
    //     }
    //     printf("%2d '%.*s'\n", token.type, token.length, token.start); 

    //     if (token.type == TOKEN_EOF) break;
    // }
}
