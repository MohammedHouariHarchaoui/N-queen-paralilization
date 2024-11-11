#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <mpi.h>

#define MAX_BOARDS 5

// An abstract representation of an NxN chess board to track open positions
typedef struct chess_board {
    uint32_t n_size;           // Number of queens on the NxN chess board
    uint32_t *queen_positions; // Store queen positions on the board
    uint32_t *column;          // Store available column moves/attacks
    uint32_t *diagonal_up;     // Store available diagonal moves/attacks
    uint32_t *diagonal_down;
    uint32_t column_j;   // Stores column to place the next queen in
    uint64_t placements; // Tracks the total number of queen placements
    uint64_t solutions;  // Tracks the number of solutions
    //indicate the intervals of rows that will be verified for each process
    uint64_t start;
    uint64_t end;
} Board;

void defineTaskStruct(MPI_Datatype *mpiTaskType) {
    int blockLengths[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    MPI_Aint displacements[8];
    MPI_Datatype types[8] = {
        MPI_UINT32_T, MPI_UINT32_T, MPI_UINT32_T, MPI_UINT32_T,
        MPI_UINT32_T, MPI_UINT64_T, MPI_UINT64_T, MPI_UINT64_T
    };

    Board board;
    MPI_Get_address(&(board.n_size), &displacements[0]);
    MPI_Get_address(&(board.queen_positions), &displacements[1]);
    MPI_Get_address(&(board.column), &displacements[2]);
    MPI_Get_address(&(board.diagonal_up), &displacements[3]);
    MPI_Get_address(&(board.diagonal_down), &displacements[4]);
    MPI_Get_address(&(board.column_j), &displacements[5]);
    MPI_Get_address(&(board.placements), &displacements[6]);
    MPI_Get_address(&(board.solutions), &displacements[7]);

    for (int i = 1; i < 8; i++) {
        displacements[i] -= displacements[0];
    }
    displacements[0] = 0;

    MPI_Type_create_struct(8, blockLengths, displacements, types, mpiTaskType);
    MPI_Type_commit(mpiTaskType);
}

Board task[MAX_BOARDS];

int main(int argc, char *argv[]) {
    static const uint32_t default_n = 4;
    int num_procs = 2;
    int rank;

    const uint32_t n_queens = (argc != 1) ? (uint32_t)atoi(argv[1]) : default_n;

    // Parallel
    clock_t start_time = clock();

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        // Initialize tasks
        for (int i = 0; i < MAX_BOARDS; i++) {
            task[i].column_j = 123 + i;
            task[i].n_size = 456 + i;
            task[i].start = 0+i;
            task[i].end = 0+i;
            task[i].placements = 5+i;
            task[i].solutions = 8+i;

            task[i].diagonal_down = malloc(sizeof(uint32_t)); // Allocate memory for diagonal_down
            task[i].diagonal_down[0] = 50 + i;
            task[i].diagonal_up = malloc(sizeof(uint32_t)); // Allocate memory for diagonal_down
            task[i].diagonal_up[0] = 50 + i;
            task[i].queen_positions = malloc(sizeof(uint32_t)); // Allocate memory for diagonal_down
            task[i].queen_positions[0]= 50 + i;
            
            printf("col: %u\n", task[i].column_j);
            printf("n: %u\n", task[i].n_size);
            printf("diag: %u\n", task[i].diagonal_down[0]);
            printf("diagup: %u\n", task[i].diagonal_up[0]);
            printf("pos: %u\n", task[i].queen_positions[0]);
            printf("placements: %u\n", task[i].placements);
            printf("solutions: %u\n", task[i].solutions);

        }
    }

    MPI_Datatype mpiTaskType;
    defineTaskStruct(&mpiTaskType);

    int packedSize;
    MPI_Pack_size(MAX_BOARDS, mpiTaskType, MPI_COMM_WORLD, &packedSize);

    char *buffer = malloc(packedSize);
    int position = 0;

    if (rank == 0) {
        // Pack the tasks into a buffer
        for (int i = 0; i < MAX_BOARDS; i++) {
            MPI_Pack(&task[i].column_j, 1, MPI_UINT32_T, buffer, packedSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&task[i].start, 1, MPI_UINT32_T, buffer, packedSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&task[i].end, 1, MPI_UINT32_T, buffer, packedSize, &position, MPI_COMM_WORLD);

            MPI_Pack(&task[i].placements, 1, MPI_UINT32_T, buffer, packedSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&task[i].solutions, 1, MPI_UINT32_T, buffer, packedSize, &position, MPI_COMM_WORLD);

            MPI_Pack(&task[i].n_size, 1, MPI_UINT32_T, buffer, packedSize, &position, MPI_COMM_WORLD);
            MPI_Pack(task[i].diagonal_down, 1, MPI_UINT32_T, buffer, packedSize, &position, MPI_COMM_WORLD);
            MPI_Pack(task[i].diagonal_up, 1, MPI_UINT32_T, buffer, packedSize, &position, MPI_COMM_WORLD);
            MPI_Pack(task[i].queen_positions, 1, MPI_UINT32_T, buffer, packedSize, &position, MPI_COMM_WORLD);

        }
    }

    // Broadcast the packed buffer to all processes
    MPI_Bcast(buffer, packedSize, MPI_PACKED, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        // Unpack the tasks from the received buffer
        for (int i = 0; i < MAX_BOARDS; i++) {
            const uint32_t diagonal_size = 2 * n_queens - 1;
            const uint32_t total_size = 2 * (n_queens + diagonal_size);

            MPI_Unpack(buffer, packedSize, &position, &task[i].column_j, 1, MPI_UINT32_T, MPI_COMM_WORLD);
            MPI_Unpack(buffer, packedSize, &position, &task[i].start, 1, MPI_UINT32_T, MPI_COMM_WORLD);
            MPI_Unpack(buffer, packedSize, &position, &task[i].end, 1, MPI_UINT32_T, MPI_COMM_WORLD);
            MPI_Unpack(buffer, packedSize, &position, &task[i].placements, 1, MPI_UINT32_T, MPI_COMM_WORLD);
            MPI_Unpack(buffer, packedSize, &position, &task[i].solutions, 1, MPI_UINT32_T, MPI_COMM_WORLD);
            MPI_Unpack(buffer, packedSize, &position, &task[i].n_size, 1, MPI_UINT32_T, MPI_COMM_WORLD);
            task[i].diagonal_down = malloc(sizeof(uint32_t)); // Allocate memory for diagonal_down
            MPI_Unpack(buffer, packedSize, &position, task[i].diagonal_down, 1, MPI_UINT32_T, MPI_COMM_WORLD);
            task[i].diagonal_up = malloc(sizeof(uint32_t)); // Allocate memory for diagonal_down
            MPI_Unpack(buffer, packedSize, &position, task[i].diagonal_up, 1, MPI_UINT32_T, MPI_COMM_WORLD);
            task[i].queen_positions = malloc(sizeof(uint32_t)); // Allocate memory for diagonal_down
            MPI_Unpack(buffer, packedSize, &position, task[i].queen_positions, 1, MPI_UINT32_T, MPI_COMM_WORLD);

        }
    }

    if (rank != 0) {
        // Print the unpacked tasks
        for (int i = 0; i < MAX_BOARDS; i++) {
            printf("col: %u\n", task[i].column_j);
            printf("n: %u\n", task[i].n_size);
            printf("diag: %u\n", task[i].diagonal_down[0]);
            printf("diagup: %u\n", task[i].diagonal_up[0]);
            printf("pos: %u\n", task[i].queen_positions[0]);
            printf("start: %u\n", task[i].start);
            printf("end: %u\n", task[i].end);
            printf("placements: %u\n", task[i].placements);
            printf("solutions: %u\n", task[i].solutions);

        }
    }

    free(buffer);
    MPI_Type_free(&mpiTaskType);
    MPI_Finalize();
    return EXIT_SUCCESS;
}
