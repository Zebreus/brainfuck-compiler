grammar brainfuck;

file
    : statements EOF
    ;

statements
    : statement*
    ;

block
    : Forward statements Backward
    ;

statement
    : Left
    | Right
    | Increment
    | Decrement
    | Write
    | Read
    | block
    ;

Left
    : '>'
    ;

Right
    : '<'
    ;

Increment
    : '+'
    ;

Decrement
    : '-'
    ;

Write
    : '.'
    ;

Read
    : ','
    ;

WS: [ \n\t\r]+ -> skip;

Comment
  :  ~( [\r] | [\n] | [\t] | [ ] | '<' | '>' | '-' | '+' | '.' | ',' | '[' | ']' )~( [\r] | [\n] )* -> skip
  ;

Forward
    : '['
    ;

Backward
    : ']'
    ;