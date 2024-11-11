#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <mpi.h>

#define level 0
#define MAX_BOARDS 500

// An abstract representation of an NxN chess board to tracking open positions
typedef struct chess_board
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
}Board;

// un pointeur vers un echéquier qui simule une tâche dans la file
typedef Board *Task;


// pour compter le nombre de tâches dans la file
uint64_t taskCount = 0;
uint64_t numberOfSolutions = 0;
// Function prototypes

//creer les taches en partant du premier colonne jusqu'à la limite : level
void createTasks(Task *task);
//enfiler la tache : task
void submitTask(Board *task);
//executer avec la fonction createTasks afin de generer les taches à partir d'un certain echiquier donné
void place_next_queen(struct chess_board *board);
//execution sequentiel de la fonction principale 
void place_next_queen_process(struct chess_board *board);
static void initialize_board(const uint32_t n_queens, struct chess_board **board, uint32_t start, uint32_t end);
//définir un nouveau type de données MPI (MPI_Datatype) qui correspond à la structure Board
void defineTaskStruct(MPI_Datatype *mpiTaskType);

Board taskQueue[MAX_BOARDS];

int main(int argc, char *argv[])
{
    static const uint32_t default_n = 4;
    int num_procs = 2;
    int rank;

    const uint32_t n_queens = (argc != 1) ? (uint32_t)atoi(argv[1]) : default_n;
    
    clock_t start_time = clock();

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    const uint32_t row_boundary = (n_queens >> 1) + (n_queens & 1);

    struct chess_board *board;

    uint32_t start = 0;
    uint32_t end =row_boundary;

    initialize_board(n_queens, &board, start, end);  
    if (rank == 0) {
        createTasks(&board);
    }
    MPI_Datatype mpiTaskType;
    defineTaskStruct(&mpiTaskType);

    int packedSize;
    MPI_Pack_size(MAX_BOARDS, mpiTaskType, MPI_COMM_WORLD, &packedSize);

    char *buffer = malloc(packedSize);
    int position = 0;
    if (rank == 0) {
        // Pack the tasks into a buffer
        MPI_Pack(taskQueue, MAX_BOARDS, mpiTaskType, buffer, packedSize, &position, MPI_COMM_WORLD);
    }
    // Broadcast the packed buffer to all processes
    MPI_Bcast(buffer, packedSize, MPI_PACKED, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        // Unpack the tasks from the received buffer
        MPI_Unpack(buffer, packedSize, &position, taskQueue, MAX_BOARDS, mpiTaskType, MPI_COMM_WORLD);
        for (int i = 0; i < 2; i++) {
            printf("col: %u\n", taskQueue[i].column_j);
            printf("n: %u\n", taskQueue[i].n_size);
        }
    }
    
    // Divide the taskQueue between processes
    uint64_t start_process = rank * taskCount / num_procs;
    uint64_t end_process = (rank + 1) * taskCount / num_procs;
    for (int i = start_process; i < end_process; i++)
    {
        printf("i: %d, rank: %d\n",i,rank);
        printf("column: %u , rank: %d\n",taskQueue[0].column_j,rank);
        place_next_queen_process(&taskQueue[i]);   
    }
    
    printf("solutions : %lu \n",numberOfSolutions);
    uint64_t total_solutions = 0;
    //Perform reduction operation to gather the local solutions from each process and compute the total solutions
    //MPI_Reduce(&numberOfSolutions, &total_solutions, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
    
    // Print the total number of solutions from the root process
    /*if (rank == 0) {
        clock_t end_time = clock();
        double time_totale = (double)(end_time-start_time)/CLOCKS_PER_SEC;
        printf("program takes : %f s \n",time_totale);
        printf("The %u-Queens problem has %lu solutions\n", n_queens, total_solutions);
    }
    */
   
    // Clean up the MPI environment
    MPI_Finalize();
    return EXIT_SUCCESS;
}


void defineTaskStruct(MPI_Datatype *mpiTaskType) {
    int blockLengths[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    MPI_Aint displacements[10];
    MPI_Datatype types[10] = {
        MPI_UINT32_T, MPI_UINT32_T, MPI_UINT32_T, MPI_UINT32_T, MPI_UINT32_T,
        MPI_UINT32_T, MPI_UINT64_T, MPI_UINT64_T, MPI_UINT64_T, MPI_UINT64_T
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
    MPI_Get_address(&(board.start), &displacements[8]);
    MPI_Get_address(&(board.end), &displacements[9]);

    for (int i = 1; i < 10; i++) {
        displacements[i] -= displacements[0];
    }
    displacements[0] = 0;

    MPI_Type_create_struct(10, blockLengths, displacements, types, mpiTaskType);
    MPI_Type_commit(mpiTaskType);
}
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

// pour créer une copie de l'échiquier
struct chess_board *copyBoard(struct chess_board *board)
{
    Task task = malloc(sizeof(struct chess_board));

    if (board->n_size < 1)
    {
        fprintf(stderr, "The number of queens must be greater than 0.\n");
        exit(EXIT_SUCCESS);
    }
    task = malloc(sizeof(struct chess_board));
    if (task == NULL)
    {
        fprintf(stderr, "Memory allocation failed for chess board.\n");
        exit(EXIT_FAILURE);
    }
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
    (task)->n_size = board->n_size;
    for (uint32_t i = 0; i < board->n_size; ++i)
    {
        task->queen_positions[i] = board->queen_positions[i];
    }
    for (uint32_t i = board->n_size; i < total_size; ++i)
    {
        task->queen_positions[i] = board->queen_positions[i];
    }
    task->column_j = board->column_j;
    task->placements = board->placements;
    task->start = board->start;
    task->end = board->end;
    return task;
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

void createTasks(Task *task)
{
    // vérifier si on est arrivé au niveau de parallélisation désiré
    if ((*task)->column_j <= level)
    {
        // on ajoute des tâches
        place_next_queen(*task);
    }
}

// pour ajouter une tâche dans la file
void submitTask(Board *task)
{
    taskQueue[taskCount] = (*task);
    taskCount++;
}
void place_next_queen(struct chess_board *board)
{
    uint64_t start = board->start;
    uint64_t end = board->end;
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    for (uint32_t row_i = start; row_i < end; ++row_i)
    {
        // on utilise ces vérification au lieu de la fonction donnéé pour vérifier la disponibilité d'un emplacement
        // c'est la même chose juste ordonné par les plus petites vérifications vers les plus grandes pour diminuer le temps d'execution
        if (board->column[row_i])
        {
            if (board->diagonal_down[board->column_j + row_i])
            {
                if (board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)])
                {
                    set_queen(row_i, board);
                    if (board->queen_positions[0] != middle)
                    {
                        board->start = 0;
                        board->end = board->n_size;
                        Board *task = copyBoard(board);
                        if(task->column_j <= level){
                            // on ajoute des tâches
                            place_next_queen(task);
                        }else{
                            submitTask(task);
                        }
                    }
                    else
                    {
                        board->start = 0;
                        board->end = middle;
                        Task task = copyBoard(board);
                        if(task->column_j <= level){
                            // on ajoute des tâches
                            place_next_queen(task);
                        }else{
                            printf("here submit col %u\n", task->column_j);
                            submitTask(task);
                        }
                    }
                    remove_queen(row_i, board);
                }
            }
        }
    }


}
void place_next_queen_process(struct chess_board *board)
{
    printf("incide\n");
    uint64_t start = board->start;
    printf("start %lu\n",start);
    uint64_t end = board->end;
    printf("end %lu\n",end);
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    for (uint32_t row_i = start; row_i < end; ++row_i)
    {
            printf("row_i: %u \n\n",row_i);
        if (board->column[row_i])
        {
            if (board->diagonal_down[board->column_j + row_i])
            {
                if (board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)])
                {
                    set_queen(row_i, board);
                    if (board->column_j == board->n_size)
                    {
                        printf("increm\n");
                        numberOfSolutions += 2;
                    }
                    else if (board->queen_positions[0] != middle)
                    {
                        board->start = 0;
                        board->end = board->n_size;
                        place_next_queen_process(board);
                    }
                    else
                    {
                        board->start = 0;
                        board->end = middle;
                        place_next_queen_process(board);
                    }
                    remove_queen(row_i, board);
                }
            }
        }
    }
}
