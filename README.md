# hammer-interpreter
An interpreter for Hammer, a dynamically-typed, expression-oriented language that is somewhat functional.

The interpreter is, at its core, an implementation of the intepreter for CLox, an imperative language with OOP features and dynamic typing, designed and implemented by Bob Nystrom for his excellent book Crafting Interpreters, which can be found [here](https://github.com/munificent/craftinginterpreters), which links to a page of several other implementations just like this one! [The book itself](https://craftinginterpreters.com) is an excellent resource and one of my favourites.

## Whats different?

In addition, Hammer contains changes such as:
  - Using an AST in combination with the bytecode interpreter to allow for additional optimisations and features, while simplifying both in isolation
  - Makes use of Tail-Call Optimisation
  - A lack of OOP features, namely classes
  - Records akin to Pythons `dict`, and Lists which support adding members
  - An extended set of builtin functions and features such as format strings/printing, custom operators, function composition, cons cells and more
  - Near-completely different syntax, favouring simple operators and semantics over keywords
  - A pivot towards expressions; all expression-statements return "something", loops are expressed through recursion, etc.
  - An extended feature set for implementation, such as support for variadic native functions and for calling non-/native functions with native ones.


## Build
Hammer can be built with `make` using gcc, it has targets for release and debug; for a first build run:
```
$ make all
```
To guarantee the existence of the build directories.

## A note
This project is what really got me into low-level programming and comfortable with languages like C. I'd never written anything to this complexity until this project. HUGE thankyous to Bob Nystrom and the other talented programmers making and publishing their own implementations for others to see. Youre legends!
