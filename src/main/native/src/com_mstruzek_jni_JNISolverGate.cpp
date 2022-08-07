#include "com_mstruzek_jni_JNISolverGate.h"

#include <algorithm>
#include <chrono>
#include <cuda_runtime_api.h>
#include <functional>
#include <memory>
#include <stdio.h>
#include <type_traits>
#include <vector>

#include "model.cuh"

#include "stopWatch.h"

/// GPU common variables

static int deviceId;
static cudaError_t error;

/// points register
static std::vector<graph::Point> points; /// poLocations id-> point_offset

/// geometricc register
static std::vector<graph::Geometric> geometrics; /// ==> Macierz A, accumulative offset for each primitive

/// constraints register
static std::vector<graph::Constraint> constraints; /// ===> Wiezy , accumulative offset for each constraint

/// parameters register
static std::vector<graph::Parameter> parameters; /// paramLocation id-> param_offset

static graph::SolverStat stat;

/// Point  Offset in computation matrix [id] -> point offset   ~~ Gather Vectors
static std::unique_ptr<int[]> pointOffset;
static std::unique_ptr<int[]> constraintOffset;
static std::unique_ptr<int[]> geometricOffset;

/// Accumulated Constraint Size
static std::unique_ptr<int[]> accConstraintSize;

/// Accymylated Geometric Object Size
static std::unique_ptr<int[]> accGeometricSize; /// 2 * point.size()

/**
 * @brief  setup all matricies for computation and prepare kernel stream  intertwined with cusolver
 *
 */
void solveSystemOnGPU(int *error);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    initDriver
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_initDriver(JNIEnv *env, jclass clazz) {
        int count;

        error = cudaGetDeviceCount(&count);
        if (error != cudaSuccess) {
                printf("driver  error %d  = %s \n", static_cast<int>(error), cudaGetErrorString(error));
                return JNI_ERROR;
        }

        deviceId = 0;

        error = cudaSetDevice(deviceId);
        if (error != cudaSuccess) {
                printf("driver  error %d  = %s \n", static_cast<int>(error), cudaGetErrorString(error));
                return JNI_ERROR;
        }
        return JNI_SUCCESS;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getLastError
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_mstruzek_jni_JNISolverGate_getLastError(JNIEnv *env, jclass clazz) {
        /// cuda error

        const char *msg = cudaGetErrorString(error);

        return env->NewStringUTF(msg);

        /// cusolver error
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    closeDriver
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_closeDriver(JNIEnv *env, jclass clazz) {
        error = cudaDeviceReset();
        if (error != cudaSuccess) {
                printf("driver  error %d  = %s \n", static_cast<int>(error), cudaGetErrorString(error));
                return JNI_ERROR;
        }
        return JNI_SUCCESS;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    resetDatabase
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_resetComputationData(JNIEnv *env, jclass clazz) {
        std::remove_if(points.begin(), points.end(), [](auto _) { return true; });
        std::remove_if(geometrics.begin(), geometrics.end(), [](auto _) { return true; });
        std::remove_if(constraints.begin(), constraints.end(), [](auto _) { return true; });
        std::remove_if(parameters.begin(), parameters.end(), [](auto _) { return true; });

        pointOffset = NULL;
        constraintOffset = NULL;
        geometricOffset = NULL;

        return JNI_SUCCESS;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    resetComputationContext
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_resetComputationContext(JNIEnv *env, jclass clazz) {
        /// workspace - zwolnic pamiec i wyzerowac wskazniki , \\cusolver
        return JNI_SUCCESS;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    initComputationContext
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_initComputationContext(JNIEnv *env, jclass clazz) {

        /// po zarejestrowaniu calego modelu w odpowiadajacych rejestrach , zainicjalizowac pomocnicze macierze
        /// przygotowac zmienne dla [cusolvera]

        /// przeliczenie pozycji absolutnej punktu na macierzy wyjsciowej

        return JNI_SUCCESS;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    solveSystem
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_solveSystem(JNIEnv *env, jclass clazz) {

        int error;

        error = 0;

        solveSystemOnGPU(&error);

        if (error != 0) {
                return JNI_ERROR;
        }

        return JNI_SUCCESS;
}

template <typename FieldType> void JniSetFieldValue(JNIEnv *env, jclass objClazz, jobject object, const char *fieldName, FieldType &sourceField) {
        if constexpr (std::is_same_v<FieldType, double>) {
                env->SetDoubleField(object, env->GetFieldID(objClazz, fieldName, "D"), sourceField);
        } else if constexpr (std::is_same_v<FieldType, long>) {
                env->SetLongField(object, env->GetFieldID(objClazz, fieldName, "J"), sourceField);
        } else if constexpr (std::is_same_v<FieldType, int>) {
                env->SetIntField(object, env->GetFieldID(objClazz, fieldName, "I"), sourceField);
        } else if constexpr (std::is_same_v<FieldType, bool>) {
                env->SetBooleanField(object, env->GetFieldID(objClazz, fieldName, "Z"), sourceField);
        }
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getSolverStatistics
 * Signature: ()Lcom/mstruzek/msketch/solver/SolverStat;
 */
JNIEXPORT jobject JNICALL Java_com_mstruzek_jni_JNISolverGate_getSolverStatistics(JNIEnv *env, jclass) {
        jclass objectClazz = env->FindClass("com/mstruzek/msketch/solver/SolverStat");
        if (objectClazz == NULL) {
                printf("SolverStat is not visible in class loader\n");
                env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "SolverStat is not visible in class loader");
                return 0;
        }

        jmethodID defaultCtor = env->GetMethodID(objectClazz, "<init>", "()V");
        if (defaultCtor == NULL) {
                printf("constructor not visible\n");
                env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "SolverStat constructor not visible");
                return 0;
        }

        jobject solverStatObject = env->NewObject(objectClazz, defaultCtor);
        if (solverStatObject == NULL) {
                printf("object statt <init> error\n");
                env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "construction instance error");
                return 0;
        }

        JniSetFieldValue(env, objectClazz, solverStatObject, "startTime", stat.startTime);
        JniSetFieldValue(env, objectClazz, solverStatObject, "stopTime", stat.stopTime);
        JniSetFieldValue(env, objectClazz, solverStatObject, "timeDelta", stat.timeDelta);
        JniSetFieldValue(env, objectClazz, solverStatObject, "size", stat.size);
        JniSetFieldValue(env, objectClazz, solverStatObject, "coefficientArity", stat.coefficientArity);
        JniSetFieldValue(env, objectClazz, solverStatObject, "dimension", stat.dimension);
        JniSetFieldValue(env, objectClazz, solverStatObject, "accEvaluationTime", stat.accEvaluationTime);
        JniSetFieldValue(env, objectClazz, solverStatObject, "accSolverTime", stat.accSolverTime);
        JniSetFieldValue(env, objectClazz, solverStatObject, "convergence", stat.convergence);
        JniSetFieldValue(env, objectClazz, solverStatObject, "error", stat.error);
        JniSetFieldValue(env, objectClazz, solverStatObject, "constraintDelta", stat.constraintDelta);
        JniSetFieldValue(env, objectClazz, solverStatObject, "iterations", stat.iterations);

        return solverStatObject;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    registerPointType
 * Signature: (IDD)I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_registerPointType(JNIEnv *env, jclass clazz, jint id, jdouble px, jdouble py) {
        points.emplace_back(id, px, py);
        return JNI_SUCCESS;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    registerGeometricType
 * Signature: (IIIIIIII)I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_registerGeometricType(JNIEnv *env, jclass clazz, jint id, jint geometricTypeId, jint p1, jint p2,
                                                                                 jint p3, jint a, jint b, jint c, jint d) {
        geometrics.emplace_back(id, geometricTypeId, p1, p2, p3, a, b, c, d);
        return JNI_SUCCESS;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    registerParameterType
 * Signature: (ID)I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_registerParameterType(JNIEnv *env, jclass clazz, jint id, jdouble value) {
        parameters.emplace_back(id, value);
        return JNI_SUCCESS;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    registerConstraintType
 * Signature: (IIIIIIDD)I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_registerConstraintType(JNIEnv *env, jclass clazz, jint id, jint jconstraintTypeId, jint k, jint l,
                                                                                  jint m, jint n, jint paramId, jdouble vecX, jdouble vecY) {
        constraints.emplace_back(id, jconstraintTypeId, k, l, m, n, paramId, vecX, vecY);
        return JNI_SUCCESS;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getPointPXCoordinate
 * Signature: (I)D
 */
JNIEXPORT jdouble JNICALL Java_com_mstruzek_jni_JNISolverGate_getPointPXCoordinate(JNIEnv *env, jclass clazz, jint id) {
        int offset = pointOffset[id];
        double px = points[offset].x;
        return (jdouble)px;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getPointPYCoordinate
 * Signature: (I)D
 */
JNIEXPORT jdouble JNICALL Java_com_mstruzek_jni_JNISolverGate_getPointPYCoordinate(JNIEnv *env, jclass clazz, jint id) {
        int offset = pointOffset[id];
        double py = points[offset].y;
        return (jdouble)py;
}

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getPointCoordinateVector
 * Signature: ()[D
 */
JNIEXPORT jdoubleArray JNICALL Java_com_mstruzek_jni_JNISolverGate_getPointCoordinateVector(JNIEnv *env, jclass clazz) {
        /// depraceted or remove !
        return (jdoubleArray)NULL;
}


static graph::StopWatch solverWatch;
static graph::StopWatch accEvoWatch;
static graph::StopWatch accLUWatch;

/// CPU#
long AllLagrangeCoffSize() { return 0; }

/// KERNEL#
double ConstraintGetFullNorm() { return 0.0; }

/// CPU#
void PointLocationSetup() {}

/// CPUtoGPU#
void CopyIntoStateVector(void *) {}

/// CPU# and GPU#
void SetupLagrangeMultipliers(void) {}

/// KERNEL#
void GeometricObjectEvaluateForceVector() { /// Sily  - F(q)
        // b.mulitply(-1);
}

// KERNEL#
void ConstraintEvaluateConstraintVector() {
        /// Wiezy  - Fi(q)

        /// b.mulitply(-1);
}

// KERNEL#
void ConstraintGetFullJacobian() {}

/// KERNEL# and last step update CPU memory for JNI synchronizationa
void PointUtilityCopyFromStateVector() {}

/// KERNEL#
void ConstraintGetFullHessian() {}

#define MAX_SOLVER_ITERATIONS 20

#define CONVERGENCE_LIMIT 10e-5

template <typename Obj, typename ObjIdFunction> std::unique_ptr<int[]> stateOffset(std::vector<Obj> objects, ObjIdFunction objectIdFunction) {
        std::unique_ptr<int[]> offsets(new int[objectIdFunction(objects.rbegin())]);
        auto iterator = objects.begin();
        int offset = 0;
        while (iterator != objects.end()) {
                offsets[offset++] = objectIdFunction(iterator);
                iterator++;
        }
        return offsets;
}

/// #/include <numeric> std::partial_sum

template <typename Obj, typename ValueFunction> std::unique_ptr<int[]> accumalatedValue(std::vector<Obj> vector, ValueFunction valueFunction) {
        int accValue = 0;
        std::unique_ptr<int[]> accumulated(new int[vector.size()]);
        for (int offset = 0; offset < vector.size(); offset++) {
                accumulated[offset] = accValue;
                accValue = accValue + valueFunction(vector[offset]);
        }
        return accumulated;
}

#include <numeric>

/**
 *  /// wydzielic pliku CU
 *
 *  /// przygotowac petle
 *
 *  ///
 *
 */
void solveSystemOnGPU(int *error) {

        int size;      /// wektor stanu
        int coffSize;  /// wspolczynniki Lagrange
        int dimension; /// dimension = size + coffSize

        solverWatch.reset();
        accEvoWatch.reset();
        accLUWatch.reset();

        pointOffset = stateOffset(points, [](auto point) { return point->id; });

        accConstraintSize = accumalatedValue(constraints, graph::constraintSize);

        accGeometricSize = accumalatedValue(geometrics, graph::geometricSetSize);

        size = std::accumulate(geometrics.begin(), geometrics.end(), 0, [](auto acc, auto const& geometric) { return acc + graph::geometricSetSize(geometric); });

        coffSize =
            std::accumulate(constraints.begin(), constraints.end(), 0, [](auto acc, auto const& constraint) { return acc + graph::constraintSize(constraint); });

        dimension = size + coffSize;

        /// Uklad rownan liniowych  [ A * x = b ] powstały z linerazycji ukladu dynamicznego

        /// 1# CPU -> GPU inicjalizacja macierzy

        graph::Tensor A;  /// Macierz głowna ukladu rownan liniowych
        graph::Tensor Fq; /// Macierz sztywnosci ukladu obiektow zawieszonych na sprezynach.
        graph::Tensor Wq; /// d(FI)/dq - Jacobian Wiezow

        /// HESSIAN
        graph::Tensor Hs;

        // Wektor prawych stron [Fr; Fi]'
        graph::Tensor b;

        // skladowe to Fr - oddzialywania pomiedzy obiektami, sily w sprezynach
        graph::Tensor Fr;

        // skladowa to Fiq - wartosci poszczegolnych wiezow
        graph::Tensor Fi;

        double norm1;            /// wartosci bledow na wiezach
        double prevNorm;         /// norma z wczesniejszej iteracji,
        double errorFluctuation; /// fluktuacja bledu

        stat.startTime = graph::TimeNanosecondsNow();

        printf("@#=================== Solver Initialized ===================#@ \n");
        printf("");

        if (points.size() == 0) {
                printf("[warning] - empty model\n");
                return;
        }

        if (constraints.size()) {
                printf("[warning] - no constraint configuration applied \n");
                return;
        }

        /// Tworzymy Macierz "A" - dla tego zadania stala w czasie

        size = points.size() * 2;
        coffSize = AllLagrangeCoffSize();
        dimension = size + coffSize;

        stat.size = size;
        stat.coefficientArity = coffSize;
        stat.dimension = dimension;

        accEvoWatch.setStartTick();

        /// Inicjalizacje bazowych macierzy rzadkich - SparseMatrix

        /// --->
        // instead loop over vector space provide static access to point location in VS.
        // - reference locations for Jacobian and Hessian evaluation !
        PointLocationSetup();

        /// cMalloc() --- przepisac na GRID -->wspolrzedne

        using graph::Tensor;

        A; //= Tensor.tensor2D(dimension, dimension, 0.0);

        Fq; // = Tensor.tensor2D(size, size, 0.0);

        Wq; // = Tensor.tensor2D(coffSize, size, 0.0);
        Hs; // = Tensor.tensor2D(size, size, 0.0);

        /// KERNEL_O

        /// ### macierz sztywnosci stala w czasie - jesli bezkosztowe bliskie zero to zawieramy w KELNER_PRE
        /// ( dla uproszczenia tylko w pierwszy przejsciu -- Thread_0_Grid_0 - synchronization Guard )

        /// inicjalizacja i bezposrednio do A
        ///   w nastepnych przejsciach -> kopiujemy

        // KernelEvaluateStiffnessMatrix<<<>>>(Fq)

        /// cMallloc

        /// Wektor prawych stron b

        b; /// ---  = Tensor.matrix1D(dimension, 0.0);

        // right-hand side vector ~ b

        Fr; // = b.viewSpan(0, 0, size, 1);
        Fi; // = b.viewSpan(size, 0, coffSize, 1);

        Tensor dmx;

        /// State Vector - zmienne stanu
        Tensor SV; // = Tensor.matrix1D(dimension, 0.0);

        /// PointUtility.

        CopyIntoStateVector(NULL);  // SV
        SetupLagrangeMultipliers(); // SV

        accEvoWatch.setStopTick();

        norm1 = 0.0;
        prevNorm = 0.0;
        errorFluctuation = 0.0;

        int itr = 0;
        while (itr < MAX_SOLVER_ITERATIONS) {

                accEvoWatch.setStartTick();

                /// --- KERNEL_PRE

                /// zerujemy macierz A

                /// # KERNEL_PRE

                A; // .reset(0.0); /// --- ze wzgledu na addytywnosc

                /// Tworzymy Macierz vector b vector `b

                /// # KERNEL_PRE
                GeometricObjectEvaluateForceVector(); // Fr /// Sily  - F(q)
                ConstraintEvaluateConstraintVector(); // Fi / Wiezy  - Fi(q)

                // b.setSubMatrix(0,0, (Fr));
                // b.setSubMatrix(size,0, (Fi));

                b; // .mulitply(-1);

                /// macierz `A

                /// # KERNEL_PRE (__shared__ JACOBIAN)

                ConstraintGetFullJacobian(); // --- (Wq); /// Jq = d(Fi)/dq --- write without intermediary matrix   `A = `A set value

                Hs; // --- .reset(0.0); /// ---- niepotrzebnie

                /// # KERNEL_PRE

                ConstraintGetFullHessian(); // --- (Hs, SV, size); // --- write without intermediate matrix '`A = `A +value

                A.setSubTensor(0, 0, Fq);  /// procedure SET
                A.plusSubTensor(0, 0, Hs); /// procedure ADD

                A.setSubTensor(size, 0, Wq);
                A.setSubTensor(0, size, Wq.transpose());

                /*
                 *  LU Decomposition  -- Colt Linear Equation Solver
                 *
                 *   rozwiazjemy zadanie [ A ] * [ dx ] = [ b ]
                 */

                /// DENSE MATRIX - pierwsze podejscie !

                /// ---- KERNEL_PRE

                accEvoWatch.setStopTick();

                accLUWatch.setStartTick();

                /// DENSE - CuSolver
                /// LU Solver

                /// ---------------- < cuSOLVER >
                /// ---------------- < cuSOLVER >
                /// ---------------- < cuSOLVER >
                // LUDecompositionQuick LU = new LUDecompositionQuick();
                // LU.decompose(matrix2DA);
                // LU.solve(matrix1Db);

                accLUWatch.setStopTick();

                /// Bind delta-x into database points

                /// --- KERNEL_POST
                dmx; // --- = Tensor.matrixDoubleFrom(matrix1Db);

                /// uaktualniamy punkty [ SV ] = [ SV ] + [ delta ]

                /// --- KERNEL_POST
                SV.plus(dmx);

                PointUtilityCopyFromStateVector(); // ---(SV); /// ??????? - niepotrzebnie , NA_KONCU

                /// --- KERNEL_POST( __synchronize__ REDUCTION )

                norm1 = ConstraintGetFullNorm();

                /// cCopySymbol()

                printf(" [ step :: %02d]  duration [ns] = %,12d  norm = %e \n", itr, (accLUWatch.stopTick - accEvoWatch.startTick), norm1);

                /// Gdy po 5-6 przejsciach iteracji, normy wiezow kieruja sie w strone minimum energii, to repozycjonowac prowadzace punkty

                if (norm1 < CONVERGENCE_LIMIT) {
                        stat.error = norm1;
                        printf("fast convergence - norm [ %e ] \n", norm1);
                        printf("constraint error = %e \n", norm1);
                        printf("");
                        break;
                }

                /// liczymy zmiane bledu
                errorFluctuation = norm1 - prevNorm;
                prevNorm = norm1;
                stat.error = norm1;

                if (itr > 1 && errorFluctuation / prevNorm > 0.70) {
                        printf("CHANGES - STOP ITERATION *******");
                        printf(" errorFluctuation          : %d \n", errorFluctuation);
                        printf(" relative error            : %f \n", (errorFluctuation / norm1));
                        solverWatch.setStopTick();
                        stat.constraintDelta = ConstraintGetFullNorm();
                        stat.convergence = false;
                        stat.stopTime = graph::TimeNanosecondsNow();
                        stat.iterations = itr;
                        stat.accSolverTime = accLUWatch.accTime;
                        stat.accEvaluationTime = accEvoWatch.accTime;
                        stat.timeDelta = solverWatch.stopTick - solverWatch.startTick;
                        return;
                }
                itr++;
        }

        solverWatch.setStopTick();
        long solutionDelta = solverWatch.delta();

        printf("# solution delta : %d \n", solutionDelta); // print execution time
        printf("\n");                                      // print execution time

        stat.constraintDelta = ConstraintGetFullNorm();
        stat.convergence = norm1 < CONVERGENCE_LIMIT;
        stat.stopTime = graph::TimeNanosecondsNow();
        stat.iterations = itr;
        stat.accSolverTime = accLUWatch.accTime;
        stat.accEvaluationTime = accEvoWatch.accTime;
        stat.timeDelta = solverWatch.stopTick - solverWatch.startTick;

        return;
}