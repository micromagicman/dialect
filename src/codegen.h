#ifndef DIALECT_CODEGEN_H
#define DIALECT_CODEGEN_H

#include "parser.h"
#include <stdio.h>

/* Пролог сгенерированной C-программы (include + main {). */
void codegen_prologue(FILE *out);
/* Эпилог (return 0; }). */
void codegen_epilogue(FILE *out);

/* Строки конфигурации агента (внутри main). */
void codegen(const Agent *a, FILE *out);
/* Строки конфигурации harness (внутри main). */
void codegen_harness(const Harness *h, FILE *out);

#endif
