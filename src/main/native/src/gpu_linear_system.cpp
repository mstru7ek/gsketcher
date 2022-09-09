#include "gpu_linear_system.h"

#include "cuerror.h"
#include "settings.h"

/** przyczyna INT na urzadzenieu __device__ __host__ ! ERROR 700       cudaErrorIllegalAddress */

namespace solver {

///
/// Visualt Studio Communit ; > Debug > Wlasciwosci Debugowania > Debugowanie > Srodowisko:
///
///         CUSOLVERDN_LOG_LEVEL=5
///         CUSOLVERDN_LOG_MASK = 16
///

GPULinearSystem::GPULinearSystem(cudaStream_t stream) : stream(stream) {

    // ! blocking - look back
    lastError = cudaPeekAtLastError();
    if (lastError != cudaSuccess) {
        const char *errorName = cudaGetErrorName(lastError);
        const char *errorStr = cudaGetErrorString(lastError);
        printf("[cuSolver]: error solver is not initialized, [ %s ] %s \n", errorName, errorStr);
        exit(1);
    }
    /// reset previous errror

    /// # cuBlas context
    checkCublasStatus(cublasCreate(&cublasHandle));
    checkCublasStatus(cublasSetStream(cublasHandle, stream));

    ///  # cuSolver setup solver -- nie zaincjalozowac wyrzej
    checkCuSolverStatus(cusolverDnCreate(&handle));
    checkCuSolverStatus(cusolverDnSetStream(handle, stream));

    checkCudaStatus(cudaMallocAsync((void **)&devInfo, 1 * sizeof(int), stream));
}

void GPULinearSystem::solveLinearEquation(double *A, double *b, size_t N) {
    //
    //     LU Solver -  !!    this solver REQUIRMENTS -  "  NxN tensor "
    //
    //     - cusolverDnDgetrf_bufferSize
    //
    //     - cusolverDnDgetrf
    //
    //     - cusolverDnDgetrs
    //
    // Considerations - zapis bezposrednio do zmiennych na urzadzeniu !
    //
    // ! blocking - look back
    lastError = cudaPeekAtLastError();
    if (lastError != cudaSuccess) {
        const char *errorName = cudaGetErrorName(lastError);
        const char *errorStr = cudaGetErrorString(lastError);
        printf("[cuSolver]: stream with given error , [ %s ] %s \n", errorName, errorStr);
        exit(1);
    }

    /// reset previous errror

    int preLwork = Lwork;


    int dN = (int)N;
    ///
    /// LU - calculate the size of work buffers needed.
    ///
    checkCuSolverStatus(cusolverDnDgetrf_bufferSize(handle, dN, dN, A, dN, &Lwork));

    checkCudaStatus(cudaPeekAtLastError());

    if (Lwork > preLwork) {
        // free mem
        checkCudaStatus(cudaFreeAsync(Workspace, stream));
        checkCudaStatus(cudaFreeAsync(devIpiv, stream));

        ///  !!!!Remark: getrf uses fastest implementation with large workspace of size m*n

        /// prealocate additional buffer before LU factorization
        Lwork = (int)(Lwork * settings::get()->CU_SOLVER_LWORK_FACTOR);

        checkCudaStatus(cudaMallocAsync((void **)&Workspace, Lwork * sizeof(double), stream));
        checkCudaStatus(cudaMallocAsync((void **)&devIpiv, dN * sizeof(double), stream));

        checkCudaStatus(cudaStreamSynchronize(stream));

        if (settings::get()->DEBUG) {
            printf("[ LU ] workspace attached, 'Lwork' (workspace size)  = %d \n", Lwork);
        }
    }

    ///
    /// LU factorization    --  P * A = L *U
    ///
    ///     P - permutation vector
    ///
    checkCuSolverStatus(cusolverDnDgetrf(handle, dN, dN, A, dN, Workspace, devIpiv, devInfo));

    /*

    */

    /// host infot
    int hInfo = 0;

    /// dont check matrix determinant
    if (settings::get()->DEBUG_CHECK_ARG) {
        checkCudaStatus(cudaMemcpyAsync(&hInfo, devInfo, 1 * sizeof(int), cudaMemcpyDeviceToHost, stream));
        checkCudaStatus(cudaStreamSynchronize(stream));
        if (hInfo < 0) {
            printf("[ LU ] error! wrong parameter %d (exclude handle)\n", -hInfo);
            exit(1);
        }
        if (hInfo > 0) {
            printf("[ LU ] error! tensor A is not positively defined ,  diagonal U(i,i) = 0 ,  i ( %d ) \n", hInfo);
            exit(1);
        }
    }

    if (settings::get()->DEBUG) {
        printf("[ LU ] factorization success ! \n");
    }

    ///
    /// LU  - solves a linear system of multiple right-hand sides
    ///
    ///               solver linear equation A * X = B
    ///
    checkCuSolverStatus(cusolverDnDgetrs(handle, CUBLAS_OP_N, (int)N, 1, A, (int)N, devIpiv, b, (int)N, devInfo));

    /*


    */

    /// dont check arguments
    if (settings::get()->DEBUG_CHECK_ARG) {
        /// inspect computation requirments
        checkCudaStatus(cudaMemcpyAsync(&hInfo, devInfo, sizeof(int), cudaMemcpyDeviceToHost, stream));
        checkCudaStatus(cudaStreamSynchronize(stream));
        ///
        if (hInfo != 0) {
            printf("[ LU ]! parameter is wrong (not counting handle). %d \n", -hInfo);
            exit(1);
        }
    }

    /// suspected data vector to be presetend in vector b  // B

    if (settings::get()->DEBUG) {
        printf("[ LU ] operation successful ! \n");
    }
}

void GPULinearSystem::vectorNorm(int n, double *x, double *result) {

    checkCublasStatus(cublasDnrm2(cublasHandle, n, x, 1, result));

    if (settings::get()->DEBUG_SOLVER_CONVERGENCE) {
        checkCudaStatus(cudaStreamSynchronize(stream));
        printf("[cublas.norm] constraint evalutated norm  = %e \n", *result);
    } else {
        // blad na cublas RESULT jest lokalny  zawsze
        /// checkCublasStatus(cublasDnrm2(cublasHandle, n, x, 1, result))
    }
}

void GPULinearSystem::cublasAPIDaxpy(int n, const double *alpha, const double *x, int incx, double *y, int incy) {

    checkCublasStatus(::cublasDaxpy(cublasHandle, n, alpha, x, incx, y, incy));

    if (settings::get()->DEBUG_SOLVER_CONVERGENCE) {
        checkCudaStatus(cudaStreamSynchronize(stream));
        printf("[cublas.norm] cublasDaxpy \n");
    }
}

GPULinearSystem::~GPULinearSystem() {

    checkCudaStatus(cudaFreeAsync(Workspace, stream));
    checkCudaStatus(cudaFreeAsync(devIpiv, stream));
    checkCudaStatus(cudaFreeAsync(devInfo, stream));

    checkCuSolverStatus(cusolverDnDestroy(handle));

    checkCublasStatus(cublasDestroy(cublasHandle));
}

} // namespace solver
