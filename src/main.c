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

int main(int argc, char **argv) {
    const char *path = "examples/hello.dialect";
    int tokens_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) tokens_only = 1;
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
    if (!r.agent) {
        fprintf(stderr, "dialect: агент не найден\n");
        return 1;
    }

    codegen(r.agent, stdout);
    parse_result_free(&r);
    return 0;
}
