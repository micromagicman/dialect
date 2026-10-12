#ifndef DIALECT_PARSER_H
#define DIALECT_PARSER_H

#include "lexer.h"
#include <stddef.h>

/* AST для v0.1: один агент (плюс, позже, harness). */
typedef struct {
    char *name;

    char *model;             /* строка модели или NULL */
    char *system;            /* системный промпт или NULL */
    int has_temperature;     /* задан ли temperature */
    double temperature;

    char **tools;            /* список имён тулов */
    size_t n_tools;

    int has_memory_persistent; /* задан ли memory persistent */
    int memory_persistent;
} Agent;

/* Периодическая задача в harness. */
typedef struct {
    char *name;      /* имя задачи */
    char *schedule;  /* cron-расписание, строка */
    char *task;      /* имя функции, которую запускать */
} CronJob;

/* AST для блока harness: среда агента (платформы, доступ, расписание). */
typedef struct {
    char *name;

    char **gateways;    /* список платформ (telegram, web, ...) */
    size_t n_gateways;

    char **allow_users; /* id доверенных пользователей */
    size_t n_allow_users;
    char **allow_chats; /* id доверенных чатов */
    size_t n_allow_chats;

    CronJob *crons;     /* периодические задачи */
    size_t n_crons;
} Harness;

/* Типы ядра языка. */
typedef enum { TYPE_INT, TYPE_STR, TYPE_BOOL, TYPE_VOID } Type;

/* Выражения. */
typedef enum {
    EXPR_NUMBER, EXPR_STRING, EXPR_BOOL, EXPR_VAR, EXPR_CALL, EXPR_UNARY, EXPR_BINARY
} ExprKind;

typedef struct Expr Expr;
struct Expr {
    ExprKind kind;
    long num;              /* EXPR_NUMBER */
    char *str;             /* EXPR_STRING */
    int boolean;           /* EXPR_BOOL */
    char *name;            /* EXPR_VAR */
    char *callee;          /* EXPR_CALL */
    Expr **args; size_t n_args; /* EXPR_CALL */
    TokenType op;          /* EXPR_UNARY / EXPR_BINARY */
    Expr *operand;         /* EXPR_UNARY */
    Expr *left, *right;    /* EXPR_BINARY */
};

/* Statement'ы. */
typedef enum {
    STMT_LET, STMT_ASSIGN, STMT_EXPR, STMT_RETURN, STMT_IF, STMT_WHILE, STMT_BLOCK
} StmtKind;

typedef struct Stmt Stmt;
struct Stmt {
    StmtKind kind;
    char *name;            /* LET / ASSIGN */
    Type type;             /* LET */
    Expr *init;            /* LET */
    Expr *value;           /* ASSIGN / RETURN */
    Expr *expr;            /* EXPR */
    Expr *cond;            /* IF / WHILE */
    Stmt *then, *else_;    /* IF */
    Stmt *body;            /* WHILE */
    Stmt **stmts; size_t n_stmts; /* BLOCK */
};

/* Функция. */
typedef struct {
    char *name;
    char **params; size_t n_params;
    Type *param_types;
    Type ret_type;
    Stmt *body;            /* блок */
} Fn;

/* Результат разбора. */
typedef struct {
    Agent *agent;     /* NULL, если агент не встречен */
    Harness *harness; /* NULL, если harness не встречен */
    char *error;      /* NULL, если ошибок нет */

    /* ядро языка (этап 1): объявления и функции верхнего уровня */
    Fn *fns;          /* список функций */
    size_t n_fns;
    char *entry;      /* имя функции-точки входа (fn main), или NULL */
} ParseResult;

/* Парсит весь исходник (одну или несколько деклараций).
   Возвращённые структуры выделены в куче; ParseResult и Agent можно освободить
   через parse_result_free(). */
ParseResult parse(const char *src);

void parse_result_free(ParseResult *r);

#endif
