#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

#define MAXBUF 4096

typedef struct ThreeAddressCode {
    char op1;
    int op1_intermediate;
    int  op1_id;

    char op2;
    int op2_intermediate;
    int op2_id;

    char operator;
    int result;
} ThreeAddressCode;


typedef struct StackRecord {
    int is_nonterminal;
    int nonterminal_id;
    int symbol;
} StackRecord;

typedef enum OperatorRelation {
    ER, // Error
    LT, // Less Than
    GT, // Greater than
    ACC // Accept
} OperatorRelation;

OperatorRelation precedence_table[20][20] = {
        /* id    +    -    *    /    $   */
/* id */ { ER,  GT,  GT,  GT,  GT,  GT  },
/* +  */ { LT,  GT,  GT,  LT,  LT,  GT  },
/* -  */ { LT,  GT,  GT,  LT,  LT,  GT  },
/* *  */ { LT,  GT,  GT,  GT,  GT,  GT  },
/* /  */ { LT,  GT,  GT,  GT,  GT,  GT  },
/* $  */ { LT,  LT,  LT,  LT,  LT,  ACC },
};

int is_operator(char s)
{
    return  (s == '+') ||
            (s == '-') ||
            (s == '*') ||
            (s == '/') ||
            (s == '$');
}

int operator_order(char s)
{
    switch (s) {
        default:  return 0; break;
        case '+': return 1; break;
        case '-': return 2; break;
        case '*': return 3; break;
        case '/': return 4; break;
        case '$': return 5; break;
    }
}

int operator_parse(char *input,
                   char postfix_code[100],
                   int *postfix_code_top,
                   ThreeAddressCode tac[100],
                   int *tac_top)
{
    StackRecord sr_stack[256];
    int sr_stack_top = 0;
    int input_offset = 0;
    char curr_input;
    int iteration = 0;
    int nonterminal_id = 0;

    int curr_terminal = 0;

    const char nonterminal = 'T';

    *tac_top = 0;
    *postfix_code_top = 0;

    sr_stack_top++;
    sr_stack[sr_stack_top - 1].symbol = '$';
    sr_stack[sr_stack_top - 1].is_nonterminal = 0;

    while (1) {

        if (input[input_offset] == '\n' || input[input_offset] == '\0') {
            curr_input = '$';
        } else {
            curr_input = input[input_offset];
        }

        printf("ITERATION  : %d\n", iteration);

        printf("STACK      : [ ");

        for (int i = 0; i < sr_stack_top; i++) {
            if (sr_stack[i].is_nonterminal) {
                printf("%c%d ", sr_stack[i].symbol, sr_stack[i].nonterminal_id);
            } else {
                printf("%c ", sr_stack[i].symbol);
            }
        }

        printf("]\n");

        printf("CURR. TERM.: %c\n", sr_stack[curr_terminal].symbol);

        printf("CURR. INPUT: %c\n", curr_input);

        printf("POSTFIX    : ");

        for (int i = 0; i < *postfix_code_top; i++) {
            printf("%c ", postfix_code[i]);
        }

        printf("\n");

        printf("ACTION     : ");

        switch (precedence_table[operator_order(sr_stack[curr_terminal].symbol)]
                                [operator_order(curr_input)]) {
        case LT:
            printf("Shifting '%c' to stack.\n", curr_input);
            sr_stack_top++;
            sr_stack[sr_stack_top - 1].symbol = curr_input;
            sr_stack[sr_stack_top - 1].is_nonterminal = 0;
            curr_terminal = sr_stack_top - 1;
            input_offset++;
            break;

        case GT:
            // The following patterns will be reduced by the reduction
            // algorithm:
            // T -> id
            // T -> T <operator> T

            // Handle top three elements being T <operator> T
            // Also Handle case of having too few to have a valid production
            // "+ T" or "$".
            if (sr_stack[sr_stack_top - 1].is_nonterminal) {
                if (sr_stack_top == 1 + 1 || sr_stack_top == 1 + 2) {
                    printf("Erroneous Input. Exiting.\n");
                    return 0;
                }

                printf("Convert id <op> id to %c ", nonterminal);
                printf("(%c%d <- %c%d ",
                    nonterminal, nonterminal_id,
                    sr_stack[sr_stack_top - 1].symbol,
                    sr_stack[sr_stack_top - 1].nonterminal_id);

                (*tac_top)++;
                tac[*tac_top - 1].op1 = sr_stack[sr_stack_top - 1].symbol;
                tac[*tac_top - 1].op1_intermediate = 1;
                tac[*tac_top - 1].op1_id = sr_stack[sr_stack_top - 1].nonterminal_id;

                sr_stack_top--; // Remove Nonterminal

                // Insert operator into postfix string
                (*postfix_code_top)++;
                postfix_code[*postfix_code_top - 1] = sr_stack[sr_stack_top - 1].symbol;

                printf("%c ", sr_stack[sr_stack_top - 1].symbol);

                tac[*tac_top - 1].operator = sr_stack[sr_stack_top - 1].symbol;

                sr_stack_top--; // Remove operator

                printf("%c%d)\n",
                    sr_stack[sr_stack_top - 1].symbol,
                    sr_stack[sr_stack_top - 1].nonterminal_id);

                tac[*tac_top - 1].op2 = sr_stack[sr_stack_top - 1].symbol;
                tac[*tac_top - 1].op2_intermediate = 1;
                tac[*tac_top - 1].op2_id = sr_stack[sr_stack_top - 1].nonterminal_id;

                tac[*tac_top - 1].result = nonterminal_id;

                sr_stack[sr_stack_top - 1].nonterminal_id = nonterminal_id;
                nonterminal_id++;
                curr_terminal = sr_stack_top - 1 - 1;

            // Handle case of top of stack being operator. Shouldn't happen.
            } else if (is_operator(sr_stack[sr_stack_top - 1].symbol)) {
                printf("Erroneous Input (or table?). Exiting.\n");
                return 0;

            // Handle case of top of stack being id
            } else {
                // Push to stack
                // Replace with T
                printf("Convert identifier to %c ", nonterminal);
                (*postfix_code_top)++;
                postfix_code[*postfix_code_top - 1] = sr_stack[sr_stack_top - 1].symbol;

                (*tac_top)++;
                tac[*tac_top - 1].op1 = sr_stack[sr_stack_top - 1].symbol;

                sr_stack[sr_stack_top - 1].symbol = nonterminal;
                sr_stack[sr_stack_top - 1].is_nonterminal = 1;
                sr_stack[sr_stack_top - 1].nonterminal_id = nonterminal_id;
                printf("(%c%d <- %c)\n", nonterminal, nonterminal_id, postfix_code[*postfix_code_top - 1]);

                tac[*tac_top - 1].op1_intermediate = 0;
                tac[*tac_top - 1].op2_intermediate = -1;
                tac[*tac_top - 1].operator = 'L';
                tac[*tac_top - 1].result = nonterminal_id;

                nonterminal_id++;
                curr_terminal = sr_stack_top - 1 - 1;
            }
            break;

        case ER:
            printf("Erroneous Input. Exiting.\n");
            return 0;
            break;

        case ACC:
            printf("Accept Input.\n");
            return 1;
            break;

        default:
            printf("Table error on [%d, %d] = %d. Exiting.\n",
            operator_order(sr_stack[sr_stack_top - 1].symbol),
            operator_order(curr_input),
            precedence_table[operator_order(sr_stack[curr_terminal].symbol)]
                                [operator_order(curr_input)]);
            return -1;
            break;
        }

        printf("\n");
        iteration++;
    }

    return -1;
}

int main()
{
    char buf[MAXBUF];
    char postfix_code[100] = { 0 };
    int  postfix_code_top = 0;

    ThreeAddressCode tac[100] = { 0 };
    int tac_top = 0;

    printf("Available Operators:  +, -, *, /\n");
    printf("Please enter a single character per identifier.\n");
    printf("\nEnter an expression to parse: ");
    fgets(buf, MAXBUF, stdin);
    int verdict = operator_parse(buf, postfix_code, &postfix_code_top, tac, &tac_top);

    printf("\n");

    if (verdict == 1) {
        printf("String Accepted.\n");

        printf("\nQuadruple Notation:\n====================\nOPER\tARG1\tARG2\tRESULT\n");

        for (int i = 0; i < tac_top; i++) {
            printf("%c\t", tac[i].operator);

            if (tac[i].op1_intermediate == 1) {
                printf("T%d",tac[i].op1_id);
            } else if (tac[i].op1_intermediate == 0) {
                printf("%c", tac[i].op1);
            }

            printf("\t");

            if (tac[i].op2_intermediate == 1) {
                printf("T%d",tac[i].op2_id);
            } else if (tac[i].op2_intermediate == 0) {
                printf("%c", tac[i].op2);
            }

            printf("\t");

            printf("T%d\n",tac[i].result);
        }
    } else {
        printf("String Rejected.\n");
    }
    return 0;
}
