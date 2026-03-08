#include <stdint.h>
#include "tez.h"

Token tez_tokens[TEZ_MAX_TOKENS];
int   tez_token_count = 0;
char  tez_strtab[TEZ_MAX_STR * 32];
int   tez_strtab_count = 0;

static int is_alpha(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
static int is_digit(char c) { return c>='0'&&c<='9'; }
static int is_alnum(char c) { return is_alpha(c)||is_digit(c); }

static int str_eq(const char *a, const char *b) {
    int i=0;
    while (a[i]&&b[i]) { if (a[i]!=b[i]) return 0; i++; }
    return a[i]==b[i];
}

/* store string literal, return index */
static int str_len(const char *s) { int i=0; while(s[i]) i++; return i; }

static int strtab_find(const char *s, int len) {
    for (int i=0; i<tez_strtab_count; i++) {
        char *entry = tez_strtab + i * 32;
        int match = 1;
        for (int j=0; j<len; j++) {
            if (entry[j] != s[j]) { match=0; break; }
        }
        if (match && entry[len] == 0) return i;
    }
    return -1;
}

static int strtab_add(const char *s, int len) {
    int existing = strtab_find(s, len);
    if (existing >= 0) return existing;
    int idx = tez_strtab_count;
    char *dst = tez_strtab + idx * 32;
    int i = 0;
    while (i < len && i < 31) { dst[i] = s[i]; i++; }
    dst[i] = 0;
    tez_strtab_count++;
    return idx;
}

static struct { const char *word; TokType type; } keywords[] = {
    {"int",   TK_INT},  {"float", TK_FLOAT}, {"ch",  TK_CH},
    {"str",   TK_STR},  {"tf",    TK_TF},    {"nr",  TK_NR},
    {"if",    TK_IF},   {"else",  TK_ELSE},  {"while",TK_WHILE},
    {"for",   TK_FOR},  {"F",     TK_FUNC},  {"rtn", TK_RTN},
    {"true",  TK_TRUE}, {"false", TK_FALSE},
    {0, TK_ERR}
};

int tez_lex(const char *src) {
    int pos = 0, line = 1;
    tez_token_count = 0;
    tez_strtab_count = 0;

    while (src[pos] && tez_token_count < TEZ_MAX_TOKENS - 1) {
        /* skip whitespace */
        while (src[pos]==' '||src[pos]=='\t'||src[pos]=='\r') pos++;
        if (src[pos]=='\n') { line++; pos++; continue; }
        if (!src[pos]) break;

        /* line comment */
        if (src[pos]=='/'&&src[pos+1]=='/') {
            while (src[pos]&&src[pos]!='\n') pos++;
            continue;
        }

        Token *tk = &tez_tokens[tez_token_count];
        tk->line = line;
        tk->str_idx = -1;

        char c = src[pos];

        /* integer / float literal */
        if (is_digit(c)) {
            int val = 0;
            while (is_digit(src[pos])) { val = val*10+(src[pos]-'0'); pos++; }
            tk->type = TK_INT_LIT;
            tk->int_val = val;
            tez_token_count++;
            continue;
        }

        /* string literal */
        if (c=='"') {
            pos++;
            int start = pos;
            while (src[pos]&&src[pos]!='"') pos++;
            tk->type = TK_STR_LIT;
            tk->str_idx = strtab_add(src+start, pos-start);
            if (src[pos]=='"') pos++;
            tez_token_count++;
            continue;
        }

        /* char literal */
        if (c=='\'') {
            pos++;
            tk->type = TK_CHAR_LIT;
            tk->int_val = src[pos] ? src[pos++] : 0;
            if (src[pos]=='\'') pos++;
            tez_token_count++;
            continue;
        }

        /* identifier or keyword */
        if (is_alpha(c)) {
            int start = pos;
            while (is_alnum(src[pos])) pos++;
            int len = pos - start;

            /* check cons.out */
            if (len==4 && src[start]=='c' && src[start+1]=='o' &&
                src[start+2]=='n' && src[start+3]=='s' && src[pos]=='.') {
                pos++; /* skip dot */
                if (src[pos]=='o'&&src[pos+1]=='u'&&src[pos+2]=='t') {
                    pos+=3;
                    tk->type = TK_CONS_OUT;
                    tez_token_count++;
                    continue;
                }
            }

            /* check keywords */
            char word[32]; int wi=0;
            while (wi<len&&wi<31) { word[wi]=src[start+wi]; wi++; }
            word[wi]=0;

            TokType kw = TK_IDENT;
            for (int i=0; keywords[i].word; i++) {
                if (str_eq(word, keywords[i].word)) { kw=keywords[i].type; break; }
            }
            tk->type = kw;
            if (kw==TK_IDENT) tk->str_idx = strtab_add(src+start, len);
            tez_token_count++;
            continue;
        }

        /* two-char operators */
        pos++;
        switch (c) {
            case '+': tk->type=TK_PLUS;     break;
            case '-': tk->type=TK_MINUS;    break;
            case '*': tk->type=TK_MUL;      break;
            case '/': tk->type=TK_DIV;      break;
            case '%': tk->type=TK_MOD;      break;
            case '(': tk->type=TK_LPAREN;   break;
            case ')': tk->type=TK_RPAREN;   break;
            case '{': tk->type=TK_LBRACE;   break;
            case '}': tk->type=TK_RBRACE;   break;
            case ';': tk->type=TK_SEMICOLON;break;
            case ',': tk->type=TK_COMMA;    break;
            case '.': tk->type=TK_DOT;      break;
            case '=': tk->type=(src[pos]=='='?(pos++,TK_EQ):TK_ASSIGN);  break;
            case '!': tk->type=(src[pos]=='='?(pos++,TK_NEQ):TK_NOT);    break;
            case '<': tk->type=(src[pos]=='='?(pos++,TK_LE):TK_LT);      break;
            case '>': tk->type=(src[pos]=='='?(pos++,TK_GE):TK_GT);      break;
            case '&': if(src[pos]=='&'){pos++;tk->type=TK_AND;}else tk->type=TK_ERR; break;
            case '|': if(src[pos]=='|'){pos++;tk->type=TK_OR;} else tk->type=TK_ERR; break;
            default:  tk->type=TK_ERR; break;
        }
        tez_token_count++;
    }

    tez_tokens[tez_token_count].type = TK_EOF;
    tez_tokens[tez_token_count].line = line;
    return tez_token_count;
}