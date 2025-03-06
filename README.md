# hammer-interpreter
An interpreter for Hammer, a dynamically-typed, expression-oriented language that is somewhat functional.

It is, at its core, an implementation of CLox, a dynamic, imperative language with OOP features, designed and implemented by Bob Nystrom for his excellent educational book Crafting Interpreters, which can be found at https://github.com/munificent/craftinginterpreters, which also has link to a page of several other implementations just like this one! The book itself can be found at https://craftinginterpreters.com and is an excellent resource.

## Whats different?

In addition, Hammer contains changes such as:
  - Using an AST in combination with the bytecode interpreter to allow for additional optimisations and features, while simplifying both stages overall
  - Makes use of Tail-Call Optimisation
  - A lack of classes, `class` and most other keywords
  - Records akin to Pythons `dict`, and Lists which support adding members
  - An extended set of builtin functions and features such as format strings/printing
  - Near-completely different syntax


# Build
Hammer can be built with `make` using gcc, it has targets for release and debug; for a first build run:
```
$ make all
```
To guarantee the existence of the build directories.

