#include "codegen.h"
#include <string.h>

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

/* Пролог: includes, вспомогательные функции (stdin, JSON-экранирование,
   разбор ответа LLM), открытие main. */
void codegen_prologue(FILE *out) {
    fputs(
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "\n"
        "static char *read_stdin(void) {\n"
        "    size_t cap = 4096, len = 0;\n"
        "    char *buf = malloc(cap);\n"
        "    if (!buf) return NULL;\n"
        "    int c;\n"
        "    while ((c = getchar()) != EOF) {\n"
        "        if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }\n"
        "        buf[len++] = (char)c;\n"
        "    }\n"
        "    buf[len] = '\\0';\n"
        "    return buf;\n"
        "}\n"
        "\n"
        "static void json_escape(FILE *f, const char *s) {\n"
        "    fputc('\"', f);\n"
        "    for (; *s; s++) {\n"
        "        switch (*s) {\n"
        "            case '\"':  fputs(\"\\\\\\\"\", f); break;\n"
        "            case '\\\\': fputs(\"\\\\\\\\\", f); break;\n"
        "            case '\\n': fputs(\"\\\\n\", f); break;\n"
        "            case '\\r': fputs(\"\\\\r\", f); break;\n"
        "            case '\\t': fputs(\"\\\\t\", f); break;\n"
        "            default: fputc(*s, f);\n"
        "        }\n"
        "    }\n"
        "    fputc('\"', f);\n"
        "}\n"
        "\n"
        "/* печатает строковое значение первого \"content\" в JSON-ответе */\n"
        "static void print_first_content(const char *json) {\n"
        "    const char *key = \"\\\"content\\\"\";\n"
        "    const char *p = strstr(json, key);\n"
        "    if (!p) { fputs(\"(нет ответа)\\n\", stdout); return; }\n"
        "    p += strlen(key);\n"
        "    while (*p == ':' || *p == ' ' || *p == '\"') p++;\n"
        "    while (*p && *p != '\"') {\n"
        "        if (*p == '\\\\' && p[1]) {\n"
        "            p++;\n"
        "            switch (*p) {\n"
        "                case 'n': fputc('\\n', stdout); break;\n"
        "                case 't': fputc('\\t', stdout); break;\n"
        "                case 'r': fputc('\\r', stdout); break;\n"
        "                case '\"': fputc('\"', stdout); break;\n"
        "                case '\\\\': fputc('\\\\', stdout); break;\n"
        "                default: fputc(*p, stdout);\n"
        "            }\n"
        "            p++;\n"
        "        } else {\n"
        "            fputc(*p, stdout); p++;\n"
        "        }\n"
        "    }\n"
        "    fputc('\\n', stdout);\n"
        "}\n",
        out);
}

void codegen_main_open(FILE *out) {
    fputs("int main(void) {\n", out);
}

void codegen_epilogue(FILE *out) {
    fputs("    return 0;\n}\n", out);
}

/* Агент: читает сообщение из stdin, строит запрос к LLM, вызывает его и
   печатает ответ. URL и ключ берутся из окружения DIALECT_LLM_URL /
   DIALECT_LLM_KEY. */
void codegen(const Agent *a, FILE *out) {
    fputs("    char *msg = read_stdin();\n"
          "    const char *url = getenv(\"DIALECT_LLM_URL\");\n"
          "    const char *key = getenv(\"DIALECT_LLM_KEY\");\n"
          "    if (!url || !key) {\n"
          "        fprintf(stderr, \"dialect: задайте DIALECT_LLM_URL и DIALECT_LLM_KEY\\n\");\n"
          "        return 1;\n"
          "    }\n", out);

    /* построить JSON-запрос во временный файл */
    fputs("    FILE *req = fopen(\"/tmp/dialect_req.json\", \"w\");\n"
          "    fputs(\"{\\\"model\\\":\", req);\n", out);
    if (a->model) {
        fputs("    json_escape(req, ", out);
        emit_c_string(out, a->model);
        fputs(");\n", out);
    } else {
        fputs("    json_escape(req, getenv(\"DIALECT_LLM_MODEL\") ? getenv(\"DIALECT_LLM_MODEL\") : \"\");\n", out);
    }
    fputs("    fputs(\",\\\"messages\\\":[\", req);\n", out);

    if (a->system) {
        fputs("    fputs(\"{\\\"role\\\":\\\"system\\\",\\\"content\\\":\", req);\n"
              "    json_escape(req, ", out);
        emit_c_string(out, a->system);
        fputs(");\n"
              "    fputs(\"},\", req);\n", out);
    }
    fputs("    fputs(\"{\\\"role\\\":\\\"user\\\",\\\"content\\\":\", req);\n"
          "    json_escape(req, msg);\n"
          "    fputs(\"}]}\", req);\n"
          "    fclose(req);\n", out);

    /* вызвать LLM через curl */
    fputs("    char cmd[8192];\n"
          "    snprintf(cmd, sizeof(cmd),\n"
          "        \"curl -s \\\"%s\\\" -H \\\"Authorization: Bearer %s\\\" -H \\\"Content-Type: application/json\\\" --data-binary @/tmp/dialect_req.json\",\n"
          "        url, key);\n"
          "    FILE *resp = popen(cmd, \"r\");\n"
          "    if (!resp) { fprintf(stderr, \"dialect: не удалось вызвать LLM\\n\"); free(msg); return 1; }\n"
          "    size_t cap = 8192, len = 0;\n"
          "    char *json = malloc(cap);\n"
          "    int c;\n"
          "    while ((c = fgetc(resp)) != EOF) {\n"
          "        if (len + 1 >= cap) { cap *= 2; json = realloc(json, cap); }\n"
          "        json[len++] = (char)c;\n"
          "    }\n"
          "    json[len] = '\\0';\n"
          "    pclose(resp);\n"
          "    print_first_content(json);\n"
          "    free(json);\n"
          "    free(msg);\n", out);
}

/* Harness: на этапе v0.1 печатает разобранную конфигурацию (среда пока
   декларативна, её исполнение — следующий этап). */
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

/* ===== Ядро языка: кодоген выражений/statement'ов/функций ===== */

static const char *ctype(Type t) {
    switch (t) {
        case TYPE_INT: return "long";
        case TYPE_STR: return "char*";
        case TYPE_BOOL: return "int";
        default: return "void";
    }
}

static void codegen_expr(const Expr *e, FILE *out);

static void codegen_block(const Stmt *b, FILE *out);

static void codegen_stmt(const Stmt *s, FILE *out) {
    switch (s->kind) {
        case STMT_LET:
            fprintf(out, "    %s %s", ctype(s->type), s->name);
            if (s->init) { fputs(" = ", out); codegen_expr(s->init, out); }
            fputs(";\n", out);
            break;
        case STMT_ASSIGN:
            fputs("    ", out); fputs(s->name, out);
            fputs(" = ", out); codegen_expr(s->value, out); fputs(";\n", out);
            break;
        case STMT_EXPR:
            fputs("    ", out); codegen_expr(s->expr, out); fputs(";\n", out);
            break;
        case STMT_RETURN:
            fputs("    return", out);
            if (s->value) { fputc(' ', out); codegen_expr(s->value, out); }
            fputs(";\n", out);
            break;
        case STMT_IF:
            fputs("    if (", out); codegen_expr(s->cond, out); fputs(") {\n", out);
            codegen_block(s->then, out);
            fputs("    }", out);
            if (s->else_) { fputs(" else {\n", out); codegen_block(s->else_, out); fputs("    }", out); }
            fputs("\n", out);
            break;
        case STMT_WHILE:
            fputs("    while (", out); codegen_expr(s->cond, out); fputs(") {\n", out);
            codegen_block(s->body, out);
            fputs("    }\n", out);
            break;
        case STMT_BLOCK:
            fputs("    {\n", out);
            codegen_block(s, out);
            fputs("    }\n", out);
            break;
    }
}

static void codegen_block(const Stmt *b, FILE *out) {
    if (b && b->kind == STMT_BLOCK) {
        for (size_t i = 0; i < b->n_stmts; i++) codegen_stmt(b->stmts[i], out);
    } else if (b) {
        codegen_stmt(b, out);
    }
}

static void codegen_expr(const Expr *e, FILE *out) {
    switch (e->kind) {
        case EXPR_NUMBER: fprintf(out, "%ld", e->num); break;
        case EXPR_STRING: emit_c_string(out, e->str); break;
        case EXPR_BOOL: fputs(e->boolean ? "1" : "0", out); break;
        case EXPR_VAR: fputs(e->name, out); break;
        case EXPR_CALL:
            if (strcmp(e->callee, "print") == 0 && e->n_args == 1) {
                const Expr *a = e->args[0];
                if (a->kind == EXPR_STRING) { fputs("printf(\"%s\\n\", ", out); codegen_expr(a, out); }
                else { fputs("printf(\"%ld\\n\", ", out); codegen_expr(a, out); }
                fputs(")", out);
            } else {
                fputs(e->callee, out);
                fputc('(', out);
                for (size_t i = 0; i < e->n_args; i++) { if (i) fputs(", ", out); codegen_expr(e->args[i], out); }
                fputc(')', out);
            }
            break;
        case EXPR_UNARY:
            fputc('(', out);
            fputs(e->op == TOK_NOT ? "!" : "-", out);
            codegen_expr(e->operand, out);
            fputc(')', out);
            break;
        case EXPR_BINARY: {
            const char *op = "?";
            switch (e->op) {
                case TOK_PLUS: op = "+"; break;
                case TOK_MINUS: op = "-"; break;
                case TOK_STAR: op = "*"; break;
                case TOK_SLASH: op = "/"; break;
                case TOK_EQ: op = "=="; break;
                case TOK_NE: op = "!="; break;
                case TOK_LT: op = "<"; break;
                case TOK_GT: op = ">"; break;
                case TOK_LE: op = "<="; break;
                case TOK_GE: op = ">="; break;
                case TOK_AND: op = "&&"; break;
                case TOK_OR: op = "||"; break;
                default: op = "?";
            }
            fputc('(', out);
            codegen_expr(e->left, out);
            fprintf(out, " %s ", op);
            codegen_expr(e->right, out);
            fputc(')', out);
            break;
        }
    }
}

void codegen_fn(const Fn *f, FILE *out) {
    const char *cname = strcmp(f->name, "main") == 0 ? "_dialect_entry" : f->name;
    fprintf(out, "%s %s(", ctype(f->ret_type), cname);
    for (size_t i = 0; i < f->n_params; i++) {
        if (i) fputs(", ", out);
        fprintf(out, "%s %s", ctype(f->param_types[i]), f->params[i]);
    }
    fputs(") {\n", out);
    codegen_block(f->body, out);
    fputs("}\n\n", out);
}

void codegen_entry(const ParseResult *r, FILE *out) {
    if (r->entry) {
        const char *cname = strcmp(r->entry, "main") == 0 ? "_dialect_entry" : r->entry;
        fprintf(out, "    %s();\n", cname);
    }
}
