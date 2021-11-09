#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef void (*parse_func)(bool canAssign);

typedef struct {
    parse_func prefix;
    parse_func infix;
    Precedence precedence;
} parse_rule_t;

typedef struct {
    token_t name;
    int depth;
} local_t;

typedef struct {
    local_t locals[UINT8_COUNT];
    int local_count;
    int scope_depth;    
} compiler_t;

_parser_t    _parser;
compiler_t* _current = NULL;
chunk_t*     _compiling_chunk;

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

static bool _check(TokenType type) {
    return _parser.current.type == type;
}

static bool _match(TokenType type) {
    if ( !_check(type) )
        return false;

    _advance();
    return true;
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

static uint8_t _make_constant(value_t value) {
    int constant = l_add_constant(_current_chunk(), value);
    if (constant > UINT8_MAX) {
        _error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void _emit_constant(value_t value) {
    _emit_bytes(OP_CONSTANT, _make_constant(value));
}

static void l_init_compiler(compiler_t* compiler) {
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    _current = compiler;
}

static void _end_compiler() {
    _emit_return();
#ifdef DEBUG_PRINT_CODE
    if (!_parser.had_error) {
        l_dissassemble_chunk(_current_chunk(), "code");
    }
#endif
}

static void _begin_scope() {
    _current->scope_depth++;
}

static void _end_scope() {
    _current->scope_depth--;

    while (_current->local_count > 0 &&
           _current->locals[_current->local_count - 1].depth > _current->scope_depth) {
        _emit_byte(OP_POP);
        _current->local_count--;
    }
}

static void          _expression();
static void          _statement();
static void          _declaration();
static parse_rule_t* _get_rule(TokenType type);
static void          _parse_precedence(Precedence precedence);

static uint8_t _identifier_constant(token_t* name) {
    return _make_constant(OBJ_VAL(l_copy_string(name->start,
                                                name->length)));
}

static bool _identifiers_equal(token_t* a, token_t* b) {
    if (a->length != b->length) 
        return false;
        
    return memcmp(a->start, b->start, a->length) == 0;
}

static int _resolve_local(compiler_t* compiler, token_t* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        local_t* local = &compiler->locals[i];
        if (_identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                _error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static void _add_local(token_t name) {
    if ( _current->local_count > UINT8_COUNT ) {
        _error("Too many local variables in function");
        return;
    }
    local_t* local = &_current->locals[_current->local_count++];
    local->name = name;
    local->depth = -1;
}

static void _declare_variable() {
    if (_current->scope_depth == 0) 
        return;

    token_t* name = &_parser.previous;
    for (int i = _current->local_count - 1; i >= 0; i--) {
        local_t* local = &_current->locals[i];
        if (local->depth != -1 && local->depth < _current->scope_depth) {
            break; 
        }

        if (_identifiers_equal(name, &local->name)) {
            _error("Already a variable with this name in this scope.");
        }
    }
    _add_local(*name);
}

static uint8_t _parse_variable(const char* errorMessage) {
    _consume(TOKEN_IDENTIFIER, errorMessage);

    _declare_variable();
    if ( _current->scope_depth > 0 )
        return 0;

    return _identifier_constant(&_parser.previous);
}

static void _mark_initialized() {
     _current->locals[_current->local_count - 1].depth = _current->scope_depth;
}

static void _define_variable(uint8_t global) {
    if ( _current->scope_depth > 0 ) {
        _mark_initialized();
        return;
    }
    _emit_bytes(OP_DEFINE_GLOBAL, global);
}

static void _binary(bool canAssign) {
    TokenType operatorType = _parser.previous.type;
    parse_rule_t* rule = _get_rule(operatorType);
    _parse_precedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:    _emit_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   _emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER:       _emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: _emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          _emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    _emit_bytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          _emit_byte(OP_ADD); break;
        case TOKEN_MINUS:         _emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:          _emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         _emit_byte(OP_DIVIDE); break;
        default: 
            return; // Unreachable.
    }
}

static void _literal(bool canAssign) {
    switch (_parser.previous.type) {
        case TOKEN_FALSE: _emit_byte(OP_FALSE); break;
        case TOKEN_NIL:   _emit_byte(OP_NIL); break;
        case TOKEN_TRUE:  _emit_byte(OP_TRUE); break;
        default: 
            return; // Unreachable.
    }
}

static void _grouping(bool canAssign) {
    _expression();
    _consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void _number(bool canAssign) {
    double number = strtod(_parser.previous.start, NULL);
    _emit_constant(NUMBER_VAL(number));
}

static void _string(bool canAssign) {
    _emit_constant(
        OBJ_VAL(
            l_copy_string(_parser.previous.start  + 1,
                          _parser.previous.length - 2)
        )
    );
}

static void _named_variable(token_t name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = _resolve_local(_current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = _identifier_constant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && _match(TOKEN_EQUAL)) {
        _expression();
        _emit_bytes(OP_SET_GLOBAL, (uint8_t)arg);
    } else {
        _emit_bytes(OP_GET_GLOBAL, (uint8_t)arg);
    }
}

static void _variable(bool canAssign) {
    _named_variable(_parser.previous, canAssign);
}

static void _unary(bool canAssign) {
    TokenType operatorType = _parser.previous.type;

    // Compile the operand.
    _parse_precedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_BANG:  _emit_byte(OP_NOT); break;
        case TOKEN_MINUS: _emit_byte(OP_NEGATE); break;
        default: 
            return; // Unreachable.
    }
}

parse_rule_t rules[] = {
    [TOKEN_LEFT_PAREN]    = {_grouping, NULL,    PREC_NONE},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,     PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,     PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,     PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,     PREC_NONE},
    [TOKEN_MINUS]         = {_unary,   _binary,  PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     _binary,  PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,     PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     _binary,  PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     _binary,  PREC_FACTOR},
    [TOKEN_BANG]          = {_unary,   NULL,     PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     _binary,  PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     _binary,  PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     _binary,  PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     _binary,  PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     _binary,  PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     _binary,  PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {_variable,NULL,     PREC_NONE},
    [TOKEN_STRING]        = {_string,  NULL,     PREC_NONE},
    [TOKEN_NUMBER]        = {_number,  NULL,     PREC_NONE},
    [TOKEN_AND]           = {NULL,     NULL,     PREC_NONE},
    [TOKEN_CLASS]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,     PREC_NONE},
    [TOKEN_FALSE]         = {_literal, NULL,     PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,     PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,     PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,     PREC_NONE},
    [TOKEN_NIL]           = {_literal, NULL,     PREC_NONE},
    [TOKEN_OR]            = {NULL,     NULL,     PREC_NONE},
    [TOKEN_PRINT]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,     PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,     PREC_NONE},
    [TOKEN_TRUE]          = {_literal, NULL,     PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,     PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,     PREC_NONE},
};

static void _parse_precedence(Precedence precedence) {
    _advance();

    parse_func prefixRule = _get_rule(_parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        _error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= _get_rule(_parser.current.type)->precedence) {
        _advance();
        parse_func infixRule = _get_rule(_parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && _match(TOKEN_EQUAL)) {
        _error("Invalid assignment target.");
    }
}

static parse_rule_t* _get_rule(TokenType type) {
  return &rules[type];
}

static void _expression() {
    _parse_precedence(PREC_ASSIGNMENT);
}

static void _block() {
    while (!_check(TOKEN_RIGHT_BRACE) && !_check(TOKEN_EOF)) {
        _declaration();
    }

    _consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void _var_declaration() {
    uint8_t global = _parse_variable("Expect variable name.");

    if (_match(TOKEN_EQUAL)) {
        _expression();
    } else {
        _emit_byte(OP_NIL);
    }
    _consume(TOKEN_SEMICOLON,
            "Expect ';' after variable declaration.");

    _define_variable(global);
}

static void _expression_statement() {
    _expression();
    _consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    _emit_byte(OP_POP);
}

static void _print_statement() {
    _expression();
    _consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    _emit_byte(OP_PRINT);
}

static void _synchronize() {
    _parser.panic_mode = false;

    while (_parser.current.type != TOKEN_EOF) {
        if (_parser.previous.type == TOKEN_SEMICOLON) 
            return;
        
        switch (_parser.current.type) {
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

        _advance();
    }
}

static void _declaration() {

    if ( _match(TOKEN_VAR) ) {
        _var_declaration();
    } else {
        _statement();
    }


    if ( _parser.panic_mode )
        _synchronize();
}

static void _statement() {
    if ( _match(TOKEN_PRINT) ) {
        _print_statement();
    } else if ( _match(TOKEN_LEFT_BRACE) ) {
        _begin_scope();
        _block();
        _end_scope();
    } else {
        _expression_statement();
    }
}



bool l_compile(const char* source, chunk_t *chunk) {
    l_init_scanner(source);
    compiler_t compiler;
    l_init_compiler(&compiler);
    _compiling_chunk = chunk;

    _parser.had_error = false;
    _parser.panic_mode = false;

    _advance();

    while (!_match(TOKEN_EOF)) {
        _declaration();
    }

    // _expression();
    // _consume(TOKEN_EOF, "Expect end of expression.");
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
