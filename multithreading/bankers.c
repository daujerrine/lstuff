/*
 * In a system, there are three types of resources: R1, R2 and R3. Four processes
 * P1, P2, P3 and P4 execute concurrently. At the outset, the processes have
 * declared their maximum resource requirements using a matrix named Max as given
 * below. For example, Max [P2, R2] is the maximum number of instances of R2 that
 * P2 would require. The number of instances of the resources allocated to the
 * various processes at any given state is given by a matrix named Allocation.
 *
 * Consider a state of the system with the Allocation matrix as shown below, and
 * in which 3 instances of R1 and 3 instances of R2 are the only resources
 * available. Write a program to check if the system is in safe state or not
 * using Banker’s algorithm.
 */

#include <stdio.h>

// Matrices
int total[20][20];     // Max
int allocated[20][20]; // Allocation
int need[20][20];      // Need

int available[20];        // Currently available resources
int work[20];             // Current Work
int finished[20] = { 0 }; // Resources satisfied for process N, initialised to 0

int order[20] = { -1 };
int order_offset = 0;

int main()
{
    int nproc, nres;
    printf("NOTE: Max limit is 20 processes.\n\n");

    printf("Enter Number of processes: ");
    scanf("%d", &nproc);
    printf("Enter Number of resources: ");
    scanf("%d", &nres);

    // INPUT MAX MATRIX

    printf("On each line, enter the required number (MAX) of instances from each resource ");
    for (int i = 0; i < nres; i++)
        printf("R%d ", i);
    printf(":\n");

    for(int i = 0; i < nproc; i++) {
        printf("P%d: ", i);
        for (int j = 0; j < nres; j++) {
            scanf("%d", &total[i][j]);
        }
    }

    printf("\nResultant table: \n");
    printf("Px\t");
    for(int i = 0; i < nres; i++)
        printf("R%d\t", i);
    printf("\n");
    for(int i = 0; i < nproc; i++) {
        printf("P%d\t", i);
        for (int j = 0; j < nres; j++) {
            printf("%d\t", total[i][j]);
        }
        printf("\n");
    }
    printf("\n\n");

    // INPUT ALLOCATION MATRIX

    printf("On each line, enter the currently allocated of instances to each resource ");
    for (int i = 0; i < nres; i++)
        printf("R%d ", i);
    printf(":\n");

    for(int i = 0; i < nproc; i++) {
        printf("P%d: ", i);
        for (int j = 0; j < nres; j++) {
            scanf("%d", &allocated[i][j]);
        }
    }

    printf("\nResultant table: \n");
    printf("Px\t");
    for(int i = 0; i < nres; i++)
        printf("R%d\t", i);
    printf("\n");
    for(int i = 0; i < nproc; i++) {
        printf("P%d\t", i);
        for (int j = 0; j < nres; j++) {
            printf("%d\t", allocated[i][j]);
        }
        printf("\n");
    }
    printf("\n\n");

    // GET CURRENT AVAILABLE RESOURCES

    printf("Enter Currently available resources for ");
    for(int i = 0; i < nres; i++)
        printf("R%d ", i);
    printf("separated by space:\n");
    for (int i = 0; i < nres; i++) {
        scanf("%d", &available[i]);
    }

    // COPY OVER TO WORK STATE
    for (int i = 0; i < nres; i++) {
        work[i] = available[i];
    }

    // NEED MATRIX CALCULATION

    for(int i = 0; i < nproc; i++) {
        for (int j = 0; j < nres; j++) {
            need[i][j]  = total[i][j] - allocated[i][j];
            if (need[i][j] < 0) {
                printf("Error in calculation: difference of total - allocated at\n" \
                       "row %d column %d is negative.\n", i, j);
                return 1;
            }
        }
    }

    printf("\nResultant need matrix: \n");
    printf("Px\t");
    for(int i = 0; i < nres; i++)
        printf("R%d\t", i);
    printf("\n");
    for(int i = 0; i < nproc; i++) {
        printf("P%d:\t", i);
        for (int j = 0; j < nres; j++) {
            printf("%d\t", need[i][j]);
        }
        printf("\n");
    }
    printf("\n\n");

    int proc_count = nproc;
    int unsafe = 1;
    int greater = 1;

    while (proc_count > 0) {
        unsafe = 1;

        for (int i = 0; i < nproc; i++) {
            greater = 1;

            if (finished[i] == 1) // Already finished?
                continue;

            for (int j = 0; j < nres; j++) {
                if (work[j] < need[i][j]) {
                    greater = 0;
                    break;
                }
            }

            if (greater == 1) {
                unsafe = 0;
                finished[i] = 1;
                for (int j = 0; j < nres; j++) {
                    work[j] += allocated[i][j];
                }
                order[order_offset++] = i;
                proc_count--;
            }
        }

        if (unsafe == 1)
            break;
    }

    if (unsafe == 1) {
        printf("System in UNSAFE state.");
        return 0;
    } else {
        printf("System in SAFE state. Execution order:\n");
        for (int i = 0; i < order_offset; i++)
            printf("P%d -> ", order[i]);
            printf("END\n");
    }
    return 0;
}
