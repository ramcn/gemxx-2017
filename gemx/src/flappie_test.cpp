#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gemx_xblas.h"
#include <vector>
using namespace std;

int max(int x, int y){
   if(x > y) return x;
   else return y;
}

int main(void)
{
    const int m = 8;
    const int n = 8;
    const int k = 8;

    short A[] = {1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8, 1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8};
    short B[] = {1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8, 1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8,   1,2,3,4,5,6,7,8};


    std::vector<GEMX_dataType> a1(m*n);
    std::vector<GEMX_dataType> b1(m*k);
    std::vector<GEMX_dataType> c1(n*k);

    //memcpy(&a1[0], A, m*n*sizeof(short));
    //memcpy(&b1[0], B, m*n*sizeof(short));
  
      for (int row = 0; row < m;  ++row) {
            for (int col = 0; col < n;  ++col) {
              a1[row*m+col] = A[row*m+col];
              b1[row*m+col] = B[row*m+col];
              c1[row*m+col] = 0;
            }
          }

    //cblas_sgemm(CblasColMajor, CblasTrans, CblasNoTrans, n,k,m, alpha, a1, m, b1, n, beta, c1, n);
    xblas_sgemm(CblasRowMajor,CblasNoTrans,CblasNoTrans,n,k,m,1,a1,m,b1,n,1,c1,n);


    fprintf(stderr,"Result after sgemm\n"); 
    for (int i = 0; i < c1.size(); i++)
        printf("%d ",c1[i]);


    clearMemory();

    return 0;
}
