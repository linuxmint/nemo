#!/bin/sh

indent --gnu-style --blank-lines-after-procedures --braces-on-if-line --braces-on-struct-decl-line --blank-before-sizeof --cuddle-else --space-after-cast --honour-newlines --indent-level8  --break-before-boolean-operator --continue-at-parentheses --space-after-procedure-calls --procnames-start-lines --start-left-side-of-comments "$@"
