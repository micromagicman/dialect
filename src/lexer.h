#ifndef DIALECT_LEXER_H
#define DIALECT_LEXER_H

#include <stddef.h>

/* Типы токенов языка Dialect (v0.1). */
typedef enum {
    TOK_EOF = 0,

    /* литералы и имена */
    TOK_IDENT,      /* имя / ключевое слово, разрешается отдельно */
    TOK_STRING,     /* "..." */
    TOK_NUMBER,     /* 123, 0.3 */

    /* скобки и знаки */
    TOK_LBRACE,     /* { */
    TOK_RBRACE,     /* } */
    TOK_LPAREN,     /* ( */
    TOK_RPAREN,     /* ) */
    TOK_LBRACKET,   /* [ */
    TOK_RBRACKET,   /* ] */
    TOK_ASSIGN,     /* = */
    TOK_COMMA,      /* , */
    TOK_COLON,      /* : */
    TOK_DOT,        /* . */
    TOK_NEWLINE,    /* перевод строки (значимый для декларативного синтаксиса) */

    /* ключевые слова */
    TOK_KW_AGENT,
    TOK_KW_HARNESS,
    TOK_KW_TOOL,
    TOK_KW_MODEL,
    TOK_KW_MEMORY,
    TOK_KW_ON,
    TOK_KW_RUN,
    TOK_KW_CRON,
    TOK_KW_ALLOW,
    TOK_KW_GATEWAY,
    TOK_KW_EVERY,
    TOK_KW_USER,
    TOK_KW_CHAT,
    TOK_KW_TRUE,
    TOK_KW_FALSE,
} TokenType;

typedef struct {
    TokenType type;
    const char *start; /* указатель в исходный буфер */
    size_t len;        /* длина лексемы */
    int line;          /* номер строки (с 1) */
    int col;           /* колонка (с 1) */
} Token;

typedef struct {
    const char *src;   /* исходный текст (NUL-terminated) */
    size_t pos;
    int line;
    int col;
} Lexer;

void lexer_init(Lexer *lx, const char *src);
Token lexer_next(Lexer *lx); /* возвращает следующий токен; последующие вызовы = TOK_EOF */

/* имя токена для отладки */
const char *token_name(TokenType t);

#endif
