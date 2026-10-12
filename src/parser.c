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

ParseResult parse(const char *src) {
    Parser p;
    lexer_init(&p.lx, src);
    p.cur = lexer_next(&p.lx);
    p.error = NULL;

    ParseResult r;
    r.agent = NULL;
    r.harness = NULL;
    r.error = NULL;

    skip_newlines(&p);
    while (!is(&p, TOK_EOF)) {
        if (is(&p, TOK_KW_AGENT)) {
            r.agent = parse_agent(&p);
            if (!r.agent) { r.error = (char *)p.error; return r; }
        } else if (is(&p, TOK_KW_HARNESS)) {
            r.harness = parse_harness(&p);
            if (!r.harness) { r.error = (char *)p.error; return r; }
        } else {
            set_error(&p, "'agent' или 'harness'", p.cur.type);
            r.error = (char *)p.error;
            return r;
        }
        skip_newlines(&p);
    }
    return r;
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
}
