
AST
==

The Abstract Syntax Tree is probably the most, if not the only, unique part of Hammer beyond it's syntax and semantics, it
is a simple IR between tokenisation and bytecode meant to connect expressions together, capable of (but not having to use)
arbitrary lookahead and more complex syntactical analysis and optimisation than that of the 'stock' CLox compiler. The main
benefit is being able to manipulate expressions _in situ_ and thus provide various optimisations and features without having
to even define new operations for them, so long as the can be expressed in terms of existing features.

The example I am simultaneously most and least proud of is partial application; when calling a function, an argument can be
replaced with `_` to instead return a function with that parameter still unfulfilled. The way this is implemented is that
the AST phase scans for this inside `call()` and reassembles the function call to be a lambda expression that applys it's
arguments to the call in-place of the `_` arguments. These arguments are generated dynamically in such a way as to
guarantee the identifiers are distinct from anything the user can make (assuming everything else is working) to avoid name
collisions. The ability to implement partial application in languages with first-class functions, anonymous functions, and
closures is already well-known, and I have not invented this general idea for the implementation, it's largely borrowed from/
stolen from/inspired by Gleam's own approach, which is functionally identical to my understanding, but I'm still proud of it 
because I have no idea how Gleam's system works, I came up with my specific implementation independently in that regard.


Optimisations
--

There are currently no specific optimisations made in the AST phase of compilation; in fact, in terms of what the compiler
itself is doing, the AST is incredibly wasteful.


Known Issues
--

There aren't any major issues in the AST that I am aware of other than a possible lack of bounds checking or NULL guards in
some areas.