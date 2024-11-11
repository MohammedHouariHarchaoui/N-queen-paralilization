#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>
// Le nombre de threads
#define THREAD_NUM 16
// Le niveau ou s'arrète la parallèlisation des tâches
#define level 1

typedef struct chess_board
{
    uint32_t n_size;
    uint32_t *queen_positions;
    uint32_t *column;
    uint32_t *diagonal_up;
    uint32_t *diagonal_down;
    uint32_t column_j;
    uint64_t placements;
    // start et end pour définir les bordures des valeurs des positions à parcourir possibles d'une queen de position j
    uint64_t start;
    uint64_t end;
} Board;
// un pointeur vers un echéquier qui simule une tâche dans la file
typedef Board *Task;
// mutex pour accéder â le queue
pthread_mutex_t mutexQueue;
// pour signaler aux threads qu'il existe une tâche dans la file
pthread_cond_t condQueue;
// mutex pour accéder au solutions
pthread_mutex_t mutexSolutions;

// file des tâches de taille assez grande
Task taskQueue[400];
// pour compter le nombre de tâches dans la file
uint64_t taskCount = 0;
// le nombre total de solutions
uint64_t numberOfSolutions = 0;

void place_next_queen_thread(struct chess_board *board);
void place_next_queen(struct chess_board *board);
void place_next_queen_without_parallelization(struct chess_board *board);
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

    (*board)->n_size = n_queens;
    for (uint32_t i = 0; i < n_queens; ++i)
    {
        (*board)->queen_positions[i] = 0;
    }
    for (uint32_t i = n_queens; i < total_size; ++i)
    {
        (*board)->queen_positions[i] = 1;
    }
    (*board)->column_j = 0;
    (*board)->placements = 0;
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

static void smash_board(struct chess_board *board)
{
    free(board->queen_positions);
    free(board);
}

static void set_queen(const uint32_t row_i, struct chess_board *board)
{
    board->queen_positions[board->column_j] = row_i;
    board->column[row_i] = 0;
    board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)] = 0;
    board->diagonal_down[board->column_j + row_i] = 0;
    ++board->column_j;
    ++board->placements;
}

static void remove_queen(const uint32_t row_i, struct chess_board *board)
{
    --board->column_j;
    board->diagonal_down[board->column_j + row_i] = 1;
    board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)] = 1;
    board->column[row_i] = 1;
}
// la tâche à executer par le thread
void executeTask(Task *task)
{
    // vérifier si on est arrivé au niveau de parallélisation désiré
    if ((*task)->column_j <= level)
    {
        // on ajoute des tâches
        place_next_queen(*task);
    }
    // sinon on execute la tâche
    else
    {
        place_next_queen_thread(*task);
    }
}
// pour ajouter une tâche dans la file
void submitTask(Task *task)
{
    pthread_mutex_lock(&mutexQueue);
    taskQueue[taskCount] = (*task);
    taskCount++;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueue);
}
// la fonction executé par chaque thread
void *startThread(void *args)
{
    while (1)
    {
        Task task;

        pthread_mutex_lock(&mutexQueue);
        // pour tuer le thread s'il ne prend pas de tâche dans cette durée
        struct timespec timeout;
        struct timeval now;

        gettimeofday(&now, NULL);

        timeout.tv_sec = now.tv_sec + 3;
        timeout.tv_nsec = now.tv_usec * 1000;
        // le thread boucle jusqu'à qu'il trouve une tâche ou que la durée est passé
        while (taskCount == 0)
        {
            if (pthread_cond_timedwait(&condQueue, &mutexQueue, &timeout) == ETIMEDOUT)
            {
                pthread_mutex_unlock(&mutexQueue);
                return NULL;
            }
        }
        // prendre la premère tâche de la file
        task = taskQueue[0];
        int i;
        // décalage vers la gauche de la file
        for (i = 0; i < taskCount - 1; i++)
        {
            taskQueue[i] = taskQueue[i + 1];
        }
        taskCount--;
        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task);
        free(task); // free task
    }
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
                        Task task = copyBoard(board);
                        submitTask(&task);
                    }
                    else
                    {
                        board->start = 0;
                        board->end = middle;
                        Task task = copyBoard(board);
                        submitTask(&task);
                    }
                    remove_queen(row_i, board);
                }
            }
        }
    }
}
void place_next_queen_without_parallelization(struct chess_board *board,uint32_t row_boundary){
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
}


void place_next_queen_thread(struct chess_board *board)
{
    uint64_t start = board->start;
    uint64_t end = board->end;
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    for (uint32_t row_i = start; row_i < end; ++row_i)
    {
        if (board->column[row_i])
        {
            if (board->diagonal_down[board->column_j + row_i])
            {
                if (board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)])
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
    }
}

int main(int argc, char *argv[])
{
    static const uint32_t default_n = 16;
    const uint32_t n_queens = (argc != 1) ? (uint32_t)atoi(argv[1]) : default_n;
    const uint32_t row_boundary = (n_queens >> 1) + (n_queens & 1);
    pthread_t th[THREAD_NUM];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_mutex_init(&mutexSolutions, NULL);
    pthread_cond_init(&condQueue, NULL);
    Board *board;
    clock_t start_time = clock();
    int i;
    // création des threads
    for (i = 0; i < THREAD_NUM; i++)
    {
        if (pthread_create(&th[i], NULL, &startThread, NULL) != 0)
        {
            perror("Failed to create the thread");
        }
    }
    initialize_board(n_queens, &board, 0, row_boundary);
    place_next_queen(board);
    // join des threads
    for (i = 0; i < THREAD_NUM; i++)
    {
        if (pthread_join(th[i], NULL) != 0)
        {
            perror("Failed to join the thread");
        }
    }
    clock_t end_time = clock();
    double time_totale = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("N = %d : Nombre de solution global : %d dans : %f s \n", n_queens, numberOfSolutions, time_totale - 3);
    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);

    //sequetial execution 
    //place_next_queen_without_parallelization(board,row_boundary);

    return 0;
}