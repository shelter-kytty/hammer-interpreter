
The Compiler
==

The Hammer Compiler is a fairly simple AST-to-bytecode conversion, most of the legwork in sorting and checking
expressions for 'immediate' consistency has already been done by the AST, so the compiler can focus on optimising
this conversion step and doing more specific checks. This stage is still largely inspired by the CLox compiler, and
there are more than a few familiar sections, but there should be plenty of fresh faces.


Optimisations
--

Most of the compile-time optimisations consist of some extremely basic constant-folds. All unnested arithmetic 
operations are done at compile-time, along with unnested string-concatenation (only on unformatted strings, 
however), and that's basically it. There is also branch-picking for if-statements, but because I didn't want
the compiler to accidentally optimise side-effects away, it's only for constant, non-printing expressions, and 
no blocks either. This step also causes the compiler to ignore assignments inside of the pivot, which is fully
intentional; checking for and throwing errors about improper assignment targets and locations is difficult, so 
I take it where I can.

Alongside these are the objectively better inlines, sadly not for functions, but it does inline integers, floats
that end in '.0', and characters, when they fit within the '17-bit limit' i.e. -UINT16_MAX to +UINT16_MAX. It emits
the opcode, the upper 8-bits, and lower 8-bits, sequentially, which are then read at runtime with READ_SHORT(). The
reason I say 17-bits is because the sign of the integer is detected at compile-time, and the magnitude is passed, with
the opcode differing based on the sign; either OP_T_N for negatives, or OP_T_P for positives, I would like to specify
and implement a UQ8.8 format for floating-point inlines, but it seems rather difficult; GCC doesn't provide a native
16-bit fixed-point format, the smallest of it's floats being `float` with 32-bits~ depending on the platform, and
without pre-existing implementation the specific mathematical conversion required is unknown to me, alongside the checks
required to ensure the 64-bit double will fit inside of it.

The compiler will also inline range operations that use number literals, which is fairly common; ranges are already 
optimised at runtime to spend as much time inside of C as possible, using a counted while-loop, but it doesn't hurt to
avoid it entirely. The only issue is that modifying this list will modify every copy of it; if you have, for example, a
function that returns a cached list, and use the receives `<<` operator on it, every copy will also contain whatever is
received. This can be avoided by instead making a single-entry list with the value you wish to add inside it and concatenating 
them together, which will instead copy the contents of both lists to a new list which can be bound elsewhere. This behaviour 
is, again, intentional, and it is up to the programmer to avoid cases where this is undesired; the reasoning is expounded 
upon in ` `.

Proper tail calls are also implemented, in a similar fashion to Lua, where call expressions inside a return prefix operation
are instead tail called. In addition to this, implicit tail-calls are implemented for single-expression functions (i.e. functions
that don't have a block `{ }` as their right operand) when that expression is a function call. This is due mostly to laziness, as
this could also be implemented for calls inside `if` and `match`.

Future/potential/unimplemented optimisations include:
    - Recursively folding constants: `1 + 1 + 1` is currently compiled to`OP_INT_P +2; OP_INT_P +1; OP_ADD`, which is better, but not alot better.


Known Issues
--

Currently there are not enough checks within the compiler for where assignment can appear, the system for handling locals 
especially is extremely complicated and somewhat intricate, but with some changes I'm hoping that this can be mitigated 
semantically without complicating things any further. The current trajectory is to remove `( )` for grouping and instead
offer the largely functionally-identical `{ }` syntax used for block expressions, which would prevent users from introducing
bindings without obeying stack semantics. This has the added benefit of clearing-up certain parsing scenarios as `( )` would
no-longer be interpreted as a head-expression. This is the current implementation though there are still some known edge-cases
that need to be resolved.
