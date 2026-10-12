#ifndef DIALECT_PARSER_H
#define DIALECT_PARSER_H

#include "lexer.h"
#include <stddef.h>

/* AST для v0.1: один агент (плюс, позже, harness). */
typedef struct {
    char *name;

    char *model;             /* строка модели или NULL */
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

/* Результат разбора. */
typedef struct {
    Agent *agent;     /* NULL, если агент не встречен */
    Harness *harness; /* NULL, если harness не встречен */
    char *error;      /* NULL, если ошибок нет */
} ParseResult;

/* Парсит весь исходник (одну или несколько деклараций).
   Возвращённые структуры выделены в куче; ParseResult и Agent можно освободить
   через parse_result_free(). */
ParseResult parse(const char *src);

void parse_result_free(ParseResult *r);

#endif
