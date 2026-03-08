#include <stdint.h>

extern int  keyboard_has_data(void);
extern char keyboard_getchar(void);
extern void vga_print(const char *s);
extern void vga_print_color(const char *s, uint8_t color);
extern void vga_putchar(char c);

#define WHITE  0x0F
#define CYAN   0x0B
#define GREEN  0x0A
#define YELLOW 0x0E
#define RED    0x0C
#define GRAY   0x08

// float utils 

typedef double f64;

static f64 f_abs(f64 x)  { return x < 0 ? -x : x; }

static f64 f_floor(f64 x) {
    int i = (int)x;
    return (f64)(x < 0.0 && x != (f64)i ? i - 1 : i);
}

// fmod
static f64 f_mod(f64 a, f64 b) {
    if (b == 0.0) return 0.0;
    return a - b * f_floor(a / b);
}

// integer power
static f64 f_ipow(f64 base, int exp) {
    f64 r = 1.0;
    int neg = (exp < 0);
    if (neg) exp = -exp;
    while (exp--) r *= base;
    return neg ? 1.0 / r : r;
}

// power via exp/log (for fractional exponents)
// ln(x) via series: ln(x) = 2*atanh((x-1)/(x+1))
static f64 f_ln(f64 x) {
    if (x <= 0.0) return -1e300;
    // scale x into [1,2) and count shifts
    int k = 0;
    while (x >= 2.0) { x /= 2.0; k++; }
    while (x < 1.0)  { x *= 2.0; k--; }
    // ln2
    f64 ln2 = 0.6931471805599453;
    // atanh series: y=(x-1)/(x+1), ln(x)=2*(y + y^3/3 + y^5/5 ...)
    f64 y = (x - 1.0) / (x + 1.0);
    f64 y2 = y * y, term = y, sum = y;
    for (int i = 1; i <= 40; i++) {
        term *= y2;
        sum += term / (2 * i + 1);
    }
    return 2.0 * sum + k * ln2;
}

static f64 f_exp(f64 x) {
    // e^x via Taylor
    f64 sum = 1.0, term = 1.0;
    for (int i = 1; i <= 50; i++) {
        term *= x / i;
        sum += term;
        if (f_abs(term) < 1e-15) break;
    }
    return sum;
}

static f64 f_pow(f64 base, f64 exp) {
    // integer exp fast path
    f64 ei = f_floor(exp);
    if (exp == ei && f_abs(exp) < 1e6)
        return f_ipow(base, (int)ei);
    if (base <= 0.0) return 0.0;
    return f_exp(exp * f_ln(base));
}

// pi constant
#define PI 3.14159265358979323846

// reduce angle to [-pi, pi]
static f64 reduce_angle(f64 x) {
    x = f_mod(x, 2.0 * PI);
    if (x >  PI) x -= 2.0 * PI;
    if (x < -PI) x += 2.0 * PI;
    return x;
}

static f64 f_sin(f64 x) {
    x = reduce_angle(x);
    f64 sum = 0.0, term = x;
    sum = x;
    for (int i = 1; i <= 20; i++) {
        term *= -x * x / ((2 * i) * (2 * i + 1));
        sum  += term;
        if (f_abs(term) < 1e-15) break;
    }
    return sum;
}

static f64 f_cos(f64 x) {
    x = reduce_angle(x);
    f64 sum = 1.0, term = 1.0;
    for (int i = 1; i <= 20; i++) {
        term *= -x * x / ((2 * i - 1) * (2 * i));
        sum  += term;
        if (f_abs(term) < 1e-15) break;
    }
    return sum;
}

static f64 f_tan(f64 x)  { f64 c = f_cos(x); return c == 0.0 ? 1e300 : f_sin(x) / c; }
static f64 f_ctan(f64 x) { f64 s = f_sin(x); return s == 0.0 ? 1e300 : f_cos(x) / s; }

// sqrt via Newton
static f64 f_sqrt(f64 x) {
    if (x < 0.0) return -1.0;
    if (x == 0.0) return 0.0;
    f64 r = x > 1.0 ? x / 2.0 : 1.0;
    for (int i = 0; i < 60; i++) {
        f64 nr = (r + x / r) * 0.5;
        if (f_abs(nr - r) < 1e-15) break;
        r = nr;
    }
    return r;
}

// float to string 

static int calc_strlen(const char *s) {
    int i = 0; while (s[i]) i++; return i;
}

// prints f64 into buf, returns length
static int f64_to_str(f64 val, char *buf, int decimals) {
    int pos = 0;

    if (val != val) { // NaN check (val != val is true for NaN)
        buf[pos++] = 'N'; buf[pos++] = 'a'; buf[pos++] = 'N';
        buf[pos] = 0; return pos;
    }

    if (val < 0.0) { buf[pos++] = '-'; val = -val; }

    // very large
    if (val > 999999999.0) {
        buf[pos++] = 'i'; buf[pos++] = 'n'; buf[pos++] = 'f';
        buf[pos] = 0; return pos;
    }

    int ipart = (int)val;
    f64 fpart = val - (f64)ipart;

    // integer part
    if (ipart == 0) {
        buf[pos++] = '0';
    } else {
        char tmp[12]; int ti = 0;
        int v = ipart;
        while (v > 0) { tmp[ti++] = '0' + (v % 10); v /= 10; }
        for (int i = ti - 1; i >= 0; i--) buf[pos++] = tmp[i];
    }

    // fractional part
    if (decimals > 0) {
        buf[pos++] = '.';
        for (int i = 0; i < decimals; i++) {
            fpart *= 10.0;
            int d = (int)fpart;
            buf[pos++] = '0' + d;
            fpart -= d;
        }
        // trim trailing zeros
        while (pos > 0 && buf[pos-1] == '0') pos--;
        if (pos > 0 && buf[pos-1] == '.') pos--;
    }

    buf[pos] = 0;
    return pos;
}

// Lexer 

typedef enum {
    TOK_NUM, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV,
    TOK_MOD, TOK_POW, TOK_LPAREN, TOK_RPAREN,
    TOK_IDENT, TOK_END, TOK_ERR
} TokType;

typedef struct {
    TokType type;
    f64     num;
    char    ident[16];
} Token;

static const char *lex_src;
static int         lex_pos;
static Token       lex_cur;

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int is_space(char c) { return c == ' ' || c == '\t'; }

static void lex_next(void) {
    while (is_space(lex_src[lex_pos])) lex_pos++;
    char c = lex_src[lex_pos];

    if (c == '\0') { lex_cur.type = TOK_END; return; }

    if (is_digit(c) || (c == '.' && is_digit(lex_src[lex_pos+1]))) {
        f64 val = 0.0;
        while (is_digit(lex_src[lex_pos]))
            val = val * 10.0 + (lex_src[lex_pos++] - '0');
        if (lex_src[lex_pos] == '.') {
            lex_pos++;
            f64 frac = 0.1;
            while (is_digit(lex_src[lex_pos])) {
                val += (lex_src[lex_pos++] - '0') * frac;
                frac *= 0.1;
            }
        }
        lex_cur.type = TOK_NUM;
        lex_cur.num  = val;
        return;
    }

    if (is_alpha(c)) {
        int i = 0;
        while ((is_alpha(lex_src[lex_pos]) || is_digit(lex_src[lex_pos])) && i < 15)
            lex_cur.ident[i++] = lex_src[lex_pos++];
        lex_cur.ident[i] = 0;
        lex_cur.type = TOK_IDENT;
        return;
    }

    lex_pos++;
    switch (c) {
        case '+': lex_cur.type = TOK_PLUS;   break;
        case '-': lex_cur.type = TOK_MINUS;  break;
        case '*':
            if (lex_src[lex_pos] == '*') { lex_pos++; lex_cur.type = TOK_POW; }
            else lex_cur.type = TOK_MUL;
            break;
        case '/': lex_cur.type = TOK_DIV;    break;
        case '%': lex_cur.type = TOK_MOD;    break;
        case '^': lex_cur.type = TOK_POW;    break;
        case '(': lex_cur.type = TOK_LPAREN; break;
        case ')': lex_cur.type = TOK_RPAREN; break;
        default:  lex_cur.type = TOK_ERR;    break;
    }
}

static void lex_init(const char *src) {
    lex_src = src; lex_pos = 0; lex_next();
}

// Parser (recursive descent) 

static int  parse_error;
static char parse_errmsg[48];

static f64 parse_expr(void);

static int ident_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) { if (a[i] != b[i]) return 0; i++; }
    return a[i] == b[i];
}

static f64 parse_primary(void) {
    if (parse_error) return 0.0;

    // number
    if (lex_cur.type == TOK_NUM) {
        f64 v = lex_cur.num; lex_next(); return v;
    }

    // unary minus
    if (lex_cur.type == TOK_MINUS) {
        lex_next(); return -parse_primary();
    }

    // unary plus
    if (lex_cur.type == TOK_PLUS) {
        lex_next(); return parse_primary();
    }

    // parentheses
    if (lex_cur.type == TOK_LPAREN) {
        lex_next();
        f64 v = parse_expr();
        if (lex_cur.type != TOK_RPAREN) {
            parse_error = 1;
            const char *m = "missing ')'";
            int i = 0; while (m[i]) parse_errmsg[i] = m[i++]; parse_errmsg[i] = 0;
        } else lex_next();
        return v;
    }

    // constants and functions
    if (lex_cur.type == TOK_IDENT) {
        char name[16];
        int i = 0; while (lex_cur.ident[i]) { name[i] = lex_cur.ident[i]; i++; } name[i] = 0;
        lex_next();

        // constants
        if (ident_eq(name, "pi") || ident_eq(name, "PI")) return PI;
        if (ident_eq(name, "e")  || ident_eq(name, "E"))  return 2.718281828459045;

        // functions — expect '('
        if (lex_cur.type != TOK_LPAREN) {
            parse_error = 1;
            const char *m = "expected '(' after function";
            i = 0; while (m[i]) parse_errmsg[i] = m[i++]; parse_errmsg[i] = 0;
            return 0.0;
        }
        lex_next();
        f64 arg = parse_expr();
        if (lex_cur.type != TOK_RPAREN) {
            parse_error = 1;
            const char *m = "missing ')' after arg";
            i = 0; while (m[i]) parse_errmsg[i] = m[i++]; parse_errmsg[i] = 0;
            return 0.0;
        }
        lex_next();

        if (ident_eq(name, "sin"))  return f_sin(arg);
        if (ident_eq(name, "cos"))  return f_cos(arg);
        if (ident_eq(name, "tan") || ident_eq(name, "tg"))  return f_tan(arg);
        if (ident_eq(name, "ctan") || ident_eq(name, "ctg")) return f_ctan(arg);
        if (ident_eq(name, "sqrt")) return f_sqrt(arg);
        if (ident_eq(name, "ln"))   return f_ln(arg);
        if (ident_eq(name, "abs"))  return f_abs(arg);

        parse_error = 1;
        const char *m = "unknown function";
        i = 0; while (m[i]) parse_errmsg[i] = m[i++]; parse_errmsg[i] = 0;
        return 0.0;
    }

    parse_error = 1;
    const char *m = "unexpected token";
    int i = 0; while (m[i]) parse_errmsg[i] = m[i++]; parse_errmsg[i] = 0;
    return 0.0;
}

// power (right-associative)
static f64 parse_power(void) {
    f64 base = parse_primary();
    if (!parse_error && lex_cur.type == TOK_POW) {
        lex_next();
        f64 exp = parse_power(); // right-associative
        return f_pow(base, exp);
    }
    return base;
}

// * / %
static f64 parse_term(void) {
    f64 left = parse_power();
    while (!parse_error) {
        if (lex_cur.type == TOK_MUL) { lex_next(); left *= parse_power(); }
        else if (lex_cur.type == TOK_DIV) {
            lex_next(); f64 r = parse_power();
            if (r == 0.0) {
                parse_error = 1;
                const char *m = "division by zero";
                int i = 0; while (m[i]) parse_errmsg[i] = m[i++]; parse_errmsg[i] = 0;
                return 0.0;
            }
            left /= r;
        }
        else if (lex_cur.type == TOK_MOD) { lex_next(); left = f_mod(left, parse_power()); }
        else break;
    }
    return left;
}

// + -
static f64 parse_expr(void) {
    f64 left = parse_term();
    while (!parse_error) {
        if      (lex_cur.type == TOK_PLUS)  { lex_next(); left += parse_term(); }
        else if (lex_cur.type == TOK_MINUS) { lex_next(); left -= parse_term(); }
        else break;
    }
    return left;
}

// Public API 

// Evaluate expression string, write result to out_buf
// Returns 0 on success, -1 on error (out_buf gets error message)
int calc_eval(const char *expr, char *out_buf) {
    parse_error = 0;
    parse_errmsg[0] = 0;
    lex_init(expr);

    f64 result = parse_expr();

    if (!parse_error && lex_cur.type != TOK_END) {
        parse_error = 1;
        const char *m = "unexpected input";
        int i = 0; while (m[i]) parse_errmsg[i] = m[i++]; parse_errmsg[i] = 0;
    }

    if (parse_error) {
        int i = 0;
        const char *e = "error: ";
        while (e[i]) { out_buf[i] = e[i]; i++; }
        int j = 0;
        while (parse_errmsg[j]) out_buf[i++] = parse_errmsg[j++];
        out_buf[i] = 0;
        return -1;
    }

    f64_to_str(result, out_buf, 10);
    return 0;
}

// Interactive REPL mode
void calc_interactive(void) {
    vga_print_color("  calc  |  Ctrl+Q to quit\n", CYAN);
    vga_print_color("  ops: + - * / % ** ^  |  fn: sin cos tan tg ctan ctg sqrt ln abs\n", GRAY);
    vga_print_color("  const: pi e\n\n", GRAY);

    char line[128];
    int pos = 0;

    vga_print_color("calc> ", CYAN);

    while (1) {
        if (!keyboard_has_data()) continue;
        char c = keyboard_getchar();

        if (c == 0x11) { // Ctrl+Q
            vga_putchar('\n');
            break;
        }

        if (c == '\n') {
            line[pos] = 0;
            vga_putchar('\n');
            if (pos > 0) {
                char result[64];
                int ok = calc_eval(line, result);
                if (ok == 0) {
                    vga_print_color("= ", GREEN);
                    vga_print_color(result, GREEN);
                } else {
                    vga_print_color(result, RED);
                }
                vga_putchar('\n');
            }
            pos = 0;
            vga_print_color("calc> ", CYAN);
        } else if (c == '\b') {
            if (pos > 0) { pos--; vga_putchar('\b'); }
        } else if (pos < 127) {
            line[pos++] = c;
            vga_putchar(c);
        }
    }
}