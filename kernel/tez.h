#ifndef TEZ_H
#define TEZ_H

#include <stdint.h>

/* ── limits ── */
#define TEZ_MAX_TOKENS   1024
#define TEZ_MAX_NODES    512
#define TEZ_MAX_VARS     64
#define TEZ_MAX_FUNCS    32
#define TEZ_MAX_PARAMS   8
#define TEZ_MAX_CODE     8192
#define TEZ_MAX_STR      256

/* ── token types ── */
typedef enum {
    TK_INT_LIT, TK_FLOAT_LIT, TK_STR_LIT, TK_CHAR_LIT, TK_IDENT,
    /* types */
    TK_INT, TK_FLOAT, TK_CH, TK_STR, TK_TF, TK_NR,
    /* keywords */
    TK_IF, TK_ELSE, TK_WHILE, TK_FOR, TK_FUNC, TK_RTN, TK_TRUE, TK_FALSE,
    TK_CONS_OUT,
    /* operators */
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV, TK_MOD,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_AND, TK_OR, TK_NOT,
    TK_ASSIGN,
    /* delimiters */
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_SEMICOLON, TK_COMMA, TK_DOT,
    TK_EOF, TK_ERR
} TokType;

typedef struct {
    TokType type;
    int     int_val;
    int     str_idx;  /* index into string table */
    int     line;
} Token;

/* ── value types ── */
typedef enum { VT_INT, VT_FLOAT, VT_CH, VT_STR, VT_TF, VT_NR } ValType;

/* ── AST node types ── */
typedef enum {
    ND_INT, ND_FLOAT, ND_STR, ND_CHAR, ND_BOOL, ND_IDENT,
    ND_BINOP, ND_UNOP,
    ND_ASSIGN, ND_VAR_DECL,
    ND_IF, ND_WHILE, ND_FOR,
    ND_BLOCK, ND_FUNC_DECL, ND_CALL, ND_RTN,
    ND_CONS_OUT
} NodeType;

typedef struct Node Node;
struct Node {
    NodeType type;
    ValType  vtype;
    int      ival;       /* int literal / bool */
    int      str_idx;    /* string table index */
    TokType  op;         /* binary/unary op */
    /* children indices into node pool */
    int      left, right, cond, body, els, next;
    /* for func decl / call */
    int      params[TEZ_MAX_PARAMS];
    int      param_count;
    int      func_idx;
};

/* ── shared pools (allocated in kernel BSS) ── */
extern Token  tez_tokens[TEZ_MAX_TOKENS];
extern int    tez_token_count;
extern Node   tez_nodes[TEZ_MAX_NODES];
extern int    tez_node_count;
extern char   tez_strtab[TEZ_MAX_STR * 32];  /* string literals */
extern int    tez_strtab_count;

/* ── API ── */
int  tez_lex(const char *src);
int  tez_parse(void);          /* returns root node index */
int  tez_codegen(int root, uint8_t *out, int max_size);
void tez_run(const char *src); /* lex + parse + codegen + exec */
void tez_repl(void);

#endif