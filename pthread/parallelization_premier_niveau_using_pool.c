#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>

#define THREAD_NUM 8

typedef struct chess_board{
    uint32_t n_size;           
    uint32_t *queen_positions; 
    uint32_t *column;          
    uint32_t *diagonal_up;     
    uint32_t *diagonal_down;
    uint32_t column_j;   
    uint64_t placements; 
    uint64_t solutions;  
    uint64_t start;
    uint64_t end;
} Board;

typedef Board *Task;


pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;
pthread_mutex_t mutexSolutions;

Task taskQueue[10];
uint64_t taskCount = 0;
uint64_t numberOfSolutions = 0;

void place_next_queen_thread(struct chess_board *board);


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

void executeTask(Task* task) {
    place_next_queen_thread(*task);
}

void submitTask(Task *task) {
    pthread_mutex_lock(&mutexQueue);
    taskQueue[taskCount] = (*task);
    taskCount++;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueue);
}




void* startThread(void* args) {
    while (1) {
        Task task;

        pthread_mutex_lock(&mutexQueue);
        struct timespec timeout;
        struct timeval now;

        // Get the current time
        gettimeofday(&now, NULL);

        timeout.tv_sec = now.tv_sec + 2;
        timeout.tv_nsec = now.tv_usec * 1000;

        while (taskCount == 0) {
            if (pthread_cond_timedwait(&condQueue, &mutexQueue, &timeout) == ETIMEDOUT) {
                pthread_mutex_unlock(&mutexQueue);
                return NULL; // Exit the thread
            }
        }

        task = taskQueue[0];
        int i;
        for (i = 0; i < taskCount - 1; i++) {
            taskQueue[i] = taskQueue[i + 1];
        }
        taskCount--;
        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task);
    }
}



void place_next_queen(struct chess_board *board)
{
    uint64_t start = board->start;
    uint64_t end = board->end;
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    for (uint32_t row_i = start; row_i < end; ++row_i)
    {
        if (square_is_free(row_i, board))
        {
            //printf("It is possible in the main thread to place the queen on row %d\n",row_i);
            set_queen(row_i, board);
            // if (board->column_j == board->n_size)
            // {
            //     board->solutions += 2;
            // }
            // else 
            if (board->queen_positions[0] != middle)
            {
                board->start = 0;
                board->end = board->n_size;
                Task task;
                initialize_board(board->n_size , &task , board->start , board->end);
                //printf("Adding task pair where column_j = %d that starts from %d ends %d\n",board->column_j,board->start,board->end);
                set_queen(row_i,task);
                submitTask(&task);
            }
            else
            {
                board->start = 0;
                board->end = middle;
                Task task;
                initialize_board(board->n_size , &task , board->start , board->end);
                //printf("Adding task impaire where column_j = %d that starts from %d ends %d\n",board->column_j,board->start,board->end);
                set_queen(row_i,task);
                submitTask(&task);
            }
            remove_queen(row_i, board);
        }
    }
}

void place_next_queen_thread(struct chess_board *board)
{
    uint64_t start = board->start;
    uint64_t end = board->end;
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    for (uint32_t row_i = start; row_i < end; ++row_i)
    {
        if (square_is_free(row_i, board))
        {
            set_queen(row_i, board);
            if (board->column_j == board->n_size)
            {
                pthread_mutex_lock(&mutexSolutions);
                numberOfSolutions += 2;
                pthread_mutex_unlock(&mutexSolutions);
            }
            else if (board->queen_positions[0] != middle)
            {
                board->start = 0;
                board->end = board->n_size;
                place_next_queen_thread(board);
            }
            else
            {
                board->start = 0;
                board->end = middle;
                place_next_queen_thread(board);
            }
            remove_queen(row_i, board);
        }
    }
}

int main(int argc, char* argv[]) {
    static const uint32_t default_n = 12;
    const uint32_t n_queens = (argc != 1) ? (uint32_t)atoi(argv[1]) : default_n;
    const uint32_t row_boundary = (n_queens >> 1) + (n_queens & 1);

    pthread_t th[THREAD_NUM];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_mutex_init(&mutexSolutions, NULL);
    pthread_cond_init(&condQueue, NULL);
    Board *board;

    clock_t start_time = clock();

    int i;
    for (i = 0; i < THREAD_NUM; i++) {
        if (pthread_create(&th[i], NULL, &startThread, NULL) != 0) {
            perror("Failed to create the thread");
        }
    }

    initialize_board(n_queens, &board, 0 , row_boundary);
    place_next_queen(board);

    for (i = 0; i < THREAD_NUM; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join the thread");
        }
    }
    clock_t end_time = clock();

    double time_totale = (double)(end_time-start_time)/CLOCKS_PER_SEC;
    printf("Nombre de solution global : %d Dans : %f s \n",numberOfSolutions,time_totale-2);

    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    return 0;
}