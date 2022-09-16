#include "gpu_computation.h"

#include "cuda_runtime_api.h"

#include <functional>
#include <numeric>
#include <typeinfo>

#include <cusparse.h>

#include "model.cuh"
#include "solver_kernel.cuh"

#include "quda.h"
#include "stop_watch.h"
#include "utility.cuh"

#define DEBUG_GPU

#ifdef DEBUG_GPU
#define validateStream validateStreamState()
#else
#define validateStream (void)(0)
#endif

namespace solver {

/// ====================

GPUComputation::GPUComputation(long computationId, cudaStream_t stream, std::shared_ptr<GPUSolverSystem> solverSystem,
                               std::shared_ptr<GPUComputationContext> cc, GPUGeometricSolver *solver)
    : computationId(computationId), solverSystem(solverSystem), tensorOperation(stream), formatEncoder(stream), stream(stream), computationContext(cc) {

    /// HOST_PAGEABLE

    /// cuda_pinned_memory !

    /// change owner of computation state
    points = std::move(solver->points);
    geometrics = std::move(solver->geometrics);
    constraints = std::move(solver->constraints);
    parameters = std::move(solver->parameters);

    if (points.size() == 0) {
        printf("empty solution space, add some geometric types\n");
        error = cudaSuccess;
        return;
    }

    // model aggreate for const immutable data
    if (points.size() == 0) {
        printf("[solver] - empty evaluation space model\n");
        error = cudaSuccess;
        return;
    }

    if (constraints.size() == 0) {
        printf("[solver] - no constraint configuration applied onto model\n");
        error = cudaSuccess;
        return;
    }

    this->computationMode = graph::getComputationMode(settings::get()->COMPUTATION_MODE);

    this->solverMode = graph::getSolverMode(settings::get()->SOLVER_MODE);

    this->evaluationWatch.setStartTick();

    /// setup all dependent structure for device , accSize or accOffset
    preInitializeData();

    /// prepera allocation and transfer structures onto device
    memcpyComputationToDevice();

    printf("[solver] model stage consistent !\n");
}

//////////////////===================================

void GPUComputation::preInitializeData() {

    /// mapping from point Id => point dest offset
    pointOffset = utility::stateOffset(points, [](auto point) { return point->id; });

    geometricOffset = utility::stateOffset(geometrics, [](auto geometric) { return geometric->id; });

    constraintOffset = utility::stateOffset(constraints, [](auto constraint) { return constraint->id; });

    /// this is mapping from Id => parameter dest offset
    parameterOffset = utility::stateOffset(parameters, [](auto parameter) { return parameter->id; });

    /// accumalted position of geometric block
    accGeometricSize = utility::accumulatedValue(geometrics, graph::geometricSetSize);

    /// accumulated position of constrain block
    accConstraintSize = utility::accumulatedValue(constraints, graph::constraintSize);

    if (computationMode != ComputationMode::DENSE_MODE) {
        /// accumulated COO - K
        accCooWriteStiffTensor = utility::accumulatedValue(geometrics, graph::tensorOpsCooStiffnesCoefficients);
        /// accumulated COO - Jacobian
        accCooWriteJacobianTensor = utility::accumulatedValue(constraints, graph::tensorOpsCooConstraintJacobian);

        if (settings::get()->SOLVER_INC_HESSIAN) {
            /// accumuldate COO - Hessian
            accCooWriteHessianTensor = utility::accumulatedValue(constraints, graph::tensorOpsCooConstraintHessian);
        } else {
            accCooWriteHessianTensor = std::vector<int, gpu_allocator<int>>(0);
        }

        if (accCooWriteHessianTensor.size() > 1 && computationMode == ComputationMode::DIRECT_MODE) {
            fprintf(stderr, "[solver.gpu] error ! direct layout mode is not available for Hessian matrix evaluation ! \n");
            // exit(-1);
        }
    }

    /// `A` tensor internal structure dimensions
    size = std::accumulate(geometrics.begin(), geometrics.end(), 0, [](auto acc, auto const &geometric) { return acc + graph::geometricSetSize(geometric); });

    coffSize =
        std::accumulate(constraints.begin(), constraints.end(), 0, [](auto acc, auto const &constraint) { return acc + graph::constraintSize(constraint); });

    dimension = size + coffSize;
}

#define FIRST_ROUND 0

#define NEXT_ROUND(v) (v)

void GPUComputation::memcpyComputationToDevice() {

    /// ============================================================
    ///         Host Computation with references to Device
    /// ============================================================

    /// const data in computation
    d_points = utility::dev_vector(points, stream);
    d_geometrics = utility::dev_vector(geometrics, stream);
    d_constraints = utility::dev_vector(constraints, stream);
    d_parameters = utility::dev_vector(parameters, stream);

    d_pointOffset = utility::dev_vector(pointOffset, stream);
    d_geometricOffset = utility::dev_vector(geometricOffset, stream);
    d_constraintOffset = utility::dev_vector(constraintOffset, stream);

    d_parameterOffset = utility::dev_vector(parameterOffset, stream);

    d_accGeometricSize = utility::dev_vector(accGeometricSize, stream);
    d_accConstraintSize = utility::dev_vector(accConstraintSize, stream);

    if (computationMode != ComputationMode::DENSE_MODE) {
        // ComputationMode::SPARSE_LAYOUT  || ComputationMode::DIRECT_LAYOUT
        /// accumulated COO
        d_accCooWriteStiffTensor = utility::dev_vector(accCooWriteStiffTensor, stream);
        d_accCooWriteJacobianTensor = utility::dev_vector(accCooWriteJacobianTensor, stream);
        d_accCooWriteHessianTensor = utility::dev_vector(accCooWriteHessianTensor, stream);

        cooWritesStiffSize = accCooWriteStiffTensor[accCooWriteStiffTensor.size() - 1];
        cooWirtesJacobianSize = accCooWriteJacobianTensor[accCooWriteJacobianTensor.size() - 1];
        int cooWritesSize = cooWritesStiffSize + cooWirtesJacobianSize * 2; /// +HESSIAN if computed

        if (settings::get()->SOLVER_INC_HESSIAN) {
            cooWritesSize += accCooWriteHessianTensor[accCooWriteHessianTensor.size() - 1];
        }

        if (false) {
            /// fillin diagonal with 0 indicies form [size, size + coffSize]
            cooWritesSize += coffSize;
        }

        // non-zero elements (coo/csr)
        nnz = cooWritesSize;

        /// sparse layout , first itr only
        d_cooVal = utility::dev_vector<double>(cooWritesSize, stream);
        d_cooRowInd = utility::dev_vector<int>(cooWritesSize, stream);
        d_cooColInd = utility::dev_vector<int>(cooWritesSize, stream);

        tensorOperation.memsetD32I(d_cooRowInd, -1, nnz, stream);
        tensorOperation.memsetD32I(d_cooColInd, -1, nnz, stream);

        d_PT = utility::dev_vector<int>(cooWritesSize, stream);
        d_INV_PT = utility::dev_vector<int>(cooWritesSize, stream);
        d_cooRowInd_order = utility::dev_vector<int>(cooWritesSize, stream);

        /// rows = m + 1
        if (computationMode == ComputationMode::COMPACT_MODE) {
            d_csrRowPtrA = utility::dev_vector<int>(nnz, stream);
        } else {
            d_csrRowPtrA = utility::dev_vector<int>(dimension + 1, stream);
        }

        d_csrColIndA = utility::dev_vector<int>(nnz, stream);
        d_csrValA = utility::dev_vector<double>(nnz, stream);
    }

    /// ?// single  data-plane transfer

    // if (!parameters.empty()) {
    //     utility::memcpyAsync(&d_parameterOffset, parameterOffset.data(), parameterOffset.size(), stream);
    // }

    const size_t N = dimension;
    ///
    ///  [ GPU ] tensors `A` `x` `dx` `b`  ------------------------
    ///
    if (computationMode == ComputationMode::DENSE_MODE) {
        dev_A = utility::dev_vector<double>(N * N, stream);
    }
    dev_b = utility::dev_vector<double>(N, stream);
    dev_dx = utility::dev_vector<double>(N, stream);

    for (int itr = 0; itr < CMAX; itr++) {
        /// each computation data with its own StateVector
        dev_SV.push_back(utility::dev_vector<double>{N, stream});
    }
}

GPUComputation *GPUComputation::_registerComputation = nullptr;

void GPUComputation::registerComputation(GPUComputation *computation) { GPUComputation::_registerComputation = computation; }

void GPUComputation::unregisterComputation(GPUComputation *computation) { GPUComputation::_registerComputation = NULL; }

void GPUComputation::computationResultHandlerDelegate(cudaStream_t stream, cudaError_t status, void *userData) {
    if (_registerComputation) {
        _registerComputation->computationResultHandler(stream, status, userData);
    }
}

void GPUComputation::validateStreamState() {
    if (settings::get()->DEBUG_CHECK_ARG) {
        /// submitted kernel into  cuda driver
        checkCudaStatus(cudaPeekAtLastError());
        /// block and wait for execution
        checkCudaStatus(cudaStreamSynchronize(stream));
    }
}

void GPUComputation::PreInitializeComputationState(ComputationState *ev, int itr, ComputationLayout computationLayout) {
    // computation data;
    ev->cID = itr;
    ev->SV = dev_SV[itr]; /// data vector lineage

    if (computationMode == ComputationMode::DENSE_MODE) {
        ev->A = dev_A;
    }
    ev->b = dev_b;
    ev->dx = dev_dx;
    ev->dev_norm = computationContext->get_dev_norm(itr);

    // geometric structure
    ev->size = size;
    ev->coffSize = coffSize;
    ev->dimension = dimension;

    ev->points = NVector<graph::Point>(d_points, points.size());
    ev->geometrics = NVector<graph::Geometric>(d_geometrics, geometrics.size());
    ev->constraints = NVector<graph::Constraint>(d_constraints, constraints.size());
    ev->parameters = NVector<graph::Parameter>(d_parameters, parameters.size());

    ev->pointOffset = d_pointOffset;
    ev->geometricOffset = d_geometricOffset;
    ev->constraintOffset = d_constraintOffset;
    ev->parameterOffset = d_parameterOffset;
    ev->accGeometricSize = d_accGeometricSize;
    ev->accConstraintSize = d_accConstraintSize;

    /// =================================
    ///     Tensor A Computation Mode
    /// =================================
    ev->singularity = -1;
    ev->computationMode = computationMode;
    ev->computationLayout = computationLayout;

    if (computationMode != ComputationMode::DENSE_MODE) {

        /// ComputationLayout::SPARSE_LAYOUT  || ComputationLayout::DIRECT_LAYOUT
        ev->nnz = nnz;
        ev->cooWritesStiffSize = cooWritesStiffSize;
        ev->cooWirtesJacobianSize = cooWirtesJacobianSize;

        ev->accCooWriteStiffTensor = d_accCooWriteStiffTensor;
        ev->accCooWriteJacobianTensor = d_accCooWriteJacobianTensor;
        ev->accCooWriteHessianTensor = d_accCooWriteHessianTensor;

        ev->cooRowInd = d_cooRowInd; /// SparseLayout access
        ev->cooColInd = d_cooColInd; /// SparseLayout access
        ev->cooVal = d_cooVal;

        utility::memsetAsync(d_cooVal.data(), 0, d_cooVal.get_size(), stream);

        ev->INV_P = d_INV_PT; /// SparseLayout(1) + DirectLayout(*)
    }
}

void GPUComputation::InitializeStateVector() {

    /// ---------------------------------------------------------------------------------------- ///
    ///                                KERNEL ( Init State Vector )                              ///
    /// ---------------------------------------------------------------------------------------- ///
    CopyIntoStateVector(stream, dev_SV[0], d_points, points.size());
    validateStream;

    /// ---------------------------------------------------------------------------------------- ///
    ///                                 Zero Lagrange Coefficients                               ///
    /// ---------------------------------------------------------------------------------------- ///

    utility::memsetAsync(dev_SV[0].data() + size, 0, coffSize, stream);
}

#define DEFAULT_BLOCK_DIM 512
#define max(a, b) ((a) > (b)) ? (a) : (b)
#define min(a, b) ((a) > (b)) ? (b) : (a)

int GPUComputation::getSharedMemoryRequirments(ComputationLayout computationLayout) {

    /// shared memory - BlockIterator requirments !
    int sharedMemory = 0;

    if (computationLayout == ComputationLayout::DIRECT_LAYOUT || computationLayout == ComputationLayout::COMPACT_LAYOUT) {
        int elements = max(geometrics.size(), constraints.size());
        int blockSize = min(DEFAULT_BLOCK_DIM, elements);
        sharedMemory = blockSize * sizeof(int);
    }
    return sharedMemory;
}

void GPUComputation::DeviceConstructTensorA(ComputationState *dev_ev, ComputationLayout computationLayout, cudaStream_t stream) {

    /// Tensor A Sparse Memory Layout
    /// mem layout = [ Stiff + Jacobian + JacobianT  + Hessian*]

    const int sharedMemory = getSharedMemoryRequirments(computationLayout);
    /// ---------------------------------------------------------------------------------------- ///
    ///                                 KERNEL ( Stiff Tensor - K )                              ///
    /// ---------------------------------------------------------------------------------------- ///
    ComputeStiffnessMatrix(stream, sharedMemory, dev_ev, geometrics.size());
    validateStream;

    if (settings::get()->DEBUG_COO_FORMAT) {
        cudaStreamSynchronize(stream);
        /// intermediate result from conversion
        tensorOperation.stdout_coo_vector(stream, dimension, dimension, nnz, d_cooRowInd, d_cooColInd, d_cooVal, "coo -K");
    }

    /// ---------------------------------------------------------------------------------------- ///
    ///                                 KERNEL ( Hessian Tensor - K )                            ///
    /// ---------------------------------------------------------------------------------------- ///
    if (settings::get()->SOLVER_INC_HESSIAN) {
        // constraint with Hessian tensor
        EvaluateConstraintHessian(stream, sharedMemory, dev_ev, constraints.size());
        validateStream;
    }

    /// Lower Tensor
    /// (Wq); /// Jq = d(Fi)/dq --- write without intermediary matrix
    ///

    /// ---------------------------------------------------------------------------------------- ///
    ///                                 KERNEL ( Lower Jacobian )                                ///
    /// ---------------------------------------------------------------------------------------- ///
    EvaluateConstraintJacobian(stream, sharedMemory, dev_ev, constraints.size());
    validateStream;

    if (settings::get()->DEBUG_COO_FORMAT) {
        cudaStreamSynchronize(stream);
        /// intermediate result from conversion
        tensorOperation.stdout_coo_vector(stream, dimension, dimension, nnz, d_cooRowInd, d_cooColInd, d_cooVal, "coo Jacobian");
    }

    ///  Transposed Jacobian - Uperr Tensor
    ///  (Wq)';  Jq' = (d(Fi)/dq)' --- transposed - write without
    ///  intermediary matrix

    /// ---------------------------------------------------------------------------------------- ///
    ///                                 KERNEL ( Upper Jacobian )                                ///
    /// ---------------------------------------------------------------------------------------- ///
    EvaluateConstraintTRJacobian(stream, sharedMemory, dev_ev, constraints.size());
    validateStream;

    if (settings::get()->DEBUG_COO_FORMAT) {
        cudaStreamSynchronize(stream);
        /// intermediate result from conversion
        tensorOperation.stdout_coo_vector(stream, dimension, dimension, nnz, d_cooRowInd, d_cooColInd, d_cooVal, "coo JacobianT");
    }
}

void GPUComputation::DeviceConstructTensorB(ComputationState *dev_ev, cudaStream_t stream) {
    ///
    /// B = [ SV ]  - right hand side
    ///
    /// ---------------------------------------------------------------------------------------- ///
    ///                                 KERNEL ( Point-Point-Tension F(q) )                      ///
    /// ---------------------------------------------------------------------------------------- ///
    EvaluateForceIntensity(stream, dev_ev, geometrics.size());
    validateStream;

    /// ---------------------------------------------------------------------------------------- ///
    ///                                 KERNEL ( Constraint Tension Fi(q) )                      ///
    /// ---------------------------------------------------------------------------------------- ///
    EvaluateConstraintValue(stream, dev_ev, constraints.size());
    validateStream;
}

void GPUComputation::DebugTensorConstruction(ComputationState *ev) {

    if (settings::get()->DEBUG_TENSOR_A) {
        /// ---------------------------------------------------------------------------------------- ///
        ///                             KERNEL ( stdout tensor )                                     ///
        /// ---------------------------------------------------------------------------------------- ///
        ///
        switch (computationMode) {
        case ComputationMode::DENSE_MODE:
            utility::stdoutTensorData(ev->A, dimension, dimension, stream, " --- A --- ");
            break;
        case ComputationMode::SPARSE_MODE:
        case ComputationMode::DIRECT_MODE:
        case ComputationMode::COMPACT_MODE:
            tensorOperation.stdout_coo_tensor(stream, dimension, dimension, nnz, d_cooRowInd_order, d_csrColIndA, d_csrValA, " --- A*");
            break;
        }
    }

    if (settings::get()->DEBUG_TENSOR_B) {
        /// ---------------------------------------------------------------------------------------- ///
        ///                             KERNEL ( stdout tensor at gpu )                              ///
        /// ---------------------------------------------------------------------------------------- ///
        utility::stdoutDeviceVector(ev->b, dimension, stream, " --- B --- ");
    }
}

void GPUComputation::DebugTensorSV(ComputationState *ev) {
    if (settings::get()->DEBUG_TENSOR_SV) {
        /// ---------------------------------------------------------------------------------------- ///
        ///                             KERNEL ( stdout state vector  )                              ///
        /// ---------------------------------------------------------------------------------------- ///
        utility::stdoutDeviceVector(ev->SV, dimension, stream, " --- State Vector --- ");

        checkCudaStatus(cudaStreamSynchronize(stream));
    }
    validateStream;
}

void GPUComputation::StateVectorAddDelta(double *dev_SV, double *dev_b) {

    /// ---------------------------------------------------------------------------------------- ///
    ///                             KERNEL ( SV = SV + delta )                                   ///
    /// ---------------------------------------------------------------------------------------- ///
    StateVectorAddDifference(stream, dev_SV, dev_b, points.size());
    validateStream;

    // Uaktualniamy punkty SV = SV + delta
    // alternative : solverSystem->cublasAPIDaxpy(point_size, &alpha, dev_b, 1, dev_SV[itr], 1);
    // alternative : SV[i] = SV[i-1] + delta[i-1]
}

void GPUComputation::RebuildPointsFrom(ComputationState *computation) {

    /// ---------------------------------------------------------------------------------------- ///
    ///                     KERNEL( Point From State Vector )                                    ///
    /// ---------------------------------------------------------------------------------------- ///
    CopyFromStateVector(stream, d_points, computation->SV, points.size());
    validateStream;

    utility::memcpyFromDevice(points, d_points.data(), stream);

    checkCudaStatus(cudaStreamSynchronize(stream));
};

void GPUComputation::CudaGraphCaptureAndLaunch(cudaGraphExec_t graphExec) {

    /// Closing Stream Graph capturing mode

    cudaGraph_t graph;
    cudaGraphExecUpdateResult updateResult;
    cudaGraphNode_t errorNode;

    /// stream caputure capability
    checkCudaStatus(cudaStreamEndCapture(stream, &graph));

    if (graphExec != NULL) {
        // updateResult will store reason of failure
        cudaGraphExecUpdate(graphExec, graph, &errorNode, &updateResult);
    }

    // First Execution if not initialize
    if (graphExec == NULL || updateResult != cudaGraphExecUpdateSuccess) {
        //
        if (graphExec != NULL) {
            checkCudaStatus(cudaGraphExecDestroy(graphExec));
        }
        const size_t bufferSize = 128;
        char pLogBuffer[bufferSize] = {};

        checkCudaStatus(cudaGraphInstantiate(&graphExec, graph, &errorNode, pLogBuffer, bufferSize));

        if (errorNode != NULL) {
            pLogBuffer[bufferSize - 1] = '\0';
            fprintf(stderr, "[error/graph] graph instantation error %s \n", pLogBuffer);
            exit(1);
        }
    }
    //
    checkCudaStatus(cudaGraphDestroy(graph));

    /// ---------------------------------------------------------------------------------------- ///
    ///                               Graph Execution                                            ///
    /// ---------------------------------------------------------------------------------------- ///
    checkCudaStatus(cudaGraphLaunch(graphExec, stream));
}

ComputationLayout GPUComputation::selectComputationLayout(int itr) {

    ComputationLayout computationLayout;

    switch (computationMode) {

    case ComputationMode::DENSE_MODE:
        return ComputationLayout::DENSE_LAYOUT;
        break;

    case ComputationMode::SPARSE_MODE:
        computationLayout = ComputationLayout::SPARSE_LAYOUT;
        break;

    case ComputationMode::DIRECT_MODE:
        computationLayout = ComputationLayout::DIRECT_LAYOUT;
        if (itr == FIRST_ROUND) {
            computationLayout = ComputationLayout::SPARSE_LAYOUT;
        }
        break;

    case ComputationMode::COMPACT_MODE:
        computationLayout = ComputationLayout::COMPACT_LAYOUT;
        break;

    default:
        fprintf(stderr, "[solver] computation mode not recognized ! %d \n", computationMode);
        exit(-1);
    }
    return computationLayout;
}

void GPUComputation::PostProcessTensorA(int round, ComputationLayout computationLayout) {

    const int m = dimension;
    const int n = dimension;

    /// non-zeror elements temporary storage
    int nnz_csr = 0;

    switch (computationLayout) {
    /// ------------------------------------------------------------------------------------- ///
    case ComputationLayout::DENSE_LAYOUT:
        /// nothing, matrix build in dense format
        break;

    /// ------------------------------------------------------------------------------------- ///
    case ComputationLayout::SPARSE_LAYOUT:
        if (round == FIRST_ROUND) {
            ///  memset -1 przed iteracja
            d_cooRowInd_order.memcpy_of(d_cooRowInd, stream);
            d_csrColIndA.memcpy_of(d_cooColInd, stream);

            ///  destructive operation - in-place  Xcoosort !!! requirment for solver and DirectLayout
            tensorOperation.convertToCsr(m, n, nnz, d_cooRowInd_order, d_csrColIndA, d_csrRowPtrA, d_PT);
            validateStream;

            /// gather d_csrValuA from cooValues;
            tensorOperation.gatherVector<double>(nnz, CUDA_R_64F, d_cooVal, d_PT, d_csrValA);

            if (settings::get()->DEBUG_COO_FORMAT) {
                cudaStreamSynchronize(stream);
                /// intermediate result from conversion
                tensorOperation.stdout_coo_tensor(stream, m, n, nnz, d_cooRowInd_order, d_csrColIndA, d_csrValA, " --- tensor A");
            }

            /// evaluate inverted permutation vector
            tensorOperation.invertPermuts(nnz, d_PT, d_INV_PT);

        } else {
            /// Inplace cooValus pivoting - async execution
            tensorOperation.gatherVector<double>(nnz, CUDA_R_64F, d_cooVal, d_PT, d_csrValA);
            validateStream;
        }
        break;

    /// ------------------------------------------------------------------------------------- ///
    case ComputationLayout::DIRECT_LAYOUT:
        /// direct format
        d_csrValA.memcpy_of(d_cooVal, stream);
        break;

    /// ------------------------------------------------------------------------------------- ///
    case ComputationLayout::COMPACT_LAYOUT:

        /// input COO non compress format, this is journal Of commands [ K + Jacobian + JacobianT + Hessian ]        
        formatEncoder.compactToCsr(nnz, m, d_cooRowInd, d_cooColInd, d_cooVal, d_csrRowPtrA, d_csrColIndA, d_csrValA, nnz_csr);

        /// output in CSR
        nnz = nnz_csr;

    /// ------------------------------------------------------------------------------------- ///
    default:
        exit(-1);
    }
}

void GPUComputation::solveSystem(SolverStat *stat, cudaError_t *error) {

    // register C-function reference delegate - registerForDelegation(this) / unregisterFromDelegation(this)
    GPUComputation::registerComputation(this);

    int N = dimension;

    solverWatch.setStartTick();

    /// prepare local offset context
    result.store(NULL, std::memory_order_seq_cst);

    checkCudaStatus(cudaPeekAtLastError());

    // checkCudaStatus(cudaStreamSynchronize(stream));

    evaluationWatch.setStopTick();

    printf("\n /// ---------------------------- Solver Initialized ---------------------------- /// \n\n");

    /// Graph Capturing Mechanism
    cudaGraphExec_t graphExec = NULL;

    int round = 0;

    /// #########################################################################
    while (round < CMAX) {
        /// preinitialize data vector
        computationContext->ComputeStart(round);

        if (round == FIRST_ROUND) {
            /// SV
            InitializeStateVector();

        } else {
            checkCudaStatus(cudaMemcpyAsync(dev_SV[round], dev_SV[round - 1], N * sizeof(double), cudaMemcpyDeviceToDevice, stream));
        }

        ///  Host Context - with references to device
        ComputationState *ev = computationContext->host_ev(round);
        ComputationState *dev_ev = computationContext->get_dev_ev(round);

        const ComputationLayout computationLayout = selectComputationLayout(round);

        /// initializa computation state
        PreInitializeComputationState(ev, round, computationLayout);

        utility::memcpyAsync(&dev_ev, ev, 1, stream);
        validateStream;

        computationContext->PrepStart(round);

        if (computationMode == ComputationMode::DENSE_MODE) {
            /// zerujemy macierz A      !!!!! second buffer
            utility::memsetAsync(dev_A.data(), 0, N * N, stream); // --- ze wzgledu na addytywnosc
        }

        /// ----------------------------------------------------------------------------------------------- ///
        /// ---------------------------------- Graph Capturing Mechanism ---------------------------------- ///

        if (settings::get()->STREAM_CAPTURING) {
            /// stream caputure capability
            checkCudaStatus(cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal));
        }

        DeviceConstructTensorA(dev_ev, computationLayout, stream);

        DeviceConstructTensorB(dev_ev, stream);

        if (settings::get()->STREAM_CAPTURING) {
            CudaGraphCaptureAndLaunch(graphExec);
        }

        /// ---------------------------------- Graph Capturing Mechanism ---------------------------------- //
        /// ----------------------------------------------------------------------------------------------- ///

        /// Tensor A post process into CSR format
        PostProcessTensorA(round, computationLayout);

        ///
        computationContext->PrepStop(round);

        DebugTensorConstruction(ev);

#undef H_DEBUG

        if (settings::get()->DEBUG_CSR_FORMAT) {
            cudaStreamSynchronize(stream);
            utility::stdout_vector(d_csrRowPtrA, "d_csrRowPtrA");
        }

        ///
        ///  ConstraintGetFullNorm
        ///
        double *host_norm = &ev->norm;
        tensorOperation.vectorNorm(static_cast<int>(coffSize), (dev_b + size), host_norm);
        validateStream;

        computationContext->SolverStart(round);

        /// ---------------------------------------------------------------------------------------- ///
        ///                                     SOLVER SYSTEM                                        ///
        /// ---------------------------------------------------------------------------------------- ///
        int const m = dimension;
        int const n = dimension;
        int *singularity = &ev->singularity;

        /// ---------------------------------------------------------------------------------------- ///
        solverSystem->setSolverMode(solverMode);

        switch (solverMode) {
        case SolverMode::DENSE_LU:
            /// ---------------------------------------------------------------------------------------- ///
            ///                    Dense - CuSolver LINER SYSTEM equation solver                         ///
            /// ---------------------------------------------------------------------------------------- ///
            solverSystem->solveSystemDN(dev_A, dev_b, N);
            validateStream;
#ifdef H_DEBUG
            DebugTensorConstruction(dev_ev);
#endif
            /// uaktualniamy punkty SV = SV + delta
            StateVectorAddDelta(dev_SV[round], dev_b);
            DebugTensorSV(dev_ev);
            break;

        case SolverMode::SPARSE_QR:
        case SolverMode::SPARSE_ILU:
            /// ---------------------------------------------------------------------------------------- ///
            ///                             Sparse - Solver                                              ///
            /// ---------------------------------------------------------------------------------------- ///
            solverSystem->solveSystemSP(m, n, nnz, d_csrRowPtrA, d_csrColIndA, d_csrValA, dev_b, dev_dx, singularity);
            validateStream;

            if (settings::get()->DEBUG_CHECK_ARG) {
                checkCudaStatus(cudaStreamSynchronize(stream));
                if (*singularity != -1) {
                    fprintf(stderr, "[solver] tensor A is not invertible at index = %d \n", *singularity);
                    result.store(ev, std::memory_order_seq_cst);
                    break;
                }
                fprintf(stderr, "[solver] tensor A is invertible ;  %d \n", *singularity);
            }
            /// uaktualniamy state vector SV = SV + delta
            StateVectorAddDelta(dev_SV[round], dev_dx);
            break;

        default:
            fprintf(stderr, "[solver] computation mode not recognized ! %d \n", computationMode);
            exit(-1);
        }

        computationContext->SolverStop(round);

        DebugTensorSV(dev_ev);

        /// ---------------------------------------------------------------------------------------- ///
        ///                             Set Computation Callback - Epsilon                           ///
        /// ---------------------------------------------------------------------------------------- ///
        SetComputationHandler(ev);

        computationContext->ComputeStop(round);

        if (result.load(std::memory_order_seq_cst) != nullptr) {
            break;
        }

        /// Continue next ITERATION
        NEXT_ROUND(round++);

    } // end_while
    /// #########################################################################

    /// Computation tiles submited
    std::unique_lock<std::mutex> ulock(mutex);

    /// Await last computation state
    if (result.load(std::memory_order_seq_cst) == NULL) {
        /// spurious wakeup
        condition.wait(ulock, [&] { return result.load(std::memory_order_seq_cst) != NULL; });
    }

    ComputationState *computation = (ComputationState *)result.load(std::memory_order_seq_cst);
    solverWatch.setStopTick();

    GPUComputation::unregisterComputation(this);

    checkCudaStatus(cudaStreamSynchronize(stream));

    if (settings::get()->DEBUG) {
        ReportComputationResult(computation);
    }

    BuildSolverStat(computation, stat);

    RebuildPointsFrom(computation);

    if (graphExec != NULL) {
        cudaGraphExecDestroy(graphExec);
    }

    solverWatch.reset();
    evaluationWatch.reset();

    *error = cudaGetLastError();
}

void GPUComputation::SetComputationHandler(ComputationState *compState) {

    void *userData = static_cast<void *>(compState); // local_Computation
    checkCudaStatus(cudaStreamAddCallback(stream, GPUComputation::computationResultHandlerDelegate, userData, 0));
}

/// <summary>
/// Single Round Computation Handler
/// </summary>
/// <param name="userData"></param>
void GPUComputation::computationResultHandler(cudaStream_t stream, cudaError_t status, void *userData) {
    // synchronize on stream
    ComputationState *computation = static_cast<ComputationState *>(userData);

    // obsluga bledow w strumieniu
    if (status != cudaSuccess) {
        auto errorName = cudaGetErrorName(status);
        auto errorStr = cudaGetErrorString(status);
        printf("[error] - computation id [%d] ,  %s = %s \n", computation->cID, errorName, errorStr);
        return;
    }

    if (settings::get()->DEBUG) {
        fprintf(stdout, "[result/handler]- computationId (%d)  \n", computation->cID);
        fprintf(stdout, "[result/handler]- norm (%e) \n", computation->norm);
    }

    bool const last = computation->cID == (CMAX - 1);
    bool const isNan = isnan(computation->norm);
    bool const isNotConvertible = computation->singularity != -1;
    double const CONVERGENCE = settings::get()->SOLVER_EPSILON;

    bool stopComputation = (computation->norm < CONVERGENCE) || isNan || last || isNotConvertible;
    if (stopComputation) {
        /// update offset
        auto expected = static_cast<ComputationState *>(nullptr);
        if (result.compare_exchange_strong(expected, computation)) {
            condition.notify_one();
        }
    }
    // synchronize with stream next computation
}

void GPUComputation::BuildSolverStat(ComputationState *computation, solver::SolverStat *stat) {

    const double SOLVER_EPSILON = (settings::get()->SOLVER_EPSILON);
    const int iter = computation->cID;

    stat->startTime = solverWatch.getStartTick();
    stat->stopTime = solverWatch.getStopTick();

    /// solverWatch.delta() + evaluationWatch.delta();
    stat->timeDelta = computationContext->getAccComputeTime(iter);

    stat->size = size;
    stat->coefficientArity = coffSize;
    stat->dimension = dimension;

    /// evaluationWatch.delta();
    stat->accEvaluationTime = computationContext->getAccPrepTime(iter);
    /// solverWatch.delta();
    stat->accSolverTime = computationContext->getAccSolverTime(iter);

    stat->convergence = computation->norm < SOLVER_EPSILON;
    stat->error = computation->norm;
    stat->constraintDelta = computation->norm;
    stat->iterations = computation->cID;
}

void GPUComputation::ReportComputationResult(ComputationState *computation) {
    long long solutionDelta = solverWatch.delta();

    printf("\n");
    printf("===================================================\n");
    printf("\n");
    printf("solution time delta [ns]        ( %zd )\n", solutionDelta);
    printf("\n");
    printf("===================================================\n");
    printf("comp ID                         ( %d )\n", computation->cID);
    printf("computation norm                ( %e )\n", computation->norm);
    printf("\n");
    printf("computation size                ( %zd )\n", computation->size);
    printf("computation coffSize            ( %zd )\n", computation->coffSize);
    printf("computation dimension           ( %zd )\n", computation->dimension);
    printf("\n");
    printf("===================================================\n");
    printf("\n");
}

double GPUComputation::getPointPXCoordinate(int id) {
    int offset = pointOffset[id];
    double px = points[offset].x;
    return px;
}

double GPUComputation::getPointPYCoordinate(int id) {
    int offset = pointOffset[id];
    double py = points[offset].y;
    return py;
}

void GPUComputation::fillPointCoordinateVector(double *stateVector) {
    graph::StopWatchAdapter stopWatch;
    stopWatch.setStartTick();
    // Effecient vector base loop
    for (size_t i = 0, size = points.size(); i < size; i++) {
        graph::Point &p = points[i];
        stateVector[2 * i] = p.x;
        stateVector[2 * i + 1] = p.y;
    }
    stopWatch.setStopTick();
    fprintf(stdout, "fillin state vector , delta[ns]: %llu ", stopWatch.delta());
}

int GPUComputation::updateConstraintState(int constraintId[], double vecX[], double vecY[], int size) {
    evaluationWatch.setStartTick();
    for (int itr = 0; itr < size; itr++) {
        int cId = constraintId[itr];
        int offset = constraintOffset[cId];
        graph::Constraint &constraint = constraints[offset];
        if (constraint.constraintTypeId != CONSTRAINT_TYPE_ID_FIX_POINT) {
            printf("[error] constraint type only supported is ConstraintFixPoint ! \n");
            return 1;
        }
        constraint.vecX = vecX[itr];
        constraint.vecY = vecY[itr];
    }
    d_constraints.memcpy_of(constraints, stream);
    return 0;
}

int GPUComputation::updateParametersValues(int parameterId[], double value[], int size) {
    for (int itr = 0; itr < size; itr++) {
        int pId = parameterId[itr];
        int offset = parameterOffset[pId];
        graph::Parameter &parameter = parameters[offset];
        parameter.value = value[itr];
    }
    d_parameters.memcpy_of(parameters, stream);
    return 0;
}

void GPUComputation::updatePointCoordinateVector(double stateVector[]) {
    for (size_t i = 0, size = points.size(); i < size; i++) {
        graph::Point &p = points[i];
        p.x = stateVector[2 * i];
        p.y = stateVector[2 * i + 1];
    }
    d_points.memcpy_of(points, stream);
}

//////////////////===================================

GPUComputation::~GPUComputation() {
    /// at least one solver computation
}

} // namespace solver

#undef validateStream