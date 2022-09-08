#pragma once

#include "model.cuh"

struct ComputationStateData {

    /// computation unique id
    int cID;     
    
    /// device variable cuBlas
    int info;    
    
    /// cublasDnrm2(...)
    double norm; 

    double *A;

    /// State Vector  [ SV = SV + dx ] , previous task -- "lineage"
    double *SV; 
    
    /// przyrosty   [ A * dx = b ]
    double *dx; 
    
    ///  right hand side vector - these are Fi / Fq
    double *b;

    ///  eventually synchronized into 'norm field on the host
    double *dev_norm; 

    /// wektor stanu geometric structure - effectievly consts
    size_t size;      

    /// wspolczynniki Lagrange
    size_t coffSize;  

    /// N - dimension = size + coffSize
    size_t dimension; 

    NVector<graph::Point> points;
    NVector<graph::Geometric> geometrics;
    NVector<graph::Constraint> constraints;
    NVector<graph::Parameter> parameters;

    // NVector direct map
    int *pointOffset;

    // NVector direct map
    int *geometricOffset;

    // NVector direct map
    int *constraintOffset;

    /// paramater offs from given ID
    int *parameterOffset;   

    /// Accumulative offs with geometric size evaluation function,  [ 0, ... , N , N + 1]
    int *accGeometricSize;  

    /// Accumulative offs with constraint size evaluation function, [ 0, ... , N , N + 1]
    int *accConstraintSize; 

    /// computation mode applied onto "A tensor" at this iteration
    ComputationMode computationMode;

    // CSR format, inverse permutation vector, direct addressing from COO to sparse matrix in CSR format
    int *P;

    /// not-transformed row vector of indicies, Coordinate Format COO
    int *cooRowInd = NULL;

    /// not-transformed column vector of indicies, Coordinate Format COO
    int *cooColInd = NULL;

    /// COO vector of values, Coordinate Format COO, or CSR format sorted
    double *cooVal = NULL;
};

