#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "scanner.h"
#include "vm.h"
#include "ast.h"

// TODO: Add support for upvalues from greater scopes
// Need to make it so upvalues are properly tracked and can be captured by 
// successive functions closing over the required values before passing them on
// to their closures at runtime
// TODO: Get repl working again
// Idk itd be??? nice??? i guess
// good for debugging some stuff


static void repl() {
    //VM vm;
    //initVM(&vm);    

    for (;;) {
        char buf[1024];
        printf("\n>>> ");

        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("\n");
            break;
        }

        //interpret(&vm, buf);
    }

    //freeVM(&vm);
}

static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "Could not open file at '%s'", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);

    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read '%s'", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

    if (bytesRead < fileSize) {
        fprintf(stderr, "Error encountered while reading from '%s'", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';
    fclose(file);

    return buffer;
}

static void writeFile(const char* path, const char* data) {
    size_t length = strlen(path);
    char* newPath = (char*)malloc((strlen(path) * sizeof(char)) + (3 * sizeof(char)));

    memcpy(newPath, path, length);
    memcpy(newPath + length, ".ml", 3);

    FILE* file = fopen(newPath, "w");

    if (file == NULL) {
        fprintf(stderr, "Could not create/write to file '%s'", newPath);
        exit(74);
    }

    fprintf(file, data);

    printf("Successfully wrote to '%s'", newPath);
    
    fclose(file);
    free(newPath);
}

int main(int argc, char* argv[])
{
    if (argc == 1) {
        repl();
    } 
    else if (memcmp(argv[1], "--help", 6) == 0) {
        printf("Usage:\n");
        printf("\thmc\t\t: Initiate a repl\n");
        printf("\thmc [path]\t: Interpret the source file at [path]\n");
        printf("\thmc -c [path] : Compile source file at [path] and output to '[fileName].ml' in [path] dir\n");
        printf("\thmc --help\t: print this dialogue\n");
    }
    else if (argc == 3 && memcmp(argv[1], "-c", 2) == 0) {
        /* put a .ml file into same dir as [path] */
        char* source = readFile(argv[2]);

        //debugAST(source);

        free(source);

        writeFile(argv[2], "did the thing!");
        printf("%s\n", __FILE__);
    } 
    else if (argc == 2) {
        /* read and execute file without writing anywhere */
        char* source = readFile(argv[1]);
        
        //debugScanner(source);
        //debugAST(source);
        interpretTEST(source);

        free(source);
    } 
    else {
        printf("Usage:\n");
        printf("\thmc\t\t: Initiate a repl\n");
        printf("\thmc [path]\t: Interpret the source file at [path]\n");
        printf("\thmc -c [path]\t: Compile source file at [path] and output bytecode to '[fileName].ml' in [path] dir\n");
        printf("\thmc --help\t: print this dialogue\n");
    }

    return 0;
}