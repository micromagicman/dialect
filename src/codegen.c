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

void codegen_prologue(FILE *out) {
    fputs("#include <stdio.h>\n\n", out);
    fputs("int main(void) {\n", out);
}

void codegen_epilogue(FILE *out) {
    fputs("    return 0;\n}\n", out);
}

void codegen(const Agent *a, FILE *out) {
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
}

void codegen_harness(const Harness *h, FILE *out) {
    fputs("    printf(\"harness: %s\\n\", ", out);
    emit_c_string(out, h->name);
    fputs(");\n", out);

    for (size_t i = 0; i < h->n_gateways; i++) {
        fputs("    printf(\"gateway: %s\\n\", ", out);
        emit_c_string(out, h->gateways[i]);
        fputs(");\n", out);
    }
    for (size_t i = 0; i < h->n_allow_users; i++) {
        fputs("    printf(\"allow.user: %s\\n\", ", out);
        emit_c_string(out, h->allow_users[i]);
        fputs(");\n", out);
    }
    for (size_t i = 0; i < h->n_allow_chats; i++) {
        fputs("    printf(\"allow.chat: %s\\n\", ", out);
        emit_c_string(out, h->allow_chats[i]);
        fputs(");\n", out);
    }
    for (size_t i = 0; i < h->n_crons; i++) {
        fputs("    printf(\"cron: %s every %s run %s\\n\", ", out);
        emit_c_string(out, h->crons[i].name);
        fputs(", ", out);
        emit_c_string(out, h->crons[i].schedule);
        fputs(", ", out);
        emit_c_string(out, h->crons[i].task);
        fputs(");\n", out);
    }
}
