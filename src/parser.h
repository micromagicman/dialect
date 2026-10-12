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

/* Результат разбора. */
typedef struct {
    Agent *agent;   /* NULL, если агент не встречен */
    char *error;    /* NULL, если ошибок нет */
} ParseResult;

/* Парсит весь исходник (одну или несколько деклараций).
   Возвращённые структуры выделены в куче; ParseResult и Agent можно освободить
   через parse_result_free(). */
ParseResult parse(const char *src);

void parse_result_free(ParseResult *r);

#endif
