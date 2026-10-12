#include "codegen.h"

/* Экранирует строку для вставки в C-строковый литерал. */
static void emit_c_string(FILE *out, const char *s) {
    fputc('"', out);
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\t': fputs("\\t", out); break;
            default: fputc(*p, out);
        }
    }
    fputc('"', out);
}

void codegen(const Agent *a, FILE *out) {
    fputs("#include <stdio.h>\n\n", out);
    fputs("int main(void) {\n", out);

    fputs("    printf(\"agent: %s\\n\", ", out);
    emit_c_string(out, a->name);
    fputs(");\n", out);

    if (a->model) {
        fputs("    printf(\"model: %s\\n\", ", out);
        emit_c_string(out, a->model);
        fputs(");\n", out);
    }
    if (a->has_temperature) {
        fprintf(out, "    printf(\"temperature: %g\\n\");\n", a->temperature);
    }
    for (size_t i = 0; i < a->n_tools; i++) {
        fputs("    printf(\"tool: %s\\n\", ", out);
        emit_c_string(out, a->tools[i]);
        fputs(");\n", out);
    }
    if (a->has_memory_persistent) {
        fprintf(out, "    printf(\"memory.persistent: %s\\n\");\n",
                a->memory_persistent ? "true" : "false");
    }

    fputs("    return 0;\n}\n", out);
}
