#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <errno.h>
#include <memory.h>

#include "common.h"

#include "scanner.h"
#include "vm.h"
#include "ast.h"

static char* readFile(const char* path);
void say_error(const char* msg, int n);
char *convertPath(const char *path, const char *ftype);

const char* argp_program_version = "hammer v0.1.3-alpha";
const char* argp_program_bug_address = "https://github.com/shelter-kytty/hammer-interpreter/issues";
static char doc[] = "An interpreter for the programming language Hammer.";
static struct argp_option options[] = {
    { "repl", 'r', 0, 0, "Start a repl session", 0 },
    { "interpret", 'i', "FILE", 0, "Interpret FILE", 0 },
    { "json", 'j', "FILE", 0, "Output AST of FILE as JSON data", 0 },
    { "compile", 'c', "FILE", 0, "Compile AST of FILE to binary", 0 },
    { "ouput", 'o', "FILENAME", 0, "Send output to FILENAME instead of stdout", 0 },
    { "link", 'l', "SRC", 0, "Link SRC with compilation unit", 0 },
    { 0, 0, 0, OPTION_DOC, "SRC is a .o or .json file executed before main unit", 0 },
    { 0 }
};

struct input {
    // working mode
    enum { ERROR_MODE, REPL_MODE, INTERPRET_MODE, JSON_DATA_MODE, COMPILE_MODE  } mode;
    // working file (for certain working modes)
    const char* arg;
    // output path (for when -o is specified)
    const char* output;
    // link paths (for when -l is specified)
    const char *links[256];
    // link count
    int linkn;
};

static error_t parse_opt(int key, char *arg, struct argp_state* state) {
    struct input *input = state->input;
    switch (key) {
        case 'r': input->mode = REPL_MODE; break;
        case 'i': input->mode = INTERPRET_MODE; input->arg = arg; break;
        case 'j': input->mode = JSON_DATA_MODE; input->arg = arg; break;
        case 'c': input->mode = COMPILE_MODE; input->arg = arg; break;
        case 'o': input->output = arg; break;
        case 'l': input->links[input->linkn++] = arg; break;
        case ARGP_KEY_ARG: {
            //non-key option passed, probably interpreting a file
            input->mode = INTERPRET_MODE;
            input->arg = arg;
            break;
        };
        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, 0, doc, 0, 0, 0 };

int main(int argc, char* argv[])
{
    struct input input;
    input.mode = REPL_MODE; //default running mode ; argp_parse leaves input untouched in the case there are no arguments passed to the program
    input.arg = NULL;
    input.output = NULL;
    input.linkn = 0;

    int result = argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &input);

    for (int i = 0; i < input.linkn; i++) {
        printf("%s\n", input.links[i]);
        VM vm; initVM(&vm);

        char *source = readFile(input.links[i]);

        interpretPrecompiled(&vm, source);

        free(source);
        freeVM(&vm);
    }

    if (input.linkn > 0)
            return 0;

    if (result != 0) {
        say_error("Error when parsing args", result);
    } else {
        switch (input.mode) {
            case ERROR_MODE: {
                fprintf(stderr, "Invalid/unknown options");
                break;
            };
            case REPL_MODE: {
                repl();
                break;
            }
            case INTERPRET_MODE: {
                char *source = readFile(input.arg);
                VM vm; initVM(&vm);

                interpret(&vm, source);

                freeVM(&vm);
                free(source);
                break;
            }
            case JSON_DATA_MODE: {
                char *source = readFile(input.arg);

                if (input.output == NULL) {
                    char *path = convertPath(input.arg, ".json");

                    FILE *file = fopen(path, "w");
                    serialiseAST(file, source);

                    fclose(file);
                    free(path);
                } else {
                    FILE *file = fopen(input.output, "w");
                    serialiseAST(file, source);
                    fclose(file);
                }

                free(source);
                break;
            }
            case COMPILE_MODE: {
                char *source = readFile(input.arg);

                if (input.output == NULL) {
                    char *path = convertPath(input.arg, ".o");

                    FILE *file = fopen(path, "w");
                    serialiseAST(file, source);

                    fclose(file);
                    free(path);
                } else {
                    FILE *file = fopen(input.output, "w");
                    serialiseAST(file, source);
                    fclose(file);
                }

                free(source);
                break;
            }
            default:
                fprintf(stderr, "Unknown option\n"); break;
        }
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

void say_error(const char* msg, int n) {
    fprintf(stderr, "%s\n%s\n", msg, strerror(n));
}

// Result must be freed
char *convertPath(const char *path, const char *ftype) {
    size_t extra = strlen(ftype);
    size_t ttl = extra + strlen(path);

    char *new_path = (char*)malloc(ttl + 1);
    memcpy(new_path, path, ttl - extra);
    memcpy(new_path + ttl - extra, ftype, extra);
    new_path[ttl] = '\0';

    return new_path;
}
