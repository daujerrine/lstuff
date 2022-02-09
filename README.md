# lstuff

This is a collection of C programs that I have written as lab assignments in the
past. No guarantee is made on the logical soundness, elegance or the 
correctness of these programs, or even if these programs run, but these were
enough to make me pass the assignments.

## Contents:

* `compiler_design/` - Implementations of various parser generators and code
  generators.
  
  * `3_address_code_generation.c` - Generates 3 address code from an operator
    precedence grammar
  
  * `ll1_parser.c` - Generates an ll1 parser from a given context free grammar.
 	Written in a hurry. Horrible implementation of the parsing table and
 	probably the first follow set generators as well. 
  
  * `lr0_parser.c` - Generates an LR parser from a given context free grammar.
    Less horrible implementation of the parsing table using actual sets. Similar
    implementation should have been used in the ll1 parser.

  * `op_precedence_parser.c` - An operator precedence grammar parser.
  
  * `recursive_descent_parser.c` - Brute force recursive descent parser
    generator.
 
  * `shift_reduce_brute_force.c` - Brute force shift reduce parser.

  * `slr1_parser.c` - SLR1 parser. Extended from the LR0 parser code.

* `multithreading/` - Implementation of various problems from the
  [Little Book of Semaphores](https://greenteapress.com/wp/semaphores/) and 
  elsewhere.
  
  * `baboon2.c` - The Baboon Problem. Incorrect solution. (page 177)
  
  * `bankers.c` - The Bankers algorithm.
  
  * `h2o.c` - The H2O problem. (page 143)
  
  * `hilzer.c` - Hilzer's Barbershop problem. (page 133)
  
  * `river.c` - River crossing problem. (page 160)
  
  * `search_insert_delete.c` - Search-Insert-Delete problem. (page 165)
