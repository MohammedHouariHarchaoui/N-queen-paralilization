#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <mpi.h>

// An abstract representation of an NxN chess board to tracking open positions
struct chess_board
{
    uint32_t n_size;           // Number of queens on the NxN chess board
    uint32_t *queen_positions; // Store queen positions on the board
    uint32_t *column;          // Store available column moves/attacks
    uint32_t *diagonal_up;     // Store available diagonal moves/attacks
    uint32_t *diagonal_down;
    uint32_t column_j;   // Stores column to place the next queen in
    uint64_t placements; // Tracks total number queen placements
    uint64_t solutions;  // Tracks number of solutions
    //indicate the intervals of rows that will be verified for each process
    uint64_t start;
    uint64_t end;
};
struct chess_board *board;

// Handles dynamic memory allocation of the arrays and sets initial values
static void initialize_board(const uint32_t n_queens, struct chess_board **board, uint32_t start, uint32_t end)
{
    if (n_queens < 1)
    {
        fprintf(stderr, "The number of queens must be greater than 0.\n");
        exit(EXIT_SUCCESS);
    }

    // Dynamically allocate memory for chessboard struct
    *board = malloc(sizeof(struct chess_board));
    if (*board == NULL)
    {
        fprintf(stderr, "Memory allocation failed for chess board.\n");
        exit(EXIT_FAILURE);
    }

    // Dynamically allocate memory for chessboard arrays that track positions
    const uint32_t diagonal_size = 2 * n_queens - 1;
    const uint32_t total_size = 2 * (n_queens + diagonal_size);
    (*board)->queen_positions = malloc(sizeof(uint32_t) * total_size);
    if ((*board)->queen_positions == NULL)
    {
        fprintf(stderr, "Memory allocation failed for the chess board arrays.\n");
        free(*board);
        exit(EXIT_FAILURE);
    }
    (*board)->column = &(*board)->queen_positions[n_queens];
    (*board)->diagonal_up = &(*board)->column[n_queens];
    (*board)->diagonal_down = &(*board)->diagonal_up[diagonal_size];

    // Initialize the chess board parameters
    (*board)->n_size = n_queens;
    for (uint32_t i = 0; i < n_queens; ++i)
    {
        (*board)->queen_positions[i] = 0;
    }
    for (uint32_t i = n_queens; i < total_size; ++i)
    {
        // Initializes values for column, diagonal_up, and diagonal_down
        (*board)->queen_positions[i] = 1;
    }
    (*board)->column_j = 0;
    (*board)->placements = 0;
    (*board)->solutions = 0;
    (*board)->start = start;
    (*board)->end = end;
}



// Check if a queen can be placed in at row 'i' of the current column
static uint32_t square_is_free(const uint32_t row_i, struct chess_board *board)
{
    return board->column[row_i] &
           board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)] &
           board->diagonal_down[board->column_j + row_i];
}

// Place a queen on the chess board at row 'i' of the current column
static void set_queen(const uint32_t row_i, struct chess_board *board)
{
    board->queen_positions[board->column_j] = row_i;
    board->column[row_i] = 0;
    board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)] = 0;
    board->diagonal_down[board->column_j + row_i] = 0;
    ++board->column_j;
    ++board->placements;
}

// Removes a queen from the NxN chess board in the given column to backtrack
static void remove_queen(const uint32_t row_i, struct chess_board *board)
{
    --board->column_j;
    board->diagonal_down[board->column_j + row_i] = 1;
    board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)] = 1;
    board->column[row_i] = 1;
}

// Recursive function for finding valid queen placements on the chess board
uint64_t place_next_queen_parallel(struct chess_board *board)
{
    // retrieve the interval to be verified by a specific thread
    uint64_t start = board->start;
    uint64_t end = board->end;
    // uint64_t row_boundary = board->end;
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    for (uint32_t row_i = start; row_i < end; ++row_i)
    {
        if (square_is_free(row_i, board))
        {
            set_queen(row_i, board);
            if (board->column_j == board->n_size)
            {
                // Due to 2-fold symmetry of the chess board, accurate counts can be
                // obtained by only searching half the board and double-counting each
                // solution found (the sole exception being the 1-Queen problem)
                board->solutions += 2;
            }
            else if (board->queen_positions[0] != middle)
            {
                board->start = 0;
                board->end = board->n_size;
                place_next_queen_parallel(board);
            }
            else
            {
                // This branch should only execute once at most, and only for odd
                // numbered N-Queens problems
                board->start = 0;
                board->end = middle;
                place_next_queen_parallel(board);
            }
            remove_queen(row_i, board);
        }
    }
    return board->solutions;
}

uint64_t place_next_queen_sequentiel(struct chess_board *board)
{
    uint64_t end = board->end;
    // uint64_t row_boundary = board->end;
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    for (uint32_t row_i = 0; row_i < end; ++row_i)
    {
        if (square_is_free(row_i, board))
        {
            set_queen(row_i, board);
            if (board->column_j == board->n_size)
            {
                // Due to 2-fold symmetry of the chess board, accurate counts can be
                // obtained by only searching half the board and double-counting each
                // solution found (the sole exception being the 1-Queen problem)
                board->solutions += 2;
            }
            else if (board->queen_positions[0] != middle)
            {
                board->start = 0;
                board->end = board->n_size;
                place_next_queen_parallel(board);
            }
            else
            {
                // This branch should only execute once at most, and only for odd
                // numbered N-Queens problems
                board->start = 0;
                board->end = middle;
                place_next_queen_parallel(board);
            }
            remove_queen(row_i, board);
        }
    }
    return board->solutions;
}

int main(int argc, char *argv[])
{
    static const uint32_t default_n = 4;
    int num_procs = 2;
    int rank;

    const uint32_t n_queens = (argc != 1) ? (uint32_t)atoi(argv[1]) : default_n;
    
    //parallel
    clock_t start_time = clock();

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // initialize_board(n_queens,board);
    // Determines the index for the middle row to take advantage of board
    // symmetry when searching for solutions
    const uint32_t row_boundary = (n_queens >> 1) + (n_queens & 1);

    struct chess_board *board;

    //devide the first colonne between processes
    uint32_t start = rank * row_boundary / num_procs;
    uint32_t end = (rank + 1) * row_boundary / num_procs;
    initialize_board(n_queens, &board, start, end);  

    uint64_t solutions = place_next_queen_parallel(board);

    uint64_t total_solutions = 0;
    //Perform reduction operation to gather the local solutions from each process and compute the total solutions
    MPI_Reduce(&solutions, &total_solutions, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
    
    // Print the total number of solutions from the root process
    if (rank == 0) {
        clock_t end_time = clock();
        double time_totale = (double)(end_time-start_time)/CLOCKS_PER_SEC;
        printf("program takes : %f s \n",time_totale);
        printf("The %u-Queens problem has %lu solutions\n", n_queens, total_solutions);
    }

    // Clean up the MPI environment
    MPI_Finalize();

    //sequential
    //uint32_t end = row_boundary;
    //place_next_queen_sequentiel(board);
    
    return EXIT_SUCCESS;
}



    

