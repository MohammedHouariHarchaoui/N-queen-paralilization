#include "n_queens_counter_lib.h"
#include <omp.h>

int tasks_created = 0;

uint32_t nqueens = 8; 

int nthreads = 4;

int partial_solutions, partial_placements;

void place_next_queen_sequential(const uint32_t row_boundary, CHESS_BOARD *board);
void place_next_queen_parallel(const uint32_t row_boundary, CHESS_BOARD *board);
void place_queens(const uint32_t row_boundary, CHESS_BOARD *board);



#pragma omp threadprivate(partial_solutions, partial_placements)

/** Lancer le processus de placement parallèle des reines sur l'échiquier. **/
void place_queens(const uint32_t row_boundary, CHESS_BOARD *board)

{
    //crée une région parallèle avec nqueens threads
    #pragma omp parallel num_threads(nthreads) 
    {
        //Initialiser le nombre de solutions partielles et de placements partiels pour chaque thread
        partial_solutions = 0;
        partial_placements = 0;
        
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


//la partie parallèle de l'algorithme, cette fonction génère des tâches à exécuter pour les threads 
void place_next_queen_parallel(const uint32_t row_boundary, CHESS_BOARD *board) {
    const uint32_t middle = board->column_j ? board->n_size : board->n_size >> 1;
    
    CHESS_BOARD *local_board;
    for (uint32_t row_i = 0; row_i < row_boundary; ++row_i) {  
        
        if (square_is_free(row_i, board)) 
        {
            #pragma omp task firstprivate(row_i) mergeable 
            {       
                local_board = copyBoard(board);

                set_queen(row_i, local_board);

                uint32_t limit = local_board->n_size;
                if (local_board->queen_positions[0] == middle) {
                    limit = middle;
                }


                if(local_board->column_j < nqueens / 4 + 1) { 
                
                    place_next_queen_parallel(limit, local_board);
                } else {
                    place_next_queen_sequential(limit, local_board);
                }

            #pragma omp taskyield
            
            }
            free(local_board); 
        }
    }
}



void place_next_queen_sequential(const uint32_t row_boundary, CHESS_BOARD *board)
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



void place_next_queen(const uint32_t row_boundary, CHESS_BOARD *board)
{
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




int main(int argc, char *argv[])
{

    CHESS_BOARD *board;
    double start_time, end_time;
    double temps_parallel_pris, temps_sequentiel_pris = 0;

    nqueens = (argc != 1) ? (uint32_t)atoi(argv[1]) : nqueens;

    const uint32_t row_boundary = (nqueens >> 1) + (nqueens & 1);

    initialize_board(nqueens, &board);
    start_time = omp_get_wtime();
    place_next_queen(row_boundary, board); 
    end_time = omp_get_wtime();
    smash_board(board);

    temps_sequentiel_pris = (end_time - start_time);
    printf("Temps sequentiel : %f\n", temps_sequentiel_pris);
    printf("final %d\n", total_solutions);

    
    total_solutions = 0;
    initialize_board(nqueens, &board);
    start_time = omp_get_wtime();
    place_queens(row_boundary, board); 
    end_time = omp_get_wtime();
    smash_board(board);

    temps_parallel_pris = (end_time - start_time);


    printf("Temps parallel : %f\n", temps_parallel_pris);
    printf("final %d\n", total_solutions);

    
    

    printf("\nAcceleration avec 4 threads : %f\n", temps_sequentiel_pris/temps_parallel_pris);


    nthreads = 8;
    total_solutions = 0;
    initialize_board(nqueens, &board);
    start_time = omp_get_wtime();
    place_queens(row_boundary, board); 
    end_time = omp_get_wtime();
    smash_board(board);

    temps_parallel_pris = (end_time - start_time);


    printf("Temps parallel avec 8 threads: %f\n", temps_parallel_pris);
    printf("final %d\n", total_solutions);

    
    

    printf("\nAcceleration : %f\n", temps_sequentiel_pris/temps_parallel_pris);


    nthreads = 16;
    total_solutions = 0;
    initialize_board(nqueens, &board);
    start_time = omp_get_wtime();
    place_queens(row_boundary, board); 
    end_time = omp_get_wtime();
    smash_board(board);

    temps_parallel_pris = (end_time - start_time);


    printf("Temps parallel avec 16 threads: %f\n", temps_parallel_pris);
    printf("final %d\n", total_solutions);

    
    

    printf("\nAcceleration : %f\n", temps_sequentiel_pris/temps_parallel_pris);


    nthreads = 32;
    total_solutions = 0;
    initialize_board(nqueens, &board);
    start_time = omp_get_wtime();
    place_queens(row_boundary, board); 
    end_time = omp_get_wtime();
    smash_board(board);

    temps_parallel_pris = (end_time - start_time);


    printf("Temps parallel avec 32 threads: %f\n", temps_parallel_pris);
    printf("final %d\n", total_solutions);

    
    

    printf("\nAcceleration : %f\n", temps_sequentiel_pris/temps_parallel_pris);

    //sequential execution
    //place_next_queen(row_boundary, board);

    return EXIT_SUCCESS;
}