#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "scanner.h"
#include "vm.h"
#include "ast.h"

// TODO: Get repl working again
// Idk itd be??? nice??? i guess
// good for debugging some stuff

// currently this setup segfaults. somewhat randomly but often. and has some strange issues
// my guess as to the cause of the issues is:
//      A. Needed values are being collected at the end of interpret(), probably by the GC, which breaks everything on the next loop.   [x]
//      B. The stack isnt being properly cleaned up by OP_RETURN, which puts everything out of alignment.                               [x]
//      C. The program is leaking HUGE amounts of memory (most likely imo), causing it to either stack-smash or completely run out.     [n]

// I made some patches to fix A and B (which did mitigate some issues), however the repl still consistently segfaults
// after about 3 inputs.

// Considering that the stack looks fine, a NULL dereference probably wouldve been caught by something else, and it runs
// perfectly fine at least thrice, to me, it seems the issue is almost definitely to do running out of memory, either being
// unable to allocate any more or the stack is smashing into the heap.

// Potential solulus:
//      1. Dynamically allocate compilers; now i know that sounds pointless but hear me out, currently every compiler that might exist 
//         during compilation is stack allocated. Running out of stack is a lot faster than running out of heap (i think? either way), 
//         it could free up enough space from the stack to prevent these issues occuring.
//      2. Cleaning up the VM (somehow); if at all possible i think that a potential solution could be to clean up as much cruft from the
//         VM as possible. Again im unsure how to but. It could work. Probably could just manually trigger a gc cleanup after execution 
//         but before the next compilation; thats my best idea at least. itll remove all the dangling strings and references and such without
//         affecting the next compilation cycle.
//      3. Mark compiler roots; idea is to, essentially, mark the objects the compiler has access to during compilation. This removes having
//         to turn the GC off during compilation. Only potential issue is actually getting to all of them, I had to add a whole new colour to
//         the GC to get what i have now working and its already pretty memory intensive.
//      4. Heap-allocated local/upvalue references; currently the compilers `locals` and `upvalues` fields are statically allocated, thats 384
//         8-bit integers and 128 pointers (64-bit on my machine), per-compiler. Thats kinda a lot? Especially since nearly none of them ever 
//         get used. The top-level compiler almost never has to worry about locals OR upvalues yet it has 512 32~ bit segments dedicated to them.
//         Heap-allocating them, combined with an active GC during the compilation phase, could be quite beneficial.

// Okay so this entire issue was just because i STILL wasnt reducing vm->frameCount after execution. Omfgggg im just. im glad it was easier than
// i thought. mhm
static void repl() {
    VM vm;
    initVM(&vm);    

    for (;;) {
        char buf[1024];
        printf("\n>>> ");

        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("\n");
            break;
        }

        InterpretResult result = interpret(&vm, buf);

        if (result == INTERPRET_RUNTIME_ERROR) {
            fprintf(stderr, "Runtime error\n");
            break;
        }
    }

    freeVM(&vm);
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

        free(source);

        writeFile(argv[2], "did the thing!");
        printf("%s\n", __FILE__);
    } 
    else if (argc == 2) {
        /* read and execute file without writing anywhere */
        char* source = readFile(argv[1]);
        VM vm; initVM(&vm);

        interpret(&vm, source);

        freeVM(&vm);
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