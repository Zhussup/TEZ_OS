#include <stdint.h>
#include "tez.h"

Node tez_nodes[TEZ_MAX_NODES];
int  tez_node_count = 0;

static int  pos = 0;
static int  parse_err = 0;
static char parse_errmsg[64];

static void err(const char *msg) {
    if (parse_err) return;
    parse_err = 1;
    int i=0; while(msg[i]) { parse_errmsg[i]=msg[i]; i++; } parse_errmsg[i]=0;
}

static Token *cur(void)  { return &tez_tokens[pos]; }
static Token *peek(int n){ return &tez_tokens[pos+n]; }
static Token *advance(void) { return &tez_tokens[pos++]; }

static int expect(TokType t) {
    if (cur()->type != t) { err("unexpected token"); return 0; }
    pos++; return 1;
}

static int new_node(NodeType type) {
    if (tez_node_count >= TEZ_MAX_NODES) { err("too many nodes"); return 0; }
    int idx = tez_node_count++;
    Node *n = &tez_nodes[idx];
    n->type=type; n->left=-1; n->right=-1;
    n->cond=-1; n->body=-1; n->els=-1; n->next=-1;
    n->param_count=0; n->func_idx=-1; n->str_idx=-1;
    n->vtype=VT_INT; n->ival=0; n->op=TK_ERR;
    return idx;
}

/* forward declarations */
static int parse_expr(void);
static int parse_stmt(void);
static int parse_stmt_no_semi(void);
static int parse_block(void);

/* ── expression parsing ── */

static int parse_primary(void) {
    if (parse_err) return -1;
    Token *t = cur();

    if (t->type == TK_INT_LIT) {
        int n = new_node(ND_INT);
        tez_nodes[n].ival = t->int_val;
        tez_nodes[n].vtype = VT_INT;
        advance(); return n;
    }
    if (t->type == TK_STR_LIT) {
        int n = new_node(ND_STR);
        tez_nodes[n].str_idx = t->str_idx;
        tez_nodes[n].vtype = VT_STR;
        advance(); return n;
    }
    if (t->type == TK_CHAR_LIT) {
        int n = new_node(ND_CHAR);
        tez_nodes[n].ival = t->int_val;
        tez_nodes[n].vtype = VT_CH;
        advance(); return n;
    }
    if (t->type == TK_TRUE || t->type == TK_FALSE) {
        int n = new_node(ND_BOOL);
        tez_nodes[n].ival = (t->type == TK_TRUE) ? 1 : 0;
        tez_nodes[n].vtype = VT_TF;
        advance(); return n;
    }
    if (t->type == TK_IDENT) {
        int str_idx = t->str_idx;
        advance();
        /* function call */
        if (cur()->type == TK_LPAREN) {
            advance();
            int n = new_node(ND_CALL);
            tez_nodes[n].str_idx = str_idx;
            while (cur()->type != TK_RPAREN && !parse_err) {
                int arg = parse_expr();
                if (tez_nodes[n].param_count < TEZ_MAX_PARAMS)
                    tez_nodes[n].params[tez_nodes[n].param_count++] = arg;
                if (cur()->type == TK_COMMA) advance();
            }
            expect(TK_RPAREN);
            return n;
        }
        int n = new_node(ND_IDENT);
        tez_nodes[n].str_idx = str_idx;
        return n;
    }
    if (t->type == TK_CONS_OUT) {
        advance();
        expect(TK_LPAREN);
        int n = new_node(ND_CONS_OUT);
        tez_nodes[n].left = parse_expr();
        expect(TK_RPAREN);
        return n;
    }
    if (t->type == TK_LPAREN) {
        advance();
        int n = parse_expr();
        expect(TK_RPAREN);
        return n;
    }
    if (t->type == TK_MINUS) {
        advance();
        int n = new_node(ND_UNOP);
        tez_nodes[n].op = TK_MINUS;
        tez_nodes[n].left = parse_primary();
        return n;
    }
    if (t->type == TK_NOT) {
        advance();
        int n = new_node(ND_UNOP);
        tez_nodes[n].op = TK_NOT;
        tez_nodes[n].left = parse_primary();
        return n;
    }
    err("expected expression"); return -1;
}

static int parse_mul(void) {
    int left = parse_primary();
    while (!parse_err && (cur()->type==TK_MUL||cur()->type==TK_DIV||cur()->type==TK_MOD)) {
        TokType op = advance()->type;
        int n = new_node(ND_BINOP);
        tez_nodes[n].op = op;
        tez_nodes[n].left = left;
        tez_nodes[n].right = parse_primary();
        left = n;
    }
    return left;
}

static int parse_add(void) {
    int left = parse_mul();
    while (!parse_err && (cur()->type==TK_PLUS||cur()->type==TK_MINUS)) {
        TokType op = advance()->type;
        int n = new_node(ND_BINOP);
        tez_nodes[n].op = op;
        tez_nodes[n].left = left;
        tez_nodes[n].right = parse_mul();
        left = n;
    }
    return left;
}

static int parse_cmp(void) {
    int left = parse_add();
    while (!parse_err && (cur()->type==TK_LT||cur()->type==TK_GT||
                          cur()->type==TK_LE||cur()->type==TK_GE)) {
        TokType op = advance()->type;
        int n = new_node(ND_BINOP);
        tez_nodes[n].op = op;
        tez_nodes[n].left = left;
        tez_nodes[n].right = parse_add();
        left = n;
    }
    return left;
}

static int parse_eq(void) {
    int left = parse_cmp();
    while (!parse_err && (cur()->type==TK_EQ||cur()->type==TK_NEQ)) {
        TokType op = advance()->type;
        int n = new_node(ND_BINOP);
        tez_nodes[n].op = op;
        tez_nodes[n].left = left;
        tez_nodes[n].right = parse_cmp();
        left = n;
    }
    return left;
}

static int parse_and(void) {
    int left = parse_eq();
    while (!parse_err && cur()->type==TK_AND) {
        advance();
        int n = new_node(ND_BINOP); tez_nodes[n].op=TK_AND;
        tez_nodes[n].left=left; tez_nodes[n].right=parse_eq();
        left=n;
    }
    return left;
}

static int parse_expr(void) {
    int left = parse_and();
    while (!parse_err && cur()->type==TK_OR) {
        advance();
        int n = new_node(ND_BINOP); tez_nodes[n].op=TK_OR;
        tez_nodes[n].left=left; tez_nodes[n].right=parse_and();
        left=n;
    }
    return left;
}

/* ── type keyword → ValType ── */
static ValType parse_type(void) {
    switch (cur()->type) {
        case TK_INT:   advance(); return VT_INT;
        case TK_FLOAT: advance(); return VT_INT; /* treat as int for now */
        case TK_CH:    advance(); return VT_CH;
        case TK_STR:   advance(); return VT_STR;
        case TK_TF:    advance(); return VT_TF;
        case TK_NR:    advance(); return VT_NR;
        default: err("expected type"); return VT_INT;
    }
}

static int is_type(TokType t) {
    return t==TK_INT||t==TK_FLOAT||t==TK_CH||t==TK_STR||t==TK_TF||t==TK_NR;
}

/* ── statement parsing ── */

static int parse_stmt(void) {
    if (parse_err) return -1;
    Token *t = cur();

    /* var declaration: int x = expr; */
    if (is_type(t->type)) {
        ValType vt = parse_type();
        int n = new_node(ND_VAR_DECL);
        tez_nodes[n].vtype = vt;
        if (cur()->type != TK_IDENT) { err("expected variable name"); return -1; }
        tez_nodes[n].str_idx = cur()->str_idx;
        advance();
        if (cur()->type == TK_ASSIGN) {
            advance();
            tez_nodes[n].right = parse_expr();
        }
        expect(TK_SEMICOLON);
        return n;
    }

    /* assignment: x = expr; */
    if (t->type == TK_IDENT && peek(1)->type == TK_ASSIGN) {
        int n = new_node(ND_ASSIGN);
        tez_nodes[n].str_idx = t->str_idx;
        advance(); advance(); /* ident, = */
        tez_nodes[n].right = parse_expr();
        expect(TK_SEMICOLON);
        return n;
    }

    /* if */
    if (t->type == TK_IF) {
        advance();
        int n = new_node(ND_IF);
        expect(TK_LPAREN);
        tez_nodes[n].cond = parse_expr();
        expect(TK_RPAREN);
        tez_nodes[n].body = parse_block();
        if (cur()->type == TK_ELSE) {
            advance();
            tez_nodes[n].els = parse_block();
        }
        return n;
    }

    /* while */
    if (t->type == TK_WHILE) {
        advance();
        int n = new_node(ND_WHILE);
        expect(TK_LPAREN);
        tez_nodes[n].cond = parse_expr();
        expect(TK_RPAREN);
        tez_nodes[n].body = parse_block();
        return n;
    }

    /* for (init; cond; step) { body } */
    if (t->type == TK_FOR) {
        advance();
        int n = new_node(ND_FOR);
        expect(TK_LPAREN);
        tez_nodes[n].left  = parse_stmt();  /* init */
        tez_nodes[n].cond  = parse_expr();  /* cond */
        expect(TK_SEMICOLON);
        tez_nodes[n].right = parse_stmt_no_semi(); /* step */
        expect(TK_RPAREN);
        tez_nodes[n].body  = parse_block();
        return n;
    }

    /* rtn */
    if (t->type == TK_RTN) {
        advance();
        int n = new_node(ND_RTN);
        if (cur()->type != TK_SEMICOLON)
            tez_nodes[n].left = parse_expr();
        expect(TK_SEMICOLON);
        return n;
    }

    /* cons.out(expr); */
    if (t->type == TK_CONS_OUT) {
        advance();
        expect(TK_LPAREN);
        int n = new_node(ND_CONS_OUT);
        tez_nodes[n].left = parse_expr();
        expect(TK_RPAREN);
        expect(TK_SEMICOLON);
        return n;
    }

    /* expr statement (func call etc) */
    int n = parse_expr();
    expect(TK_SEMICOLON);
    return n;
}

/* for-loop step: assignment without semicolon */
static int parse_stmt_no_semi(void) {
    if (cur()->type == TK_IDENT && peek(1)->type == TK_ASSIGN) {
        int n = new_node(ND_ASSIGN);
        tez_nodes[n].str_idx = cur()->str_idx;
        advance(); advance();
        tez_nodes[n].right = parse_expr();
        return n;
    }
    return parse_expr();
}

static int parse_block(void) {
    if (cur()->type == TK_LBRACE) {
        advance();
        int head = -1, tail = -1;
        while (cur()->type != TK_RBRACE && cur()->type != TK_EOF && !parse_err) {
            int s = parse_stmt();
            if (head == -1) head = tail = s;
            else { tez_nodes[tail].next = s; tail = s; }
        }
        expect(TK_RBRACE);
        return head;
    }
    return parse_stmt();
}

/* ── function declaration: type F name(params) { body } ── */
static int parse_func(void) {
    ValType ret = parse_type(); /* return type */
    expect(TK_FUNC);           /* F keyword */
    if (cur()->type != TK_IDENT) { err("expected function name"); return -1; }
    int n = new_node(ND_FUNC_DECL);
    tez_nodes[n].vtype   = ret;
    tez_nodes[n].str_idx = cur()->str_idx;
    advance();
    expect(TK_LPAREN);
    while (cur()->type != TK_RPAREN && !parse_err) {
        if (!is_type(cur()->type)) { err("expected param type"); break; }
        parse_type(); /* consume type */
        if (cur()->type != TK_IDENT) { err("expected param name"); break; }
        if (tez_nodes[n].param_count < TEZ_MAX_PARAMS)
            tez_nodes[n].params[tez_nodes[n].param_count++] = cur()->str_idx;
        advance();
        if (cur()->type == TK_COMMA) advance();
    }
    expect(TK_RPAREN);
    tez_nodes[n].body = parse_block();
    return n;
}

/* ── top-level parse ── */
int tez_parse(void) {
    pos = 0;
    parse_err = 0;
    tez_node_count = 0;

    int root = new_node(ND_BLOCK);
    int tail = -1;

    while (cur()->type != TK_EOF && !parse_err) {
        int s;
        /* function declaration starts with type F */
        if (is_type(cur()->type) && peek(1)->type == TK_FUNC)
            s = parse_func();
        else
            s = parse_stmt();

        if (tez_nodes[root].body == -1)
            tez_nodes[root].body = tail = s;
        else {
            tez_nodes[tail].next = s; tail = s;
        }
    }

    if (parse_err) {
        /* store error in string table, return -1 */
        tez_strtab_count = 0;
        int i=0;
        while (parse_errmsg[i]) {
            tez_strtab[i] = parse_errmsg[i]; i++;
        }
        tez_strtab[i] = 0;
        return -1;
    }
    return root;
}

/* expose error message */
const char *tez_parse_error(void) { return parse_errmsg; }