/*
 * =============================================================================
 * Algorithm (For Left Recursive Production Removal)
 * =============================================================================
 *
 * 1. Detection of Left Recursive Grammars
 *    ------------------------------------
 *
 * A left recursive grammar for a given nonterminal (say 'A') has one or more
 * production rules in the form:
 *
 * A -> Ax
 *
 * where x is a string of terminals and nonterminals.
 *
 * We may simply check the first symbol of each production in the given
 * production set of a nonterminal to find out which nonterminals are left
 * recursive.
 *
 * 2. Removal of Left Recursive Grammars
 *    ----------------------------------
 *
 * Given a nonterminal 'A' that is left recursive it will have at least the
 * productions in the following form:
 *
 * A -> Ax
 * A -> b
 *
 * where 'x' is a set of terminals and nonterminals, and 'b' is a terminal or a
 * production that is not left recursive.
 *
 * These are then transformed to a right recursive grammar as follows:
 *
 * A -> bZ
 * Z -> xZ
 * Z -> (epsilon)
 *
 * 'Z' is a newly introduced nonterminal.
 *
 * So in essence, in our algorithm:
 * --------------------------------
 *
 * 1. We have to remove the existing two productions of 'A'
 * 2. Add a new nonterminal 'Z', with one production being 'xZ', and the other
 *    being epsilon. This allows us to perform the same recursion, but in a
 *    right linear fashion. Due to the presence of epsilon, the matching string
 *    is simply delimited by end of input, or by a symbol in input not present
 *    in the grammar.
 * 3. Introduce a new production of 'A', putting in 'b', followed by the new non
 *    terminal 'Z'.
 * 4. Check all productions again if left recursion persists. If so, go back to
 *    step 1.
 *
 * =============================================================================
 * Algorithm (For the analysis process)
 * =============================================================================
 *
 * We will describe the parsing algorithm as a purely recursive function rather
 * than as the "emulation" of a recursive function using an explicit stack (as
 * is done in this implementation).
 *
 * We will call this function parse(), which returns a boolean value indicating
 * success or failure of satisfying a rule by a given input. It will take in
 * as arguments:
 *   - Grammar grammar: the grammar data structure.
 *   - Production rule: the current production rule that is being read.
 *   - ProductionSymbol symbol: The current symbol being processed in the
 *     production rule.
 *   - char input[]: The given input
 *   - int offset: offset in the input character array.
 *
 * We also assume that the grammar data structure stores production rules in the
 * order of largest number of symbols o the smallest number of symbols, since
 * this algorithm will greedily report the first match.
 *
 * Function parse(grammar, rule, symbol, input, offset):
 * 1. Start
 *
 * 2. Declare integer `satisfied`.
 *
 * 3. while rule is not NULL:
 *        Set `satisfied` to 1
 *
 *        while symbol is not NULL:
 *            if symbol is a terminal:
 *                if symbol terminal value is not equal to input[offset]:
 *                    Rule has been left unsatisfied.
 *                    Set satisfied to 0
 *                    Break.
 *                else
 *                    Go to next symbol (symbol = symbol->next)
 *            else if symbol is non terminal:
 *                From grammar, find the production set for the symbol value,
 *                get the first rule in the production set, and store it in a
 *                variable called `new_rule`
 *
 *                From new_rule, get a pointer for the first symbol of the
 *                first rule and store it in `new_symbol`
 *
 *                Call parse(grammar, new_rule, new_symbol, input, offset) and
 *                store its return value in 'ret'
 *
 *                if ret is 0 (unsatisfied):
 *                    Rule has been left unsatisfied.
 *                    Set satisfied to 0
 *                    Break.
 *                else:
 *                    Go to next symbol (symbol = symbol->next)
 *
 *        If satisfied is 1 (production has been satisfied):
 *            return 1
 *        Else
 *            Cycle to next rule (rule = rule->next)
 *
 * 4. If satisfied is 1:
 *        return 1 (since rule is satisfied)
 *    Else:
 *        return 0
 *
 * 5. Stop
 *
 *
 * The result of the parsing will be the return value of this function. If
 * 1 is returned, then it means that the grammar accepts the input value,
 * otherwise if it returns 0, it does not.
 */

#include <stdio.h>
#include <stdlib.h>

#define MAXBUF 4096

typedef struct ProductionSymbol ProductionSymbol;
typedef struct Production Production;
typedef struct ProductionSet ProductionSet;

// Decides the type of a symbol. If it is a nonterminal, the symbol will point
// to the appropriate ProductionSet
typedef enum ProductionSymbolType {
    TYPE_TERMINAL = 0,
    TYPE_NONTERMINAL = 1
} ProductionSymbolType;

// The pointer or symbol holder
typedef union ProductionSymbolData {
//    ProductionSet *nonterminal_target; // Unimplemented. We directly search
                                         // for the nonterminal instead.
    char symbol;
} ProductionSymbolData;

// The symbol set of a production for a given nonterminal
typedef struct ProductionSymbol {
    ProductionSymbol *next;
    ProductionSymbolData data;
    ProductionSymbolType type;
} ProductionSymbol;

// Productions of a production set for a given nonterminal
typedef struct Production {
    Production *next;
    ProductionSymbol *symbols;
} Production;

// Production sets of nonterminals of a Grammar
typedef struct ProductionSet {
    ProductionSet *next;
    Production *productions;
    char symbol;
} ProductionSet;

// Information required in the stack
typedef struct StackRecord {
    int offset;
    int visited;
    ProductionSet *prodset;
    Production *prod;
    ProductionSymbol *symbol;
} StackRecord;

// The Main User-Facing Data Struct
typedef struct Grammar {
    ProductionSet *set;
    ProductionSet *starting_symbol;
    StackRecord stack[256];
    unsigned int stack_top;
} Grammar;


// Initialises the grammar
void grammar_init(Grammar *p)
{
    p->set = NULL;
    p->stack_top = 0;
}

// Prints the grammar
void grammar_print(Grammar const * const p)
{
    ProductionSet *prod_set_ptr  = NULL;
    Production *prod_ptr         = NULL;
    ProductionSymbol *symbol_ptr = NULL;

    for (prod_set_ptr = p->set; prod_set_ptr != NULL; prod_set_ptr = prod_set_ptr->next) {
        printf("%c ->\n", prod_set_ptr->symbol);
        for (prod_ptr = prod_set_ptr->productions; prod_ptr != NULL; prod_ptr = prod_ptr->next) {
            printf("  | ");
            if (prod_ptr->symbols == NULL) { // Epsilon/Empty production?
                printf("<empty>")
            }
            for (symbol_ptr = prod_ptr->symbols; symbol_ptr != NULL; symbol_ptr = symbol_ptr-> next) {
                if (symbol_ptr->type == TYPE_TERMINAL) {
                    printf("<terminal '%c'> ", symbol_ptr->data.symbol);
                } else if (symbol_ptr->type == TYPE_NONTERMINAL) {
                    printf("<nonterminal '%c'> ", symbol_ptr->data.symbol);
                }
            }
            printf("\n");
        }
    }

    printf("'%c' is the starting symbol.\n",  p->starting_symbol->symbol);
}

void production_print(Production *p)
{
    ProductionSymbol *symbol_ptr = NULL;
    for (symbol_ptr = p->symbols; symbol_ptr != NULL; symbol_ptr = symbol_ptr->next) {
        if (symbol_ptr->type == TYPE_TERMINAL) {
            printf("%c ", symbol_ptr->data.symbol);
        } else if (symbol_ptr->type == TYPE_NONTERMINAL) {
            printf("%c ", symbol_ptr->data.symbol);
        }
    }
}

// Finds a nonterminal. NULL on failure.
ProductionSet *grammar_find(Grammar *p, char symbol)
{
    ProductionSet *ptr = NULL;
    for (ptr = p->set; ptr != NULL; ptr = ptr->next) {
        if (ptr->symbol == symbol)
            return ptr;
    }

    return NULL;
}

void production_set_init(ProductionSet *p, char symbol)
{
    p->next = NULL;
    p->productions = NULL;
    p->symbol = symbol;
}

void production_init(Production *p)
{
    p->next = NULL;
    p->symbols = NULL;
}

ProductionSymbol *production_symbol_new()
{
    ProductionSymbol *p = malloc(sizeof(*p));
    p->next = NULL;
    return p;
}


/*
 * =============================================================================
 * Left recursive production detection takes place here.
 * =============================================================================
 */

/*
 * Gets the next left recursive production.
 *
 * Returns:
 * 1 if left recursion present. ptr and prev_ptr will be set
 * 0 if not present. ptr and prev_ptr will not be set
 * 2 if self loop. ptr and prev_ptr will not be set.
 *
 * prev_ptr will not be set if ptr is at start of productions.
 * ptr is used as the starting production. If NULL, first production of p is
 * used.
 */
int production_get_next_left_recursive(ProductionSet *p, Production **ptr, Production **prev_ptr)
{
    Production *prod_ptr = NULL;
    Production *prev_prod_ptr = NULL;

    if (*ptr == NULL)
        prod_ptr = p->productions;
    else
        prod_ptr = *ptr;

    for (; prod_ptr != NULL; prod_ptr = prod_ptr->next) {
        // Check first symbol
        if (prodptr->symbols->type != TYPE_TERMINAL)
            continue;

        // Is leftmost production the same as the current?
        if (p->symbol == prodptr->symbols->data.symbol) {
            if (prod_ptr->symbols->next == NULL) {
                // This means that this is the only symbol in the production,
                // which means that this production is a self loop. Not what we
                // want.
                printf("Warning: self Loop detected in grammar.");
                return 2;
            }
        } else {
            *ptr = prod_ptr;
            return 1;
        }

        // set prev ptr to current.
        *prev_prod_ptr = prod_ptr;
    }

    return 0;
}

/*
 * Gets the next non left recursive production.
 *
 * Returns:
 * 1 if present. ptr and prev_ptr will be set
 * 0 if not present. ptr and prev_ptr will not be set
 *
 * prev_ptr will not be set if ptr is at start of productions.
 * ptr is used as the starting production. If NULL, first production of p is
 * used.
 */
int production_get_next_non_left_recursive(ProductionSet *p, Production **ptr, Production **prev_ptr)
{
    *ptr = NULL;
    *prev_ptr = NULL;

    if (*ptr == NULL)
        *ptr = p->productions;
    else
        *ptr = *ptr;

    for (; *ptr != NULL; *ptr = (*ptr)->next) {
        // Is leftmost production the same as the current?
        if (p->symbol == (*ptr)->symbols->data.symbol) {
            continue;
        } else {
            return 1;
        }

        // set prev ptr to current.
        *prev_ptr = *ptr;
    }

    return 0;
}

// Checks if the grammar has been properly defined or not.
// Returns less than 0 on failure

int grammar_prepare(Grammar const * const p)
{
    ProductionSet *prod_set_ptr  = NULL;
    Production *prod_ptr         = NULL;
    ProductionSymbol *symbol_ptr = NULL;

    Production *prev_ptr = NULL;
    Production *ptr = NULL;

    Production *nonrec_prev_ptr = NULL;
    Production *nonrec_ptr = NULL;

    char new_nonterminal = 'Z';

    // Cannot proceed without a starting symbol.
    if (p->starting_symbol == NULL)
        return -1;

    // Rectify left recursive grammars
    for (prod_set_ptr = p->set; prod_set_ptr != NULL; prod_set_ptr = prod_set_ptr->next) {
        for (prod_ptr = prod_set_ptr->productions; prod_ptr != NULL; prod_ptr = prod_ptr->next) {
            if (prod_ptr->symbols == NULL) { // Epsilon/Empty production?
            }
            for (symbol_ptr = prod_ptr->symbols; symbol_ptr != NULL; symbol_ptr = symbol_ptr-> next) {
                if (symbol_ptr->type == TYPE_TERMINAL) {
                } else if (symbol_ptr->type == TYPE_NONTERMINAL) {
                }
            }
            printf("\n");
        }
    }
/*
 * * 2. Removal of Left Recursive Grammars
 *    ----------------------------------
 *
 * Given a nonterminal 'A' that is left recursive it will have at least the
 * productions in the following form:
 *
 * A -> Ax
 * A -> b
 *
 * where 'x' is a set of terminals and nonterminals, and 'b' is a terminal or a
 * production that is not left recursive.
 *
 * These are then transformed to a right recursive grammar as follows:
 *
 * A -> bZ
 * Z -> xZ
 * Z -> (epsilon)
 *
 * 'Z' is a newly introduced nonterminal.
 *
 * So in essence, in our algorithm:
 * --------------------------------
 *
 * 1. We have to remove the existing two productions of 'A'
 * 2. Add a new nonterminal 'Z', with one production being 'xZ', and the other
 *    being epsilon. This allows us to perform the same recursion, but in a
 *    right linear fashion. Due to the presence of epsilon, the matching string
 *    is simply delimited by end of input, or by a symbol in input not present
 *    in the grammar.
 * 3. Introduce a new production of 'A', putting in 'b', followed by the new non
 *    terminal 'Z'.
 * 4. Check all productions again if left recursion persists. If so, go back to
 *    step 1.
 */
    int ret;
    int no_productions = 1;
    ProductionSet *aux = NULL;
    ProductionSymbol *head_ptr;

    // Left recursions are removed first, then we remove the right recursions.
    for (prod_set_ptr = p->set; prod_set_ptr != NULL; prod_set_ptr = prod_set_ptr->next) {

        // Iterate through the LR productions
        while (ret = production_get_next_left_recursive(prod_set_ptr, &ptr, &prev_ptr)) {
            if (ret == 2) {
                printf("Infinite production loop detected. Aborting. Check your grammar.\n");
                abort()
            }

            // Aux production has not been created?
            if (aux == NULL) {
                // Set up new production.
                *new = malloc(sizeof(*aux));
                production_set_init(aux, new_nonterminal);
            }

            // Go through each Non LR production and make a new production from it.
            while (ret = production_get_next_left_non_recursive(prod_set_ptr, &nonrec_ptr, &nonrec_prev_ptr) {
                no_productions = 0;

            }

            // If flag hasn't been unset yet, it means that grammar has an infinite
            // loop.
            // Transitive loop checks are not yet present.
            if (no_productions) {
                printf("Infinite production loop detected. Aborting. Check your grammar.\n");
                abort()
            }
        }

        // If aux production has been created,
        if (aux != NULL) {
            // Insert the epsilon symbol.
            head_ptr = malloc(sizeof(*head_ptr));
            production_init(head_ptr);
            aux->productions = head_ptr;

            // Put it into the grammar

        }
    }
    return 0;
}

void grammar_insert_internal(ProductionSet *p, char *rule)
{
    Production *prod_ptr;
    ProductionSymbol *symbol_ptr;

    if (p->productions != NULL) {
        // Get the list tail
        for (prod_ptr = p->productions; prod_ptr->next != NULL; prod_ptr = prod_ptr->next) {
            // nop
        }

        // Append a node
        prod_ptr->next = malloc(sizeof(*prod_ptr->next));
        production_init(prod_ptr->next);
        prod_ptr = prod_ptr->next;
    } else {
        p->productions = malloc(sizeof(*p->productions));
        production_init(p->productions);
        prod_ptr = p->productions;
    }

    for (int i = 0; rule[i] != '\0'; i++) {
        // Skip Whitespace
        if (rule[i] == ' ' ||
            rule[i] == '\t' ||
            rule[i] == '\n' ) {
            continue;
        }

        // Whitespaces have been ruled out.
        // Whatever else comes now requires a node.
        if (prod_ptr->symbols == NULL) {
            prod_ptr->symbols = production_symbol_new();
            symbol_ptr = prod_ptr->symbols;
        } else {
            symbol_ptr->next = production_symbol_new();
            symbol_ptr = symbol_ptr->next;
        }

        // We assume that nonterminals are always single characters and
        // capitalised.
        if (rule[i] >= 'A' && rule[i] <= 'Z') {
            // Find reference (TODO)
            // ProductionSet *ref = grammar_find(g, rule[i]);
            // if (ref == NULL) {
            //     printf("Error: no")
            // }
            symbol_ptr->data.symbol = rule[i];
            symbol_ptr->type = TYPE_NONTERMINAL;

        // Everything else is a terminal symbol
        } else {
            symbol_ptr->data.symbol = rule[i];
            symbol_ptr->type = TYPE_TERMINAL;
        }
    }
}

// Insert a production rule for a given nonterminal
void grammar_insert_rule(Grammar *p, char nonterminal, char *rule)
{
    ProductionSet *ptr;

    // Is set nonempty?
    if (p->set != NULL) {
        // Probe set for nonterminal
        for (ptr = p->set; ptr->next != NULL; ptr = ptr->next) {
            if (ptr->symbol == nonterminal) // Found the nonterminal. Break.
                break;
        }

        // Nonterminal not in set?
        if (ptr->symbol != nonterminal && ptr->next == NULL) {
            // Create a new entry then
            ptr->next = malloc(sizeof(*ptr->next));
            ptr = ptr->next;
            production_set_init(ptr, nonterminal);
        }
    } else {
        // Add the first entry to the set.
        p->set = malloc(sizeof(*p->set));
        ptr = p->set;
        production_set_init(ptr, nonterminal);
    }

    // Actual Insertion now will take place now that production set has been
    // determined.

    grammar_insert_internal(ptr, rule);
}

// Sets starting symbol on grammar
void grammar_set_starting_symbol(Grammar *p, char symbol)
{
    ProductionSet *ref = grammar_find(p, symbol);

    if (ref == NULL) {
        printf("Error: Nonterminal %c does not exist.\n", symbol);
        return;
    }

    p->starting_symbol = ref;
}

// Get next character in string
// Returns -1 if end of string.
int next_char(char *string, int *offset)
{
    for (;string[*offset] != '\0'; (*offset)++) {
        // Skip Whitespace
        if (string[*offset] == ' '  ||
            string[*offset] == '\t' ||
            string[*offset] == '\n' ) {
            continue;
        }

        return string[(*offset)++];
    }

    return -1;
}

void space_pad(int num) {
    for (int i = 0; i < (num - 1) * 4; i++) {
        putchar(' ');
    }
}

void grammar_input_check(Grammar *g, char *input)
{
    ProductionSet *prod_set_ptr  = NULL;
    Production *prod_ptr         = NULL;
    ProductionSymbol *symbol_ptr = NULL;

    int offset = 0;
    int starting_offset = 0; // If a production is unsatisfied, we start from
                             // here.
    int current_char;

    prod_set_ptr = g->starting_symbol;
    prod_ptr = prod_set_ptr->productions;
    symbol_ptr = prod_ptr->symbols;

    // Boolean flags
    int production_satisfied = 1;
    int prev_production_satisfied = 1;
    int production_continue = 0;

    // Push a value into the stack
    g->stack_top++;
    g->stack[g->stack_top - 1].offset  = offset;
    g->stack[g->stack_top - 1].prodset = prod_set_ptr;
    g->stack[g->stack_top - 1].prod    = prod_ptr;
    g->stack[g->stack_top - 1].symbol  = symbol_ptr;
    g->stack[g->stack_top - 1].visited = 0;

    printf("\n");

    while (g->stack_top != 0) {
        if (g->stack_top > 255) {
            printf("Stack smashed. Check for bugs.\n");
            abort();
        }

        production_satisfied = 1;
        prod_set_ptr = g->stack[g->stack_top - 1].prodset;

        // Have we not gone down the stack?
        if (!g->stack[g->stack_top - 1].visited) {
            space_pad(g->stack_top);
            printf("[depth: %d] Checking Productions for '%c'.\n", g->stack_top - 1, prod_set_ptr->symbol);

            prod_ptr = prod_set_ptr->productions;
            symbol_ptr = prod_ptr->symbols;

            // Set string offset to the previous offset
            starting_offset = offset;
        } else {
            space_pad(g->stack_top);
            printf("[depth: %d] Again Checking Productions for '%c'.\n", g->stack_top - 1, prod_set_ptr->symbol);

            // Go to the next production. This one has not been satisfied.
            if (!prev_production_satisfied) {
                // Go to next prodcution
                prod_ptr = g->stack[g->stack_top - 1].prod->next;

                if (prod_ptr == NULL) {
                    // We have exhausted all productions. String Rejected.
                    printf("Productions left unsatisfied.\n\n");
                    production_satisfied = 0;
                    break;
                }

                // Reset string offset
                starting_offset = g->stack[g->stack_top - 1].offset;
                // Reset Symbols
                symbol_ptr = prod_ptr->symbols;

            } else {
                // Continue on with this one.
                prod_ptr = g->stack[g->stack_top - 1].prod;
                symbol_ptr = g->stack[g->stack_top - 1].symbol->next;

                // Set string offset to the previous offset
                starting_offset = offset;
                // Switch for the for loop below.
                production_continue = 1;
            }
        }

        for (; prod_ptr != NULL; prod_ptr = prod_ptr->next) {
            offset = starting_offset;
            if (production_continue) {
                production_continue = 0;
            } else {
                symbol_ptr = prod_ptr->symbols;
            }
            space_pad(g->stack_top);
            printf("Checking production ' ");
            production_print(prod_ptr);
            printf("'\n");
            for (; symbol_ptr != NULL; symbol_ptr = symbol_ptr->next) {
                if (symbol_ptr->type == TYPE_NONTERMINAL) {
                    space_pad(g->stack_top);
                    printf("Non Terminal '%c', expected. Stepping Down.\n\n", symbol_ptr->data.symbol);
                    // We need to save the stack now and move to the next one.
                    goto recursive_step;
                } else if (symbol_ptr->type == TYPE_TERMINAL) {
                    // Fetch character from stream.
                    int current_char_err = next_char(input, &offset);

                    space_pad(g->stack_top);
                    printf("Terminal '%c' expected ", symbol_ptr->data.symbol);

                    // Handle input exhaustion case
                    if (current_char_err < 0) {
                        production_satisfied = 0;
                        printf("but input exhausted.\n");
                        break;
                    } else {
                        current_char = current_char_err;
                    }

                    if (symbol_ptr->data.symbol != current_char) {
                        // If current char is not equal to required terminal, we
                        // move to the next production.
                        production_satisfied = 0;
                        printf("but '%c' given.\n", current_char);
                        break;
                    }

                    production_satisfied = 1;
                    printf("and found.\n");
                }
            }

            // If production has been satisfied, we no longer need to check
            // other productions.
            if (production_satisfied) {
                space_pad(g->stack_top);
                printf("Production satisfied.\n");
                break;
            }

            space_pad(g->stack_top);
            printf("Cycling to next alternative production.\n");
        }

        if (production_satisfied) {
            // A production has been satisfied, which means the string
            // conforms with the grammar in the current recursive step. We now
            // go down one step.
            space_pad(g->stack_top);
            printf("Stepping up.\n\n");
            prev_production_satisfied = 1;
            g->stack_top--;
        } else {
            // This condition will happen if we have exhausted all the
            // productions of the nonterminal. Hence it does not satisfy the
            // current production. We decrement the stack and keep going on to
            // the next production on ascension.
            space_pad(g->stack_top);
            printf("Productions left unsatisfied. Reporting and stepping up.\n\n");
            prev_production_satisfied = 0;
            g->stack_top--;
        }

        continue;

        // Here we perform our recursive descent.
recursive_step:
        // Save State
        g->stack[g->stack_top - 1].offset  = starting_offset;
        g->stack[g->stack_top - 1].prodset = prod_set_ptr;
        g->stack[g->stack_top - 1].prod    = prod_ptr;
        g->stack[g->stack_top - 1].symbol  = symbol_ptr;
        g->stack[g->stack_top - 1].visited = 1;

        // Set Next State
        g->stack_top++;
        // printf(">> %c\n", symbol_ptr->data.symbol);
        prod_set_ptr = grammar_find(g, symbol_ptr->data.symbol);
        if (prod_set_ptr == NULL) {
            // Some error. Abort.
            printf("Error: Couldn't find production '%c'\n", symbol_ptr->data.symbol);
            printf("Aborting\n");
            return;
        }

        g->stack[g->stack_top - 1].offset  = offset;
        g->stack[g->stack_top - 1].prodset = prod_set_ptr;
        g->stack[g->stack_top - 1].prod    = prod_set_ptr->productions;
        g->stack[g->stack_top - 1].symbol  = prod_set_ptr->productions->symbols;
        g->stack[g->stack_top - 1].visited = 0;
    }

    if (production_satisfied && prev_production_satisfied) {
        printf("String Accepted.\n");
    } else {
        printf("Did not satisfy production rules. String Rejected.\n");
    }
}


int main()
{
    Grammar g;
    char buf[MAXBUF];

    grammar_init(&g);
    // IMPORTANT:
    // Rules per non terminal must be inserted in order of LARGEST number of
    // symbols to SMALLEST, since matching is greedy.
    // Later facilities may be implemented for automatically doing this.
    grammar_insert_rule(&g, 'S', "A");
    grammar_insert_rule(&g, 'A', "(A)");
    grammar_insert_rule(&g, 'A', "x * A");
    grammar_insert_rule(&g, 'A', "x");

    printf("Grammar Contents:\n====================\n");
    grammar_set_starting_symbol(&g, 'S');

    if (grammar_self_verify(&g) < 0) {
        printf("Error: Grammar not properly defined.");
    }

    grammar_print(&g);

    printf("\nEnter an expression to parse: ");
    fgets(buf, MAXBUF, stdin);
    grammar_input_check(&g, buf);

    return 0;
}
