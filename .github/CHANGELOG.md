# Change Log

## Hammer v0.2.1-alpha (current)
- Changes:
    - Added install/uninstall targets to make; build release and create a symlink/clean build and remove symlink, respectively.
- Fixes/Minor changes:
    - Moved nativeFn definitions into `c_lib/builtins.c`

## Hammer v0.2.0-alpha
- Changes:
    - Added more CLI options
    - Can now output AST as json with `-j`/`--json`
    - Can now link json AST modules with `-l`/`--link`
    - Can now output relevant outputs to a file with `-o`/`--output`
    - Can now start repl with `-r`/`--repl`
- Fixes/Minor changes
    - Updated CLI to use argparse
    - Small bugfixes here and there.

## Hammer v0.1.0-alpha
Initial version!
