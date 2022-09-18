#ifndef _SOLVER_KERNEL_CUH_
#define _SOLVER_KERNEL_CUH_

#include "cuda_runtime_api.h"
#include "math.h"
#include "stdio.h"

#include "model.cuh"

#include "computation_state.cuh"

/// KERNEL#

/// ==============================================================================
///
///                             tensor utility
///
/// ==============================================================================

/// =========================================
/// Enable Tensor Sparse and Direct Layout
/// =========================================
//#define TENSOR_SPARSE_LAYOUT

//#undef TENSOR_SPARSE_LAYOUT


#define KERNEL_EXECUTOR

/// ===========================================================================================
/// <summary>
/// Inverse COO indicies map ;  this is Direct Form !
/// </summary>
/// <param name="K_gridDim"></param>
/// <param name="K_blockDim"></param>
/// <param name="K_stream"></param>
/// <param name="INP">indicies desne in vector</param>
/// <param name="OUTP">inverse dense out vector - direct form</param>
/// <param name="N">size of intput/output vector</param>
/// <returns></returns>
KERNEL_EXECUTOR void inversePermutationVector(cudaStream_t stream,
                                              int *INP, int *OUTP, size_t N); 


/// ===========================================================================================
/// <summary>
/// Inverse COO indicies map ;  this is Direct Form !
/// </summary>
/// <param name="K_gridDim"></param>
/// <param name="K_blockDim"></param>
/// <param name="K_stream"></param>
/// <param name="INP">indicies desne in vector</param>
/// <param name="OUTP">inverse dense out vector - direct form</param>
/// <param name="N">size of intput/output vector</param>
/// <returns></returns>
KERNEL_EXECUTOR void kernelMemsetD32I(cudaStream_t stream, int *devPtr, int value, size_t size); 


///  ==========================================================================================


/// ================================= SET STATE VECTOR ========================================
/// <summary>
/// Initialize State vector from actual points (without point id).
/// Reference mapping exists in Evaluation.pointOffset
/// </summary>
/// <param name="ec"></param>
/// <returns></returns>
KERNEL_EXECUTOR void CopyIntoStateVector(cudaStream_t stream, double *SV, graph::Point *points, size_t size);


/// ================================= UPDATE POINT VECTOR =====================================
/// <summary>
/// Move computed position from State Vector into corresponding point object.
/// </summary>
/// <param name="ec"></param>
/// <returns></returns>
KERNEL_EXECUTOR void CopyFromStateVector(cudaStream_t stream, graph::Point *points, double *SV, size_t size);


/// ================================= UPDATE STATE VECTOR =====================================
/// <summary>
/// Accumulate difference from newton-raphson method;  SV[] = SV[] + dx;
/// </summary>
/// <param name="SV"></param>
/// <param name="dx"></param>
/// <param name="N"></param>
/// <returns></returns>
KERNEL_EXECUTOR void StateVectorAddDifference(cudaStream_t stream, double *SV, double *dx, size_t N);


/// ==================================== STIFFNESS MATRIX =====================================
/// <summary>
/// Compute Stiffness Matrix on each geometric object.
/// Single cuda thread is responsible for evalution of an assigned geometric object.
/// </summary>
/// <param name="stream"></param>
/// <param name="NS">CTA shared memory</param>
/// <param name="ecdata"></param>
/// <param name="N"></param>
/// <returns></returns>
KERNEL_EXECUTOR void ComputeStiffnessMatrix(cudaStream_t stream,int NS, ComputationState *ecdata, size_t N);


/// ==================================== FORCE INTENSITY ======================================
/// <summary>
/// Force Intesity
/// </summary>
/// <param name="stream"></param>
/// <param name="ecdata"></param>
/// <param name="N"></param>
/// <returns></returns>
KERNEL_EXECUTOR void EvaluateForceIntensity(cudaStream_t stream , ComputationState *ecdata, size_t N);


/// ===================================== CONSTRAINT VALUE ====================================
/// <summary>
/// Constraint Value 
/// </summary>
/// <param name="stream"></param>
/// <param name="ecdata"></param>
/// <param name="N"></param>
/// <returns></returns>
KERNEL_EXECUTOR void EvaluateConstraintValue(cudaStream_t stream, ComputationState *ecdata, size_t N);


/// ============================ CONSTRAINT JACOBIAN MATRIX  ==================================
/// <summary>
/// Evaluate Constraint Jacobian  (FI) - (dfi/dq)   lower slice matrix of A
/// </summary>
/// <param name="stream"></param>
/// <param name="NS">CTA shared memeory</param>
/// <param name="ecdata"></param>
/// <param name="N"></param>
/// <returns></returns>
KERNEL_EXECUTOR void EvaluateConstraintJacobian(cudaStream_t stream, int NS, ComputationState *ecdata, size_t N);


/// ====================== CONSTRAINT TRANSPOSED JACOBIAN MATRIX ==============================
/// <summary>
/// Evaluate Constraint Transposed Jacobian  (FI)' - (dfi/dq)'   tr-transponowane - upper slice matrix  of A
/// </summary>
/// <param name="stream"></param>
/// <param name="NS">CTA shared memeory</param>
/// <param name="ecdata"></param>
/// <param name="N"></param>
/// <returns></returns>
KERNEL_EXECUTOR void EvaluateConstraintTRJacobian(cudaStream_t stream, int NS, ComputationState *ecdata, size_t N);


/// ================================== HESSIAN MATRIX  ========================================
///
/// <summary>
/// Evaluate Constraint Hessian Matrix (FI)' - ((dfi/dq)`)/dq
/// </summary>
/// <param name="stream"></param>
/// <param name="NS">CTA shared memeory</param>
/// <param name="ecdata"></param>
/// <param name="N"></param>
/// <returns></returns>
KERNEL_EXECUTOR void EvaluateConstraintHessian(cudaStream_t stream, int NS, ComputationState *ecdata, size_t N);


/// =======================================================================================
/// <summary>
/// Fill-In diagonal with value - iLU02 solver requirments.
/// </summary>
/// <param name="stream">[in]</param>
/// <param name="ecdata">[in]</param>
/// <param name="value">[in] numerical zero </param>
/// <param name="coeffSize">[in]cefficient size</param>
/// <returns></returns>
KERNEL_EXECUTOR void EvaluateFillInDiagonalValue(cudaStream_t stream, ComputationState *ecdata, double value, size_t coeffSize);

#endif // #ifndef _SOLVER_KERNEL_CUH_