#!/bin/sh

# 
# xocc(TM)
# runme.sh: a xocc-generated Runs Script for UNIX
# Copyright 1986-2017 Xilinx, Inc. All Rights Reserved.
# 

if [ -z "$PATH" ]; then
  PATH=/opt/Xilinx/SDK/2017.4.op/bin:/opt/Xilinx/SDx/2017.4.op/bin:/opt/Xilinx/Vivado/2017.4.op/bin
else
  PATH=/opt/Xilinx/SDK/2017.4.op/bin:/opt/Xilinx/SDx/2017.4.op/bin:/opt/Xilinx/Vivado/2017.4.op/bin:$PATH
fi
export PATH

if [ -z "$LD_LIBRARY_PATH" ]; then
  LD_LIBRARY_PATH=
else
  LD_LIBRARY_PATH=:$LD_LIBRARY_PATH
fi
export LD_LIBRARY_PATH

HD_PWD='/home/centos/src/project_data/aws-fpga/git/gemxx-2017/gemx/out_sw_emu/k0dir/_xocc_compile_gemx_kernel_gemx.dir/impl/kernels/gemxKernel_0'
cd "$HD_PWD"

HD_LOG=runme.log
/bin/touch $HD_LOG

ISEStep="./ISEWrap.sh"
EAStep()
{
     $ISEStep $HD_LOG "$@" >> $HD_LOG 2>&1
     if [ $? -ne 0 ]
     then
         exit
     fi
}

EAStep vivado_hls -f gemxKernel_0.tcl -messageDb vivado_hls.pb
