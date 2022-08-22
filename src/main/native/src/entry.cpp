#include <iostream>
#include <stdio.h>

#include "stop_watch.h"

#include "model_config.h"
#include "geometric_solver.h"

#include "com_mstruzek_jni_JNISolverGate.h"


#define jni_initDriver                 Java_com_mstruzek_jni_JNISolverGate_initDriver
#define jni_getLastError               Java_com_mstruzek_jni_JNISolverGate_getLastError
#define jni_closeDriver                Java_com_mstruzek_jni_JNISolverGate_closeDriver
#define jni_resetComputationData       Java_com_mstruzek_jni_JNISolverGate_resetComputationData
#define jni_resetComputationContext    Java_com_mstruzek_jni_JNISolverGate_resetComputationContext
#define jni_initComputationContext     Java_com_mstruzek_jni_JNISolverGate_initComputationContext
#define jni_solveSystem                Java_com_mstruzek_jni_JNISolverGate_solveSystem
#define jni_registerPointType          Java_com_mstruzek_jni_JNISolverGate_registerPointType
#define jni_registerGeometricType      Java_com_mstruzek_jni_JNISolverGate_registerGeometricType
#define jni_registerParameterType      Java_com_mstruzek_jni_JNISolverGate_registerParameterType
#define jni_registerConstraintType     Java_com_mstruzek_jni_JNISolverGate_registerConstraintType
#define jni_getPointPXCoordinate       Java_com_mstruzek_jni_JNISolverGate_getPointPXCoordinate
#define jni_getPointPYCoordinate       Java_com_mstruzek_jni_JNISolverGate_getPointPYCoordinate
#define jni_getPointCoordinateVector   Java_com_mstruzek_jni_JNISolverGate_getPointCoordinateVector


int main(int argc, char* args[]) 
{

    int err;

    printf("empty inspector \n");

    long stopTime = graph::TimeNanosecondsNow();
    long startTime = graph::TimeNanosecondsNow();    


    ///  MOCK
    JNINativeInterface_ functions_iface = {};
        
    JNIEnv env = {};
    env.functions = &functions_iface;

    jclass eclass = {};
        

    err = jni_initDriver(&env, eclass);

    err = jni_resetComputationData(&env, eclass);
    
    /// line_1

    err = jni_registerPointType(&env, eclass, 0, 0.0, 20.0);
    err = jni_registerPointType(&env, eclass, 1, 20.0, 20.0);
    err = jni_registerPointType(&env, eclass, 2, 60.0, 20.0);
    err = jni_registerPointType(&env, eclass, 3, 80.0, 20.0);    
    err = jni_registerConstraintType(&env, eclass, 1, CONSTRAINT_TYPE_ID_FIX_POINT, 0, -1, -1, -1, -1, 0.0, 20.0);
    err = jni_registerConstraintType(&env, eclass, 2, CONSTRAINT_TYPE_ID_FIX_POINT, 3, -1, -1, -1, -1, 80.0, 20.0);    
    err = jni_registerGeometricType(&env, eclass, 1, GEOMETRIC_TYPE_ID_LINE, 1, 2, 0, 3, -1, -1, -1);
     

    /// line_2
    err = jni_registerPointType(&env, eclass, 4, 20.0, 0.0);
    err = jni_registerPointType(&env, eclass, 5, 20.0, 20.0);
    err = jni_registerPointType(&env, eclass, 6, 20.0, 60.0);
    err = jni_registerPointType(&env, eclass, 7, 20.0, 80.0);    
    err = jni_registerConstraintType(&env, eclass, 3, CONSTRAINT_TYPE_ID_FIX_POINT, 4, -1, -1, -1, -1, 20.0, 0.0);
    err = jni_registerConstraintType(&env, eclass, 4, CONSTRAINT_TYPE_ID_FIX_POINT, 7, -1, -1, -1, -1, 20.0, 80.0);    
    err = jni_registerGeometricType(&env, eclass, 2, GEOMETRIC_TYPE_ID_LINE, 5, 6, 4, 7, -1, -1, -1);
   
    // constraint(1,5)(Connect2Points)
    err = jni_registerConstraintType(&env, eclass, 5, CONSTRAINT_TYPE_ID_CONNECT_2_POINTS, 1, 5, -1, -1, -1, -1, -1);    


    err = jni_initComputationContext(&env, eclass);


    err = jni_solveSystem(&env, eclass);


    return 0;
}