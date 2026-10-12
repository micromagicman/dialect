# Dialect

A programming language for describing AI agents and their harnesses (execution environments).

Dialect is declarative: an agent and its environment are described as text rather than assembled through imperative code. It compiles to native code (no virtual machine at runtime).

## Idea

Two parts in one language:

- **core** — Turing-complete (types, functions, structs, loops, I/O) — sufficient to write Dialect itself in Dialect;
- **agent layer** — `agent` (behavior) and `harness` (environment: platforms, tools, memory, scheduling) blocks.

## Self-hosting goal

The first version of the compiler is written in C. The second version of the compiler should be written in Dialect itself and built by the first version.

## Status

**v0.1 — compiler skeleton.** An end-to-end pipeline is implemented:

`lexer → parser → AST → C codegen → gcc → native binary`

The current (minimal) grammar recognizes an `agent` block with fields (`model`, arbitrary `name = value` fields, `tool`, `memory`). The codegen currently emits a "hello world" — a program that prints the parsed configuration — to verify the pipeline end-to-end.

## Build

```sh
make            # build the compiler (dialect)
make compile    # .dialect → C → native binary → run
make tokens     # lexer debugging
```

## Example

```dialect
agent Support {
    model = "gpt-4o"
    temperature = 0.3

    tool web_search
    tool file_read

    memory persistent = true
}
```

## Structure

```
src/lexer.c    — lexical analyzer
src/parser.c   — recursive descent, AST
src/codegen.c  — C code generation
examples/      — Dialect examples
```

## Roadmap

1. Language core: types, expressions, variables, functions, control flow.
2. Full `harness` block.
3. Agent semantics.
4. LLVM backend instead of the C layer.
5. Self-hosting: the compiler in Dialect.
