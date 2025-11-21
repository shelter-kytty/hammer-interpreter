# Hammer Interpreter
An interpreter for Hammer, a dynamically-typed, expression-oriented language that is somewhat functional.

The interpreter is, at its core, an implementation of the intepreter for CLox, an imperative language with OOP features and dynamic typing, designed and implemented by Bob Nystrom for his excellent book Crafting Interpreters, which can be found [here](https://github.com/munificent/craftinginterpreters), which links to a page of several other implementations just like this one! [The book itself](https://craftinginterpreters.com) is an excellent resource and one of my favourites.

---

## Building Hammer
Hammer can be built with `make` using `gcc` on linux. The main program follows C11 for decent compatibility.

To download and build Hammer, run the following:

```
$ git clone https://github.com/shelter-kytty/hammer-interpreter.git <dir>
$ cd <dir>
$ make prep
$ make release
```

This will leave `hmc` in the release subdirectory of build.

You can also run the debug target for additional information during compilation and execution, which can be tuned with the debug flags found in `common.h`. Other options that alter the interpreter's behaviour are available there.


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

## Notes
This project is what really got me into low-level programming and comfortable with languages like C. I'd never written anything to this complexity until this project. HUGE thankyous to Bob Nystrom and the other talented programmers making and publishing their own implementations for others to see. Youre legends!

Special thankyous and acknowledgements to:
- Dave Gamble, Max Bruckner, Alan Wang and all the contributors working on [cJSON](https://github.com/DaveGamble/cJSON), used in deserialising

---

## v0.1.0
Initial version!
Hammer is now in a state where Im confident about working on it further, adding more complex features and and libraries.

## v0.2.0
- Output AST as JSON
  - Expand CLI parsing to this end
- Read JSON AST from file to be run

## v0.3.0 (working on)
- Separate nativeFNs out into libraries and add `include` imperatives/similar to include them in projects
  - Implement namespaces/objects and syntax for them to this end
  - Add the ability to `include` other hammer source files
- Simple JSON library

## v0.4.0
- Classes/Object types
