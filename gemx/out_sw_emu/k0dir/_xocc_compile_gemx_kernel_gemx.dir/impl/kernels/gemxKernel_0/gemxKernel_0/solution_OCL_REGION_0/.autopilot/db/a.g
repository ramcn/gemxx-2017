#!/bin/sh
lli=${LLVMINTERP-lli}
exec $lli \
    /home/centos/src/project_data/aws-fpga/git/gemxx-2017/gemx/out_sw_emu/k0dir/_xocc_compile_gemx_kernel_gemx.dir/impl/kernels/gemxKernel_0/gemxKernel_0/solution_OCL_REGION_0/.autopilot/db/a.g.bc ${1+"$@"}
