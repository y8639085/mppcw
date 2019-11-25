# Documentation of Percolation

## Introduction
This is a message-passing parallel code for a simple two-dimensional grid-based calculation that
employs a two-dimensional domain decomposition and uses non-blocking communications, it has some
differences with case study, including:

+ A working MPI code (in C) for the percolation problem.
+ Use periodic boundary conditions in the horizontal direction.
+ Calculate the average of the map array during the iterative loop, and print at every 100 steps.
+ It is able to terminate the calculation when all clusters have been identified.
+ It has the same correct answer when increasing the number of processes.

### Compilers and versions
Compiler: intel-compilers-18
Flags:
+ -03

## Source files
The source files in folder src includes: percolate.c percolate.h uni.c percwritedynamic.c
and a Makefile for compiling and linking all the source files, and a .pbs file for submitting to Cirrus.

## Functions
All the important functions are in percolate.c, and they are declared in percolate.h, including:

+ void callRank_0(int seed);
+ void callRank_i(int rank);
+ void bigMat2vec(int mat[M + 2][N + 2], int vec[(length_m + 2)*(length_n + 2)], int rank);
+ void vec2BigMat(int mat[M + 2][N + 2], int vec[(length_m + 2)*(length_n + 2)], int rank);
+ void Mat2vec(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)*(length_n + 2)]);
+ void vec2Mat(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)*(length_n + 2)]);
+ void Mat2vec_UpDown(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)], int type);
+ void Mat2vec_LeftRight(int mat[length_m + 2][length_n + 2], int vec[(length_n + 2)], int type);
+ void vec2Mat_UpDown(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)], int type);
+ void vec2Mat_LeftRight(int mat[length_m + 2][length_n + 2], int vec[(length_n + 2)], int type);
+ void writeBigMat(int mat[M + 2][N + 2]);
+ void writeSmallMat(int mat[length_m + 2][length_n + 2], int id);

All of them are commented in the source files.

## Compiling and running
Before compile, users need to type "module load mpt" and "module load intel-compilers-18" because
this program is compiled by intel compiler.
To compile the program part, users need to input "make" in the
command line to invoke "Makefile". If users want to run it on Cirrus,
type "qsub percolate.pbs".

To clean the mid product such as *.o files, input "make clean" 
to delete all the auto-generated files created during compilation.

After compilation, type "mpirun -n 4 ./percoate 1564" to run this program using default variables.
Users can change the number of processes and in the program and change seed in the command line.
Note that the number after "-n" must be the total number of processes.
Users should input them in the command line in a specific order and must includes the seed, or it will generate an error.

After running, in addition to the results printed in terminal, your results will also be stored in a file named "map.pgm",
it is the result map with clusterings after percolating whether it was successful or not.
To open it, users can type "cat map.pgm" in the command line.

## Result
This program will run and show you whether it percolates or not using MPI with two-dimensional grid-based.
After that it will generate a file named "map.pgm" which shows all the clusters.
