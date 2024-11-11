#include "n_queens_counter_lib.h"


void initialize_board(const uint32_t n_queens, CHESS_BOARD **board){
    if (n_queens < 1){
        fprintf(stderr, "The number of queens must be greater than 0.\n");
        exit(EXIT_SUCCESS);
    }

    *board = malloc(sizeof(CHESS_BOARD));
    if (*board == NULL){
        fprintf(stderr, "Memory allocation failed for chess board.\n");
        exit(EXIT_FAILURE);
    }


    const uint32_t diagonal_size = 2 * n_queens - 1;
    const uint32_t total_size = 2 * (n_queens + diagonal_size);
    (*board)->queen_positions = malloc(sizeof(uint32_t) * total_size);
    if ((*board)->queen_positions == NULL){
        fprintf(stderr, "Memory allocation failed for the chess board arrays.\n");
        free(*board);
        exit(EXIT_FAILURE);
    }
    (*board)->column = &(*board)->queen_positions[n_queens];
    (*board)->diagonal_up = &(*board)->column[n_queens];
    (*board)->diagonal_down = &(*board)->diagonal_up[diagonal_size];


    (*board)->n_size = n_queens;

    for (uint32_t i = 0; i < n_queens; ++i){
        (*board)->queen_positions[i] = 0;
    }

    for (uint32_t i = n_queens; i < total_size; ++i){
        (*board)->queen_positions[i] = 1;
    }
    (*board)->column_j = 0;
}




void smash_board(CHESS_BOARD *board) {
    free(board->queen_positions);
    free(board);
}




uint32_t square_is_free(const uint32_t row_i, CHESS_BOARD *board) {
    return board->column[row_i] &
           board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)] &
           board->diagonal_down[board->column_j + row_i];
}




void set_queen(const uint32_t row_i, CHESS_BOARD *board) {
    board->queen_positions[board->column_j] = row_i;
    board->column[row_i] = 0;
    board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)] = 0;
    board->diagonal_down[board->column_j + row_i] = 0;
    ++board->column_j;
}



void remove_queen(const uint32_t row_i, CHESS_BOARD *board) {
    --board->column_j;
    board->diagonal_down[board->column_j + row_i] = 1;
    board->diagonal_up[(board->n_size - 1) + (board->column_j - row_i)] = 1;
    board->column[row_i] = 1;
}
   


CHESS_BOARD *copyBoard(CHESS_BOARD *board) {
    struct chess_board *task = malloc(sizeof(CHESS_BOARD));

    if (board->n_size < 1) {
        fprintf(stderr, "The number of queens must be greater than 0.\n");
        exit(EXIT_SUCCESS);
    }

 
    task = malloc(sizeof(CHESS_BOARD));
    if (task == NULL){
        fprintf(stderr, "Memory allocation failed for chess board.\n");
        exit(EXIT_FAILURE);
    }

  
    const uint32_t diagonal_size = 2 * board->n_size - 1;
    const uint32_t total_size = 2 * (board->n_size + diagonal_size);
    task->queen_positions = malloc(sizeof(uint32_t) * total_size);

    if ((task)->queen_positions == NULL){
        fprintf(stderr, "Memory allocation failed for the chess board arrays.\n");
        free(task);
        exit(EXIT_FAILURE);
    }
    task->column = &(task)->queen_positions[board->n_size];
    task->diagonal_up = &(task)->column[board->n_size];
    task->diagonal_down = &(task)->diagonal_up[diagonal_size];


    task->n_size = board->n_size;
    for (uint32_t i = 0; i < board->n_size; ++i){
        task->queen_positions[i] = board->queen_positions[i];
    }

    for (uint32_t i = board->n_size; i < total_size; ++i){
    // Initializes values for column, diagonal_up, and diagonal_down
        task->queen_positions[i] = board->queen_positions[i];
    }
    task->column_j = board->column_j;
    return task;
}



void printQueenPositions(CHESS_BOARD *board) {
  printf("positions ");
  for (uint32_t i = 0; i < board->n_size; i++) {
      printf("%d ", board->queen_positions[i]);
    }
  printf("\n");
  
  printf("Columns: ");
  for (uint32_t i = 0; i < board->n_size; i++) {
    printf("%d ", board->column[i]);
  }
  printf("\n");

  printf("Diagonal Up: ");
  for (uint32_t i = 0; i < (2 * board->n_size - 1); i++) {
    printf("%d ", board->diagonal_up[i]);
  }
  printf("\n");

  printf("Diagonal Down: ");
  for (uint32_t i = 0; i < (2 * board->n_size - 1); i++) {
    printf("%d ", board->diagonal_down[i]);
  }
  printf("\n\n");
}