#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <mpi.h>

uint64_t partial_solutions = 0;
uint64_t total_solutions = 0;
uint64_t solutions = 0; // Shared variable to store the sum of solutions

int nthreads = 4;

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
    //indicate the intervals of rows that will be verified for each thread or process
    uint64_t start;
    uint64_t end;
};

struct chess_board *board;

struct chess_board *copyBoard(const struct chess_board *board);

void place_next_queen_sequential(const uint32_t row_boundary, struct chess_board  *board);

// The parallel part of the algorithm, this function generates tasks to be executed by the threads.
void place_next_queen_parallel(const uint32_t row_boundary, struct chess_board  *board);
/** Launch the parallel process of placing queens on the chessboard. **/
void place_queens(const uint32_t row_boundary, struct chess_board  *board);


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


#pragma omp threadprivate(partial_solutions)
/** Lancer le processus de placement parallèle des reines sur l'échiquier. **/
void place_queens(const uint32_t row_boundary, struct chess_board *board)

{
    //crée une région parallèle avec nqueens threads
    #pragma omp parallel num_threads(nthreads) 
    {
        //Initialiser le nombre de solutions partielles et de placements partiels pour chaque thread
        partial_solutions = 0;

        //Assurer que la ligne suivante soit appelée par un seul thread et que les autres threads ne l'attendent pas
        #pragma omp single 
        {
            //Fonction parallélisée permettant de placer la reine suivante sur l'échiquier board.
            place_next_queen_parallel(row_boundary, board);
        }

        //Additionner le nombre de solutions et de placements trouvés par chaque thread
        #pragma omp critical
        total_solutions += partial_solutions;
    }

    
}

/*
void place_queens_without_parallelization(const uint32_t row_boundary,struct chess_board *board){
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    for (uint32_t row_i = 0; row_i < row_boundary; ++row_i) {
        if (square_is_free(row_i, board)) {
            set_queen(row_i, board);
            if (board->column_j == board->n_size) {

                total_solutions += 2;
            } else if (board->queen_positions[0] != middle) {
                place_next_queen(board->n_size, board);
            } else {
                place_next_queen(middle, board);
            }
            remove_queen(row_i, board);
        }
    }
}*/

//la partie parallèle de l'algorithme, cette fonction génère des tâches à exécuter pour les threads 
void place_next_queen_parallel(const uint32_t row_boundary, struct chess_board *board) {
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    uint64_t start = board->start;
    uint64_t end = board->end;
    struct chess_board *local_board;

    for (uint32_t row_i = start; row_i < end; ++row_i) {  
        
        if (square_is_free(row_i, board)) 
        {
            #pragma omp task firstprivate(row_i) mergeable 
            {       
                local_board = copyBoard(board);

                set_queen(row_i, local_board);

                local_board->start = 0;
                local_board->end = local_board->n_size;
                uint32_t limit = local_board->n_size;
                if (local_board->queen_positions[0] == middle) {
                    local_board->end = middle;
                    limit = middle;
                }


                if(local_board->column_j < local_board->n_size / 4 + 1) { 
                
                    place_next_queen_parallel(limit, local_board);
                } else {
                    place_next_queen_sequential(limit, local_board);
                }

            #pragma omp taskyield
            
            }
        }
    }
}
void place_next_queen_sequential(const uint32_t row_boundary, struct chess_board *board)
{
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;


    for (uint32_t row_i = 0; row_i < row_boundary; ++row_i)
    {
        if (square_is_free(row_i, board))
        {
            set_queen(row_i, board);


            if (board->column_j == board->n_size){
                partial_solutions += 2;
            } else {
                uint32_t limit = board->n_size;
                if (board->queen_positions[0] == middle) {
                    limit = middle;
                }

                place_next_queen_sequential(limit, board);
            }


            remove_queen(row_i, board);
        }
    }
}


struct chess_board *copyBoard(const struct chess_board *board)

{
  struct chess_board *task = malloc(sizeof(struct chess_board));

  if (board->n_size < 1)
  {
    fprintf(stderr, "The number of queens must be greater than 0.\n");
    exit(EXIT_SUCCESS);
  }

  // Dynamically allocate memory for chessboard struct
  task = malloc(sizeof(struct chess_board));
  if (task == NULL)
  {
    fprintf(stderr, "Memory allocation failed for chess board.\n");
    exit(EXIT_FAILURE);
  }

  // Dynamically allocate memory for chessboard arrays that track positions
  const uint32_t diagonal_size = 2 * board->n_size - 1;
  const uint32_t total_size = 2 * (board->n_size + diagonal_size);
  (task)->queen_positions = malloc(sizeof(uint32_t) * total_size);
  if ((task)->queen_positions == NULL)
  {
    fprintf(stderr, "Memory allocation failed for the chess board arrays.\n");
    free(task);
    exit(EXIT_FAILURE);
  }
  (task)->column = &(task)->queen_positions[board->n_size];
  (task)->diagonal_up = &(task)->column[board->n_size];
  (task)->diagonal_down = &(task)->diagonal_up[diagonal_size];

  // Initialize the chess board parameters
  (task)->n_size = board->n_size;
  for (uint32_t i = 0; i < board->n_size; ++i)
  {
    task->queen_positions[i] = board->queen_positions[i];
  }
  for (uint32_t i = board->n_size; i < total_size; ++i)
  {
    // Initializes values for column, diagonal_up, and diagonal_down
    task->queen_positions[i] = board->queen_positions[i];
  }
  task->column_j = board->column_j;
  task->placements = board->placements;
  task->start = board->start;
  task->end = board->end;

  return task;
}





int main(int argc, char *argv[])
{
    static const uint32_t default_n = 4;
    int num_procs = 2;
    int rank;

    const uint32_t n_queens = (argc != 1) ? (uint32_t)atoi(argv[1]) : default_n;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    const uint32_t row_boundary = (n_queens >> 1) + (n_queens & 1);


    struct chess_board *board;

    uint32_t start = rank * row_boundary / num_procs;
    uint32_t end = (rank + 1) * row_boundary / num_procs;

    initialize_board(n_queens, &board, start, end);  
    omp_set_num_threads(nthreads);

    clock_t start_time = clock();

    place_queens(row_boundary,board);

    uint64_t totalS = 0;
    printf(" %lu solutions\n", total_solutions);
    MPI_Reduce(&total_solutions, &totalS, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
    // Print the total number of solutions from the root process
    if (rank == 0) {
        clock_t end_time = clock();
        double time_totale = (double)(end_time-start_time)/CLOCKS_PER_SEC;
        printf("program takes : %f s \n",time_totale);
        printf("The %u-Queens problem has %lu solutions\n", n_queens, totalS);
    }

    // Clean up the MPI environment
    MPI_Finalize();


    return EXIT_SUCCESS;
}



    

