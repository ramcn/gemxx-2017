#include "libspir_types.h"
#define EXPORT_PIPE_SYMBOLS 1
#include "cpu_pipes.h"
#undef EXPORT_PIPE_SYMBOLS
#include "xcl_half.h"
#include <cstddef>
#include <vector>
#include <pthread.h>

extern "C" {

void gemxKernel_0(size_t p_DdrRd, size_t p_DdrWr);

static pthread_mutex_t __xlnx_cl_gemxKernel_0_mutex = PTHREAD_MUTEX_INITIALIZER;
void __stub____xlnx_cl_gemxKernel_0(char **argv) {
  void **args = (void **)argv;
  size_t p_DdrRd = *((size_t*)args[0+1]);
  size_t p_DdrWr = *((size_t*)args[1+1]);
  pthread_mutex_lock(&__xlnx_cl_gemxKernel_0_mutex);
  gemxKernel_0(p_DdrRd, p_DdrWr);
  pthread_mutex_unlock(&__xlnx_cl_gemxKernel_0_mutex);
}
}
