#include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <stdio.h>  // fgets for popen

#include "gemx_xblas.h"

int main(int argc, char **argv) {   
      int size=64;
      std::vector<GEMX_dataType> a2(size*size);
      std::vector<GEMX_dataType> b2(size*size);
      std::vector<GEMX_dataType> c2(size*size);

      for (int row = 0; row < size;  ++row) {
	    for (int col = 0; col < size;  ++col) {
	      GEMX_dataType l_val2 = 2.0;
	      GEMX_dataType l_val3 = 2.0;
	      a2[row*size+col] = l_val2;
	      b2[row*size+col] = l_val3;
	      c2[row*size+col] = 0.0;
	    }
	  }
      xblas_sgemm(CblasRowMajor,CblasNoTrans,CblasNoTrans,size,size,size,1,a2,size,b2,size,1,c2,size);
  

        fprintf(stderr,"Result after sgemm\n");
    for (int i = 0; i < c2.size(); i++)
        printf("%f ",c2[i]);

  clearMemory();
  
}
