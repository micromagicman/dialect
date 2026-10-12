#include "lexer.h"
#include <string.h>

static int is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int is_digit(int c) { return c >= '0' && c <= '9'; }
static int is_alnum(int c) { return is_alpha(c) || is_digit(c); }

static int peek(Lexer *lx) { return lx->src[lx->pos]; }
static int peek2(Lexer *lx) { return lx->src[lx->pos + 1]; }

static char advance(Lexer *lx) {
    char c = lx->src[lx->pos];
    lx->pos++;
    if (c == '\n') { lx->line++; lx->col = 1; } else { lx->col++; }
    return c;
}

static Token make_tok(Lexer *lx, TokenType t, const char *start, size_t len, int line, int col) {
    (void)lx;
    Token tok;
    tok.type = t;
    tok.start = start;
    tok.len = len;
    tok.line = line;
    tok.col = col;
    return tok;
}

void lexer_init(Lexer *lx, const char *src) {
    lx->src = src;
    lx->pos = 0;
    lx->line = 1;
    lx->col = 1;
}

static void skip_ws_and_comments(Lexer *lx) {
    for (;;) {
        char c = peek(lx);
        if (c == ' ' || c == '\t' || c == '\r') { advance(lx); continue; }
        if (c == '/') {
            if (peek2(lx) == '/') {           /* // до конца строки */
                while (peek(lx) && peek(lx) != '\n') advance(lx);
                continue;
            }
            if (peek2(lx) == '*') {           /* блочный комментарий */
                advance(lx); advance(lx);
                while (peek(lx)) {
                    if (peek(lx) == '*' && peek2(lx) == '/') { advance(lx); advance(lx); break; }
                    advance(lx);
                }
                continue;
            }
        }
        break;
    }
}

static Token lex_keyword_or_ident(Lexer *lx) {
    int line = lx->line, col = lx->col;
    const char *start = lx->src + lx->pos;
    size_t len = 0;
    while (is_alnum(peek(lx))) { advance(lx); len++; }

    static const struct { const char *kw; TokenType t; } kws[] = {
        {"agent",   TOK_KW_AGENT},
        {"harness", TOK_KW_HARNESS},
        {"tool",    TOK_KW_TOOL},
        {"model",   TOK_KW_MODEL},
        {"memory",  TOK_KW_MEMORY},
        {"on",      TOK_KW_ON},
        {"run",     TOK_KW_RUN},
        {"cron",    TOK_KW_CRON},
        {"allow",   TOK_KW_ALLOW},
        {"true",    TOK_KW_TRUE},
        {"false",   TOK_KW_FALSE},
    };
    for (size_t i = 0; i < sizeof(kws) / sizeof(kws[0]); i++) {
        if (strlen(kws[i].kw) == len && strncmp(start, kws[i].kw, len) == 0) {
            return make_tok(lx, kws[i].t, start, len, line, col);
        }
    }
    return make_tok(lx, TOK_IDENT, start, len, line, col);
}

static Token lex_number(Lexer *lx) {
    int line = lx->line, col = lx->col;
    const char *start = lx->src + lx->pos;
    size_t len = 0;
    while (is_digit(peek(lx))) { advance(lx); len++; }
    if (peek(lx) == '.' && is_digit(peek2(lx))) {
        advance(lx); len++;
        while (is_digit(peek(lx))) { advance(lx); len++; }
    }
    return make_tok(lx, TOK_NUMBER, start, len, line, col);
}

static Token lex_string(Lexer *lx) {
    int line = lx->line, col = lx->col;
    advance(lx); /* открывающая кавычка */
    const char *start = lx->src + lx->pos;
    size_t len = 0;
    while (peek(lx) && peek(lx) != '"') {
        if (peek(lx) == '\\' && peek2(lx)) advance(lx); /* пропускаем escape */
        advance(lx); len++;
    }
    if (peek(lx) == '"') advance(lx); /* закрывающая кавычка */
    return make_tok(lx, TOK_STRING, start, len, line, col);
}

Token lexer_next(Lexer *lx) {
    skip_ws_and_comments(lx);
    char c = peek(lx);
    int line = lx->line, col = lx->col;
    const char *start = lx->src + lx->pos;

    if (c == '\0') return make_tok(lx, TOK_EOF, start, 0, line, col);
    if (c == '\n') { advance(lx); return make_tok(lx, TOK_NEWLINE, start, 1, line, col); }
    if (c == '{') { advance(lx); return make_tok(lx, TOK_LBRACE, start, 1, line, col); }
    if (c == '}') { advance(lx); return make_tok(lx, TOK_RBRACE, start, 1, line, col); }
    if (c == '(') { advance(lx); return make_tok(lx, TOK_LPAREN, start, 1, line, col); }
    if (c == ')') { advance(lx); return make_tok(lx, TOK_RPAREN, start, 1, line, col); }
    if (c == '[') { advance(lx); return make_tok(lx, TOK_LBRACKET, start, 1, line, col); }
    if (c == ']') { advance(lx); return make_tok(lx, TOK_RBRACKET, start, 1, line, col); }
    if (c == '=') { advance(lx); return make_tok(lx, TOK_ASSIGN, start, 1, line, col); }
    if (c == ',') { advance(lx); return make_tok(lx, TOK_COMMA, start, 1, line, col); }
    if (c == ':') { advance(lx); return make_tok(lx, TOK_COLON, start, 1, line, col); }
    if (c == '.') { advance(lx); return make_tok(lx, TOK_DOT, start, 1, line, col); }
    if (c == '"') return lex_string(lx);
    if (is_digit(c)) return lex_number(lx);
    if (is_alpha(c)) return lex_keyword_or_ident(lx);

    /* неизвестный символ — возвращаем как однобуквенный IDENT-подобный токен,
       чтобы парсер мог дать внятную ошибку, а не молчать */
    advance(lx);
    return make_tok(lx, TOK_IDENT, start, 1, line, col);
}

const char *token_name(TokenType t) {
    switch (t) {
        case TOK_EOF: return "EOF";
        case TOK_IDENT: return "ident";
        case TOK_STRING: return "string";
        case TOK_NUMBER: return "number";
        case TOK_LBRACE: return "{";
        case TOK_RBRACE: return "}";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_ASSIGN: return "=";
        case TOK_COMMA: return ",";
        case TOK_COLON: return ":";
        case TOK_DOT: return ".";
        case TOK_NEWLINE: return "newline";
        case TOK_KW_AGENT: return "agent";
        case TOK_KW_HARNESS: return "harness";
        case TOK_KW_TOOL: return "tool";
        case TOK_KW_MODEL: return "model";
        case TOK_KW_MEMORY: return "memory";
        case TOK_KW_ON: return "on";
        case TOK_KW_RUN: return "run";
        case TOK_KW_CRON: return "cron";
        case TOK_KW_ALLOW: return "allow";
        case TOK_KW_TRUE: return "true";
        case TOK_KW_FALSE: return "false";
    }
    return "?";
}
