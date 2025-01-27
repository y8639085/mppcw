#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "percolate.h"

/*
 * Parallel program to test for percolation of a cluster.
 */

int main(int argc, char *argv[]) {
  int size; // number of processes
  int rank; // process ID

  /* initialize MPI */
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  /* inspect command line arguments */
  if (size != nP*mP+1) {
    printf("Wrong number of processes!\n");
    MPI_Finalize();
    return 1;
  }

  if (argc != 2) {
    printf("Usage: percolate <seed>\n");
    MPI_Finalize();
    return 1;
  }

  /* master process operations */
  if (rank == 0) {
    printf("Number of processes is %d\n", size);
    int seed = atoi(argv[1]);
    callRank_0(seed);

    printf("End of master process\n");
  }
  /* child processes operations */
  else {
    callRank_i(rank);

    printf("End of child process %d\n", rank);
  }
	
  MPI_Finalize();
  return 0;
}

void callRank_0(int seed) {
  int map[L][L], old[M + 2][N + 2];

  MPI_Status status;

  double rho;
  int i, j, nhole, step, nchange, nchange_global;
  int itop, ibot, perc;
  double r;
  int vec[(length_m + 2)*(length_n + 2)];// array for pass matrix
  double avg, avg_global;
  double start, finish;
  int iD;

  rho = 0.411;

  printf("percolate: params are L = %d, rho = %f, seed = %d\n", L, rho, seed);
	
  rinit(seed);

  nhole = 0;

  /* initialize map without halo, give each square a unique value */
  for (i = 0; i < L; i++) {
    for (j = 0; j < L; j++) {
      r = uni();
      if (r < rho) {
	map[i][j] = 0;
      }
      else {
	nhole++;
	map[i][j] = nhole;
      }
    }
  }

  printf("percolate: rho = %f, actual density = %f\n",rho, 1.0 - ((double)nhole) / ((double)L*L));

  /* copy map to old */
  for (i = 1; i <= M; i++) {
    for (j = 1; j <= N; j++) {
      old[i][j] = map[i - 1][j - 1];
    }
  }

  // zero the bottom and top halos
  for (i = 0; i <= M + 1; i++) {
    old[i][0] = 0;
    old[i][N + 1] = 0;
  }
  // zero the left and right halos
  for (j = 0; j <= N + 1; j++) {
    old[0][j] = 0;
    old[M + 1][j] = 0;
  }

  // start timing
  start = MPI_Wtime();

  /* assign a small block to each child process */
  for (iD = 1; iD <= nP*mP; iD++) {
    bigMat2vec(old, vec, iD);
    MPI_Send(&vec, (length_m + 2)*(length_n + 2), MPI_INT, iD, 1, MPI_COMM_WORLD);
  }

  //	writeBigMat(old);

  nchange_global = 1;
  step = 1;

  while (step <= maxstep) {
    nchange = 0;

    for (j=1; j <= N; j++) {
      old[0][j]   = old[M][j];
      old[M+1][j] = old[1][j];
    }

    /*
     * using reduction to calculate number of changes
     * and average of map every 100 steps
     */
    if (step % printfreq == 0) {
      MPI_Allreduce(&nchange, &nchange_global, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
      printf("master ID = %d: number of changes on step %d is %d\n", 0, step, nchange_global);

      avg_global = 0;
      avg = 0;
      MPI_Allreduce(&avg, &avg_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

      avg_global /= (1.0*M*N);
      printf("master avecage of the map array : %f\n\n", avg_global);
    }

    // termination condition, when changes is 0, break the loop
    if (nchange_global == 0) {
      break;
    }
    step++;
  }

  // receive data from all child processes and then make a big map
  for (iD = 1; iD <= nP*mP; iD++)  {
    MPI_Recv(&vec, (length_m + 2)*(length_n + 2), MPI_INT, iD, 1, MPI_COMM_WORLD, &status);
    vec2BigMat(old, vec, iD);
  }

  // end timing
  finish = MPI_Wtime();
  printf("Elapsed time is : %f s\n", finish - start);

  //	writeBigMat(old);
  if (nchange != 0) {
    printf("percolate: WARNING max steps = %d reached before nchange = 0\n",maxstep);
  }

  // copy old back to map
  for (i = 1; i <= M; i++) {
    for (j = 1; j <= N; j++) {
      map[i - 1][j - 1] = old[i][j];
    }
  }
	
  perc = 0;

  // determine if percolates
  for (itop = 0; itop < L; itop++) {
    if (map[itop][L - 1] > 0) {
      for (ibot = 0; ibot < L; ibot++) {
	if (map[itop][L - 1] == map[ibot][0]) {
	  perc = 1;
	}
      }
    }
  }
	
  if (perc != 0) {
    printf("percolate: cluster DOES percolate\n");
  }
  else {
    printf("percolate: cluster DOES NOT percolate\n");
  }

  // write data to "pgm" file
  percwritedynamic("map.pgm", map, L, 3);

  return;
}

/*
 * This function includes all operations need to
 * be done by child processes. Including "halo-swapping",
 * receving and sending data from master process
 */
void callRank_i(int rank) {
  MPI_Status status;

  /*
   * Create two 2D arrays for halo-swapping.
   * lemgth_m: length of rectangle in m direction
   * length_n: length of rectangle in n direction
   */
  int old[length_m + 2][length_n + 2], new[length_m + 2][length_n + 2];
  int i, j, step, oldval, newval, nchange, nchange_global;
  MPI_Request reqs[8];
  MPI_Status Status[8];

  // prepare for receiving chunk from master
  int vec[(length_m + 2)*(length_n + 2)];

  /* length */
  int up_vec[(length_m + 2)];   // send up
  int down_vec[(length_m + 2)]; // send down
  int left_vec[(length_n + 2)];
  int right_vec[(length_n + 2)];

  int r_up_vec[(length_m + 2)];
  int r_down_vec[(length_m + 2)];
  int r_left_vec[(length_n + 2)];
  int r_right_vec[(length_n + 2)];
  int m0, n0, id;

  int tID = rank - 1;  // child process ID except master
  int mTh = tID % mP;  // position of process in m direction
  int nTh = tID / mP;  // position of process in n direction

  double avg, avg_global;

  // receive chunk from master
  MPI_Recv(&vec, (length_m + 2)*(length_n + 2), MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
  vec2Mat(old, vec);

  for (i = 0; i <= length_m+1; i++) {
    for (j = 0; j <= length_n+1; j++) {
      new[i][j] = old[i][j];
    }
  }

  step = 1;
  nchange = 1;
  nchange_global = 1;

  /* update squares by comparing with its four neighbours */
  while (step <= maxstep) {
    nchange = 0;

    /*
     *  Implement periodic boundaries in the first dimension "i",
     *  i.e. map  i=0 to i=M and i=M+1 to i=1
     */
    for (j=1; j <= N; j++) {
      old[0][j]   = old[M][j];
      old[M+1][j] = old[1][j];
    }

    for (i = 1; i <= length_m; i++) {
      for (j = 1; j <= length_n; j++) {
	oldval = old[i][j];
	newval = oldval;
	if (oldval != 0) {
	  if (old[i - 1][j] > newval) newval = old[i - 1][j];
	  if (old[i + 1][j] > newval) newval = old[i + 1][j];
	  if (old[i][j - 1] > newval) newval = old[i][j - 1];
	  if (old[i][j + 1] > newval) newval = old[i][j + 1];

	  if (newval != oldval) {
	    ++nchange;
	  }
	}
	new[i][j] = newval;
      }
    }

    for (i = 0; i <= length_m + 1; i++) {
      for (j = 0; j <= length_n + 1; j++) {
	old[i][j] = new[i][j];
      }
    }

    //sending, intereact with neighbours
    //up
    m0 = mTh;     // m0th block in the m direction
    n0 = nTh - 1; // n0th block in the n direction
    // if not beyonds up border, then intereact
    if (n0 >= 0) {
      id = n0*mP + m0 + 1;
      Mat2vec_UpDown(new, up_vec, 0);
      MPI_Isend(&up_vec, (length_m + 2), MPI_INT, id, 1, MPI_COMM_WORLD, &reqs[0]);
    }
	    
    //down
    m0 = mTh;
    n0 = nTh + 1;
    // if right side not beyonds border, then intereact
    if (n0 <nP) {
      id = n0*mP + m0 + 1;
      Mat2vec_UpDown(new, down_vec, 1);
      MPI_Isend(&down_vec, (length_m + 2), MPI_INT, id, 1, MPI_COMM_WORLD, &reqs[1]);
    }
	    
    // left
    m0 = mTh - 1;
    n0 = nTh;
    // if left side not beyonds border, then intereact
    if (m0 >= 0) {
      id = n0*mP + m0 +1;
      Mat2vec_LeftRight(new, left_vec, 0);
      MPI_Isend(&left_vec, (length_n + 2), MPI_INT, id, 1, MPI_COMM_WORLD, &reqs[2]);
    }
	    
    //right
    m0 = mTh + 1;
    n0 = nTh;
    if (m0 <mP) {
      id = n0*mP + m0 +1;
      Mat2vec_LeftRight(new, right_vec, 1);
      MPI_Isend(&right_vec, (length_n + 2), MPI_INT, id, 1, MPI_COMM_WORLD, &reqs[3]);
    }


    // receiving, intereact with neighbours
    // up
    m0 = mTh;
    n0 = nTh - 1;
    if (n0 >= 0) {
      id = n0*mP + m0 + 1;
      MPI_Irecv(&r_up_vec, (length_m + 2), MPI_INT, id, 1, MPI_COMM_WORLD, &reqs[4]);
      MPI_Wait(&reqs[4], &Status[4]);

      vec2Mat_UpDown(old, r_up_vec, 0);
    }
	    
    // down
    m0 = mTh;
    n0 = nTh + 1;
    if (n0 <nP) {
      id = n0*mP + m0 + 1;
      MPI_Irecv(&r_down_vec, (length_m + 2), MPI_INT, id, 1, MPI_COMM_WORLD, &reqs[5]);
      MPI_Wait(&reqs[5], &Status[5]);

      vec2Mat_UpDown(old, r_down_vec, 1);
    }
	    
    // left
    m0 = mTh - 1;
    n0 = nTh;
    if (m0 >= 0) {
      id = n0*mP + m0+1;
      MPI_Irecv(&r_left_vec, (length_n + 2), MPI_INT, id, 1, MPI_COMM_WORLD, &reqs[6]);
      MPI_Wait(&reqs[6], &Status[6]);
      vec2Mat_LeftRight(old, r_left_vec, 0);
    }
	    
    // right
    m0 = mTh + 1;
    n0 = nTh;
    if (m0 <mP) {
      id = n0*mP + m0+1;

      MPI_Irecv(&r_right_vec, (length_n + 2), MPI_INT, id, 1, MPI_COMM_WORLD, &reqs[7]);
      MPI_Wait(&reqs[7], &Status[7]);
      vec2Mat_LeftRight(old, r_right_vec, 1);
    }
	    

    if (step % printfreq == 0) {
      printf(" - Child ID = %d: number of changes on step %d is %d\n", rank,step, nchange);

      // calculate changes of all child processes
      MPI_Allreduce(&nchange, &nchange_global, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
			
      avg_global = 0;
      avg = 0;

      for (i = 1; i <= length_m; i++) {
	for (j = 1; j <= length_n; j++) {
	  avg += old[i][j];// / (1.0*length_m*length_n);
	}
      }
      // calculate average of map
      MPI_Allreduce(&avg, &avg_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

      // stop condition
      if (nchange_global == 0) {
	break;
      }
    }
    step++;
  }

  // send the data back to master
  Mat2vec(old, vec);
  MPI_Send(&vec, (length_m + 2)*(length_n + 2), MPI_INT, 0, 1, MPI_COMM_WORLD);
}

/* split big matrix into small vectors */
void bigMat2vec(int mat[M + 2][N + 2], int vec[(length_m + 2)*(length_n + 2)], int rank) {
  int i, j;
  rank = rank - 1;
  int mTh = rank % mP;
  int nTh = rank / mP;

  int mBegin = mTh*length_m; // beginning coordinate in m direction
  int mEnd = mBegin + length_m + 2;  // ending coordinate in m direction
  int nBegin = nTh*length_n;
  int nEnd = nTh*length_n + length_n + 2;

  // copy values from big matrix to rectangle
  int ith = 0;
  for (i = mBegin; i < mEnd; i++) {
    for (j = nBegin; j < nEnd; j++) {
      vec[ith] = mat[i][j];
      ith++;
    }
  }
}

/* Combine all small rectangles to a complete matrix(map) after updating */
void vec2BigMat(int mat[M + 2][N + 2], int vec[(length_m + 2)*(length_n + 2)], int rank) {
  int mat2[length_m + 2][length_n + 2];

  rank = rank - 1;
  int mTh = rank % mP;
  int nTh = rank / mP;
  int i, j;
  int mBegin = mTh*length_m;
  int mEnd = mBegin + length_m + 2;
  int nBegin = nTh*length_n;
  int nEnd = nTh*length_n + length_n + 2;

  int ith = 0;

  for (i = 0; i < length_m + 2; i++) {
    for (j = 0; j < length_n + 2; j++) {
      mat2[i][j] = vec[ith];
      ith++;
    }
  }

  for (i = 1; i <= length_m; i++) {
    for ( j = 1; j <= length_n; j++)
      {
	mat[mBegin + i][nBegin + j] = mat2[i][j];
      }
  }
}
// copy old to small rectangles in child processes
// then send data to master process
void Mat2vec(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)*(length_n + 2)]) {
  int i, j;
  int ith = 0;
  for (i = 0; i < length_m + 2; i++) {
    for (j = 0; j < length_n + 2; j++) {
      vec[ith] = mat[i][j];
      ith++;
    }
  }
}
// copy small rectangles to old after receving data
// from master process
void vec2Mat(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)*(length_n + 2)]) {
  int i, j;
  int ith = 0;
  for (i = 0; i < length_m + 2; i++) {
    for (j = 0; j < length_n + 2; j++) {
      mat[i][j] = vec[ith];
      ith++;
    }
  }
}

/*
 * these functions deal with halo swapping,
 * convert new to small rectangles, then interact
 * with up or down neighbours
 * 
 * mat: "new" map (small rectangle)
 * vec: up side or down side of rectangle
 * type: 0 is up, 1 is down
 */
void Mat2vec_UpDown(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)], int type) {
  int j;
  int ith = 0;
  if (type == 0) {
    for (j = 0; j < length_m + 2; j++) {
      vec[j] = mat[j][1];
    }
  }
  else {
    for (j = 0; j < length_m + 2; j++) {
      vec[j] = mat[j][length_n + 2 - 2];
    }
  }

}

void Mat2vec_LeftRight(int mat[length_m + 2][length_n + 2], int vec[(length_n + 2)], int type) {
  int i;
  int ith = 0;
  if (type == 0) {
    for (i = 0; i < length_n + 2; i++) {
      vec[i] = mat[1][i];
    }
  }
  else {
    for (i = 0; i < length_n + 2; i++) {
      vec[i] = mat[length_m + 2 - 2][i];
    }
  }

}

void vec2Mat_UpDown(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)], int type) {
  int j;
  int ith = 0;
  if (type == 0) {
    for (j = 0; j < length_m + 2; j++) {
      mat[j][0] = vec[j];
    }
  }
  else {
    for (j = 0; j < length_m + 2; j++) {
      mat[j][length_n + 2 - 1] = vec[j];
    }
  }

}
void vec2Mat_LeftRight(int mat[length_m + 2][length_n + 2], int vec[(length_n + 2)], int type) {
  int ith = 0;
  int i;
  if (type == 0) {
    for (i = 0; i < length_n + 2; i++) {
      mat[0][i] = vec[i];
    }
  }
  else {
    for (i = 0; i < length_n + 2; i++) {
      mat[length_m + 2 - 1][i] = vec[i];
    }
  }

}

/* these two function are to print results in middle phase for testing */
void writeBigMat(int mat[M + 2][N + 2]) {
  int i, j;
  for (j = 0; j < N + 2; j++) {
    for (i = 0; i < M + 2; i++) {
      printf("%1d ", mat[i][j]);
    }
    printf(" \n");
  }

}
void writeSmallMat(int mat[length_m + 2][length_n + 2], int id) {
  int i, j;
  if (id != 0 && id != 1) {
    return;
  }
  printf(" \n");
  printf("rank��%1d  \n", id);

  for (j = 0; j < length_n + 2; j++) {
    for (i = 0; i < length_m + 2; i++) {
      printf("%1d ", mat[i][j]);
    }
    printf(" \n");
  }
}
