#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>

#include "common.h"

#include "scanner.h"
#include "vm.h"
#include "ast.h"

static char* readFile(const char* path);

const char* argp_program_version = "hammer v0.1.2-alpha";
const char* argp_program_bug_address = "https://github.com/shelter-kytty/hammer-interpreter/issues";
static char doc[] = "An interpreter for the programming language Hammer.";
static struct argp_option options[] = {
    { "repl", 'r', 0, 0, "Start a repl session", 0 },
    { "interpret", 'i', "FILE", 0, "Interpret FILE", 0 },
    { "json", 'j', "FILE", 0, "Output AST of FILE as JSON data", 0 },
    { "compile", 'c', "FILE", 0, "Compile AST of FILE to binary", 0 },
    { "ouput", 'o', "FILENAME", 0, "Send output to FILENAME instead of stdout", 0},
    { 0 }
};

struct input {
    enum { ERROR_MODE, REPL_MODE, INTERPRET_MODE, JSON_DATA_MODE, COMPILE_MODE  } mode;
    const char* arg;
    int iter;
};

static error_t parse_opt(int key, char *arg, struct argp_state* state) {
    struct input *input = state->input;
    switch (key) {
        case 'r': input->mode = REPL_MODE; break;
        case 'i': input->mode = INTERPRET_MODE; input->arg = arg; break;
        case 'j': input->mode = JSON_DATA_MODE; input->arg = arg; break;
        case 'c': input->mode = COMPILE_MODE; input-> arg = arg; break;
        case ARGP_KEY_ARG: {
            int optn = state->next;
            if (optn == 2) { //non-key option passed, probably interpreting a file
                input->mode = INTERPRET_MODE;
                input->arg = arg;
            } else /*if (optn == 3)*/ {
                // something to check what the option passed was, for now  return an error
                return ARGP_ERR_UNKNOWN;
            }
            break;
        };
        default: return ARGP_ERR_UNKNOWN;
    }

    input->iter++;
    return 0;
}

static struct argp argp = { options, parse_opt, 0, doc, 0, 0, 0 };

int main(int argc, char* argv[])
{
    struct input input;
    input.mode = REPL_MODE; //default running mode ; argp_parse leaves input untouched in the case there are no arguments passed to the program
    input.iter = 0;

    int result = argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &input);

    if (result != 0) {
        fprintf(stderr, "Unknown option\n");
    } else {
        switch (input.mode) {
            case ERROR_MODE: printf("ERROR_MODE\n"); break;
            case REPL_MODE: printf("REPL_MODE\n"); break;
            case INTERPRET_MODE: printf("INTERPRET_MODE\n"); break;
            case JSON_DATA_MODE: printf("JSON_DATA_MODE\n"); break;
            case COMPILE_MODE: printf("COMPILE_MODE\n"); break;
            default:
                fprintf(stderr, "Unknown option\n"); break;
        }
    }

    return 0;
}

// void option_compile() {
    // char* source = readFile();
//
    // serialiseAST(source);
//
    // free(source);
// }

int main_2(int argc, char* argv[])
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

        serialiseAST(source);

        free(source);
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
