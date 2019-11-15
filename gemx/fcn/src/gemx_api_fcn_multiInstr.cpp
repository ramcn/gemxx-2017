 #include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <stdio.h>  // fgets for popen

#include "gemx_fcn_kernel.h"
#include "gemx_fcn_fpga.h"
#include "gemx_fcn_gen_bin.h"

//#define VERBOSE 0 

bool checkDim(unsigned int p_Val, unsigned int p_Mod, unsigned int p_Min) {
    bool l_ok = true;
    if (p_Val % p_Mod != 0) {
        std::cerr << "ERROR: value " << p_Val << " must be multiple of " << p_Mod << "\n";
        l_ok = false;
    }
    if (p_Val < p_Min) {
        std::cerr << "ERROR: value " << p_Val << " must be at least " << p_Min << "\n";
        l_ok = false;
    }
    return(l_ok);
}

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
        l_freq = 246;
        std::cout << "INFO: Failed to get board frequency by xbsak. This is normal for cpu and hw emulation, using -1 MHz for reporting.\n";
    }
    return(l_freq);
}



int main(int argc, char **argv)
{
    //############  UI and FCN problem size  ############
    if (argc < 6) {
        std::cerr << "Usage:\n"
                <<  "  gemx_api_fcn_multiInstr.exe <path/gemx.xclbin> [M K N  LdA LdB LdC LdX postScaleVal postScaleShift PReluScale PReluAlpha HandleA HandleB HandleC HandleX]\n"
                <<  "  gemx_api_fcn_multiInstr.exe <path/gemx.xclbin> [0 0 0 insFile matAFile matBFile matXFile] \n";
        exit(2);
    }
    
    unsigned int l_argIdx = 1;
    std::string l_xclbinFile(argv[l_argIdx]);
    unsigned int l_instrCount = 0;
    bool readFromFile = 0;
    bool readFromFiles = 0;
    
    if(atoi(argv[2]) == 0 && atoi(argv[3]) == 0 && atoi(argv[4]) == 0){
      if((argc-2)% 7 == 0){
	l_instrCount = ((argc-2)/7>1)?((argc-2)/7):1; //number of instructions
	readFromFiles = 1;
      }else{
	 std::cerr << "  usage: \n" 
		   << "         gemx_api_fcn_multiInstr.exe <path/gemx.xclbin> 0 0 0 insFile matAFile matBFile matXFile\n";
	 exit(2);
      }
    }else{
      
      if ((argc - 2) % 15 != 0) {
	  std::cerr << "  For each instruction, [M K N  LdA LdB LdC LdX postScaleVal postScaleShift PReluScale PReluAlpha HandleA HandleB HandleC HandleX] could not be missing\n";
	  exit(2);
      }
      l_instrCount = ((argc-2)/15>1)?((argc-2)/15):1; //number of instructions
    }
    
    if(l_instrCount > 15){
        std::cerr << "  Too many instructions at same time\n";
        exit(2);
    }

    unsigned int l_ddrW = GEMX_ddrWidth;
    unsigned int l_m[l_instrCount];
    unsigned int l_k[l_instrCount];
    unsigned int l_n[l_instrCount];

    unsigned int l_lda;
    unsigned int l_ldb;
    unsigned int l_ldc;
    unsigned int l_ldx;
    
   
    int32_t l_postScaleVal;
    int32_t l_postScaleShift;
    int32_t l_postScale;
    int16_t l_PReluScale;
    int16_t l_PReluAlpha;
    int16_t l_PReluVal;

    printf("GEMX-fcn C++ API example using accelerator image \n",
            l_xclbinFile.c_str());
    ProgramType l_program[GEMX_numKernels];
    ProgramType l_program_golden;
    //unsigned long int l_Ops[l_instrCount]; //operations carried out by each kernel
    
    for(int i = 0; i < GEMX_numKernels; i++) {
        l_argIdx = 2;
        for(int index = 0; index<l_instrCount; index++){
	    if(readFromFiles){
	     
	     l_argIdx=l_argIdx+3;
	     std::string l_insFileName(argv[l_argIdx++]);
	     std::string l_matAFileName(argv[l_argIdx++]);
	     std::string l_matBFileName(argv[l_argIdx++]);
	     std::string l_matXFileName(argv[l_argIdx++]);

	     unsigned int l_pageA, l_pageB, l_pageC, l_pageX;
	     std::string l_handleA, l_handleB, l_handleC, l_handleX;
	     bool l_newAllocA, l_newAllocB, l_newAllocC, l_newAllocX;
	     
	     std::ifstream l_fs_ins(l_insFileName.c_str(),   std::ios_base::in | std::ios_base::binary);
	     std::ifstream l_fs_matA(l_matAFileName.c_str(), std::ios_base::in | std::ios_base::binary);
	     std::ifstream l_fs_matB(l_matBFileName.c_str(), std::ios_base::in | std::ios_base::binary);
	     std::ifstream l_fs_matX(l_matXFileName.c_str(), std::ios_base::in | std::ios_base::binary);
	     
	     boost::iostreams::filtering_istream l_bs_ins;
	     l_bs_ins.push(l_fs_ins);
	     boost::iostreams::filtering_istream l_bs_matA;
	     l_bs_matA.push(l_fs_matA);
	     boost::iostreams::filtering_istream l_bs_matB;
	     l_bs_matB.push(l_fs_matB);
	     boost::iostreams::filtering_istream l_bs_matX;
	     l_bs_matX.push(l_fs_matX);
	     bool l_good = l_bs_ins.good() && l_bs_matA.good() && l_bs_matB.good() && l_bs_matX.good();

	     bool ok=true;
	     const unsigned int l_Edge = GEMX_ddrWidth;
	     const unsigned int l_kMin = 2 * GEMX_ddrWidth;
	     
	     if (l_good) {
		      unsigned int l_index;
		      while (l_bs_ins.peek()=='#') l_bs_ins.ignore(2048, '\n');
		      l_bs_ins >> l_index;
		      while (l_index < index){
			l_bs_ins.ignore(2048, '\n');
			l_bs_ins >> l_index;		
		      }
		      std::cout<<"INFO instr number : "<<l_index<<"\n";
		      l_bs_ins >> l_postScale >> l_PReluVal;
		      std::cout << "INFO " << l_postScale << " " << l_PReluVal << "\n";
		      //read A dimensions
		      l_bs_ins >> l_handleA >> l_m[index] >> l_k[index] >> l_lda;
		      std::cout << "INFO " << l_handleA << " " << l_m[index] << " " << l_k[index] << " " << l_lda << "\n";

		      //allocate host memory and initialize it with data from the file
		      l_pageA = l_program[i].allocPages(l_handleA, l_newAllocA, l_m[index] * l_lda);
		      MatType l_matA(l_m[index], l_k[index], l_lda, l_program[i].getPageAddr(l_pageA));
		      while (l_bs_matA.peek()=='#') l_bs_matA.ignore(2048, '\n');
		      if (l_newAllocA) {
			      l_matA.fillFromFile(l_bs_matA);	
		      }
	      
		      //read matrix B dimensions
		      unsigned int l_bK;
		      l_bs_ins >> l_handleB >> l_bK >> l_n[index] >> l_ldb;
		      std::cout << "INFO " << l_handleB << " " << l_bK << " " << l_n[index] << " " << l_ldb << "\n";
		      assert(l_bK == l_k[index]);


		      l_pageB = l_program[i].allocPages(l_handleB, l_newAllocB, l_k[index] * l_ldb);
		      MatType l_matB(l_k[index], l_n[index], l_ldb, l_program[i].getPageAddr(l_pageB));
		      while (l_bs_matB.peek()=='#') l_bs_matB.ignore(2048, '\n');
		      if (l_newAllocB) {
			      l_matB.fillFromFile(l_bs_matB);	
		      }
		      
		      //read matrix X dimensions
		      unsigned int l_xM, l_xN;
		      l_bs_ins >> l_handleX >> l_xM >> l_xN >> l_ldx;
		      std::cout << "INFO " << l_handleX << " " << l_xM << " " << l_xN << " " << l_ldx << "\n";
		      assert(l_xM == l_m[index]);
		      assert(l_xN == l_n[index]);

		      l_pageX = l_program[i].allocPages(l_handleX, l_newAllocX, l_m[index]*l_n[index]*(sizeof(GEMX_XdataType)/sizeof(GEMX_dataType)));
		      XMatType l_matX(l_m[index], l_n[index], l_ldx, (GEMX_XdataType *) l_program[i].getPageAddr(l_pageX));
		      while (l_bs_matX.peek()=='#') l_bs_matX.ignore(2048, '\n');
		      if (l_newAllocX) {
			      l_matX.fillFromFile(l_bs_matX);
		      }
		      //read matrix C dimensions
		      unsigned int l_cM, l_cN;
		      l_bs_ins >> l_handleC >> l_cM >> l_cN >> l_ldc;
		      std::cout << "INFO " << l_handleC << " " << l_cM << " " << l_cN << " " << l_ldc << "\n";
		      assert(l_cM == l_m[index]);
		      assert(l_cN == l_n[index]);
		      l_pageC = l_program[i].allocPages(l_handleC, l_newAllocC, l_m[index] * l_ldc);
		      MatType l_matC(l_m[index], l_n[index], l_ldc, l_program[i].getPageAddr(l_pageC));

		      FcnArgsType l_fcnArgs(
			      l_pageA, l_pageB, l_pageC, l_pageX,
			      l_m[index], l_k[index], l_n[index],
			      l_lda, l_ldb, l_ldc, l_ldx,
			      l_postScale,
			      l_PReluVal
		      );
		      KargsType l_kargs;
		      l_kargs.setFcnArgs(l_fcnArgs);
		      l_kargs.store(l_program[i].addInstr(),0);
		      
		      //l_Ops[index] = 2ull * l_m[index] * l_n[index] * l_k[index] + l_m[index] * l_n[index] * 3;
		      std::cout << "Added FCN" << l_m[index] << "x" << l_k[index]  << "x" << l_n[index] << " postScale: " << l_postScale << " PReluVal: " << l_PReluVal << "\n";
		      boost::iostreams::close(l_bs_ins);
		      boost::iostreams::close(l_bs_matA);
		      boost::iostreams::close(l_bs_matB);
		      boost::iostreams::close(l_bs_matX);
		      
	      } else {
		      std::cout << "ERROR: bad filename" << "\n";
	      }

	     
	    }else{
	      l_m[index] = atoi(argv[l_argIdx++]);
	      l_k[index] = atoi(argv[l_argIdx++]);
	      l_n[index] = atoi(argv[l_argIdx++]);
	    
	      l_lda = atoi(argv[l_argIdx++]);
	      l_ldb = atoi(argv[l_argIdx++]);
	      l_ldc = atoi(argv[l_argIdx++]);
	      l_ldx = atoi(argv[l_argIdx++]);
	    
	      l_postScaleVal = atoi(argv[l_argIdx++]);
	      l_postScaleShift = atoi(argv[l_argIdx++]);
	      l_postScale = (l_postScaleVal << 8) | (l_postScaleShift & 0x000000ff);
	      
	      l_PReluScale = atoi(argv[l_argIdx++]);
	      l_PReluAlpha = atoi(argv[l_argIdx++]);
	      l_PReluVal = (l_PReluScale << 6) | (l_PReluAlpha & 0x003f);

	      std::string l_handleA = argv[l_argIdx++];
	      std::string l_handleB = argv[l_argIdx++];
	      std::string l_handleC = argv[l_argIdx++];
	      std::string l_handleX = argv[l_argIdx++];
	      

	      assert(l_lda >= l_k[index]);
	      assert(l_ldb >= l_n[index]);
	      assert(l_ldc >= l_n[index]);
	      assert(l_ldx >= l_n[index]);
	      if (! (
		      checkDim(l_m[index], l_ddrW*GEMX_gemmMBlocks, l_ddrW*GEMX_gemmMBlocks) &&
		      checkDim(l_k[index], l_ddrW*GEMX_gemmKBlocks, l_ddrW*GEMX_gemmKBlocks) &&
		      checkDim(l_n[index], l_ddrW*GEMX_gemmNBlocks, l_ddrW*GEMX_gemmNBlocks) &&
		      checkDim(l_lda, l_ddrW, l_k[index]) &&
		      checkDim(l_ldb, l_ddrW, l_n[index]) &&
		      checkDim(l_ldc, l_ddrW, l_n[index]) &&
		      checkDim(l_ldx, l_ddrW, l_n[index])
	      )) {
		  return EXIT_FAILURE;
	      }

	      //l_Ops[index] = 2ull * l_m[index] * l_n[index] * l_k[index] + l_m[index] * l_n[index] * 3;

	      // Allocate all pages before getting any address
	      bool l_newAllocA, l_newAllocB, l_newAllocC, l_newAllocX;
	      unsigned int l_pageA = l_program[i].allocPages(l_handleA, l_newAllocA, l_m[index] * l_lda);
	      unsigned int l_pageB = l_program[i].allocPages(l_handleB, l_newAllocB, l_k[index] * l_ldb);
	      unsigned int l_pageC = l_program[i].allocPages(l_handleC, l_newAllocC, l_m[index] * l_ldc);
	      unsigned int l_pageX = l_program[i].allocPages(l_handleX, l_newAllocX, l_m[index] * l_ldx * (sizeof(GEMX_XdataType)/sizeof(GEMX_dataType)));
	      

	      // Get addresses where matrices are stored
	      MatType l_matA(l_m[index], l_k[index], l_lda, l_program[i].getPageAddr(l_pageA));
	      MatType l_matB(l_k[index], l_n[index], l_ldb, l_program[i].getPageAddr(l_pageB));
	      XMatType l_matX(l_m[index], l_n[index], l_ldx, (GEMX_XdataType *) l_program[i].getPageAddr(l_pageX));
	      MatType l_matC(l_m[index], l_n[index], l_ldc, l_program[i].getPageAddr(l_pageC));

	      if (l_newAllocA) {
		  l_matA.fillMod(3, index);
	      }
	      if (l_newAllocB) {
		  l_matB.fillMod(2, index);
	      }
	      
	      if (l_newAllocX) {
		  l_matX.fillMod(1,0);
	      }

	      // Instruction
	      FcnArgsType l_fcnArgs(
		l_pageA, l_pageB, l_pageC, l_pageX,
		l_m[index], l_k[index], l_n[index],
		l_lda, l_ldb, l_ldc, l_ldx,
		l_postScale, l_PReluVal
	      );
	      
	      KargsType l_kargs;
	      l_kargs.setFcnArgs(l_fcnArgs);
	      l_kargs.store(l_program[i].addInstr(), 0);
	      
	      std::cout << "Added FCN " << l_m[index] << "x" << l_k[index] << "x" << l_n[index] <<" in kernel " << i << "  \n";
	    }
        }
    }
    //golden program
    l_argIdx = 2;
    std::cout << "Calculate golden result on host, for large matrix size, this will take long time.\n" << std::endl;
    if(!getenv("SKIPPED_GOLD_CAL")){
        for(int index = 0; index<l_instrCount; index++){
	  if(readFromFiles){
	     
	     l_argIdx=l_argIdx+3;
	     std::string l_insFileName(argv[l_argIdx++]);
	     std::string l_matAFileName(argv[l_argIdx++]);
	     std::string l_matBFileName(argv[l_argIdx++]);
	     std::string l_matXFileName(argv[l_argIdx++]);

	     unsigned int l_pageA, l_pageB, l_pageC, l_pageX;
	     std::string l_handleA, l_handleB, l_handleC, l_handleX;
	     bool l_newAllocA, l_newAllocB, l_newAllocC, l_newAllocX;
	     
	     std::ifstream l_fs_ins(l_insFileName.c_str(),   std::ios_base::in | std::ios_base::binary);
	     std::ifstream l_fs_matA(l_matAFileName.c_str(), std::ios_base::in | std::ios_base::binary);
	     std::ifstream l_fs_matB(l_matBFileName.c_str(), std::ios_base::in | std::ios_base::binary);
	     std::ifstream l_fs_matX(l_matXFileName.c_str(), std::ios_base::in | std::ios_base::binary);
	     
	     boost::iostreams::filtering_istream l_bs_ins;
	     l_bs_ins.push(l_fs_ins);
	     boost::iostreams::filtering_istream l_bs_matA;
	     l_bs_matA.push(l_fs_matA);
	     boost::iostreams::filtering_istream l_bs_matB;
	     l_bs_matB.push(l_fs_matB);
	     boost::iostreams::filtering_istream l_bs_matX;
	     l_bs_matX.push(l_fs_matX);
	     bool l_good = l_bs_ins.good() && l_bs_matA.good() && l_bs_matB.good() && l_bs_matX.good();

	     bool ok=true;
	     const unsigned int l_Edge = GEMX_ddrWidth;
	     const unsigned int l_kMin = 2 * GEMX_ddrWidth;
	     
	     if (l_good) {
		      unsigned int l_index;
		      while (l_bs_ins.peek()=='#') l_bs_ins.ignore(2048, '\n');
		      l_bs_ins >> l_index;
		      while (l_index < index){
			l_bs_ins.ignore(2048, '\n');
			l_bs_ins >> l_index;		
		      }
		      std::cout<<"INFO instr number : "<<l_index<<"\n";
		      l_bs_ins >> l_postScale >> l_PReluVal;
		      std::cout << "INFO " << l_postScale << " " << l_PReluVal << "\n";
		      //read A dimensions
		      l_bs_ins >> l_handleA >> l_m[index] >> l_k[index] >> l_lda;
		      std::cout << "INFO " << l_handleA << " " << l_m[index] << " " << l_k[index] << " " << l_lda << "\n";

		      //allocate host memory and initialize it with data from the file
		      l_pageA = l_program_golden.allocPages(l_handleA, l_newAllocA, l_m[index] * l_lda);
		      MatType l_matA(l_m[index], l_k[index], l_lda, l_program_golden.getPageAddr(l_pageA));
		      while (l_bs_matA.peek()=='#') l_bs_matA.ignore(2048, '\n');
		      if (l_newAllocA) {
			      l_matA.fillFromFile(l_bs_matA);	
		      }
	      
		      //read matrix B dimensions
		      unsigned int l_bK;
		      l_bs_ins >> l_handleB >> l_bK >> l_n[index] >> l_ldb;
		      std::cout << "INFO " << l_handleB << " " << l_bK << " " << l_n[index] << " " << l_ldb << "\n";
		      assert(l_bK == l_k[index]);


		      l_pageB = l_program_golden.allocPages(l_handleB, l_newAllocB, l_k[index] * l_ldb);
		      MatType l_matB(l_k[index], l_n[index], l_ldb, l_program_golden.getPageAddr(l_pageB));
		      while (l_bs_matB.peek()=='#') l_bs_matB.ignore(2048, '\n');
		      if (l_newAllocB) {
			      l_matB.fillFromFile(l_bs_matB);	
		      }
		      
		      //read matrix X dimensions
		      unsigned int l_xM, l_xN;
		      l_bs_ins >> l_handleX >> l_xM >> l_xN >> l_ldx;
		      std::cout << "INFO " << l_handleX << " " << l_xM << " " << l_xN << " " << l_ldx << "\n";
		      assert(l_xM == l_m[index]);
		      assert(l_xN == l_n[index]);


		      l_pageX = l_program_golden.allocPages(l_handleX, l_newAllocX, l_m[index]*l_n[index]*(sizeof(GEMX_XdataType)/sizeof(GEMX_dataType)));
		      XMatType l_matX(l_m[index], l_n[index], l_ldx, (GEMX_XdataType *) l_program_golden.getPageAddr(l_pageX));
		      while (l_bs_matX.peek()=='#') l_bs_matX.ignore(2048, '\n');
		      if (l_newAllocX) {
			      l_matX.fillFromFile(l_bs_matX);
		      }
		      //read matrix C dimensions
		      unsigned int l_cM, l_cN;
		      l_bs_ins >> l_handleC >> l_cM >> l_cN >> l_ldc;
		      std::cout << "INFO " << l_handleC << " " << l_cM << " " << l_cN << " " << l_ldc << "\n";
		      assert(l_cM == l_m[index]);
		      assert(l_cN == l_n[index]);

		      l_pageC = l_program_golden.allocPages(l_handleC, l_newAllocC, l_m[index] * l_ldc);
		      MatType l_matC(l_m[index], l_n[index], l_ldc, l_program_golden.getPageAddr(l_pageC));


		      l_matC.matMultWithScaleAndPRelu(l_matA, l_matB, l_matX, l_postScale, l_PReluVal);


		      FcnArgsType l_fcnArgs(
			      l_pageA, l_pageB, l_pageC, l_pageX,
			      l_m[index], l_k[index], l_n[index],
			      l_lda, l_ldb, l_ldc, l_ldx,
			      l_postScale,
			      l_PReluVal
		      );
		      KargsType l_kargs;
		      l_kargs.setFcnArgs(l_fcnArgs);
		      l_kargs.store(l_program_golden.addInstr(),0);

		      std::cout << "Calculated golden result fcn"<< l_m[index] << "x" << l_k[index] << "x" <<  l_n[index] <<" on host\n";
		      boost::iostreams::close(l_bs_ins);
		      boost::iostreams::close(l_bs_matA);
		      boost::iostreams::close(l_bs_matB);
		      boost::iostreams::close(l_bs_matX);
		      
	      } else {
		      std::cout << "ERROR: bad filename" << "\n";
	      }

	     
	    }else{
            l_m[index] = atoi(argv[l_argIdx++]);
            l_k[index] = atoi(argv[l_argIdx++]);
            l_n[index] = atoi(argv[l_argIdx++]);
           
            l_lda = atoi(argv[l_argIdx++]);
            l_ldb = atoi(argv[l_argIdx++]);
            l_ldc = atoi(argv[l_argIdx++]);
	    l_ldx = atoi(argv[l_argIdx++]);
	   
	    l_postScaleVal = atoi(argv[l_argIdx++]);
	    l_postScaleShift = atoi(argv[l_argIdx++]);
	    l_postScale = (l_postScaleVal << 8) | (l_postScaleShift & 0x000000ff);
	    
	    l_PReluScale = atoi(argv[l_argIdx++]);
	    l_PReluAlpha = atoi(argv[l_argIdx++]);
	    l_PReluVal = (l_PReluScale << 6) | (l_PReluAlpha & 0x003f);

            std::string l_handleA = argv[l_argIdx++];
            std::string l_handleB = argv[l_argIdx++];
            std::string l_handleC = argv[l_argIdx++];
	    std::string l_handleX = argv[l_argIdx++];

          
            // Allocate all pages before getting any address
            bool l_newAllocA, l_newAllocB, l_newAllocC, l_newAllocX;
            unsigned int l_pageA = l_program_golden.allocPages(l_handleA, l_newAllocA, l_m[index] * l_lda);
            unsigned int l_pageB = l_program_golden.allocPages(l_handleB, l_newAllocB, l_k[index] * l_ldb);
            unsigned int l_pageC = l_program_golden.allocPages(l_handleC, l_newAllocC, l_m[index] * l_ldc);
	    unsigned int l_pageX = l_program_golden.allocPages(l_handleX, l_newAllocX, l_m[index] * l_ldx * (sizeof(GEMX_XdataType)/sizeof(GEMX_dataType)));

            // Get addresses where matrices are stored
            MatType l_matA(l_m[index], l_k[index], l_lda, l_program_golden.getPageAddr(l_pageA));
            MatType l_matB(l_k[index], l_n[index], l_ldb, l_program_golden.getPageAddr(l_pageB));
	    XMatType l_matX(l_m[index], l_n[index], l_ldx, (GEMX_XdataType *) l_program_golden.getPageAddr(l_pageX));
            MatType l_matC(l_m[index], l_n[index], l_ldc, l_program_golden.getPageAddr(l_pageC));

            if (l_newAllocA) {
                l_matA.fillMod(3, index);
            }
            if (l_newAllocB) {
                l_matB.fillMod(2, index);
            }
            
            if (l_newAllocX) {
		l_matX.fillMod(1,0);
	    }

            // Instruction
	    FcnArgsType l_fcnArgs(
	      l_pageA, l_pageB, l_pageC, l_pageX,
	      l_m[index], l_k[index], l_n[index],
              l_lda, l_ldb, l_ldc, l_ldx,
	      l_postScale, l_PReluVal
	    );
	    
	    KargsType l_kargs;
	    l_kargs.setFcnArgs(l_fcnArgs);
	    l_kargs.store(l_program_golden.addInstr(), 0);
	    l_matC.matMultWithScaleAndPRelu(l_matA, l_matB, l_matX, l_postScale, l_PReluVal);
	    
            std::cout << "Calculated golden result fcn"<< l_m[index] << "x" << l_k[index] << "x" <<  l_n[index] <<" on host\n";
	    }
        }
    }
    std::string kernelNames[GEMX_numKernels];
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
    gemx::Fpga l_fpga;

    for (int i=0; i<GEMX_numKernels; ++i){
        kernelNames[i] = "gemxKernel_" + std::to_string(i);
    }
    if (l_fpga.loadXclbin(l_xclbinFile, kernelNames)) {
        std::cout << "INFO: created kernels" << std::endl;
    } else {
        std::cerr << "ERROR: failed to load " + l_xclbinFile + "\n";
        return EXIT_FAILURE;
    }
    showTimeData("loadXclbin", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;

    //create buffers for transferring data to FPGA
    if (!l_fpga.createBuffers(l_memDesc)) {
        std::cerr << "ERROR: failed to create buffers for transffering data to FPGA DDR\n";
        return EXIT_FAILURE;
    }
    showTimeData("created buffers", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;

    // Transfer data to FPGA
    if (l_fpga.copyToFpga()) {
        (VERBOSE > 0) && std::cout << "INFO: transferred data to FPGA" << std::endl;
    } else {
        std::cerr << "ERROR: failed to copy data to FPGA DDR\n";
        return EXIT_FAILURE;
    }
    showTimeData("copyToFpga", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;

    // Gemx kernel ops
    if (l_fpga.callKernels()) {
        (VERBOSE > 0) && std::cout << "INFO: Executed kernel" << std::endl;
    } else {
        std::cerr << "ERROR: failed to call kernels ";
        for (int i=0; i<GEMX_numKernels; ++i) {
            std::cerr << kernelNames[i] << " ";
        }
        std::cerr << "\n";
        return EXIT_FAILURE;
    }
    showTimeData("callKernel", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;

    // Transfer data back to host - due to lazy evaluation this is generally wheer the accelerator performs the work
    if (l_fpga.copyFromFpga()) {
        (VERBOSE > 0) && std::cout << "INFO: Transferred data from FPGA" << std::endl;
    } else {
        std::cerr << "ERROR: failed to copy data from FPGA DDR\n";
        return EXIT_FAILURE;
    }
    showTimeData("copyFromFpga", l_tp[l_tpIdx], l_tp[l_tpIdx+1]); l_tpIdx++;
    showTimeData("total", l_tp[0], l_tp[l_tpIdx]); l_tpIdx++;
    double l_timeApiInMs = -1;
    showTimeData("subtotalFpga", l_tp[2], l_tp[l_tpIdx], &l_timeApiInMs); l_tpIdx++; // Host->DDR, kernel, DDR->host

    //############  Get the exact kernel time from HW cycle counters on the accelerator  ############
    float l_boardFreqMHz = getBoardFreqMHz(0);
    //unsigned long int l_Ops = 2ull * l_m * l_n * l_k * 2; //operations carried out by each kernel
    KargsType l_kargsRes[GEMX_numKernels];
    KargsOpType l_op[GEMX_numKernels*l_instrCount];
    gemx::InstrResArgs l_instrRes[GEMX_numKernels*l_instrCount];
    unsigned long int l_cycleCount[GEMX_numKernels];
    unsigned long int l_maxCycleCount=0;
    double l_timeKernelInMs[GEMX_numKernels];
    double l_maxTimeKernelInMs=0;
    double l_perfKernelInTops[GEMX_numKernels];
    double l_totalPerfKernelInTops=0;
    double l_perfApiInTops;
    double l_timeMsAt100pctEff;
    double l_effKernelPct;
    double l_effApiPct;

    unsigned long int l_total_Ops = 0;
    unsigned long int l_total_parallel_Ops = 0;
    for(int j=0;j<l_instrCount;++j){
      l_total_Ops += 2ull * l_m[j] * l_n[j] * l_k[j] + l_m[j] * l_n[j] * 3;
      l_total_parallel_Ops += 2ull * l_m[j] * l_k[j] * l_n[j];
    }

    for (int i=0; i<GEMX_numKernels; ++i) {
        l_cycleCount[i] = 0;
        for(int j=0;j<l_instrCount;++j){ //number of instructions
            l_op[i*l_instrCount+j] = l_kargsRes[i].load(l_program[i].getBaseResAddr(), j * l_kargsRes[i].getInstrWidth());
            //l_op[i*2+j] = l_kargsRes[i].load(l_program[i].getBaseResAddr(), j);
            assert(l_op[i*l_instrCount+j] == KargsType::OpResult);
            l_instrRes[i*l_instrCount+j] = l_kargsRes[i].getInstrResArgs();
            l_cycleCount[i] += l_instrRes[i*l_instrCount+j].getDuration();
            std::cout << std::string("cycles in kernel ") <<i<<"  "<< l_instrRes[i*l_instrCount+j].getDuration() <<std::endl;
        }
        l_maxCycleCount = (l_cycleCount[i] > l_maxCycleCount)? l_cycleCount[i]: l_maxCycleCount;
        l_timeKernelInMs[i] = l_cycleCount[i] / (l_boardFreqMHz * 1e6) * 1e3;
        l_maxTimeKernelInMs = (l_timeKernelInMs[i] > l_maxTimeKernelInMs)? l_timeKernelInMs[i]: l_maxTimeKernelInMs;
        l_perfKernelInTops[i] = l_total_Ops / (l_timeKernelInMs[i] * 1e-3) / 1e12;
        l_totalPerfKernelInTops += l_perfKernelInTops[i];
    }
    l_perfApiInTops = (l_total_Ops*GEMX_numKernels) / (l_timeApiInMs * 1e-3) / 1e12;
    l_timeMsAt100pctEff = l_total_parallel_Ops / 2 / GEMX_ddrWidth / GEMX_ddrWidth / (l_boardFreqMHz * 1e6) * 1e3;
    l_effKernelPct = 100 * l_timeMsAt100pctEff / l_maxTimeKernelInMs;
    l_effApiPct = 100 * l_timeMsAt100pctEff / l_timeApiInMs;
    // Show time, Tops in csv format
    std::cout <<"In each kernel, it ran "<<l_instrCount<<" instructions, size for matrices are: " <<"\n";
    for(int i=0;i<l_instrCount;++i){
        std::cout <<"m,k,n: "<<l_m[i]<<","<<l_k[i]<<","<<l_n[i]<<" \n";
    }
    std::cout << std::string("DATA_CSV:,DdrWidth,Freq,")
    + "Ops,KernelCycles,"
    + "TimeKernelMs,TimeApiMs,"
    + "EffKernelPct,EffApiPct,"
    + "PerfKernelTops,PerfApiTops\n"
    << "DATA_CSV:," <<  GEMX_ddrWidth << "," << l_boardFreqMHz << ","
    << l_total_Ops*GEMX_numKernels << "," << l_maxCycleCount << ","
    << l_maxTimeKernelInMs << "," << l_timeApiInMs << ","
    << l_effKernelPct << "," << l_effApiPct << ","
    << l_totalPerfKernelInTops << "," << l_perfApiInTops
    << std::endl;

    //############  Compare tha FPGA results with the reference results  ############
    // Calculate reference C = A * B
    // Since the reference is not needed on the acclerator allocate memory in any way
    if(!getenv("SKIPPED_GOLD_CAL")){
        float  l_TolRel = 1e-3,  l_TolAbs = 1e-5;
        bool l_isLastOp = false;
        bool l_compareOk = true;
        KargsType l_kargs0, l_kargs1;
        unsigned int l_pc = 0;
        GenFcn l_fcn;

        do {
            KargsOpType l_op0 = l_kargs0.load(l_program_golden.getBaseInstrAddr(), l_pc);
            KargsOpType l_op1 = l_kargs1.load(l_program[0].getBaseInstrAddr(), l_pc);
            if (l_op1 == KargsType::OpResult) {
                break;
            }
            assert(l_op0 == l_op1);
            switch(l_op0) {
                case KargsType::OpFcn: {
                    FcnArgsType l_fcnArgs = l_kargs0.getFcnArgs();
                    bool l_opOk = l_fcn.compare(l_TolRel, l_TolAbs, l_program_golden, l_program[0], l_fcnArgs);
                    l_compareOk = l_compareOk && l_opOk;
                    break;
                }
            }
            l_pc += l_kargs0.getInstrWidth();
        } while(!l_isLastOp);

        if (!l_compareOk) {
	std::cout << "fail\n" << std::endl;
      }else{
	std::cout << "pass\n" << std::endl;
      }
        return l_compareOk ? EXIT_SUCCESS : EXIT_FAILURE;
    }else{
        std::cout << "INFO: skipped gold calculation on host since it may take too long\n" << std::endl;
    }
    return EXIT_SUCCESS;
}
