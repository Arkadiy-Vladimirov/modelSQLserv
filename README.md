
# Model SQL Interpreter Readme  

## Requirements 
- Linux system, to run the program at;
- gcc, make, bash to compile and test the program.

## Build
Compile program with with make utilite, use:  
```
$ make -f Makefile-reg  
```
for regular build; 
```
$ make -f Makefile-regsan  
```
for build with sanitizer;
```
$ make -f Makefile-test  
```
for testing build.

## Run
Type in build directory:
```
$ ./ModelSQLserver
```

Insert SQL expression one after another. The programm will reply to each of them 
demonstrating up to date table or giving diagnostics.


## Testing script:
The directory contains a testing script just to make sure that everything works fine.
To run tests you need to compile test build first
```
$ make -f Makefile-test  
```
and run test script
```
$ bash tester.sh ./ModelSQLserver ./tests/tests_ok 
```
to look how the program responds to correct sequence of requests.

Also you may run:
```
$ bash tester.sh ./ModelSQLserver ./tests/tests_err
```
to check that the interpreter handles inadequate requests properly.

## Clean-up
If you need the executable program no more, run:
```
$ make -f Makefile-{one that was used, e.g. regsan} clean
```
to clean up your directory.

# About the program:

This project is a realization of Model SQL Interpreter built on client-server
architecture. (practicum 4th semester cmc msu assignment)

The program can be logically divided into parts:
- the CLIENT takes user requets from std::in and sends them to the SERVER 
- the SERVER recieves the message and gives it to the INTERPRETER for processing        
- the INTERPRETER consists of the LEXER and the PARSER
    - the LEXER analyses regular language of lexemes providing the parser with lexeme sequence 
    - the PARSER analyses syntax language with recoursive descent method. In process it calls corresponding functions at the DATA BASE INTERFACE to execute SQL commands given.
- the DATA BASE INTERFACE implements its  functions as sequences of calls of C library `table.h`.
- `table.h` is responsible for internal form of sql data base.

# Other
## Noticed puculiarities:


If there are invalid field names requested to SELECT the program will simply
ignore them and will not give any diagnostics.

If there are less atributes than expected to INSERT the program will ignore it 
and fill residual fields with neutral values. The diagnostics will not be
given.

## TODO notes:


in tableint.cpp:
- print() : level off the output
- select() : change order (in cycle go through user's list instead of table fields list)
- get_type: mb return type and no close/open table  

other:
- separate tablint.hpp and table.h interpreter.cpp/tableint.cpp
- change poliz form from two std::vectors to one vector of structs

____
### The project is done by: 
    Evgeny Gurov, Egor Zadorin, Arkadiy Vladimirov. 2020.
