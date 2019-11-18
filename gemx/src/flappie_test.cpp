#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gemx_xblas.h"

int max(int x, int y){
   if(x > y) return x;
   else return y;
}

int main(void)
{
    const int m = 4;
    const int n = 8;
    const int k = 1;

    short A[] = {1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8};
    short B[] = { 1, 2, 3, 4 };


      std::vector<GEMX_dataType> a1(m*n);
      std::vector<GEMX_dataType> b1(m*1);
      std::vector<GEMX_dataType> c1(n*1);

    //memcpy(a1, A, m * n * sizeof(short));
    //memcpy(b1, B, m * 1 * sizeof(short));
    //memset(c1, 0, n * 1 * sizeof(short));

    //cblas_sgemm(CblasColMajor, CblasTrans, CblasNoTrans, n,k,m, alpha, a1, m, b1, n, beta, c1, n);
    xblas_sgemm(CblasRowMajor,CblasTrans,CblasNoTrans,n,k,m,1,a1,m,b1,n,1,c1,n);


    fprintf(stderr,"Result after sgemm\n"); 

    clearMemory();

    return 0;
}
