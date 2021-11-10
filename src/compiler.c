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

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct compiler_t compiler_t;
typedef struct compiler_t {
    compiler_t* enclosing;
    obj_function_t* function;
    FunctionType    type;

    local_t locals[UINT8_COUNT];
    int local_count;
    int scope_depth;    
} compiler_t;

_parser_t   _parser;
compiler_t* _current = NULL;
chunk_t*    _compiling_chunk;

static chunk_t* _current_chunk() {
    return &_current->function->chunk;
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

static void _emit_loop(int loopStart) {
    _emit_byte(OP_LOOP);

    int offset = _current_chunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) 
        _error("Loop body too large.");

    _emit_byte((offset >> 8) & 0xff);
    _emit_byte(offset & 0xff);
}

static int _emit_jump(uint8_t instruction) {
    _emit_byte(instruction);
    _emit_byte(0xff);
    _emit_byte(0xff);
    return _current_chunk()->count - 2;
}

static void _emit_return() {
    _emit_byte(OP_NIL);
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

static void _patch_jump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = _current_chunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        _error("Too much code to jump over.");
    }

    _current_chunk()->code[offset] = (jump >> 8) & 0xff;
    _current_chunk()->code[offset + 1] = jump & 0xff;
}

static void l_init_compiler(compiler_t* compiler, FunctionType type) {

    // set the enclosing compiler if one exists
    compiler->enclosing = ( _current != NULL ) ? _current : NULL;

    compiler->function = NULL;
    compiler->type = type;

    compiler->local_count = 0;
    compiler->scope_depth = 0;

    compiler->function = l_new_function();

    _current = compiler;

    if (type != TYPE_SCRIPT) {
        _current->function->name = l_copy_string(_parser.previous.start,
                                                 _parser.previous.length);
    }

    local_t* local = &_current->locals[_current->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static obj_function_t* _end_compiler() {
    _emit_return();
    obj_function_t* function = _current->function;
#ifdef DEBUG_PRINT_CODE
    if (!_parser.had_error) {
        l_dissassemble_chunk(
            _current_chunk(), 
            function->name != NULL ? function->name->chars : "<script>"
        );
    }
#endif
    _current = _current->enclosing;
    return function;
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
    if (_current->scope_depth == 0)
        return;
     _current->locals[_current->local_count - 1].depth = _current->scope_depth;
}

static void _define_variable(uint8_t global) {
    if ( _current->scope_depth > 0 ) {
        _mark_initialized();
        return;
    }
    _emit_bytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t _argument_list() {
    uint8_t argCount = 0;
    if (!_check(TOKEN_RIGHT_PAREN)) {
        do {
            _expression();
            if (argCount == 255) {
                _error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (_match(TOKEN_COMMA));
    }
    _consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void _and_(bool canAssign) {
    int endJump = _emit_jump(OP_JUMP_IF_FALSE);

    _emit_byte(OP_POP);
    _parse_precedence(PREC_AND);

    _patch_jump(endJump);
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

static void _call(bool canAssign) {
    uint8_t argCount = _argument_list();
    _emit_bytes(OP_CALL, argCount);
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

static void _or_(bool canAssign) {
    int elseJump = _emit_jump(OP_JUMP_IF_FALSE);
    int endJump = _emit_jump(OP_JUMP);

    _patch_jump(elseJump);
    _emit_byte(OP_POP);

    _parse_precedence(PREC_OR);
    _patch_jump(endJump);
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
        _emit_bytes(setOp, (uint8_t)arg);
    } else {
        _emit_bytes(getOp, (uint8_t)arg);
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
    [TOKEN_LEFT_PAREN]    = {_grouping, _call,    PREC_CALL},
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
    [TOKEN_AND]           = {NULL,     _and_,    PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,     PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,     PREC_NONE},
    [TOKEN_FALSE]         = {_literal, NULL,     PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,     PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,     PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,     PREC_NONE},
    [TOKEN_NIL]           = {_literal, NULL,     PREC_NONE},
    [TOKEN_OR]            = {NULL,     _or_,     PREC_OR},
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

static void _function(FunctionType type) {
    compiler_t compiler;
    l_init_compiler(&compiler, type);
    _begin_scope();

    _consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");


    if ( !_check(TOKEN_RIGHT_PAREN) ) {
        do {
            _current->function->arity++;
            if (_current->function->arity > 255) {
                _error_at_current("Can't have more than 255 parameters.");
            }
            uint8_t constant = _parse_variable("Expect parameter name.");
            _define_variable(constant);
        } while (_match(TOKEN_COMMA));
    }

    _consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    _consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    _block();

    obj_function_t* function = _end_compiler();
    _emit_bytes(OP_CONSTANT, _make_constant(OBJ_VAL(function)));
}

static void _fun_declaration() {
    uint8_t global = _parse_variable("Expect function name.");
    _mark_initialized();
    _function(TYPE_FUNCTION);
    _define_variable(global);
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

static void _for_statement() {
    _begin_scope();

    _consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    // Process initializer + expression
    if (_match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (_match(TOKEN_VAR)) {
        _var_declaration();
    } else {
        _expression_statement();
    }
    
    int loopStart = _current_chunk()->count;
    
    // Process the for exit condition
    int exitJump = -1;
    if (!_match(TOKEN_SEMICOLON)) {
        _expression();
        _consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = _emit_jump(OP_JUMP_IF_FALSE);
        _emit_byte(OP_POP); // Condition.
    }

    // process the increment

    if (!_match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = _emit_jump(OP_JUMP);
        int incrementStart = _current_chunk()->count;
        _expression();
        _emit_byte(OP_POP);
        _consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        _emit_loop(loopStart);
        loopStart = incrementStart;
        _patch_jump(bodyJump);
    }
    
    _statement();
    _emit_loop(loopStart);

    if (exitJump != -1) {
        _patch_jump(exitJump);
        _emit_byte(OP_POP); // Condition.
    }

    _end_scope();
}

static void _if_statement() {
    // process the logical expression
    _consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    _expression();
    _consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition."); 

    // run the 'then' statement
    int thenJump = _emit_jump(OP_JUMP_IF_FALSE);
    _emit_byte(OP_POP);
    _statement();

    // finshed then - jump over the 'else' statement
    int elseJump = _emit_jump(OP_JUMP);

    // otherwise run the 'else' statement
    _patch_jump(thenJump);
    _emit_byte(OP_POP);

    if (_match(TOKEN_ELSE))
        _statement();
    
    // set the post if statement jump point
    _patch_jump(elseJump);
}

static void _print_statement() {
    _expression();
    _consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    _emit_byte(OP_PRINT);
}

static void _return_statement() {
    if (_current->type == TYPE_SCRIPT) {
        _error("Can't return from top-level code.");
    }

    if (_match(TOKEN_SEMICOLON)) {
        _emit_return();
    } else {
        _expression();
        _consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        _emit_byte(OP_RETURN);
    }
}

static void _while_statement() {
    int loopStart = _current_chunk()->count;
    _consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    _expression();
    _consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = _emit_jump(OP_JUMP_IF_FALSE);
    _emit_byte(OP_POP);
    _statement();
    _emit_loop(loopStart);

    _patch_jump(exitJump);
    _emit_byte(OP_POP);
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

    if ( _match(TOKEN_FUN) ) {
        _fun_declaration();
    } else if ( _match(TOKEN_VAR) ) {
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
    } else if ( _match(TOKEN_IF) ) {
        _if_statement();
    } else if ( _match(TOKEN_RETURN) ) {
        _return_statement();
    } else if ( _match(TOKEN_WHILE) ) {
        _while_statement();
    } else if ( _match(TOKEN_FOR) ) {
        _for_statement();
    } else if ( _match(TOKEN_LEFT_BRACE) ) {
        _begin_scope();
        _block();
        _end_scope();
    } else {
        _expression_statement();
    }
}



obj_function_t* l_compile(const char* source) {
    l_init_scanner(source);
    compiler_t compiler;

    l_init_compiler(&compiler, TYPE_SCRIPT);

    _parser.had_error = false;
    _parser.panic_mode = false;

    _advance();

    while (!_match(TOKEN_EOF)) {
        _declaration();
    }

    obj_function_t* function = _end_compiler();

    return _parser.had_error ? NULL : function;    
}
