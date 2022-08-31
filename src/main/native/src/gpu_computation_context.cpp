#include "gpu_computation_context.h"

#include "cuerror.h"

#include "utility.cuh"

namespace solver {

GPUComputationContext::GPUComputationContext() {
    // initialize all static cuda context - no direct or indirect dependent on geometric model.

    if (stream == nullptr) {
        // implicit in utility::
        checkCudaStatus(cudaStreamCreate(&stream));

        for (int itr = 0; itr < CMAX; itr++) {
            // #observations
            checkCudaStatus(cudaEventCreate(&prepStart[itr]));
            checkCudaStatus(cudaEventCreate(&prepStop[itr]));
            checkCudaStatus(cudaEventCreate(&computeStart[itr]));
            checkCudaStatus(cudaEventCreate(&computeStop[itr]));
            checkCudaStatus(cudaEventCreate(&solverStart[itr]));
            checkCudaStatus(cudaEventCreate(&solverStop[itr]));
        }

        for (int itr = 0; itr < CMAX; itr++) {
            /// each computation data with its own device Evalution Context
            utility::mallocAsync(&dev_ev[itr], 1, stream);
            utility::mallocAsync(&dev_norm[itr], 1, stream);
        }

        for (int itr = 0; itr < CMAX; itr++) {
            /// each computation data with its own host Evalution Context
            utility::mallocHost(&ev[itr], 1);
        }
    }
}

GPUComputationContext::~GPUComputationContext() {

    if (stream != nullptr) {

        // !!! linear_system_method_cuSolver_reset(stream);

        for (int itr = 0; itr < CMAX; itr++) {
            /// each computation data with its own host Evalution Context
            utility::freeMemHost(&ev[itr]);
        }

        for (int itr = 0; itr < CMAX; itr++) {
            /// each computation data with its own device Evalution Context
            utility::freeAsync(dev_ev[itr], stream);
            utility::freeAsync(dev_norm[itr], stream);
        }

        for (int itr = 0; itr < CMAX; itr++) {
            // #observations
            checkCudaStatus(cudaEventDestroy(prepStart[itr]));
            checkCudaStatus(cudaEventDestroy(prepStop[itr]));
            checkCudaStatus(cudaEventDestroy(computeStart[itr]));
            checkCudaStatus(cudaEventDestroy(computeStop[itr]));
            checkCudaStatus(cudaEventDestroy(solverStart[itr]));
            checkCudaStatus(cudaEventDestroy(solverStop[itr]));
        }

        checkCudaStatus(cudaStreamSynchronize(stream));
        // implicit object for utility

        checkCudaStatus(cudaStreamDestroy(stream));
        stream = nullptr;
    }
}

cudaStream_t GPUComputationContext::get_stream() const { return stream; }

double *GPUComputationContext::get_dev_norm(size_t itr) { return dev_norm[itr]; }

void GPUComputationContext::info_solver_version() const {
    /// cuSolver component settings
    int major = 0;
    int minor = 0;
    int patch = 0;

    checkCuSolverStatus(cusolverGetProperty(MAJOR_VERSION, &major));
    checkCuSolverStatus(cusolverGetProperty(MINOR_VERSION, &minor));
    checkCuSolverStatus(cusolverGetProperty(PATCH_LEVEL, &patch));
    printf("[ CUSOLVER ]  version ( Major.Minor.PatchLevel): %d.%d.%d\n", major, minor, patch);
}

void GPUComputationContext::recordComputeStart(size_t itr) {
    checkCudaStatus(cudaEventRecord(computeStart[itr], stream));
}

void GPUComputationContext::recordComputeStop(size_t itr) {
    checkCudaStatus(cudaEventRecord(computeStop[itr], stream));
}

void GPUComputationContext::recordPrepStart(size_t itr) { checkCudaStatus(cudaEventRecord(prepStart[itr], stream)); }

void GPUComputationContext::recordPrepStop(size_t itr) { checkCudaStatus(cudaEventRecord(prepStop[itr], stream)); }

void GPUComputationContext::recordSolverStart(size_t itr) {
    checkCudaStatus(cudaEventRecord(solverStart[itr], stream));
}

void GPUComputationContext::recordSolverStop(size_t itr) { checkCudaStatus(cudaEventRecord(solverStop[itr], stream)); }

} // namespace solver