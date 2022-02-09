#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

/*
 * -----------------------------------------------------------------------------
 * Constants
 * -----------------------------------------------------------------------------
 */

#define MAXBUF 4096
#define SETMAXALLOCSIZE 40
#define PRODSETMAXALLOCSIZE 10
#define PRODMAXALLOCSIZE 20
#define SYMBOLMAXALLOCSIZE 10

/*
 * -----------------------------------------------------------------------------
 * Macros
 * -----------------------------------------------------------------------------
 */

#define PRINT_COMMA_LIST(list, size, fmt) \
{ \
    for (int i = 0; i < ((size) - 1); i++) { \
        printf((fmt), (list)[i]); \
        printf(", "); \
    } \
    printf((fmt), (list)[((size) - 1)]); \
}

/*
 * -----------------------------------------------------------------------------
 * Data Structures
 * -----------------------------------------------------------------------------
 */

/*
 * Grammar Storage
 */

typedef char ProductionSymbol;
typedef struct Production Production;
typedef struct ProductionSet ProductionSet;
typedef char ProductionSymbolData;

/*
 * Only implementations of sets for integers and chars are required in this
 * program. This same datatype is used for both since int can hold both int and
 * char.
 */

typedef struct Set {
    int data[40];
    int data_size;
} Set;

void set_init(Set *s)
{
    s->data_size = 0;
}

void set_clear(Set *s)
{
    s->data_size = 0;
}

// Returns 1 when element already exists in set, and 0 when not
int set_insert(Set *s, int k)
{
    int i;

    for (i = 0; i < s->data_size; i++) {
        if (s->data[i] == k) {
            return 1;
        }
    }

    if (i < SETMAXALLOCSIZE) {
        s->data[i] = k;
        s->data_size++;
    } else {
        printf("Overflow\n");
    }

    return 0;
}

// Returns the index of an element in the set array. Used for getting table
// column indices below.
int set_order(Set *s, int k)
{
    int i;

    for (i = 0; i < s->data_size; i++) {
        if (s->data[i] == k) {
            return i;
        }
    }

    return -1;
}

// Returns whether an element exists or not in the set
int set_exists(Set *s, int k)
{
    return set_order(s, k) >= 0;
}

// Productions of a production set for a given nonterminal
typedef struct Production {
    ProductionSymbol symbols[SYMBOLMAXALLOCSIZE];
    int size;
} Production;

// Production sets of nonterminals of a Grammar
typedef struct ProductionSet {
    Production productions[PRODMAXALLOCSIZE];
    int size;
    char symbol;
    char nullable;
} ProductionSet;

// The Main User-Facing Data Struct
typedef struct Grammar {
    ProductionSet set[PRODSETMAXALLOCSIZE];
    Set nonterminal_set;
    Set terminal_set;
    int size;
    int starting_symbol;
    unsigned int stack_top;
} Grammar;

/*
 * Item DFA Storage
 */

typedef struct ItemProduction {
    char nonterminal;
    int production;
    int dot_offset;
} ItemProduction;

typedef struct Item {
    // The first ItemProduction in the Item is considered the production from
    // which the closure is derived, and that is used to generate the DFA
    ItemProduction productions[20];
    int productions_size;
    int closure_offset;
    Set nonterminal_set;
} Item;

typedef struct ItemSet {
    Item items[20];
    int items_size;
    char initial_symbol;
} ItemSet;


/*
 * LR0 Table Storage (Acts as the transition table)
 */

#define MAX_DIMENSIONS 20

typedef enum ActionType {
    ACTION_ERROR = 0, // The default, empty state
    ACTION_SHIFT = 1,
    ACTION_REDUCE,
    ACTION_ACCEPT
} ActionType;

// The below is used to indicate error or empty state in a goto table cell

#define GOTO_ERROR -1

typedef int GotoRecord;

typedef struct ActionRecord {
    ActionType type;
    int target_state;

    // Used for reduce actions
    char nonterminal;
    int production;
} ActionRecord;

typedef struct LR0ParsingTable {
    ActionRecord action_table[MAX_DIMENSIONS][MAX_DIMENSIONS];
    GotoRecord goto_table[MAX_DIMENSIONS][MAX_DIMENSIONS];
} LR0ParsingTable;

typedef struct LR0StackRecord {
    char symbol;
    int target_state;
} LR0StackRecord;

/*
 * -----------------------------------------------------------------------------
 * Grammar manipulation/presentation functions
 * -----------------------------------------------------------------------------
 */

// Initialises the grammar
void grammar_init(Grammar *g)
{
    g->starting_symbol = '\0';
    g->stack_top = 0;
    g->size = 0;

    set_init(&g->nonterminal_set);
    set_init(&g->terminal_set);
}

void production_set_init(ProductionSet *p, char symbol)
{
    p->size = 0;
    p->nullable = 0;
    p->symbol = symbol;
}

void production_init(Production *p)
{
    p->size = 0;
}

// Prints the grammar
void grammar_print(Grammar const * const g)
{
    int order = 0;

    for (int prod_set = 0; prod_set < g->size; prod_set++) {
        printf("%c ->\n", g->set[prod_set].symbol);
        for (int prod = 0; prod < g->set[prod_set].size; prod++) {
            printf("  | (%d) ", order);
            if (g->set[prod_set].productions[prod].size == 0) { // Epsilon/Empty production?
                printf("<epsilon>");
            }
            for (int symbol = 0; symbol < g->set[prod_set].productions[prod].size; symbol++) {
                printf("%c", g->set[prod_set].productions[prod].symbols[symbol]);
            }
            printf("\n");
            order++;
        }
    }

    printf("\nNonterminals: ");
    PRINT_COMMA_LIST(g->nonterminal_set.data, g->nonterminal_set.data_size, "%c");
    printf("\n");

    printf("Terminals: ");
    PRINT_COMMA_LIST(g->terminal_set.data, g->terminal_set.data_size, "%c");
    printf("\n");

    printf("'%c' is the starting symbol.\n",  g->starting_symbol);
    printf("The number in parantheses on the left of each production is the order\n"
           "number of the production.\n");
}

int is_nonterminal(char symbol)
{
    return isupper(symbol);
}

int is_terminal(char symbol)
{
    return !isupper(symbol);
}

void production_print(Production *p)
{
    if (p->size == 0) {
        printf("<epsilon>");
        return;
    }

    for (int symbol = 0; symbol < p->size; symbol++) {
        printf("%c", p->symbols[symbol]);
    }
}

void production_print_small(Production *p)
{
    if (p->size == 0) {
        printf("@");
        return;
    }

    for (int symbol = 0; symbol < p->size; symbol++) {
        printf("%c", p->symbols[symbol]);
    }
}

void grammar_insert_internal(Grammar *g, ProductionSet *p, char *rule)
{
    Production *production;

    p->size++;
    production = &p->productions[p->size - 1];
    production_init(production);

    if (rule[0] == '@') {
        // We are done here because this is an epsilon production. Simply set
        // the nullable fag.
        p->nullable = 1;
        return;
    }

    for (int i = 0; rule[i] != '\0'; i++) {
        // Skip Whitespace
        if (rule[i] == ' ' ||
            rule[i] == '\t' ||
            rule[i] == '\n' ) {
            continue;
        }

        if (is_terminal(rule[i])) {
            set_insert(&g->terminal_set, rule[i]);
        }

        p->productions[p->size - 1].size++;
        p->productions[p->size - 1].symbols[p->productions[p->size - 1].size - 1] = rule[i];
    }
}

// Insert a production rule for a given nonterminal
void grammar_insert_rule(Grammar *g, char nonterminal, char *rule)
{
    int set = 0;

    set_insert(&g->nonterminal_set, nonterminal);

    // Is set nonempty?
    if (g->size > 0) {
        // Probe set for nonterminal
        for (set = 0; set < g->size; set++) {
            if (g->set[set].symbol == nonterminal) // Found the nonterminal. Break.
                break;
        }

        // Nonterminal not in set?
        if (set == g->size) {
            g->size++;
            production_set_init(&g->set[set], nonterminal);
        }
    } else {
        g->size++;
        set = g->size - 1;
        production_set_init(&g->set[g->size - 1], nonterminal);
    }

    // Actual Insertion now will take place now that production set has been
    // determined.

    grammar_insert_internal(g, &g->set[set], rule);
}

// Finds a nonterminal. NULL on failure.
int grammar_find(Grammar *g, char symbol)
{
    for (int prodset = 0; prodset < g->size; prodset++) {
        if (g->set[prodset].symbol == symbol)
            return prodset;
    }

    return -1;
}

// Sets starting symbol on grammar
void grammar_set_starting_symbol(Grammar *g, char symbol)
{
    int prodset = grammar_find(g, symbol);

    if (prodset < 0) {
        printf("Error: Nonterminal %c does not exist.\n", symbol);
        return;
    }

    g->starting_symbol = g->set[prodset].symbol;
}

// Checks if the grammar has been properly defined or not.
// Returns less than 0 on failure

int grammar_finish(Grammar *g)
{
    // Cannot proceed without a starting symbol.
    if (g->starting_symbol == '\0')
        return -1;

    set_insert(&g->terminal_set, '$');
    return 0;
}

/*
 * -----------------------------------------------------------------------------
 * LR0 Preliminiary functions
 * -----------------------------------------------------------------------------
 */

// Printing functions

void item_production_print(Grammar *g, ItemProduction *s)
{
    int prodset = grammar_find(g, s->nonterminal);

    printf("%c -> ", s->nonterminal);

    for (int i = 0; i < g->set[prodset].productions[s->production].size; i++) {
        if (i == s->dot_offset) {
            printf(".");
        }
        printf("%c", g->set[prodset].productions[s->production].symbols[i]);
    }

    if (s->dot_offset == g->set[prodset].productions[s->production].size) {
            printf(".");
    }

}

void item_set_print(Grammar *g, ItemSet *s)
{
    for (int i = 0; i < s->items_size; i++) {
        printf("I%d :-\n", i);
        for (int j = 0; j < s->items[i].productions_size; j++) {
            printf("    ");
            item_production_print(g, &s->items[i].productions[j]);
            printf("\n");
        }

        printf("\n");
    }
    printf("\n");
}

/*
 * This initialises our parsing table with empty rows and columns.
 */
void lr0_parsing_table_init(LR0ParsingTable *p)
{
    for (int i = 0; i < MAX_DIMENSIONS; i++) {
        for (int j = 0; j < MAX_DIMENSIONS; j++) {
            p->action_table[i][j].type = ACTION_ERROR;
            p->goto_table[i][j] = GOTO_ERROR;
        }
    }
}

/*
 * This inserts the E' -> E production into the grammar.
 */
void grammar_lr0_augment(Grammar *g)
{
    // We determine our new production in this manner
    char new_production[2] = { g->starting_symbol, '\0' };

    // We reserve the symbol Z for this production.
    grammar_insert_rule(g, 'Z', new_production);
}

void item_set_init(ItemSet *s, char initial_symbol)
{
    s->items_size = 0;
    s->initial_symbol = initial_symbol;
}

Item *item_insert(ItemSet *s)
{
    s->items_size++;
    s->items[s->items_size - 1].productions_size = 0;
    s->items[s->items_size - 1].closure_offset = 1;
    set_init(&s->items[s->items_size - 1].nonterminal_set);

    return &s->items[s->items_size - 1];
}

int item_production_insert(Item *s, char nonterminal, int production, int dot_offset)
{
    for (int i = 0; i < s->productions_size; i++) {
        if (s->productions[i].nonterminal == nonterminal &&
            s->productions[i].production  == production &&
            s->productions[i].dot_offset  == dot_offset) {
            return 1;
        }
    }

    s->productions_size++;
    s->productions[s->productions_size - 1].nonterminal = nonterminal;
    s->productions[s->productions_size - 1].production  = production;
    s->productions[s->productions_size - 1].dot_offset  = dot_offset;

    return 0;
}

Production *item_get_production(Grammar *g, ItemProduction *s)
{
    int prodset = grammar_find(g, s->nonterminal);

    if (prodset < 0)
        return NULL;

    return &g->set[prodset].productions[s->production];
}

int item_insert_closure_internal(Grammar *g, Item *s, char nonterminal)
{
    int prodset = grammar_find(g, nonterminal);

    if (prodset < 0) {
        printf("Error: Missing Nonterminal '%c'\n", nonterminal);
        return -1;
    }

    // Has already been inserted?

    set_insert(&s->nonterminal_set, nonterminal);

    for (int i = 0; i < g->set[prodset].size; i++) {
        // Insert all closure productions one by one
        // If already exists, we do not handle the next condition. Otherwise
        // it will be an infinite loop.
        if (item_production_insert(s, nonterminal, i, 0) == 1)
            continue;

        // Is the dot adjacent symbol a nonterminal?
        if (g->set[prodset].productions[i].size > 0 &&
            is_nonterminal(g->set[prodset].productions[i].symbols[0])) {
            // Recurse and insert the closure
            if (item_insert_closure_internal(g, s, g->set[prodset].productions[i].symbols[0]) < 0) {
                return -1;
            }
        }
    }

    return 0;
}

int item_insert_closure(Grammar *g, Item *s)
{
    Set visited;
    int no_outgoing = 1;
    set_init(&visited);

    if (s->productions_size == 0) {
        printf("Item set empty\n");
        return -1;
    }

    for (int i = 0; i < s->closure_offset; i++) {
        // if (set_insert(&s->nonterminal_set, s->productions[i].nonterminal);

        Production *p = item_get_production(g, &s->productions[i]);

        if (s->productions[i].dot_offset >= p->size) {
            continue;
        }

        if (is_nonterminal(p->symbols[s->productions[i].dot_offset])) {
            if (set_insert(&visited, p->symbols[s->productions[i].dot_offset]) == 1)
                continue;
            if (item_insert_closure_internal(g, s, p->symbols[s->productions[i].dot_offset]) < 0) {
                printf("Error: Closure insertion failed. Aborting.\n");
                return -1;
            }
        }

        no_outgoing = 0;
    }

    return no_outgoing;
}

/*
 * Finds an item equal to the first production.
 */
int item_find(ItemSet *s, ItemProduction *p)
{
    // printf("Looking for: %d %c %d\n", p->production,
    //                        p->nonterminal,
    //                        p->dot_offset);
    for (int i = 0; i < s->items_size; i++) {
        for (int j = 0; j < s->items[i].closure_offset; j++) {
            if (s->items[i].productions[j].production  == p->production  &&
                s->items[i].productions[j].nonterminal == p->nonterminal &&
                s->items[i].productions[j].dot_offset  == p->dot_offset) {
                return i;
            }
        }
    }

    // printf("Did not find\n");
    return -1;
}

/*
 * -----------------------------------------------------------------------------
 * Creation of Grammar DFA Parsing Table
 * -----------------------------------------------------------------------------
 */

int grammar_get_order_from_item(Grammar *g, ItemProduction *k)
{
    int prodset = grammar_find(g, k->nonterminal);
    int num = -1;

    for (int i = 0; i < prodset; i++) {
        num += g->set[i].size;
    }

    for (int i = 0; i <= k->production; i++) {
        num += 1;
    }

    return num;
}

int grammar_create_lr_table_internal(Grammar *g, ItemSet *set, LR0ParsingTable *table, int item_offset)
{
    Set visited;
    Item *item = &set->items[item_offset];

    set_init(&visited);

    int no_outgoing = item_insert_closure(g, item);

    // Find closure
    if (no_outgoing == 1) {
        // If we have shited the dot to the end of the production, perform the
        // reduce action.

        // Do we need to insert accept in this one?
        if (item->productions[0].nonterminal == 'Z') {
            table->action_table[item_offset][set_order(&g->terminal_set, '$')].type = ACTION_ACCEPT;

        } else {
            // TODO handle 'grammar LR0 or not' case here
            for (int i = 0; i < g->terminal_set.data_size; i++) {
                if (table->action_table[item_offset][i].type != ACTION_ERROR) {
                    printf("Reduce Overlap on I%d,'%c' %c%d -> R%d\n",
                    item_offset,
                    g->terminal_set.data[i],
                    ((table->action_table[item_offset][i].type == ACTION_SHIFT) ? 'S' : 'R'),
                    table->action_table[item_offset][i].target_state,

                    grammar_get_order_from_item(g, &item->productions[0]));
                }
                table->action_table[item_offset][i].type = ACTION_REDUCE;
                table->action_table[item_offset][i].target_state = grammar_get_order_from_item(g, &item->productions[0]);
                table->action_table[item_offset][i].nonterminal = item->productions[0].nonterminal;
                table->action_table[item_offset][i].production = item->productions[0].production;
            }
            // printf("returning for: ");
            //item_production_print(g, &item->productions[0]);
            // printf("\n");
        }
        return 1;
    } else if (no_outgoing < 0) {
        printf("Error: LR0 table creation failed.\n");
        return -1;
    }

    // Iterate over closure and find nonterminals next to dots
    // For each nonterminal Insert a goto
    // Recurse

    for (int i = 0; i < item->productions_size; i++) {

        Production *p = item_get_production(g, &item->productions[i]);

        if (item->productions[i].dot_offset >= p->size) {
            if (item->productions[i].nonterminal == 'Z') {
                table->action_table[item_offset][set_order(&g->terminal_set, '$')].type = ACTION_ACCEPT;
            } else {
                // TODO handle 'grammar LR0 or not' case here
                for (int j = 0; j < g->terminal_set.data_size; j++) {
                    if (table->action_table[item_offset][j].type != ACTION_ERROR) {
                        printf("Reduce Overlap on I%d,'%c' %c%d -> R%d\n",
                        item_offset,
                        g->terminal_set.data[j],
                        ((table->action_table[item_offset][j].type == ACTION_SHIFT) ? 'S' : 'R'),
                        table->action_table[item_offset][j].target_state,

                        grammar_get_order_from_item(g, &item->productions[i]));
                    }
                    table->action_table[item_offset][j].type = ACTION_REDUCE;
                    table->action_table[item_offset][j].target_state = grammar_get_order_from_item(g, &item->productions[i]);
                    table->action_table[item_offset][j].nonterminal = item->productions[i].nonterminal;
                    table->action_table[item_offset][j].production = item->productions[i].production;
                }
            }
        }

        ItemProduction new;
        new.nonterminal = item->productions[i].nonterminal;
        new.production = item->productions[i].production;
        new.dot_offset = item->productions[i].dot_offset + 1;

        if (set_insert(&visited, p->symbols[item->productions[i].dot_offset]) == 1) {
            continue;
        }

        int next = item_find(set, &new);

        // Has this item not been inserted yet?
        if (next < 0) {
            //printf("Inserting\n");
            // Insert and recurse.
            Item *new_item = item_insert(set);
            next = set->items_size - 1;
            item_production_insert(new_item, new.nonterminal, new.production, new.dot_offset);

            for (int j = i + 1; j < item->productions_size; j++) {
                // Add any productions with the same value adjacent to the dot.
                Production *extra_p = item_get_production(g, &item->productions[j]);
                if (item->productions[j].dot_offset == extra_p->size) {
                    continue;
                }
                if (extra_p->symbols[item->productions[j].dot_offset] ==  p->symbols[item->productions[i].dot_offset]) {
                    printf("WARNING: Grammar likely not LR0\n");
                    item_production_insert(new_item,
                        item->productions[j].nonterminal,
                        item->productions[j].production,
                        item->productions[j].dot_offset + 1);
                    new_item->closure_offset++;
                }
            }

            if (grammar_create_lr_table_internal(g, set, table, next) < 0) {
                return -1;
            }
        }

        if (is_nonterminal(p->symbols[item->productions[i].dot_offset])) {
            //assert(table->goto_table
            //   [item_offset]
            //   [set_order(&g->nonterminal_set, p->symbols[item->productions[i].dot_offset])] == GOTO_ERROR);
            if (table->goto_table
                [item_offset]
                [set_order(&g->nonterminal_set, p->symbols[item->productions[i].dot_offset])] != GOTO_ERROR) {
                printf("Goto Overlap? %d -> %d\n", table->goto_table
                [item_offset]
                [set_order(&g->nonterminal_set, p->symbols[item->productions[i].dot_offset])], next);
            }

            table->goto_table
               [item_offset]
               [set_order(&g->nonterminal_set, p->symbols[item->productions[i].dot_offset])]
                = next;
        } else {
            //assert(table->action_table
            //   [item_offset]
            //   [set_order(&g->terminal_set, p->symbols[item->productions[i].dot_offset])]
            //    .type == ACTION_ERROR);

            if (table->action_table
               [item_offset]
               [set_order(&g->terminal_set, p->symbols[item->productions[i].dot_offset])].type != ACTION_ERROR) {
                printf("Shift Overlap? %d -> %d\n", table->action_table
                [item_offset]
                [set_order(&g->nonterminal_set, p->symbols[item->productions[i].dot_offset])].target_state, next);
            }

            table->action_table
               [item_offset]
               [set_order(&g->terminal_set, p->symbols[item->productions[i].dot_offset])]
                .type = ACTION_SHIFT;
            table->action_table
               [item_offset]
               [set_order(&g->terminal_set, p->symbols[item->productions[i].dot_offset])]
                .target_state = next;
        }
    }

    return 0;
}

int grammar_create_lr_table(Grammar *g, ItemSet *set, LR0ParsingTable *p)
{
    Set next_set;

    set_init(&next_set);
    lr0_parsing_table_init(p);

    // Insert The newly made production in the set (Z)
    Item *item = item_insert(set);
    item_production_insert(item, 'Z', 0, 0);

    // Now, Recursively find all productions.
    return grammar_create_lr_table_internal(g, set, p, set->items_size - 1);
}


void grammar_print_lr_table(Grammar *g, LR0ParsingTable *p, int num_items)
{
    // Header 1

    printf(" \t");

    printf("ACTION");

    for (int i = 0; i < g->terminal_set.data_size; i++) {
        printf(" \t");
    }

    printf("  |\t");

    printf("GOTO\n");

    // Header 2

    printf(" \t");

    for (int i = 0; i < g->terminal_set.data_size; i++) {
        printf("%c\t", g->terminal_set.data[i]);
    }

    printf("  |\t");

    for (int i = 0; i < g->nonterminal_set.data_size; i++) {
        printf("%c\t", g->nonterminal_set.data[i]);
    }

    printf("\n");

    // Contents

    for (int i = 0; i < num_items; i++) {
        printf("I%d:\t", i);
        for (int j = 0; j < g->terminal_set.data_size; j++) {
            switch (p->action_table[i][j].type) {
            case ACTION_ERROR:
                printf("_\t");
                break;

            case ACTION_SHIFT:
                printf("S%d\t", p->action_table[i][j].target_state);
                break;

            case ACTION_REDUCE:
                printf("R%d\t", p->action_table[i][j].target_state);
                break;

            case ACTION_ACCEPT:
                printf("ACCEPT\t");
                break;
            }
        }

        printf("  |\t");

        for (int j = 0; j < g->nonterminal_set.data_size; j++) {
            if (p->goto_table[i][j] == -1) {
                printf("_\t");
            } else {
                printf("%d\t", p->goto_table[i][j]);
            }
        }

        printf("\n");
    }
}


/*
 * -----------------------------------------------------------------------------
 * Parsing Algorithm
 * -----------------------------------------------------------------------------
 */

int grammar_input_check(Grammar *g, LR0ParsingTable *table, char *input)
{
    LR0StackRecord lr_stack[256];
    int lr_stack_top = 0;
    int iteration = 1;
    int input_offset = 0;
    char curr_input = '\0';

    ActionRecord action;

    lr_stack_top++;
    lr_stack[lr_stack_top - 1].symbol = '$'; // Stream end symbol
    lr_stack[lr_stack_top - 1].target_state = 0; // Initial state

    while (lr_stack_top > 0) {

        if (input[input_offset] == '\n' || input[input_offset] == '\0') {
            curr_input = '$';
        } else {
            curr_input = input[input_offset];
        }

        printf("ITERATION  : %d\n", iteration);

        printf("STACK      : [ ");

        for (int i = 0; i < lr_stack_top; i++) {
            printf("<%c, %d> ", lr_stack[i].symbol, lr_stack[i].target_state);
        }

        printf("]\n");

        printf("CURR. INPUT: %c\n", curr_input);

        if (set_order(&g->terminal_set, curr_input) < 0) {
            printf("Error: input symbol '%c' not in grammar. Rejecting Input.\n", curr_input);
            return 0;
        }

        printf("ACTION     : ");

        action = table->action_table
                    [lr_stack[lr_stack_top - 1].target_state]
                    [set_order(&g->terminal_set, curr_input)];

        switch (action.type) {
        case ACTION_ACCEPT:
            printf("Accept Input String.\n");
            return 1;
            break;

        case ACTION_SHIFT:
            printf("Shift '%c' onto stack and goto state %d.\n",
                curr_input, action.target_state);

            lr_stack_top++;
            lr_stack[lr_stack_top - 1].symbol = curr_input;
            lr_stack[lr_stack_top - 1].target_state = action.target_state;

            input_offset++;
            break;

        case ACTION_REDUCE:
            printf("Pop production '");
            int prodset = grammar_find(g, action.nonterminal);
            Production *p = &g->set[prodset].productions[action.production];
            production_print_small(p);
            printf("' from stack...");

            for (int i = p->size - 1; i >= 0; i--) {
                if (lr_stack[lr_stack_top - 1].symbol != p->symbols[i]) {
                    printf("\nSymbol mismatch in production. Rejecting Input.");
                    printf(" ('%c' != '%c')\n", lr_stack[lr_stack_top - 1].symbol, p->symbols[i]);
                    return 0;
                }
                lr_stack_top--;
            }

            int goto_state = table->goto_table
                                [lr_stack[lr_stack_top - 1].target_state]
                                [set_order(&g->nonterminal_set, action.nonterminal)];

            printf(" and reduce it to '%c' on state %d.\n", action.nonterminal, goto_state);

            lr_stack_top++;
            lr_stack[lr_stack_top - 1].symbol = action.nonterminal;
            lr_stack[lr_stack_top - 1].target_state = goto_state;
            break;


        case ACTION_ERROR:
            printf("Error State. Rejecting.\n");
            return 0;
            break;
        }

        iteration++;
        printf("\n");
    }

    return -1;
}

/*
 * -----------------------------------------------------------------------------
 * Main Function
 * -----------------------------------------------------------------------------
 */

// Input Flush
void flush()
{
    int c;
    while (((c = getchar()) != EOF) && (c != '\n'));
}


int main()
{
    Grammar g;
    ItemSet set;
    LR0ParsingTable table;

    char ch;
    char buf[MAXBUF];

    // printf("Starting Program...\n");

    grammar_init(&g);

    printf("To enter the productions of your grammar:\n");
    printf("Enter a nonterminal (capital letter), then a space followed by the\n"
           "contents of its production, then press Enter when done. Enter each\n"
           "alternate production on a separate line. Enter any letter aside\n"
           "from A - Z followed by a newline to stop.\n");
    printf("Use the @ symbol after a nonterminal to denote an epsilon production:\n\n");


    while (1) {
        ch = getchar();
        if (ch < 'A' || ch > 'Z') {
            break;
        }

        scanf("%s", buf);
        grammar_insert_rule(&g, ch, buf);

        // Flush input
        flush();
    }

    // Flush input
    flush();

    printf("\nEnter the starting symbol: ");

    scanf("%c", &ch);

    // Flush input
    flush();

    grammar_set_starting_symbol(&g, ch);

    if (grammar_finish(&g) < 0) {
        printf("Error: Grammar not properly defined.");
        return 1;
    }


    printf("\nInput Grammar Contents:\n"
           "=======================\n");

    grammar_print(&g);

    printf("\n");

    grammar_lr0_augment(&g);

    printf("\nAugmented Grammar:\n"
           "==================\n");

    grammar_print(&g);

    printf("\n");

    item_set_init(&set, 'Z');

    if (grammar_create_lr_table(&g, &set, &table) < 0) {
        printf("LR0 table creation failed. Check your grammar.\n");
        return 1;
    }

    printf("\nDFA Items:\n"
           "==========\n");

    item_set_print(&g, &set);

    printf("\nParsing Table:\n"
           "==============\n");
    grammar_print_lr_table(&g, &table, set.items_size);

    printf("\n");

    printf("\nEnter an expression to parse: ");
    fgets(buf, MAXBUF, stdin);
    int verdict = grammar_input_check(&g, &table, buf);

    printf("\n");

    if (verdict == 1) {
        printf("String Accepted.\n");
    } else {
        printf("String Rejected.\n");
    }
    return 0;
}
