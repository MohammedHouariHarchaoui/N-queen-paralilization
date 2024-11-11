#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct chess_board
{
    uint32_t n_size;  
    uint32_t *queen_positions; 
    uint32_t *column;  
    uint32_t *diagonal_up;  
    uint32_t *diagonal_down;
    uint32_t column_j;   
} CHESS_BOARD;

uint32_t total_solutions;
uint32_t total_placements;

/* Initialiser un échiquier n_queens x n_queens */
void initialize_board(const uint32_t n_queens, CHESS_BOARD **board);

/* Détruire l'échiquier en libérant l'espace mémoire */
void smash_board(CHESS_BOARD *board);


/* Vérifie si une case de l'échiquier est libre */
uint32_t square_is_free(const uint32_t row_i, CHESS_BOARD *board);

/* Placer une reine sur une case */
void set_queen(const uint32_t row_i, CHESS_BOARD *board);

/* Retirer une reine d'une case */
void remove_queen(const uint32_t row_i, CHESS_BOARD *board);

/* créer une copie de l'échiquier */
CHESS_BOARD *copyBoard(CHESS_BOARD *board);

/* Afficher les positions des reines */
void printQueenPositions(CHESS_BOARD *board);

/*  */


