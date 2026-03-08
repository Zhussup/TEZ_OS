#include <stdint.h>
#include "tez.h"

/* ── register allocation (simple: eax=result, stack for vars) ──
   calling convention: cdecl-like, ebp frame
   vars: [ebp - 4*slot]
   params: [ebp + 8 + 4*i]
*/

#define MAX_VARS   64
#define MAX_FUNCS  32

typedef struct { int str_idx; int slot; } VarEntry;
typedef struct { int str_idx; int code_offset; int param_count; } FuncEntry;

static uint8_t *code;
static int      cp;       /* code pointer */
static int      max_code;
static int      cg_err;
static char     cg_errmsg[48];

static void cg_error(const char *msg) {
    if (cg_err) return;
    cg_err = 1;
    int i=0; while(msg[i]) { cg_errmsg[i]=msg[i]; i++; } cg_errmsg[i]=0;
}

const char *tez_codegen_error(void) { return cg_errmsg; }

static VarEntry  vars[MAX_VARS];
static int       var_count;
static FuncEntry funcs[MAX_FUNCS];
static int       func_count;
static int       local_slots; /* current frame size */

/* forward */
static void emit_node(int idx);

/* ── emit helpers ── */
static void emit1(uint8_t b)  { if (cp<max_code) code[cp++]=b; else cg_error("code buffer full"); }
static void emit4(uint32_t v) {
    emit1(v&0xFF); emit1((v>>8)&0xFF);
    emit1((v>>16)&0xFF); emit1((v>>24)&0xFF);
}
static void patch4(int at, uint32_t v) {
    code[at]=(v&0xFF); code[at+1]=((v>>8)&0xFF);
    code[at+2]=((v>>16)&0xFF); code[at+3]=((v>>24)&0xFF);
}

/* ── variable lookup ── */
extern void vga_print(const char *s);
extern void vga_putchar(char c);

static void dbg_int(int v) {
    char tmp[12]; int i=0;
    if (v<0){vga_putchar('-');v=-v;}
    if (v==0){vga_putchar('0');return;}
    while(v>0){tmp[i++]='0'+(v%10);v/=10;}
    for(int j=i-1;j>=0;j--)vga_putchar(tmp[j]);
}

static int var_find(int str_idx) {
    for (int i=var_count-1; i>=0; i--)
        if (vars[i].str_idx == str_idx) return vars[i].slot;
    vga_print("[dbg var_find: not found str_idx="); dbg_int(str_idx);
    vga_print(" var_count="); dbg_int(var_count);
    vga_print(" known=");
    for(int i=0;i<var_count;i++){dbg_int(vars[i].str_idx);vga_putchar(',');}
    vga_print("]\n");
    return -1;
}
static int var_alloc(int str_idx) {
    local_slots++;
    vars[var_count].str_idx = str_idx;
    vars[var_count].slot    = local_slots;
    var_count++;
    return local_slots;
}

/* ── function lookup ── */
static int func_find(int str_idx) {
    for (int i=0; i<func_count; i++)
        if (funcs[i].str_idx == str_idx) return i;
    return -1;
}

/* ── vga_print / vga_putchar runtime support ──
   we store addresses of kernel functions to call them from JIT code */
static uint32_t rt_vga_print    = 0;
static uint32_t rt_vga_putchar  = 0;
static uint32_t rt_print_int    = 0; /* our helper */

/* print_int helper: prints eax as decimal via vga_putchar */
static char pi_buf[12];
static void print_int_helper(int val) {
    extern void vga_putchar(char c);
    if (val < 0) { vga_putchar('-'); val = -val; }
    if (val == 0) { vga_putchar('0'); return; }
    int i=0;
    while (val>0) { pi_buf[i++]='0'+(val%10); val/=10; }
    for (int j=i-1;j>=0;j--) vga_putchar(pi_buf[j]);
}

/* ── x86 instruction macros ── */
/* push imm32 */
#define PUSH_IMM32(v)  emit1(0x68); emit4(v)
/* push eax */
#define PUSH_EAX       emit1(0x50)
/* pop eax */
#define POP_EAX        emit1(0x58)
/* pop ebx */
#define POP_EBX        emit1(0x5B)
/* mov eax, imm32 */
#define MOV_EAX_IMM(v) emit1(0xB8); emit4(v)
/* mov eax, [ebp - slot*4] */
#define MOV_EAX_VAR(s) emit1(0x8B); emit1(0x45); emit1((uint8_t)(-(s)*4))
/* mov [ebp - slot*4], eax */
#define MOV_VAR_EAX(s) emit1(0x89); emit1(0x45); emit1((uint8_t)(-(s)*4))
/* add eax, ebx */
#define ADD_EAX_EBX    emit1(0x01); emit1(0xD8)
/* sub eax, ebx  (eax = ebx - eax) */
#define SUB_EBX_EAX    emit1(0x29); emit1(0xC3); /* ebx-=eax */ \
                       emit1(0x89); emit1(0xD8)  /* mov eax,ebx */
/* imul eax, ebx */
#define IMUL_EAX_EBX   emit1(0x0F); emit1(0xAF); emit1(0xC3)
/* idiv: eax = ebx / eax */
#define IDIV_EBX_EAX   emit1(0x93); /* xchg eax,ebx */ \
                       emit1(0x99); /* cdq */ \
                       emit1(0xF7); emit1(0xFB) /* idiv ebx */
/* imod: eax = ebx % eax */
#define IMOD_EBX_EAX   emit1(0x93); emit1(0x99); \
                       emit1(0xF7); emit1(0xFB); /* idiv ebx */ \
                       emit1(0x89); emit1(0xD0)  /* mov eax,edx */
/* cmp ebx, eax */
#define CMP_EBX_EAX    emit1(0x3B); emit1(0xD8)
/* setCC al, movzx eax,al */
#define SETCC_EAX(cc)  emit1(0x0F); emit1(cc); emit1(0xC0); \
                       emit1(0x0F); emit1(0xB6); emit1(0xC0)
/* ret */
#define RET            emit1(0xC3)
/* call rel32 */
#define CALL_REL32(off) emit1(0xE8); emit4(off)
/* jmp rel32 */
#define JMP_REL32(off)  emit1(0xE9); emit4(off)
/* jz rel32 */
#define JZ_REL32(off)   emit1(0x0F); emit1(0x84); emit4(off)
/* test eax,eax */
#define TEST_EAX        emit1(0x85); emit1(0xC0)

static void emit_prologue(int slots) {
    emit1(0x55);             /* push ebp */
    emit1(0x89); emit1(0xE5);/* mov ebp, esp */
    if (slots > 0) {
        emit1(0x81); emit1(0xEC); emit4(slots * 4); /* sub esp, N */
    }
}
static void emit_epilogue(void) {
    emit1(0x89); emit1(0xEC); /* mov esp, ebp */
    emit1(0x5D);              /* pop ebp */
    RET;
}

/* emit absolute call to C function */
static void emit_call_abs(uint32_t addr) {
    /* mov eax, addr; call eax */
    MOV_EAX_IMM(addr);
    emit1(0xFF); emit1(0xD0);
}

/* ── string table base address for str literals ── */
static uint32_t strtab_base = 0;

/* ── emit expression → result in eax ── */
static void emit_expr(int idx) {
    if (idx < 0 || cg_err) return;
    Node *n = &tez_nodes[idx];

    switch (n->type) {
        case ND_INT:
        case ND_BOOL:
        case ND_CHAR:
            MOV_EAX_IMM((uint32_t)n->ival);
            break;

        case ND_STR:
            /* eax = pointer to string literal */
            MOV_EAX_IMM(strtab_base + n->str_idx * 32);
            break;

        case ND_IDENT: {
            int slot = var_find(n->str_idx);
            if (slot < 0) { cg_error("undefined variable"); return; }
            break;
        }

        case ND_UNOP:
            emit_expr(n->left);
            if (n->op == TK_MINUS) {
                emit1(0xF7); emit1(0xD8); /* neg eax */
            } else { /* TK_NOT */
                TEST_EAX;
                emit1(0x0F); emit1(0x94); emit1(0xC0); /* sete al */
                emit1(0x0F); emit1(0xB6); emit1(0xC0); /* movzx eax,al */
            }
            break;

        case ND_BINOP: {
            /* eval right → push, eval left → pop ebx, op */
            emit_expr(n->right); PUSH_EAX;
            emit_expr(n->left);  POP_EBX;
            /* now eax=left, ebx=right */
            switch (n->op) {
                case TK_PLUS:  ADD_EAX_EBX; break;
                case TK_MINUS: SUB_EBX_EAX; break;
                case TK_MUL:   IMUL_EAX_EBX; break;
                case TK_DIV:   IDIV_EBX_EAX; break;
                case TK_MOD:   IMOD_EBX_EAX; break;
                case TK_LT:  CMP_EBX_EAX; SETCC_EAX(0x9C); break; /* setl */
                case TK_GT:  CMP_EBX_EAX; SETCC_EAX(0x9F); break; /* setg */
                case TK_LE:  CMP_EBX_EAX; SETCC_EAX(0x9E); break; /* setle */
                case TK_GE:  CMP_EBX_EAX; SETCC_EAX(0x9D); break; /* setge */
                case TK_EQ:  CMP_EBX_EAX; SETCC_EAX(0x94); break; /* sete */
                case TK_NEQ: CMP_EBX_EAX; SETCC_EAX(0x95); break; /* setne */
                case TK_AND:
                    /* eax = (left != 0) && (right != 0) */
                    emit1(0x85); emit1(0xC0); /* test eax,eax */
                    emit1(0x0F); emit1(0x95); emit1(0xC0); /* setne al */
                    emit1(0x85); emit1(0xDB); /* test ebx,ebx */
                    emit1(0x0F); emit1(0x95); emit1(0xC3); /* setne bl */
                    emit1(0x20); emit1(0xD8); /* and al,bl */
                    emit1(0x0F); emit1(0xB6); emit1(0xC0);
                    break;
                case TK_OR:
                    emit1(0x09); emit1(0xD8); /* or eax,ebx */
                    emit1(0x0F); emit1(0x95); emit1(0xC0); /* setne al */
                    emit1(0x0F); emit1(0xB6); emit1(0xC0);
                    break;
                default: break;
            }
            break;
        }

        case ND_CALL: {
            int fi = func_find(n->str_idx);
            if (fi < 0) { cg_error("undefined function"); return; }
            /* push args right-to-left */
            for (int i = n->param_count-1; i >= 0; i--) {
                emit_expr(n->params[i]); PUSH_EAX;
            }
            int rel = funcs[fi].code_offset - (cp + 5);
            CALL_REL32((uint32_t)rel);
            /* clean stack */
            if (n->param_count > 0) {
                emit1(0x81); emit1(0xC4); emit4(n->param_count * 4); /* add esp, N */
            }
            break;
        }

        case ND_CONS_OUT: {
            emit_expr(n->left);
            Node *arg = &tez_nodes[n->left];
            if (arg->type == ND_STR) {
                PUSH_EAX;
                emit_call_abs(rt_vga_print);
                emit1(0x83); emit1(0xC4); emit1(4);
            } else {
                PUSH_EAX;
                emit_call_abs(rt_print_int);
                emit1(0x83); emit1(0xC4); emit1(4);
            }
            /* newline */
            MOV_EAX_IMM('\n');
            PUSH_EAX;
            emit_call_abs(rt_vga_putchar);
            emit1(0x83); emit1(0xC4); emit1(4);
            break;
        }

        default: break;
    }
}

/* ── emit statement ── */
static void emit_stmt(int idx) {
    if (idx < 0 || cg_err) return;
    Node *n = &tez_nodes[idx];

    switch (n->type) {
        case ND_VAR_DECL: {
            int slot = var_alloc(n->str_idx);
            if (n->right >= 0) {
                emit_expr(n->right);
                MOV_VAR_EAX(slot);
            }
            break;
        }

        case ND_ASSIGN: {
            int slot = var_find(n->str_idx);
            if (slot < 0) { cg_error("undefined variable in assign"); return; }
            break;
        }

        case ND_IF: {
            emit_expr(n->cond);
            TEST_EAX;
            /* jz to else/end */
            emit1(0x0F); emit1(0x84); int jz_patch = cp; emit4(0);
            /* body */
            int s = n->body;
            while (s >= 0) { emit_stmt(s); s = tez_nodes[s].next; }
            if (n->els >= 0) {
                /* jmp over else */
                JMP_REL32(0); int jmp_patch = cp - 4;
                patch4(jz_patch, cp - (jz_patch+4));
                s = n->els;
                while (s >= 0) { emit_stmt(s); s = tez_nodes[s].next; }
                patch4(jmp_patch, cp - (jmp_patch+4));
            } else {
                patch4(jz_patch, cp - (jz_patch+4));
            }
            break;
        }

        case ND_WHILE: {
            int loop_start = cp;
            emit_expr(n->cond);
            TEST_EAX;
            emit1(0x0F); emit1(0x84); int jz_patch = cp; emit4(0);
            int s = n->body;
            while (s >= 0) { emit_stmt(s); s = tez_nodes[s].next; }
            JMP_REL32(loop_start - (cp+4)); /* jmp back */
            patch4(jz_patch, cp - (jz_patch+4));
            break;
        }

        case ND_FOR: {
            /* init */
            if (n->left >= 0) emit_stmt(n->left);
            int loop_start = cp;
            /* cond */
            emit_expr(n->cond);
            TEST_EAX;
            emit1(0x0F); emit1(0x84); int jz_patch = cp; emit4(0);
            /* body */
            int s = n->body;
            while (s >= 0) { emit_stmt(s); s = tez_nodes[s].next; }
            /* step */
            if (n->right >= 0) emit_stmt(n->right);
            JMP_REL32(loop_start - (cp+4));
            patch4(jz_patch, cp - (jz_patch+4));
            break;
        }

        case ND_RTN:
            if (n->left >= 0) emit_expr(n->left);
            emit_epilogue();
            break;

        case ND_CONS_OUT:
        case ND_CALL:
            emit_expr(idx);
            break;

        case ND_FUNC_DECL: {
            /* record function entry point */
            int fi = func_count++;
            funcs[fi].str_idx    = n->str_idx;
            funcs[fi].code_offset = cp;
            funcs[fi].param_count = n->param_count;

            /* save/restore var state for new scope */
            int saved_var_count   = var_count;
            int saved_local_slots = local_slots;
            local_slots = 0;

            /* params: [ebp+8], [ebp+12], ... */
            /* we don't pre-allocate slots for params — access via ebp offset */
            /* actually map param names to negative slots for simplicity */
            for (int i=0; i<n->param_count; i++) {
                vars[var_count].str_idx = n->params[i];
                vars[var_count].slot    = -(i+2); /* [ebp+8+i*4] */
                var_count++;
            }

            /* we'll patch sub esp after we know frame size */
            emit1(0x55); emit1(0x89); emit1(0xE5); /* push ebp; mov ebp,esp */
            int frame_patch = cp;
            emit1(0x81); emit1(0xEC); emit4(0); /* sub esp, ? — patch later */

            int s = n->body;
            while (s >= 0 && !cg_err) { emit_stmt(s); s = tez_nodes[s].next; }

            /* patch frame size */
            patch4(frame_patch+2, local_slots * 4);

            /* default return */
            emit_epilogue();

            var_count   = saved_var_count;
            local_slots = saved_local_slots;
            break;
        }

        default:
            emit_expr(idx);
            break;
    }
}

/* ── public entry ── */
int tez_codegen(int root, uint8_t *out, int max_sz) {
    code     = out;
    cp       = 0;
    max_code = max_sz;
    cg_err   = 0;
    cg_errmsg[0] = 0;
    var_count   = 0;
    func_count  = 0;
    local_slots = 0;

    /* runtime function pointers */
    extern void vga_print(const char *s);
    extern void vga_putchar(char c);
    rt_vga_print   = (uint32_t)vga_print;
    rt_vga_putchar = (uint32_t)vga_putchar;
    rt_print_int   = (uint32_t)print_int_helper;
    strtab_base    = (uint32_t)tez_strtab;

    if (root < 0) return -1;

    /* emit top-level: first pass collects func offsets,
       second pass handles calls — for simplicity we do single pass
       (functions must be declared before use, like C) */

    emit1(0x55); emit1(0x89); emit1(0xE5); /* push ebp; mov ebp,esp */
    int frame_patch = cp;
    emit1(0x81); emit1(0xEC); emit4(0); /* sub esp, ? — patch later */

    Node *rn = &tez_nodes[root];
    int s = rn->body;
    while (s >= 0 && !cg_err) {
        emit_stmt(s);
        s = tez_nodes[s].next;
    }

    patch4(frame_patch + 2, local_slots * 4);
    emit_epilogue();

    return cg_err ? -1 : cp;
}