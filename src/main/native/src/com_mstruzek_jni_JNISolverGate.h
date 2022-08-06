/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_mstruzek_jni_JNISolverGate */

#ifndef _Included_com_mstruzek_jni_JNISolverGate
#define _Included_com_mstruzek_jni_JNISolverGate
#ifdef __cplusplus
extern "C" {
#endif
#undef com_mstruzek_jni_JNISolverGate_JNI_SUCCESS
#define com_mstruzek_jni_JNISolverGate_JNI_SUCCESS 0L
#undef com_mstruzek_jni_JNISolverGate_JNI_ERROR
#define com_mstruzek_jni_JNISolverGate_JNI_ERROR 1L



#define JNI_SUCCESS com_mstruzek_jni_JNISolverGate_JNI_SUCCESS
#define JNI_ERROR   com_mstruzek_jni_JNISolverGate_JNI_ERROR


/// correspond to GeometricType enum definition order
#define GEOMETRIC_TYPE_ID_FreePoint   0
#define GEOMETRIC_TYPE_ID_Line        1
#define GEOMETRIC_TYPE_ID_FixLine     2
#define GEOMETRIC_TYPE_ID_Circle      3
#define GEOMETRIC_TYPE_ID_Arc         4

/// correspond to ConstraintType enum definition order
#define CONSTRAINT_TYPE_ID_FixPoint            0
#define CONSTRAINT_TYPE_ID_ParametrizedXFix    1
#define CONSTRAINT_TYPE_ID_ParametrizedYFix    2
#define CONSTRAINT_TYPE_ID_Connect2Points      3
#define CONSTRAINT_TYPE_ID_HorizontalPoint     4
#define CONSTRAINT_TYPE_ID_VerticalPoint       5
#define CONSTRAINT_TYPE_ID_LinesParallelism    6
#define CONSTRAINT_TYPE_ID_LinesPerpendicular  7
#define CONSTRAINT_TYPE_ID_EqualLength         8
#define CONSTRAINT_TYPE_ID_ParametrizedLength  9
#define CONSTRAINT_TYPE_ID_Tangency            10
#define CONSTRAINT_TYPE_ID_CircleTangency      11
#define CONSTRAINT_TYPE_ID_Distance2Points     12
#define CONSTRAINT_TYPE_ID_DistancePointLine   13
#define CONSTRAINT_TYPE_ID_Angle2Lines         14
#define CONSTRAINT_TYPE_ID_SetHorizontal       15
#define CONSTRAINT_TYPE_ID_SetVertical         16


/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    initDriver
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_initDriver
  (JNIEnv *, jclass);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getLastError
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_mstruzek_jni_JNISolverGate_getLastError
  (JNIEnv *, jclass);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    closeDriver
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_closeDriver
  (JNIEnv *, jclass);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    resetComputationData
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_resetComputationData
  (JNIEnv *, jclass);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    resetComputationContext
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_resetComputationContext
  (JNIEnv *, jclass);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    initComputationContext
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_initComputationContext
  (JNIEnv *, jclass);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    solveSystem
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_solveSystem
  (JNIEnv *, jclass);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getSolverStatistics
 * Signature: ()Lcom/mstruzek/msketch/solver/SolverStat;
 */
JNIEXPORT jobject JNICALL Java_com_mstruzek_jni_JNISolverGate_getSolverStatistics
  (JNIEnv *, jclass);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    registerPointType
 * Signature: (IDD)I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_registerPointType
  (JNIEnv *, jclass, jint, jdouble, jdouble);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    registerGeometricType
 * Signature: (IIIIIIIII)I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_registerGeometricType
  (JNIEnv *, jclass, jint, jint, jint, jint, jint, jint, jint, jint, jint);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    registerParameterType
 * Signature: (ID)I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_registerParameterType
  (JNIEnv *, jclass, jint, jdouble);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    registerConstraintType
 * Signature: (IIIIIIIDD)I
 */
JNIEXPORT jint JNICALL Java_com_mstruzek_jni_JNISolverGate_registerConstraintType
  (JNIEnv *, jclass, jint, jint, jint, jint, jint, jint, jint, jdouble, jdouble);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getPointPXCoordinate
 * Signature: (I)D
 */
JNIEXPORT jdouble JNICALL Java_com_mstruzek_jni_JNISolverGate_getPointPXCoordinate
  (JNIEnv *, jclass, jint);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getPointPYCoordinate
 * Signature: (I)D
 */
JNIEXPORT jdouble JNICALL Java_com_mstruzek_jni_JNISolverGate_getPointPYCoordinate
  (JNIEnv *, jclass, jint);

/*
 * Class:     com_mstruzek_jni_JNISolverGate
 * Method:    getPointCoordinateVector
 * Signature: ()[D
 */
JNIEXPORT jdoubleArray JNICALL Java_com_mstruzek_jni_JNISolverGate_getPointCoordinateVector
  (JNIEnv *, jclass);

#ifdef __cplusplus
}
#endif
#endif
