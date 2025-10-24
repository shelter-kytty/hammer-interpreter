# Hammer Interpreter
An interpreter for Hammer, a dynamically-typed, expression-oriented language that is somewhat functional.

The interpreter is, at its core, an implementation of the intepreter for CLox, an imperative language with OOP features and dynamic typing, designed and implemented by Bob Nystrom for his excellent book Crafting Interpreters, which can be found [here](https://github.com/munificent/craftinginterpreters), which links to a page of several other implementations just like this one! [The book itself](https://craftinginterpreters.com) is an excellent resource and one of my favourites.

## Whats different?
In addition, Hammer contains changes such as:
  - An AST in combination with the bytecode interpreter, allowing for additional optimisations and features, while simplifying parsing and compiling in isolation
  - Functions in Hammer can Tail-call, a masive optimisation for recursive algorithms
  - A lack of OOP features, namely classes
  - Records and Lists, which are front-end access to the hashtables used by the VM itself and C arrays respectively
  - An extended set of builtin functions and features such as format strings/printing, custom operators, function composition, cons cells and more
  - Near-completely different syntax, favouring simple operators and semantics over keywords
  - A pivot towards expressions; all expression-statements return "something", loops are expressed through recursion, etc.
  - An extended feature set for implementation, such as support for variadic native functions and for calling non-/native functions with native ones.


## Building Hammer
Hammer can be built with `make` if you have gcc in your `$PATH`, it has targets for release and debug; for a first build run:
```
$ make all
```
To guarantee the existence of the build directories.


# A Note
This project is what really got me into low-level programming and comfortable with languages like C. I'd never written anything to this complexity until this project. HUGE thankyous to Bob Nystrom and the other talented programmers making and publishing their own implementations for others to see. Youre legends!


# Project Roadmap
Right now I like the state Hammer is in *as a language*, a syntax and semantics, and I hope to work on it well into the future. What I dislike is the state of its implementation. While its certainly in a largely functional and borderline useful state there is a lot Wrong with it before Im comfortable calling it 'released'; right now it is still a toy, a very complicated one and one that is very dear to me, but a toy.

My main goals are to clean up the project, removing things that *should have been* lost in the transition to a new language and syntax, to get things 'standard'- removing experimental/sloppily-made/rushed features, and generally cleaning up the project in terms of commented-out segments and formatting to reach a 'decent' baseline for 1.0.0.

From there I have a lot in mind; user type/class definitions, an object model encapsulating all basic types, embedding capabilities, including other source files and namespaces, libraries for everything from parsing JSON to interfacing with C/C++ libraries, a serialisation for Hammer object code akin to `.pyc`. But those are just ideas.

## 1.0.0 (in progress)
- [x] Add proper argument checks to NativeFns missing them
- [x] Remove unnecessary token types/features, namely `print`, `put`, `func` and `let`, and their corresponding opcodes
- [ ] Remove the optimising parts of the compiler
- [x] Rework `applyOperator()` NativeFn to be variadic
- [ ] More sensible debug/error messages
- [ ] More consistent/better formatting for control flow, etc.