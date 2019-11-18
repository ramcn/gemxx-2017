#include <stdio.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>


#include "gemx_xblas.h"

int main(int argc, char **argv) {   
      int N=64;
      std::vector<GEMX_dataType> a2(N*N);
      std::vector<GEMX_dataType> b2(N*N);
      std::vector<GEMX_dataType> c2(N*N);
      float *A=(float *)malloc(N*N*(sizeof(float)));  
      float *B=(float *)malloc(N*N*(sizeof(float)));  

      for (int row = 0; row < N;  ++row) {
	    for (int col = 0; col < N;  ++col) {
	      GEMX_dataType l_val2 = 2.0;
	      GEMX_dataType l_val3 = 2.0;
	      A[row*N+col] = l_val2;
	      B[row*N+col] = l_val3;
	      c2[row*N+col] = 0.0;
	    }
	  }
      memcpy(&a2[0], A, N*N*sizeof(float));
      memcpy(&b2[0], B, N*N*sizeof(float));

      xblas_sgemm(CblasRowMajor,CblasNoTrans,CblasNoTrans,N,N,N,1,a2,N,b2,N,1,c2,N);
  

        fprintf(stderr,"Result after sgemm\n");
    for (int i = 0; i < c2.size(); i++)
        printf("%f ",c2[i]);

  clearMemory();
  
}
