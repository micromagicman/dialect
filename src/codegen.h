#ifndef DIALECT_CODEGEN_H
#define DIALECT_CODEGEN_H

#include "parser.h"
#include <stdio.h>

/* Генерирует C-код для агента в поток out.
   На этапе v0.1 это «привет, мир»: программа печатает разобранную конфигурацию,
   чтобы проверить пайплайн лексер→парсер→кодоген→нативный бинарь end-to-end. */
void codegen(const Agent *a, FILE *out);

#endif
