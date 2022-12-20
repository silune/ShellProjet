Implementation of a Bash like shell for a school project at ENS Paris Saclay
Lexer and Parser are not from me

I mostly created :
 - main.c
 - builtin.h
 - job.h
 - the tests

Some Bash builtin feature are missing : here there is only
 - ```cd```
 - ```history```
 - ```job``` (including ```jobs```, ```fg```, ```bg```)

Job implementation is just a try and might be not complete (or even buggy ...)
