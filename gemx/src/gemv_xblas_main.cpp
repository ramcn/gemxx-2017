#include <stdio.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>


#include "gemv_xblas.h"
#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus
   #include <cblas.h>
#ifdef __cplusplus
}
#endif //__cplusplus


int main(int argc, char **argv) {   
      int M=768, N=256;
      std::vector<GEMX_dataType> a2(M*N);
      std::vector<GEMX_dataType> b2(N*1);
      std::vector<GEMX_dataType> c2(M*1);

      for (int row = 0; row < M;  ++row) {
	    for (int col = 0; col < N;  ++col) {
	      GEMX_dataType l_val2 = 2.0;
	      a2[row*N+col] = l_val2;
	      GEMX_dataType l_val3 = 2.0;
	      b2[col] = l_val3;
	    }
	      c2[row] = 0.0;
	  }

      xblas_sgemv(M,N,a2,N,b2,c2);
      //cblas_sgemv(CblasRowMajor,CblasNoTrans,M,N,1.0,&a2[0],256,&b2[0],1, 1.0, &c2[0],1);
  
      fprintf(stderr,"Result after sgemm\n");
      for (int i = 0; i < c2.size(); i++)
        printf("%f ",c2[i]);

      clearMemory();
  
}
