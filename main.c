#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "generator.h"
#include "analyzer.h"
#include "allocator.h"
#include "gen_x86.h"
#include "ast.h"

int main(int argc, char *argv[]) {
    if (argc == 2) {
        FILE *fp = fopen(argv[1], "r");
        if (fp == NULL) 
            fprintf(stderr, "Failed to open file '%s' for reading", argv[1]);
        fseek(fp, 0, SEEK_END);
        long flen = ftell(fp);
        char *chunk = malloc(flen + 1);
        fseek(fp, 0L, SEEK_SET);
        fread(chunk, flen, 1, fp);
        chunk[flen] = 0;

        Lexer *lexer = NewLexer(argv[1], chunk);
        Parser *parser = NewParser(lexer);
        Program *program = ParseProgram(parser);
        GenProgram(program);
        Optimize(program);
        Analyze(program);
        Allocate(program);
        Genx86(program);
    } else {
        printf("Oops! No input files given.\n");
		printf("xacc 0.3.1 2020.11.13 Copyright (C) 2020 xaxys.\n");
		printf("usage: xacc [file]\n");
    }
}