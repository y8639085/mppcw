/*
 *  Main header file for percolation code.
 */

/*
 *  System size L
 */

#define L 288

/*
 *  Although ovecall system is square, i.e. size L x L, we will define
 *  different variables for the first and second dimensions. This is
 *  because, in the parallel code, the local arrays will not be
 *  square. For example, using a simple 1D decomposition ovec P
 *  processes, then M = L/P and N = L
 */

#define M L
#define N L

/*
 *  Prototypes for supplied functions
 */

/*
 *  Visualisation
 */

void percwritedynamic(char *percfile, int map[L][L], int l, int ncluster);

/*
 *  Random numbers
 */

void rinit(int ijkl);
float uni(void);

//mpi use function
#define mP 1 // number of divisions in the m direction
#define nP 3 // number of divisions in the n direction

#define length_m M/mP //矩形m方向大小
#define length_n N/nP //矩形n方向大小
#define maxstep 20000 // 16 * L;
#define printfreq 100 // 100;

void callRank_0(int seed);
void callRank_i(int rank);
void bigMat2vec(int mat[M + 2][N + 2], int vec[(length_m + 2)*(length_n + 2)], int rank);
void vec2BigMat(int mat[M + 2][N + 2], int vec[(length_m + 2)*(length_n + 2)], int rank);
void Mat2vec(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)*(length_n + 2)]);
void vec2Mat(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)*(length_n + 2)]);
void Mat2vec_UpDown(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)], int type);
void Mat2vec_LeftRight(int mat[length_m + 2][length_n + 2], int vec[(length_n + 2)], int type);
void vec2Mat_UpDown(int mat[length_m + 2][length_n + 2], int vec[(length_m + 2)], int type);
void vec2Mat_LeftRight(int mat[length_m + 2][length_n + 2], int vec[(length_n + 2)], int type);
void writeBigMat(int mat[M + 2][N + 2]);
void writeSmallMat(int mat[length_m + 2][length_n + 2], int id);
