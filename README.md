# xacc

A experimental C programming language compiler

I developed it for some learning purpose, but it haven't been used in any formal project.

WARNING! Please expect breaking changes and unstable APIs. Most of them are currently at an early, experimental stage.

## Update

* 2020/10/29 Made the first commitment
  * Finished Lexer and Parser.
* 2020/11/04 Released xacc v0.1.0.
  * Finished generator.
  * Added macro support in lexer.
* 2020/11/05 Released xacc v0.2.0.
  * Finished Analyzer, Allocator and Gen_x86.
* 2020/11/08 Updated to xacc v0.2.3.
  * Fixed multidimensional array parse.
  *  Add keyword `default` in switch statement.
  * Added var declaration in initialize of `for` statement.
* 2020/11/12 Released xacc v0.3.0.
  * Fixed `AddressTaken` of `EXP_VARREF`
  * Add `StringClone` method
  * Add parser
* 2020/11/13 Updated to xacc v0.3.1.
  * Added multiple var declaration.
* 2020/11/14 Released xacc v0.3.2.
  * Fixed generation of global variable initialization.
  * Fixed useless `mov` after allocating real register numbers.
  * Fixed `StringClone` bugs.
  * Fixed generation of `IR_IMM` with a negative number.

### EBNF

```ebnf
prog    :   { dcl ';'  |  func }
dcl     :   lvar_decl
        |   [ extern ] type id '(' parm_types ')' { ',' id '(' parm_types ')' }
        |   [ extern ] void id '(' parm_types ')' { ',' id '(' parm_types ')' }
lvar_decl:  type var_decl { ',' var_decl }
var_decl:   id [ '[' intcon ']' ] [ '=' expr ]
type    :   char
        |   int
        |   type '*'
parm_types  :   void
            |   type id [ '[' ']' ] { ',' type id [ '[' ']' ] }
func    :   type id '(' parm_types ')' '{' { lvar_decl ';' } { stmt } '}'
        |   void id '(' parm_types ')' '{' { lvar_decl ';' } { stmt } '}'
stmt    :   if '(' expr ')' stmt [ else stmt ]
        |   while '(' expr ')' stmt
        |   for '(' [ expr | lvar_decl ] ';' [ expr ] ';' [ expr ] ')' stmt
        |   switch '(' expr ')' '{' stmt '}'
        |   case expr ':' stmt
        |   return [ expr ] ';'
        |   expr ';'
        |   id '(' [expr { ',' expr } ] ')' ';'
        |   '{' { stmt } '}'
        |   ';'
expr    :   id [ '[' expr ']' ] = expr
        |   unop expr
        |   expr binop expr
        |   expr binop '=' expr
        |   expr relop expr
        |   expr logical_op expr
        |   expr '?' expr ':' expr
        |   id [ '(' [expr { ',' expr } ] ')' | '[' expr ']' ]
        |   '(' expr ')'
        |   intcon
        |   charcon
        |   stringcon
unop    :   &
        |   *
        |   !
        |   *
        |   ~
        |   -
binop   :   +
        |   –
        |   *
        |   /
        |   %
relop   :   ==
        |   !=
        |   <=
        |   <
        |   >=
        |   >
logical_op  :   &&
            |   ||
```

## About

Contact me: E-mail: gz@oasis.run, QQ: 963796543, WebSite: [http://www.oasis.run](http://www.oasis.run)
