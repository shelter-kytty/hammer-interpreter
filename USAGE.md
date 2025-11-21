# USAGE

```
Usage: hmc [-r?V] [-c FILE] [-i FILE] [-j FILE] [-l SRC] [-o FILENAME]
            [--compile=FILE] [--interpret=FILE] [--json=FILE] [--link=SRC]
            [--ouput=FILENAME] [--repl] [--help] [--usage] [--version]
            
An interpreter for the programming language Hammer.

  -c, --compile=FILE         Compile AST of FILE to binary
  -i, --interpret=FILE       Interpret FILE
  -j, --json=FILE            Output AST of FILE as JSON data
  -l, --link=SRC             Link SRC with compilation unit
  -o, --ouput=FILENAME       Send output to FILENAME instead of stdout
  -r, --repl                 Start a repl session

 SRC is a .o or .json file executed before main unit

  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to https://github.com/shelter-kytty/hammer-interpreter/issues.
```
