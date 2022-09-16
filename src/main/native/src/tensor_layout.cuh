#ifndef _TENSOR_LAYOUT_CUH_
#define _TENSOR_LAYOUT_CUH_

#include "device_launch_parameters.h"

#include "math.h"

#include "model.cuh"

#ifndef __GPU_COMM_INL__
#define __GPU_COMM_INL__ __host__ __device__
#endif

#ifndef __GPU_DEV_INL__
#define __GPU_DEV_INL__ __device__
#endif

#ifndef __GPU_DEV_INLF__
#define __GPU_DEV_INLF__ __forceinline__ __device__
#endif

#define DEGREES_TO_RADIANS 0.017453292519943295;

__GPU_DEV_INLF__ double toRadians(double angdeg) { return angdeg * DEGREES_TO_RADIANS; }

namespace graph {

class Vector;

//=================================================================================

// 2x2 small grid
class BlockLayout {
  public:
    __GPU_DEV_INLF__ BlockLayout() {}

    __GPU_DEV_INLF__ void set(int row, int col, double value) {
        if (row < ld && col < cols) {
            tensor[ld * row + col] = value;
        }
    }

    __GPU_DEV_INLF__ double get(int row, int col) const {
        if (row < ld && col < cols) {
            return tensor[ld * row + col];
        }
        return NAN;
    }

  private:
    double tensor[4] = {0.0};
    const int ld = 2;
    const int cols = 2;
};
//=================================================================================

//=================================================================================

class DenseLayout {

  public:
    __GPU_DEV_INLF__ DenseLayout() : ld(0), rowOffset(0), colOffset(0), m_A(NULL) {}

    __GPU_DEV_INLF__ DenseLayout(size_t ld, size_t rowOffset, size_t colOffset, double *A) : ld(ld), rowOffset(rowOffset), colOffset(colOffset), m_A(A) {}

    __GPU_DEV_INLF__ void set(int row, int col, double value) {
        ///
        m_A[ld * (colOffset + col) + rowOffset + row] = value;
    }

    __GPU_DEV_INLF__ void add(int row, int col, double value) {
        ///
        m_A[ld * (colOffset + col) + rowOffset + row] += value;
    }

    __GPU_DEV_INLF__ double get(int row, int col) const {
        ///
        return m_A[ld * (colOffset + col) + rowOffset + row];
    }

  public:
    // leading dimension
    size_t ld;

    // row offset
    size_t rowOffset;

    // column offset
    size_t colOffset;

    // device storage
    double *m_A;
};

//=================================================================================

class SparseLayout {
  public:
    __GPU_DEV_INLF__ SparseLayout() : baseOffset(0), accWriteOffset(NULL), cooRowInd(NULL), cooColInd(NULL), cooVal(NULL) {}

    __GPU_DEV_INLF__ SparseLayout(int baseOffset, const int *_accWriteOffset, int *_cooRowInd, int *_cooColInd, double *_cooVal)
        : baseOffset(baseOffset), accWriteOffset(_accWriteOffset), cooRowInd(_cooRowInd), cooColInd(_cooColInd), cooVal(_cooVal) {}

    __GPU_DEV_INLF__ void set(int row, int col, double value) {
        /// standard 128 byte cache line - no additional contention in a warp
        int threadId = blockIdx.x * blockDim.x + threadIdx.x;
        int offset = baseOffset + accWriteOffset[threadId];
        int nextOffset = baseOffset + accWriteOffset[threadId + 1]; /// uzupelnic skladowa
        int blockSize = nextOffset - offset;

        for (int t = 0; t < blockSize; ++t) {
            const int row_at = cooRowInd[offset + t];
            const int col_at = cooColInd[offset + t];
            if (row_at == row && col_at == col) {
                /// write value into coo vector - reuse COO patern applied in first round
                cooVal[offset + t] = value; /// 1 Xwrite into cache L2
            } else if (row_at == -1 && col_at == -1) {
                /// SET VALUE
                cooRowInd[offset + t] = row; /// 3 Xwrite into cache L2
                cooColInd[offset + t] = col;
                cooVal[offset + t] = value;
                return;
            }
        }
    }

    /// NOTES: format COO budowac jako journal zdarzen set() , plus(new_record)
    ///         na koncu przeprwadzamy redukcje do thrust::reduce_by_key() [ coo compaction ]
    ///         na koncu konwersja csr !

    __GPU_DEV_INLF__ void add(int row, int col, double value) {
        /// 128 byte cache line - contention in a warp
        int threadId = blockIdx.x * blockDim.x + threadIdx.x;
        int offset = baseOffset + accWriteOffset[threadId];
        int nextOffset = baseOffset + accWriteOffset[threadId + 1]; /// uzupelnic skladowa
        int blockSize = nextOffset - offset;

        for (int t = 0; t < blockSize; ++t) {
            const int row_at = cooRowInd[offset + t];
            const int col_at = cooColInd[offset + t];
            if (row_at == row && col_at == col) {
                ///
                /// HESSIAN - istnieje niebezpieczenstow RMW zapisu do tej samej komorki Non-Atomic (RMW)
                ///
                /// SharedLock __shared__[ row? ]  __atomicAdd_block
                ///
                /// cooVal[offset + t] += value; /// 1 XWrite into cache L2
                /// 
                if (gridDim.x == 1) {
                    atomicAdd_block(&cooVal[offset + t], value);
                } else {
                    atomicAdd(&cooVal[offset + t], value);
                }                
                return;
            } else if (row_at == -1 && col_at == -1) {
                /// SET VALUE
                cooRowInd[offset + t] = row; /// 3 XWrite into cache L2
                cooColInd[offset + t] = col;
                cooVal[offset + t] = value;
                return;
            }
        }
    }

    __GPU_DEV_INLF__ double get(int row, int col) const {
        int threadId = blockIdx.x * blockDim.x + threadIdx.x;
        int offset = baseOffset + accWriteOffset[threadId];
        int nextOffset = baseOffset + accWriteOffset[threadId + 1];
        int blockSize = nextOffset - offset;

        for (int t = 0; t < blockSize; ++t) {
            if (cooRowInd[offset + t] == row && cooColInd[offset + t] == col) {
                return cooVal[offset + t];
            }
        }
        return 0.0; /// invalidate computation state
    }

  private:
    const int baseOffset;                   /// base offset for jacobian , transponse jacobina or hessian*
    const int *__restrict__ accWriteOffset; /// cub::exclusive_scan , offset for this Constraint or Geometric Block

    int *__restrict__ cooRowInd; /// COO row indicies
    int *__restrict__ cooColInd; /// COO column indicies
    double *__restrict__ cooVal; /// COO values
};

//=================================================================================

/// Local Thread Index Storage
template <typename DataType> class BlockIterator {
  public:
    __GPU_DEV_INLF__ BlockIterator() {

        /// 128 byte cache line
        extern __shared__ DataType kernel_shared[];

        /// kernel absolute owning thread access id
        threadId = blockIdx.x * blockDim.x + threadIdx.x;

        /// shared buffer of persmissions/slots
        slot = kernel_shared;
    }

    __GPU_DEV_INLF__ void reset(DataType value = DataType()) { slot[threadIdx.x] = value; }

    __GPU_DEV_INLF__ DataType next() {
        DataType value = slot[threadIdx.x];
        ++slot[threadIdx.x];
        return value;
    }

    __GPU_DEV_INLF__ DataType value() {
        DataType value = slot[threadIdx.x];
        return value;
    }

    /// kernel absolute threadId
    __GPU_DEV_INLF__ int thread_id() { return threadId; }

  private:
    /// kernel shared reference
    DataType *slot;

    /// one owning slot for each kernel thread
    int threadId;
};

//=================================================================================

/// Direct Access - no permission checks !  
/// 
/// ---- z HESSIANEM - wektor P jest skorumpowany ( non-unique coordinates ).
///
class DirectSparseLayout {
  public:
    __GPU_DEV_INLF__ DirectSparseLayout() : baseOffset(0), accOffset(NULL), P(NULL), cooRowInd(NULL), cooColInd(NULL), cooVal(NULL) {}

    __GPU_DEV_INLF__ DirectSparseLayout(int baseOffset, const int *_accOffset, const int *_P, const int *_cooRowInd, const int *_cooColInd, double *_cooVal)
        : baseOffset(baseOffset), accOffset(_accOffset), P(_P), cooRowInd(_cooRowInd), cooColInd(_cooColInd), cooVal(_cooVal) {
        iterator.reset();
    }

    __GPU_DEV_INLF__ void set(int row, int col, double value) {
        unsigned threadId = iterator.thread_id();
        unsigned offset = iterator.next();
        unsigned threadOffset = baseOffset + accOffset[threadId];

        /// override value - inversed indicies
        cooVal[P[threadOffset + offset]] = value;
    }

    __GPU_DEV_INLF__ void add(int row, int col, double value) {
        unsigned threadId = iterator.thread_id();
        unsigned threadOffset = baseOffset + accOffset[threadId];
        unsigned itr_offset = iterator.value();

        // ADD
        for (unsigned itr = 0; itr < itr_offset; itr++) {
            int at_row = cooRowInd[threadOffset + itr];
            int at_col = cooColInd[threadOffset + itr];
            if (at_row == row && at_col == col) {
                
                ///
                /// HESSIAN - komponenty macierzy skorelowane na punktach uczestnicza czesci tego bloku.
                ///
                //if (gridDim.x == 1) {
                //    atomicAdd_block(&cooVal[P[threadOffset + itr]], value);
                //} else {
                //    atomicAdd(&cooVal[P[threadOffset + itr]], value);
                //}    

                cooVal[P[threadOffset + itr]] += value; /// 1 XWrite into L2 memory
                return;
            } else if (at_row == -1 && at_col == -1) {
                // SET value
                break;
            }
        }

        // SET
        unsigned offset = iterator.next();
        /// override value - inversed indicies

        cooVal[P[threadOffset + offset]] = value; /// 1 XWrite into L2 memory
    }

    __GPU_DEV_INLF__ double get(int row, int col) {
        int threadId = iterator.thread_id();
        int threadOffset = baseOffset + accOffset[threadId];
        int nextOffset = baseOffset + accOffset[threadId + 1];
        int blockSize = nextOffset - threadOffset;

        // GET
        for (int itr = 0; itr < blockSize; ++itr) {
            const int row_at = cooRowInd[threadOffset + itr];
            const int col_at = cooColInd[threadOffset + itr];
            if (row_at == row && col_at == col) {
                return cooVal[P[threadOffset + itr]];
            }
        }
        return NAN;
    }

  private:
    BlockIterator<int> iterator{};

  private:
    int baseOffset;                    /// base offset for Jacobian, or Transposed Jacobian, or Hessian.
    const int *__restrict__ accOffset; /// thread acc offset
    const int *__restrict__ P;         /// direct indicies dense vector

    double *__restrict__ cooVal; /// COO values

#define __BEFORE_TRANSFORMATION__
    const int *__restrict__ cooRowInd; /// COO row computed at first round of SparseLayout mode
    const int *__restrict__ cooColInd; /// COO col computed at first round of SparseLayout mode
};

//=================================================================================

/// <summary>
///  Compact Layout - store column, row, and value as journal of "writes"
///  After completion this session, coo object have to be sorted and reduced (std::plus<double>)
/// </summary>
class CompactLayout {
  public:
    __GPU_DEV_INLF__ CompactLayout() : baseOffset(0), accWriteOffset(NULL), cooRowInd(NULL), cooColInd(NULL), cooVal(NULL) {}

    __GPU_DEV_INLF__ CompactLayout(int baseOffset, const int *_accWriteOffset, int *_cooRowInd, int *_cooColInd, double *_cooVal)
        : baseOffset(baseOffset), accWriteOffset(_accWriteOffset), cooRowInd(_cooRowInd), cooColInd(_cooColInd), cooVal(_cooVal) {}

    __GPU_DEV_INLF__ void set(int row, int col, double value) {
        int threadId = iterator.thread_id();
        int offset = baseOffset + accWriteOffset[threadId];        
        int itr_offset = iterator.value();

        cooRowInd[offset + itr_offset] = row; /// 3 Xwrite into cache L2
        cooColInd[offset + itr_offset] = col;
        cooVal[offset + itr_offset] = value;
    }

    __GPU_DEV_INLF__ void add(int row, int col, double value) {
        int threadId = iterator.thread_id();
        int offset = baseOffset + accWriteOffset[threadId];
        int itr_offset = iterator.value();

        cooRowInd[offset + itr_offset] = row; /// 3 Xwrite into cache L2
        cooColInd[offset + itr_offset] = col;
        cooVal[offset + itr_offset] = value;
    }

    __GPU_DEV_INLF__ double get(int row, int col) {
        int threadId = iterator.thread_id();
        int offset = baseOffset + accWriteOffset[threadId];
        int nextOffset = baseOffset + accWriteOffset[threadId + 1];
        int blockSize = nextOffset - offset;        /// BLOCK_SIZE <= 32

        for (int t = 0; t < blockSize; ++t) {
            if (cooRowInd[offset + t] == row && cooColInd[offset + t] == col) {
                return cooVal[offset + t];
            }
        }
        return 0.0; /// invalidate computation state
    }

  private:
    BlockIterator<int> iterator{}; /// this session iterator

  private:
    const int baseOffset;                   /// base offset for jacobian , transponse jacobina or hessian*
    const int *__restrict__ accWriteOffset; /// cub::exclusive_scan , offset for this Constraint or Geometric Block

    int *__restrict__ cooRowInd; /// COO row indicies
    int *__restrict__ cooColInd; /// COO column indicies
    double *__restrict__ cooVal; /// COO values
};


//=================================================================================

template <typename LLayout = BlockLayout> class Tensor {
  public:
    __GPU_DEV_INLF__ Tensor(LLayout const &layout = LLayout(), bool intention = false, int offset_row = 0, int offset_col = 0)
        : u(layout), intention(intention), offset_row(offset_row), offset_col(offset_col) {}

    __GPU_DEV_INLF__ Tensor(double a00, double a01, double a10, double a11) : Tensor(LLayout()) {
        u.set(0, 0, a00);
        u.set(0, 1, a01);
        u.set(1, 0, a10);
        u.set(1, 1, a11);
    }

    __GPU_DEV_INLF__ void setVector(int row, int col, graph::Vector const &value) {
        /// vertical
        if (intention) { // tensor "B"
            u.set(offset_row + row + 0, offset_col + col, getVectorX(value));
            u.set(offset_row + row + 1, offset_col + col, getVectorY(value));
        } else { // tensor "Jacobian"
            u.set(offset_row + row, offset_col + col + 0, getVectorX(value));
            u.set(offset_row + row, offset_col + col + 1, getVectorY(value));
        }
    };

    __GPU_DEV_INLF__ void setValue(int row, int col, double const &value) { u.set(offset_row + row, offset_col + col, value); }

    __GPU_DEV_INL__ double getValue(int row, int col) const { return u.get(offset_row + row, offset_col + col); }

    __GPU_DEV_INLF__ void plusSubTensor(int row, int col, Tensor<graph::BlockLayout> const &mt) {
        /// small tensor
        double a00 = mt.u.get(0, 0);
        double a01 = mt.u.get(0, 1);
        double a10 = mt.u.get(1, 0);
        double a11 = mt.u.get(1, 1);

        u.add(offset_row + row + 0, offset_col + col, a00);
        u.add(offset_row + row + 1, offset_col + col, a10);
        u.add(offset_row + row + 0, offset_col + col + 1, a01);
        u.add(offset_row + row + 1, offset_col + col + 1, a11);
    }

    __GPU_DEV_INLF__ void setSubTensor(int row, int col, Tensor<graph::BlockLayout> const &mt) {
        /// small tensor
        double a00 = mt.u.get(0, 0);
        double a01 = mt.u.get(0, 1);
        double a10 = mt.u.get(1, 0);
        double a11 = mt.u.get(1, 1);

        u.set(offset_row + row + 0, offset_col + col, a00);
        u.set(offset_row + row + 1, offset_col + col, a10);
        u.set(offset_row + row + 0, offset_col + col + 1, a01);
        u.set(offset_row + row + 1, offset_col + col + 1, a11);
    }

    __GPU_DEV_INLF__ Tensor<graph::BlockLayout> multiplyC(double scalar) {
        if constexpr (std::is_same_v<LLayout, graph::BlockLayout>) {
            /// SmallTensor
            double a00 = u.get(0, 0) * scalar;
            double a01 = u.get(0, 1) * scalar;
            double a10 = u.get(1, 0) * scalar;
            double a11 = u.get(1, 1) * scalar;
            return Tensor<graph::BlockLayout>(a00, a01, a10, a11);
        }
        return Tensor();
    }

    __GPU_DEV_INLF__ Tensor<graph::BlockLayout> plus(Tensor<graph::BlockLayout> const &other) {
        if constexpr (std::is_same_v<LLayout, graph::BlockLayout>) {
            /// SmallTensor
            double a00 = u.get(0, 0) + other.u.get(0, 0);
            double a01 = u.get(0, 1) + other.u.get(0, 1);
            double a10 = u.get(1, 0) + other.u.get(1, 0);
            double a11 = u.get(1, 1) + other.u.get(1, 1);
            return Tensor<graph::BlockLayout>(a00, a01, a10, a11);
        }
        return Tensor();
    }

    __GPU_DEV_INLF__ Tensor<graph::BlockLayout> transpose() const {
        if constexpr (std::is_same_v<LLayout, graph::BlockLayout>) {
            double a00 = u.get(0, 0);
            double a01 = u.get(0, 1);
            double a10 = u.get(1, 0);
            double a11 = u.get(1, 1);
            return Tensor<graph::BlockLayout>(a00, a10, a01, a11);
        }
        return Tensor();
    }

    friend class Tensor<graph::BlockLayout>;
    friend class Tensor<graph::SparseLayout>;
    friend class Tensor<graph::DenseLayout>;
    friend class Tensor<graph::DirectSparseLayout>;

  private:
    bool intention; /// vector put operation vertical if true / horizontal otherwise
    LLayout u;
    int offset_row; /// base offset in parrent tensor
    int offset_col; /// base offset in parent tensor
};

//=================================================================================

__GPU_DEV_INLF__ Tensor<DenseLayout> tensorDevMem(DenseLayout parent, int rowOffset, int colOffset, bool intention = true) {
    DenseLayout layout(parent.ld, parent.rowOffset + rowOffset, parent.colOffset + colOffset, parent.m_A);
    Tensor<DenseLayout> tensor(layout, intention);
    return tensor;
}

__GPU_DEV_INLF__ Tensor<SparseLayout> tensorDevMem(SparseLayout layout, bool intention, int offset_row, int offset_col) {
    Tensor<SparseLayout> tensor(layout, intention, offset_row, offset_col);
    return tensor;
}

__GPU_DEV_INLF__ Tensor<DirectSparseLayout> tensorDevMem(DirectSparseLayout layout, bool intention = true) {
    Tensor<DirectSparseLayout> tensor(layout, intention);
    return tensor;
}

__GPU_DEV_INLF__ Tensor<BlockLayout> make_block_tensor(double a00, double a01, double a10, double a11) {
    return Tensor<graph::BlockLayout>(a00, a10, a01, a11);
}

//=================================================================================

/// 2x2
class SmallTensor {
  public:
    __GPU_DEV_INLF__ static Tensor<BlockLayout> tensorR() {
        double a00 = 0.0;
        double a01 = -1.0;
        double a10 = 1.0;
        double a11 = 0.0;
        return Tensor<BlockLayout>(a00, a01, a10, a11);
    }

    __GPU_DEV_INLF__ static Tensor<BlockLayout> rotation(double alfa) {
        double radians = toRadians(alfa);
        double a00 = cos(radians);
        double a01 = -1.0 * sin(radians);
        double a10 = sin(radians);
        double a11 = cos(radians);
        return Tensor<BlockLayout>(a00, a01, a10, a11);
    }

    __GPU_DEV_INLF__ static Tensor<BlockLayout> identity(double value) { return Tensor<BlockLayout>(value, 0.0, 0.0, value); }

    __GPU_DEV_INLF__ static Tensor<BlockLayout> diagonal(double diagonal) { return Tensor<BlockLayout>(diagonal, 0.0, 0.0, diagonal); }
};

//=================================================================================

typedef Tensor<BlockLayout> TensorBlock;

///
/// Transpose Tensor Adapter - utility class for transposed storage operations into output tensor
///  -- transpose/inversed offsetRow/offsetCol order
///
template <typename LLayout> class AdapterTensor : public Tensor<LLayout> {

  public:
    __GPU_DEV_INLF__ AdapterTensor(LLayout layout, bool intention, int offset_row, int offset_col)
        : Tensor<LLayout>(layout, intention, offset_row, offset_col) {}

    __GPU_DEV_INLF__ void setVector(int row, int col, Vector const &value) {
        Tensor<LLayout>::setValue(col + 0, row, getVectorX(value));
        Tensor<LLayout>::setValue(col + 1, row, getVectorY(value));
    }

    __GPU_DEV_INLF__ void setValue(int row, int col, double const &value) {
        ///
        Tensor<LLayout>::setValue(col, row, value);
    }

    __GPU_DEV_INLF__ void setSubTensor(int row, int col, Tensor<graph::BlockLayout> const &mt) {
        ///
        Tensor<LLayout>::setSubTensor(col, row, mt.transpose());
    }

    __GPU_DEV_INLF__ void plusSubTensor(int row, int col, Tensor<graph::BlockLayout> const &mt) {    
        /// only hessian
        Tensor<LLayout>::plusSubTensor(col, row, mt.transpose());
    }

};

// transponsed operations
template <typename LLayout>
__GPU_DEV_INLF__ static AdapterTensor<LLayout> transposeTensorDevMem(LLayout parent, bool intention, int offset_row = 0, int offset_col = 0) {
    AdapterTensor<LLayout> t{parent, intention, offset_row, offset_col};
    return t;
}

//=================================================================================

} // namespace graph

#endif // _TENSOR_LAYOUT_CUH_