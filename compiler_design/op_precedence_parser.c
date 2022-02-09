#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

#define MAXBUF 4096

typedef struct ThreeAddressCode {
    char lhs;
    char operand1;
    char operand2;
    char operator;
} ThreeAddressCode;

typedef enum OperatorRelation {
    ER, // Error
    LT, // Less Than
    GT, // Greater than
    ACC // Accept
} OperatorRelation;

OperatorRelation precedence_table[20][20] = {
        /* +    -    *    /    (    )    ^    id    $    */
/* +  */ { GT,  GT,  LT,  LT,  LT,  GT,  LT,  LT,  GT  },
/* -  */ { GT,  GT,  LT,  LT,  LT,  GT,  LT,  LT,  GT  },
/* *  */ { GT,  GT,  GT,  GT,  LT,  GT,  LT,  LT,  GT  },
/* /  */ { GT,  GT,  GT,  GT,  LT,  GT,  LT,  LT,  GT  },
/* (  */ { LT,  LT,  LT,  LT,  LT,  LT,  LT,  LT,  ER  },
/* )  */ { GT,  GT,  GT,  GT,  GT,  GT,  GT,  ER,  GT  },
/* ^  */ { GT,  GT,  GT,  GT,  LT,  GT,  LT,  LT,  GT  },
/* id */ { GT,  GT,  GT,  GT,  ER,  GT,  GT,  ER,  GT  },
/* $  */ { LT,  LT,  LT,  LT,  LT,  GT,  LT,  LT,  ACC },
};

int is_operator(char s)
{
    return  (s == '+') ||
            (s == '-') ||
            (s == '*') ||
            (s == '/') ||
            (s == '(') ||
            (s == ')') ||
            (s == '^') ||
            (s == '$');
}

int operator_order(char s)
{
    switch (s) {
        default:  return 7; break;
        case '+': return 0; break;
        case '-': return 1; break;
        case '*': return 2; break;
        case '/': return 3; break;
        case '(': return 4; break;
        case ')': return 5; break;
        case '^': return 6; break;
        case '$': return 8; break;
    }
}

int operator_parse(char *input, char postfix_code[100], int *postfix_code_top)
{
    char sr_stack[256];
    int sr_stack_top = 0;
    int input_offset = 0;
    char curr_input;
    int iteration = 0;

    int curr_terminal = 0;

    const char nonterminal = 'T';

    sr_stack_top++;
    sr_stack[sr_stack_top - 1] = '$';

    while (1) {

        if (input[input_offset] == '\n' || input[input_offset] == '\0') {
            curr_input = '$';
        } else {
            curr_input = input[input_offset];
        }

        printf("ITERATION  : %d\n", iteration);

        printf("STACK      : [ ");

        for (int i = 0; i < sr_stack_top; i++) {
            printf("%c ", sr_stack[i]);
        }

        printf("]\n");

        printf("CURR. TERM.: %c\n", sr_stack[curr_terminal]);

        printf("CURR. INPUT: %c\n", curr_input);

        printf("POSTFIX    : ");

        for (int i = 0; i < *postfix_code_top; i++) {
            printf("%c ", postfix_code[i]);
        }

        printf("\n");

        printf("ACTION     : ");

        switch (precedence_table[operator_order(sr_stack[curr_terminal])]
                                [operator_order(curr_input)]) {
        case LT:
            printf("Shifting '%c' to stack.\n", curr_input);
            sr_stack_top++;
            sr_stack[sr_stack_top - 1] = curr_input;
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
            if (sr_stack[sr_stack_top - 1] == nonterminal) {
                if (sr_stack_top == 1 + 1 || sr_stack_top == 1 + 2) {
                    printf("Erroneous Input. Exiting.\n");
                    return 0;
                }

                printf("Convert id <op> id to %c\n", nonterminal);
                sr_stack_top--; // Remove Nonterminal

                // Insert operator into postfix string
                (*postfix_code_top)++;
                postfix_code[*postfix_code_top - 1] = sr_stack[sr_stack_top - 1];

                sr_stack_top--; // Remove operator

                curr_terminal = sr_stack_top - 1 - 1;

            // Handle case of top of stack being operator. Shouldn't happen.
            } else if (sr_stack[sr_stack_top - 1] == ')') {
                if (sr_stack_top < 3) {
                    printf("Erroneous Input. Exiting");
                    return 0;
                }
                sr_stack_top--; // Remove operator
                sr_stack_top--; // Remove nonterminal

                // Replace with nonterminal
                sr_stack[sr_stack_top - 1] = nonterminal;

                curr_terminal = sr_stack_top - 1 - 1;

                printf("Convert ( T ) to %c\n", nonterminal);

            } else if (is_operator(sr_stack[sr_stack_top - 1])) {
                printf("Erroneous Input (or table?). Exiting.\n");
                return 0;

            // Handle case of top of stack being id
            } else {
                // Push to stack
                // Replace with T
                printf("Convert identifier to %c\n", nonterminal);
                (*postfix_code_top)++;
                postfix_code[*postfix_code_top - 1] = sr_stack[sr_stack_top - 1];
                sr_stack[sr_stack_top - 1] = nonterminal;
                curr_terminal = sr_stack_top - 1 - 1;
            }
            break;

        case ER:
            printf("Erroneous Input. Exiting.\n");
            return 0;
            break;

        case ACC:
            printf("Input Accepted.\n");
            return 1;
            break;

        default:
            printf("Table error on [%d, %d] = %d. Exiting.\n",
            operator_order(sr_stack[sr_stack_top - 1]),
            operator_order(curr_input),
            precedence_table[operator_order(sr_stack[curr_terminal])]
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

    printf("Available Operators:  +, -, *, /\n");
    printf("Please enter a single character per identifier.\n");
    printf("\nEnter an expression to parse: ");
    fgets(buf, MAXBUF, stdin);
    int verdict = operator_parse(buf, postfix_code, &postfix_code_top);

    printf("\n");

    if (verdict == 1) {
        printf("String Accepted.\n");
    } else {
        printf("String Rejected.\n");
    }
    return 0;
}
