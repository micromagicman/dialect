#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Lexer lx;
    Token cur;      /* текущий токен */
    const char *error;
} Parser;

static void advance(Parser *p) { p->cur = lexer_next(&p->lx); }

static int is(Parser *p, TokenType t) { return p->cur.type == t; }

/* пропускает переводы строк, пока они идут подряд */
static void skip_newlines(Parser *p) {
    while (is(p, TOK_NEWLINE)) advance(p);
}

static int expect(Parser *p, TokenType t) {
    if (is(p, t)) { advance(p); return 1; }
    return 0;
}

static char *dup_tok(const Token *t) {
    char *s = malloc(t->len + 1);
    memcpy(s, t->start, t->len);
    s[t->len] = '\0';
    return s;
}

static void set_error(Parser *p, const char *fmt, TokenType got) {
    static char buf[256];
    snprintf(buf, sizeof(buf), "строка %d:%d — ожидался %s, встречен '%s'",
             p->cur.line, p->cur.col, fmt, token_name(got));
    p->error = buf;
}

static void add_tool(Agent *a, const char *name) {
    a->tools = realloc(a->tools, (a->n_tools + 1) * sizeof(char *));
    a->tools[a->n_tools++] = strdup(name);
}

static void add_str(char ***arr, size_t *n, const char *s) {
    *arr = realloc(*arr, (*n + 1) * sizeof(char *));
    (*arr)[(*n)++] = strdup(s);
}

static void add_cron(Harness *h, char *name, char *schedule, char *task) {
    h->crons = realloc(h->crons, (h->n_crons + 1) * sizeof(CronJob));
    CronJob *c = &h->crons[h->n_crons++];
    c->name = name;
    c->schedule = schedule;
    c->task = task;
}

static Agent *parse_agent(Parser *p) {
    Agent *a = calloc(1, sizeof(Agent));
    advance(p); /* agent */

    if (!is(p, TOK_IDENT)) { set_error(p, "имя агента", p->cur.type); goto fail; }
    a->name = dup_tok(&p->cur);
    advance(p);

    if (!expect(p, TOK_LBRACE)) { set_error(p, "'{'", p->cur.type); goto fail; }
    skip_newlines(p);

    while (!is(p, TOK_RBRACE) && !is(p, TOK_EOF)) {
        if (is(p, TOK_KW_MODEL)) {
            advance(p);
            if (!expect(p, TOK_ASSIGN)) { set_error(p, "'='", p->cur.type); goto fail; }
            if (!is(p, TOK_STRING)) { set_error(p, "строка модели", p->cur.type); goto fail; }
            free(a->model);
            a->model = dup_tok(&p->cur);
            advance(p);
        } else if (is(p, TOK_KW_TOOL)) {
            advance(p);
            if (!is(p, TOK_IDENT)) { set_error(p, "имя тула", p->cur.type); goto fail; }
            add_tool(a, dup_tok(&p->cur));
            advance(p);
        } else if (is(p, TOK_KW_MEMORY)) {
            advance(p);
            if (!is(p, TOK_IDENT)) { set_error(p, "поле memory", p->cur.type); goto fail; }
            char *field = dup_tok(&p->cur);
            advance(p);
            if (strcmp(field, "persistent") == 0) {
                if (!expect(p, TOK_ASSIGN)) { set_error(p, "'='", p->cur.type); free(field); goto fail; }
                if (is(p, TOK_KW_TRUE)) { a->has_memory_persistent = 1; a->memory_persistent = 1; advance(p); }
                else if (is(p, TOK_KW_FALSE)) { a->has_memory_persistent = 1; a->memory_persistent = 0; advance(p); }
                else { set_error(p, "true/false", p->cur.type); free(field); goto fail; }
            }
            free(field);
        } else if (is(p, TOK_IDENT)) {
            /* произвольное поле вида ident = <число|строка|true|false> */
            char *field = dup_tok(&p->cur);
            advance(p);
            if (!expect(p, TOK_ASSIGN)) { set_error(p, "'='", p->cur.type); free(field); goto fail; }
            if (strcmp(field, "temperature") == 0 && is(p, TOK_NUMBER)) {
                a->has_temperature = 1;
                a->temperature = strtod(p->cur.start, NULL);
                advance(p);
            } else if (strcmp(field, "system") == 0 && is(p, TOK_STRING)) {
                free(a->system);
                a->system = dup_tok(&p->cur);
                advance(p);
            } else if (is(p, TOK_NUMBER) || is(p, TOK_STRING)) {
                advance(p); /* неизвестное поле — принимаем и пропускаем значение */
            } else {
                set_error(p, "значение поля", p->cur.type); free(field); goto fail;
            }
            free(field);
        } else {
            set_error(p, "поле агента или '}'", p->cur.type);
            goto fail;
        }
        skip_newlines(p);
    }

    if (!expect(p, TOK_RBRACE)) { set_error(p, "'}'", p->cur.type); goto fail; }
    return a;

fail:
    if (a) { free(a->name); free(a->model); free(a->system); for (size_t i = 0; i < a->n_tools; i++) free(a->tools[i]); free(a->tools); free(a); }
    return NULL;
}

static Harness *parse_harness(Parser *p) {
    Harness *h = calloc(1, sizeof(Harness));
    advance(p); /* harness */

    if (!is(p, TOK_IDENT)) { set_error(p, "имя harness", p->cur.type); goto fail; }
    h->name = dup_tok(&p->cur);
    advance(p);

    if (!expect(p, TOK_LBRACE)) { set_error(p, "'{'", p->cur.type); goto fail; }
    skip_newlines(p);

    while (!is(p, TOK_RBRACE) && !is(p, TOK_EOF)) {
        if (is(p, TOK_KW_GATEWAY)) {
            advance(p);
            if (!is(p, TOK_IDENT)) { set_error(p, "имя платформы", p->cur.type); goto fail; }
            add_str(&h->gateways, &h->n_gateways, dup_tok(&p->cur));
            advance(p);
        } else if (is(p, TOK_KW_ALLOW)) {
            advance(p);
            if (is(p, TOK_KW_USER)) {
                advance(p);
                if (!is(p, TOK_NUMBER) && !is(p, TOK_IDENT)) { set_error(p, "id пользователя", p->cur.type); goto fail; }
                add_str(&h->allow_users, &h->n_allow_users, dup_tok(&p->cur));
                advance(p);
            } else if (is(p, TOK_KW_CHAT)) {
                advance(p);
                if (!is(p, TOK_NUMBER) && !is(p, TOK_IDENT)) { set_error(p, "id чата", p->cur.type); goto fail; }
                add_str(&h->allow_chats, &h->n_allow_chats, dup_tok(&p->cur));
                advance(p);
            } else {
                set_error(p, "'user' или 'chat'", p->cur.type); goto fail;
            }
        } else if (is(p, TOK_KW_CRON)) {
            advance(p);
            if (!is(p, TOK_STRING)) { set_error(p, "имя задачи (строка)", p->cur.type); goto fail; }
            char *name = dup_tok(&p->cur);
            advance(p);
            if (!is(p, TOK_KW_EVERY)) { set_error(p, "'every'", p->cur.type); free(name); goto fail; }
            advance(p);
            if (!is(p, TOK_STRING)) { set_error(p, "расписание (строка)", p->cur.type); free(name); goto fail; }
            char *schedule = dup_tok(&p->cur);
            advance(p);
            if (!is(p, TOK_KW_RUN)) { set_error(p, "'run'", p->cur.type); free(name); free(schedule); goto fail; }
            advance(p);
            if (!is(p, TOK_IDENT)) { set_error(p, "имя функции", p->cur.type); free(name); free(schedule); goto fail; }
            char *task = dup_tok(&p->cur);
            advance(p);
            add_cron(h, name, schedule, task);
        } else {
            set_error(p, "поле harness или '}'", p->cur.type);
            goto fail;
        }
        skip_newlines(p);
    }

    if (!expect(p, TOK_RBRACE)) { set_error(p, "'}'", p->cur.type); goto fail; }
    return h;

fail:
    if (h) {
        free(h->name);
        for (size_t i = 0; i < h->n_gateways; i++) free(h->gateways[i]);
        free(h->gateways);
        for (size_t i = 0; i < h->n_allow_users; i++) free(h->allow_users[i]);
        free(h->allow_users);
        for (size_t i = 0; i < h->n_allow_chats; i++) free(h->allow_chats[i]);
        free(h->allow_chats);
        for (size_t i = 0; i < h->n_crons; i++) { free(h->crons[i].name); free(h->crons[i].schedule); free(h->crons[i].task); }
        free(h->crons);
        free(h);
    }
    return NULL;
}

/* ===== Ядро языка: выражения ===== */

static Expr *new_expr(ExprKind k) {
    Expr *e = calloc(1, sizeof(Expr));
    e->kind = k;
    return e;
}

static int bin_prec(TokenType t) {
    switch (t) {
        case TOK_OR: return 1;
        case TOK_AND: return 2;
        case TOK_EQ: case TOK_NE: case TOK_LT: case TOK_GT: case TOK_LE: case TOK_GE: return 3;
        case TOK_PLUS: case TOK_MINUS: return 4;
        case TOK_STAR: case TOK_SLASH: return 5;
        default: return 0;
    }
}

static Expr *parse_expr(Parser *p);

static Expr *parse_primary(Parser *p) {
    if (is(p, TOK_NUMBER)) {
        Expr *e = new_expr(EXPR_NUMBER);
        e->num = strtol(p->cur.start, NULL, 10);
        advance(p);
        return e;
    }
    if (is(p, TOK_STRING)) {
        Expr *e = new_expr(EXPR_STRING);
        e->str = dup_tok(&p->cur);
        advance(p);
        return e;
    }
    if (is(p, TOK_KW_TRUE) || is(p, TOK_KW_FALSE)) {
        Expr *e = new_expr(EXPR_BOOL);
        e->boolean = is(p, TOK_KW_TRUE);
        advance(p);
        return e;
    }
    if (is(p, TOK_IDENT)) {
        char *name = dup_tok(&p->cur);
        advance(p);
        if (is(p, TOK_LPAREN)) {
            Expr *e = new_expr(EXPR_CALL);
            e->callee = name;
            advance(p);
            if (!is(p, TOK_RPAREN)) {
                for (;;) {
                    e->args = realloc(e->args, (e->n_args + 1) * sizeof(Expr *));
                    e->args[e->n_args++] = parse_expr(p);
                    if (is(p, TOK_COMMA)) { advance(p); continue; }
                    break;
                }
            }
            if (!expect(p, TOK_RPAREN)) { set_error(p, "')'", p->cur.type); return NULL; }
            return e;
        }
        Expr *e = new_expr(EXPR_VAR);
        e->name = name;
        return e;
    }
    if (is(p, TOK_LPAREN)) {
        advance(p);
        Expr *e = parse_expr(p);
        if (!expect(p, TOK_RPAREN)) { set_error(p, "')'", p->cur.type); return NULL; }
        return e;
    }
    set_error(p, "выражение", p->cur.type);
    return NULL;
}

static Expr *parse_unary(Parser *p) {
    if (is(p, TOK_MINUS) || is(p, TOK_NOT)) {
        TokenType op = p->cur.type;
        advance(p);
        Expr *e = new_expr(EXPR_UNARY);
        e->op = op;
        e->operand = parse_unary(p);
        return e;
    }
    return parse_primary(p);
}

static Expr *parse_binary(Parser *p, int min_prec) {
    Expr *left = parse_unary(p);
    for (;;) {
        int prec = bin_prec(p->cur.type);
        if (prec < min_prec) break;
        TokenType op = p->cur.type;
        advance(p);
        Expr *right = parse_binary(p, prec + 1);
        Expr *e = new_expr(EXPR_BINARY);
        e->op = op; e->left = left; e->right = right;
        left = e;
    }
    return left;
}

static Expr *parse_expr(Parser *p) {
    return parse_binary(p, 1);
}

/* ===== Ядро языка: типы, statement'ы, функции ===== */

static Type parse_type(Parser *p) {
    if (is(p, TOK_KW_INT)) { advance(p); return TYPE_INT; }
    if (is(p, TOK_KW_STR)) { advance(p); return TYPE_STR; }
    if (is(p, TOK_KW_BOOL)) { advance(p); return TYPE_BOOL; }
    return TYPE_VOID;
}

static Token peek_ahead(Parser *p) {
    size_t pos = p->lx.pos;
    int line = p->lx.line, col = p->lx.col;
    Token ahead = lexer_next(&p->lx);
    p->lx.pos = pos; p->lx.line = line; p->lx.col = col;
    return ahead;
}

static Stmt *parse_stmt(Parser *p);

static Stmt *parse_block(Parser *p) {
    Stmt *b = calloc(1, sizeof(Stmt));
    b->kind = STMT_BLOCK;
    expect(p, TOK_LBRACE);
    skip_newlines(p);
    while (!is(p, TOK_RBRACE) && !is(p, TOK_EOF)) {
        Stmt *s = parse_stmt(p);
        if (!s) return NULL;
        b->stmts = realloc(b->stmts, (b->n_stmts + 1) * sizeof(Stmt *));
        b->stmts[b->n_stmts++] = s;
        skip_newlines(p);
    }
    if (!expect(p, TOK_RBRACE)) { set_error(p, "'}'", p->cur.type); return NULL; }
    return b;
}

static Stmt *parse_stmt(Parser *p) {
    if (is(p, TOK_KW_LET)) {
        advance(p);
        Stmt *s = calloc(1, sizeof(Stmt));
        s->kind = STMT_LET;
        if (!is(p, TOK_IDENT)) { set_error(p, "имя переменной", p->cur.type); return NULL; }
        s->name = dup_tok(&p->cur);
        advance(p);
        if (is(p, TOK_COLON)) { advance(p); s->type = parse_type(p); }
        else s->type = TYPE_INT;
        if (is(p, TOK_ASSIGN)) { advance(p); s->init = parse_expr(p); }
        return s;
    }
    if (is(p, TOK_KW_IF)) {
        advance(p);
        Stmt *s = calloc(1, sizeof(Stmt));
        s->kind = STMT_IF;
        s->cond = parse_expr(p);
        s->then = parse_block(p);
        skip_newlines(p);
        if (is(p, TOK_KW_ELSE)) { advance(p); s->else_ = parse_block(p); }
        return s;
    }
    if (is(p, TOK_KW_WHILE)) {
        advance(p);
        Stmt *s = calloc(1, sizeof(Stmt));
        s->kind = STMT_WHILE;
        s->cond = parse_expr(p);
        s->body = parse_block(p);
        return s;
    }
    if (is(p, TOK_KW_RETURN)) {
        advance(p);
        Stmt *s = calloc(1, sizeof(Stmt));
        s->kind = STMT_RETURN;
        if (!is(p, TOK_NEWLINE) && !is(p, TOK_RBRACE)) s->value = parse_expr(p);
        return s;
    }
    if (is(p, TOK_IDENT)) {
        Token ahead = peek_ahead(p);
        if (ahead.type == TOK_ASSIGN) {
            Stmt *s = calloc(1, sizeof(Stmt));
            s->kind = STMT_ASSIGN;
            s->name = dup_tok(&p->cur);
            advance(p);
            advance(p); /* = */
            s->value = parse_expr(p);
            return s;
        }
        Stmt *s = calloc(1, sizeof(Stmt));
        s->kind = STMT_EXPR;
        s->expr = parse_expr(p);
        return s;
    }
    Stmt *s = calloc(1, sizeof(Stmt));
    s->kind = STMT_EXPR;
    s->expr = parse_expr(p);
    return s;
}

static Fn *parse_fn(Parser *p) {
    Fn *f = calloc(1, sizeof(Fn));
    advance(p); /* fn */
    if (!is(p, TOK_IDENT)) { set_error(p, "имя функции", p->cur.type); return NULL; }
    f->name = dup_tok(&p->cur);
    advance(p);
    if (!expect(p, TOK_LPAREN)) { set_error(p, "'('", p->cur.type); return NULL; }
    if (!is(p, TOK_RPAREN)) {
        for (;;) {
            if (!is(p, TOK_IDENT)) { set_error(p, "имя параметра", p->cur.type); return NULL; }
            f->params = realloc(f->params, (f->n_params + 1) * sizeof(char *));
            f->param_types = realloc(f->param_types, (f->n_params + 1) * sizeof(Type));
            f->params[f->n_params] = dup_tok(&p->cur);
            f->param_types[f->n_params] = TYPE_INT;
            f->n_params++;
            advance(p);
            if (is(p, TOK_COLON)) { advance(p); f->param_types[f->n_params - 1] = parse_type(p); }
            if (is(p, TOK_COMMA)) { advance(p); continue; }
            break;
        }
    }
    if (!expect(p, TOK_RPAREN)) { set_error(p, "')'", p->cur.type); return NULL; }
    f->ret_type = TYPE_VOID;
    if (is(p, TOK_ARROW)) { advance(p); f->ret_type = parse_type(p); }
    f->body = parse_block(p);
    return f;
}

ParseResult parse(const char *src) {
    Parser p;
    lexer_init(&p.lx, src);
    p.cur = lexer_next(&p.lx);
    p.error = NULL;

    ParseResult r;
    r.agent = NULL;
    r.harness = NULL;
    r.error = NULL;
    r.fns = NULL;
    r.n_fns = 0;
    r.entry = NULL;

    skip_newlines(&p);
    while (!is(&p, TOK_EOF)) {
        if (is(&p, TOK_KW_AGENT)) {
            r.agent = parse_agent(&p);
            if (!r.agent) { r.error = (char *)p.error; return r; }
        } else if (is(&p, TOK_KW_HARNESS)) {
            r.harness = parse_harness(&p);
            if (!r.harness) { r.error = (char *)p.error; return r; }
        } else if (is(&p, TOK_KW_FN)) {
            Fn *f = parse_fn(&p);
            if (!f) { r.error = (char *)p.error; return r; }
            r.fns = realloc(r.fns, (r.n_fns + 1) * sizeof(Fn));
            r.fns[r.n_fns] = *f;
            free(f); /* указатели внутри перенесены по значению */
            if (strcmp(r.fns[r.n_fns].name, "main") == 0) r.entry = r.fns[r.n_fns].name;
            r.n_fns++;
        } else {
            set_error(&p, "'agent', 'harness' или 'fn'", p.cur.type);
            r.error = (char *)p.error;
            return r;
        }
        skip_newlines(&p);
    }
    return r;
}

static void free_expr(Expr *e) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_STRING: free(e->str); break;
        case EXPR_VAR: free(e->name); break;
        case EXPR_CALL:
            free(e->callee);
            for (size_t i = 0; i < e->n_args; i++) free_expr(e->args[i]);
            free(e->args);
            break;
        case EXPR_UNARY: free_expr(e->operand); break;
        case EXPR_BINARY: free_expr(e->left); free_expr(e->right); break;
        default: break;
    }
    free(e);
}

static void free_stmt(Stmt *s) {
    if (!s) return;
    switch (s->kind) {
        case STMT_LET: free(s->name); free_expr(s->init); break;
        case STMT_ASSIGN: free(s->name); free_expr(s->value); break;
        case STMT_EXPR: free_expr(s->expr); break;
        case STMT_RETURN: free_expr(s->value); break;
        case STMT_IF: free_expr(s->cond); free_stmt(s->then); free_stmt(s->else_); break;
        case STMT_WHILE: free_expr(s->cond); free_stmt(s->body); break;
        case STMT_BLOCK:
            for (size_t i = 0; i < s->n_stmts; i++) free_stmt(s->stmts[i]);
            free(s->stmts);
            break;
    }
    free(s);
}

static void free_fn(Fn *f) {
    free(f->name);
    for (size_t i = 0; i < f->n_params; i++) free(f->params[i]);
    free(f->params);
    free(f->param_types);
    free_stmt(f->body);
}

void parse_result_free(ParseResult *r) {
    if (r->agent) {
        free(r->agent->name);
        free(r->agent->model);
        free(r->agent->system);
        for (size_t i = 0; i < r->agent->n_tools; i++) free(r->agent->tools[i]);
        free(r->agent->tools);
        free(r->agent);
    }
    if (r->harness) {
        Harness *h = r->harness;
        free(h->name);
        for (size_t i = 0; i < h->n_gateways; i++) free(h->gateways[i]);
        free(h->gateways);
        for (size_t i = 0; i < h->n_allow_users; i++) free(h->allow_users[i]);
        free(h->allow_users);
        for (size_t i = 0; i < h->n_allow_chats; i++) free(h->allow_chats[i]);
        free(h->allow_chats);
        for (size_t i = 0; i < h->n_crons; i++) { free(h->crons[i].name); free(h->crons[i].schedule); free(h->crons[i].task); }
        free(h->crons);
        free(h);
    }
    for (size_t i = 0; i < r->n_fns; i++) free_fn(&r->fns[i]);
    free(r->fns);
}
