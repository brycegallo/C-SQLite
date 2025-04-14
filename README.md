# C-SQLite
A clone of SQLite written in C
Written by following https://cstack.github.io/db_tutorial/

Basic Representation of Architecture:

[Core]
Interface
↓
SQL Command Processor
↓
    [SQL Compiler] Tokenizer -> Parser -> Code Generator 
↓
Virtual Machine
↓
[Backend]
B-Tree
↓
Pager
↓
OS Interface
