#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "dialect: не могу открыть %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (n > 0) fread(buf, 1, (size_t)n, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void dump_tokens(const char *src) {
    Lexer lx;
    lexer_init(&lx, src);
    Token t;
    do {
        t = lexer_next(&lx);
        printf("  %d:%d  %-8s  '%.*s'\n", t.line, t.col, token_name(t.type),
               (int)t.len, t.start);
    } while (t.type != TOK_EOF);
}

/* генерирует C-код в поток out */
static void emit_c(FILE *out, ParseResult *r) {
    codegen_prologue(out);
    if (r->n_fns > 0) {
        for (size_t i = 0; i < r->n_fns; i++) codegen_fn(&r->fns[i], out);
        codegen_main_open(out);
        codegen_entry(r, out);
    } else {
        codegen_main_open(out);
        if (r->agent) codegen(r->agent, out);
        if (r->harness) codegen_harness(r->harness, out);
    }
    codegen_epilogue(out);
}

int main(int argc, char **argv) {
    const char *path = "examples/hello.dialect";
    const char *output = NULL;
    int tokens_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) tokens_only = 1;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "-c") == 0) { /* только C в stdout (по умолчанию) */ }
        else if (argv[i][0] != '-') path = argv[i];
    }

    char *src = read_file(path);
    if (!src) return 1;

    if (tokens_only) { dump_tokens(src); free(src); return 0; }

    ParseResult r = parse(src);
    free(src);

    if (r.error) {
        fprintf(stderr, "dialect: ошибка разбора: %s\n", r.error);
        return 1;
    }
    if (!r.agent && !r.harness && r.n_fns == 0) {
        fprintf(stderr, "dialect: ни agent, ни harness, ни функций не найдены\n");
        return 1;
    }

    if (output) {
        /* сгенерировать C во временный файл и собрать бинарь системным cc */
        const char *tmp = "/tmp/dialect_out.c";
        FILE *c = fopen(tmp, "w");
        if (!c) { fprintf(stderr, "dialect: не могу создать %s\n", tmp); parse_result_free(&r); return 1; }
        emit_c(c, &r);
        fclose(c);

        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cc -O2 -o %s %s", output, tmp);
        int status = system(cmd);
        remove(tmp);
        parse_result_free(&r);
        if (status != 0) { fprintf(stderr, "dialect: сборка бинаря не удалась\n"); return 1; }
        return 0;
    }

    /* по умолчанию — печатаем C-код в stdout */
    emit_c(stdout, &r);
    parse_result_free(&r);
    return 0;
}
