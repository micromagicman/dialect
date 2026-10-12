#ifndef DIALECT_CODEGEN_H
#define DIALECT_CODEGEN_H

#include "parser.h"
#include <stdio.h>

/* Пролог сгенерированной C-программы (include + helper). */
void codegen_prologue(FILE *out);
/* Открытие main() — после определения функций ядра. */
void codegen_main_open(FILE *out);
/* Эпилог (return 0; }). */
void codegen_epilogue(FILE *out);

/* Строки конфигурации агента (внутри main). */
void codegen(const Agent *a, FILE *out);
/* Строки конфигурации harness (внутри main). */
void codegen_harness(const Harness *h, FILE *out);

/* Ядро языка: генерирует определение функции (вне main). */
void codegen_fn(const Fn *f, FILE *out);
/* Генерирует тело main(): вызывает точку входа. */
void codegen_entry(const ParseResult *r, FILE *out);

#endif
