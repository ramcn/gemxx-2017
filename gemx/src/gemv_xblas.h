#ifndef GEMX_XBLAS_H__
#define GEMX_XBLAS_H__

#include <vector>

#ifdef  __cplusplus
extern "C" {
#endif

enum CBLAS_ORDER_XBLAS { CblasRowMajorXblas, CblascolMajorXblas };
enum CBLAS_TRANSPOSE_XBLAS { CblasNoTransXblas, CblasTransXblas };

void xblas_sgemv(const int __M, const int __K, std::vector<GEMX_dataType>& __A, const int __lda, std::vector<GEMX_dataType>& __B, std::vector<GEMX_dataType>& __C);

void clearMemory();

#ifdef __cplusplus
}
#endif


#endif //define GEMX_XBLAS_H__
