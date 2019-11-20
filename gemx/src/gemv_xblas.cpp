
#include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <stdio.h>  // fgets for popen

#include "gemx_kernel.h"
#include "gemx_fpga.h"
#include "gemx_gen_bin.h"

#include "gemx_xblas.h"

#define l_calcGold 0
#define performance_check 0
//#define VERBOSE -1

//enum CBLAS_ORDER_XBLAS { CblasRowMajor, CblascolMajor };
//enum CBLAS_TRANSPOSE_XBLAS { CblasNoTrans, CblasTrans };

#ifdef  __cplusplus
extern "C" {
#endif
    static bool hasLoadXclbin = false;
    static gemx::Fpga l_fpga;
    
    
    float getBoardFreqMHz(unsigned int p_BoardId) {
    std::string l_freqCmd = "$XILINX_OPENCL/runtime/bin/xbsak query -d" + std::to_string(p_BoardId);;
    float l_freq = -1;
    char l_lineBuf[256];
    std::shared_ptr<FILE> l_pipe(popen(l_freqCmd.c_str(), "r"), pclose);
    //if (!l_pipe) throw std::runtime_error("ERROR: popen(" + l_freqCmd + ") failed");
    if (!l_pipe) std::cout << ("ERROR: popen(" + l_freqCmd + ") failed");
    bool l_nextLine_isFreq = false;
    while (l_pipe && fgets(l_lineBuf, 256, l_pipe.get()) ) {
      std::string l_line(l_lineBuf);
      //std::cout << "DEBUG: read line " << l_line << std::endl;
      if (l_nextLine_isFreq) {
	std::string l_prefix, l_val, l_mhz;
	std::stringstream l_ss(l_line);
	l_ss >> l_prefix >> l_val >> l_mhz;
	l_freq = std::stof(l_val);
	assert(l_mhz == "MHz");
	break;
      } else if (l_line.find("OCL Frequency:") != std::string::npos) {
	l_nextLine_isFreq = true;
      }
    }
    if (l_freq == -1) {
	  //if xbsak does not work, as happens on F1, put the XOCC achieved kernel frequcy here
	  l_freq = 220.7;
      std::cout << "INFO: Failed to get board frequency by xbsak. This is normal for cpu and hw emulation, using -1 MHz for reporting.\n";
    }
    return(l_freq);
  }
  
  void clearMemory(){
    if(hasLoadXclbin){
      l_fpga.clearAll();
    }else{
      std::cout<<"Error: xclbin hasn't been loaded yet \n";
    }
  }

   void xblas_sgemv(const int __M, const int __K, std::vector<GEMX_dataType>& __A, const int __lda, std::vector<GEMX_dataType>& __B, std::vector<GEMX_dataType>& __C){   
    
    //############  Client code - prepare the gemm problem input  ############
    ProgramType l_program[GEMX_numKernels];
    std::string l_handleA[GEMX_numKernels];
    std::string l_handleB[GEMX_numKernels];
    std::string l_handleC[GEMX_numKernels];
    //std::string l_handleA("A"), l_handleB("B"), l_handleC("C");
    //bool l_newAllocA, l_newAllocB, l_newAllocC;
    bool l_newAllocA[GEMX_numKernels];
    bool l_newAllocB[GEMX_numKernels];
    bool l_newAllocC[GEMX_numKernels];
    
    unsigned int l_pageA[GEMX_numKernels];
    unsigned int l_pageB[GEMX_numKernels];
    unsigned int l_pageC[GEMX_numKernels];
    
    MatType l_matA[GEMX_numKernels];
    MatType l_matB[GEMX_numKernels];
    MatType l_matC[GEMX_numKernels];
    
    KargsType l_kargs[GEMX_numKernels];
    int32_t l_postScale;
  // unsigned int l_pageA, l_pageB, l_pageC;
  // MatType l_matA, l_matB, l_matC;
  // GemmArgsType l_gemmArgs;
  // KargsType l_kargs;

    for (int i=0; i<GEMX_numKernels; ++i) {
      l_handleA[i] = "A"+std::to_string(i);
      l_handleB[i] = "B"+std::to_string(i);
      l_handleC[i] = "C"+std::to_string(i);
      
      l_pageA[i] = l_program[i].allocPages(l_handleA[i], l_newAllocA[i], __M * __lda);
      l_pageB[i] = l_program[i].allocPages(l_handleB[i], l_newAllocB[i], __K * 1);
      l_pageC[i] = l_program[i].allocPages(l_handleC[i], l_newAllocC[i], __M * 1);

      l_matA[i].init(__M, __K, __lda, l_program[i].getPageAddr(l_pageA[i]));
      l_matB[i].init(__K, 1, 1, l_program[i].getPageAddr(l_pageB[i]));
      l_matC[i].init(__M, 1, 1, l_program[i].getPageAddr(l_pageC[i]));

      for (int row = 0; row < l_matA[i].rows();  ++row) {
	for (int col = 0; col < l_matA[i].cols();  ++col) {
	  GEMX_dataType l_val = __A[row * __M + col];
	  l_matA[i].getVal(row, col) = l_val;
	}
      }
      
      for (int row = 0; row < l_matB[i].rows();  ++row) {
	for (int col = 0; col < 1;  ++col) {
	  GEMX_dataType l_val = __B[row * 1 + col];
	  l_matB[i].getVal(row, col) = l_val;
	}
      }
      
      
      GemvArgsType l_gemvArgs( l_pageA[i], l_pageB[i], l_pageC[i], __M, __K, __lda);
      l_kargs[i].setGemvArgs(l_gemvArgs);
      l_kargs[i].store(l_program[i].addInstr(), 0);
    }
  
      
    gemx::MemDesc l_memDesc[GEMX_numKernels];
    for (int i=0; i<GEMX_numKernels; ++i) { 
	  l_memDesc[i] = l_program[i].getMemDesc();
    }
  //############  Runtime reporting Infra  ############
  TimePointType l_tp[10];
  unsigned int l_tpIdx = 0;
  l_tp[l_tpIdx] = std::chrono::high_resolution_clock::now(); 
    //############  Run FPGA accelerator  ############
    // Init FPGA
    
    std::string kernelNames[GEMX_numKernels];
    
    for (int i=0; i<GEMX_numKernels; ++i){
	  kernelNames[i] = "gemxKernel_" + std::to_string(i);
    }
    //Please set environment variable of the path of gemx.xclbin
    std::string l_xclbinFile = std::getenv("GEMX_XCLBIN_PATH");
    
    if(!hasLoadXclbin){
      if (l_fpga.loadXclbin(l_xclbinFile, kernelNames)) {
	hasLoadXclbin = true;
	//std::cout << "INFO: created kernels" << std::endl;
      } else {
	std::cerr << "ERROR: failed to load " + l_xclbinFile + "\n";
      }
      //showTimeData("loadXclbin", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;
    }
    //create buffers for transferring data to FPGA
    if (!l_fpga.createBuffers(l_memDesc)) {
      std::cerr << "ERROR: failed to create buffers for transffering data to FPGA DDR\n";
    }
    //showTimeData("created buffers", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;

    // Transfer data to FPGA
    if (l_fpga.copyToFpga()) {
      //std::cout << "INFO: transferred data to FPGA" << std::endl;
    } else {
      std::cerr << "ERROR: failed to copy data to FPGA DDR\n";
    }
    //showTimeData("copyToFpga", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;

    // Gemx kernel ops
    if (l_fpga.callKernels()) {
      (VERBOSE > 0) && std::cout << "INFO: Executed kernel" << std::endl;
    } else {
      std::cerr << "ERROR: failed to call kernels ";
	  for (int i=0; i<GEMX_numKernels; ++i) {
		  std::cerr << kernelNames[i] << " ";
	  }
	  std::cerr << "\n";
    }
    //showTimeData("callKernel", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;

    // Transfer data back to host - due to lazy evaluation this is generally wheer the accelerator performs the work
    if (l_fpga.copyFromFpga()) {
      (VERBOSE > 0) && std::cout << "INFO: Transferred data from FPGA" << std::endl;
    } else {
      std::cerr << "ERROR: failed to copy data from FPGA DDR\n";
    }
    //showTimeData("copyFromFpga", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;
    
    //############  Write the results back to the array of Matrix C  ############
    
      /*for (int row = 0; row < l_matC[0].rows();  ++row) {
	for (int col = 0; col < 1;  ++col) {
	  //C= alpha*A*B + beta*C
	  __C[row * __M + col] = l_matC[0].getVal(row, col);
	}
      }*/
      for (int row = 0; row < l_matC[0].rows();  ++row) {
        for (int col = 0; col < 1;  ++col) {
          GEMX_dataType l_val = l_matC[0].getVal(row, col);
          __C[row * 1 + col] =  l_val;
        }
      }

 }
#ifdef __cplusplus
}
#endif
