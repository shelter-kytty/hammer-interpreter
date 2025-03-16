#ifndef common_h_hammer
#define common_h_hammer

#include <stddef.h>
#include <stdint.h>

typedef enum { false, true } bool;
typedef struct Compiler Compiler;
typedef struct VM VM;

typedef enum {
    MEM_BLACK,
    MEM_GREY,
    MEM_WHITE,
} Colour;

#define FRAME_MAX 384
#define STACK_SIZE 256 * FRAME_MAX

// DEBUGGING ------------------->
#ifdef DEBUG
/* Compilation info */
//#define DEBUG_DISPLAY_TOKENS
//#define DEBUG_DISPLAY_AST
//#define DEBUG_COMPILER_PROGRESS

/* Runtime info */
#define DEBUG_DISPLAY_PROGRAM
//#define DEBUG_DISPLAY_INSTRUCTIONS
//#define DEBUG_DISPLAY_STACK
//#define DEBUG_DISPLAY_TABLES
//#define DEBUG_DISPLAY_STRINGS

/* Memory and Garbage Collector debug options & info */
//#define DEBUG_LOG_MEMORY
//#define DEBUG_LOG_GC
#define DEBUG_STRESS_GC

/* Miscellaneous info */
//#define DEBUG_SCOPE_UPDATES
//#define DEBUG_CHUNK_UPDATES
//#define DEBUG_STRING_DETAILS
#endif
// <------------------- DEBUGGING

// USER OPTIONS ---------------->
/* Character sets/formats */
#define OPTION_ASCII_ONLY
#define OPTION_STRICT_ASCII

/* Performance/appearance */
#define OPTION_DETAILED_PRINTING
//#define OPTION_RECURSIVE_TRUTHINESS
#define OPTION_RECURSIVE_PRINTING

/* Lists/maps */
#define OPTION_ONE_INDEXED
// <---------------- USER OPTIONS

#define simple static inline

#endif
