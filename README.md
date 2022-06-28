============================|
Model SQL Interpreter Readme|
============================|


How to make it work:
====================

Compile program with with make utilite: 
    use Makefile-reg for regular run, 
    use Makefile-regsan for regular run with sanitizer,
    use Makefile-test for from tester.sh.

Run ./ModelSQLserver

Insert SQL expression one after another. The programm will reply to each of them 
demonstrating up to date table or giving diagnostics.


Testing script:
===============

To run tests from the directory of tester.sh:

bash tester.sh <path to compiled prog> <path to directory with .dat and .ans>

Noticed puculiarities:
======================

If there are invalid field names requested to SELECT the program will simply
ignore them and will not give any diagnostics.

If there are less atributes than expected to INSERT the program will ignore it 
and fill residuare fields with neutral values. The diagnostics will not be
given.

TODO  programm notes:
=====================

tableint.cpp:
    -print() : level off the output
    -select() : change order (in cycle go through user's list instead of table
    fields list)
    -get_type: mb return type and no close/open table
    -separate tablint.hpp and table.h 
interpreter.cpp/tableint.cpp
    -change poliz form frim two std::vectors to one vector of structs

Something about the program:
============================

This project is a realization of Model SQL Interpreter built on client-server
architecture. (practikum 4th semester cmc msu assignment)

The program can be logically diveded into parts
    -the CLIENT takes user requets from std::in and sends them to the SERVER 
    -the SERVER recieves the message and gives it to  INTERPRETER for processing        
    -the INTERPRETER consists of the LEXER and the PARSER
    -the LEXER anylizes regular language of lexemes providing the parser with lexeme sequence 
    -the PARSER analizes syntax language with recoursive descent method. In process it calls
        appropriate functions of the DATA BASE INTERFACE to execute SQL commands given.
    -the DATA BASE INTERFACE implements it's short functions as sequences of
        calls of C library table.h.
    -table.h is responsible for internal form of sql data base.

========================
The project is done by: ================================
    Evgeny Gurov, Egor Zadorin, Arkadiy Vladimirov. 2020.
