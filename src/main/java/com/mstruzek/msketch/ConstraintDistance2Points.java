package com.mstruzek.msketch;

import Jama.Matrix;
import com.mstruzek.msketch.matrix.MatrixDouble;

import static com.mstruzek.msketch.Parameter.dbParameter;
import static com.mstruzek.msketch.Point.dbPoint;

/**
 * Klasa reprezentujaca wiez odleglosci pomiedzy 2 punktami
 *
 * @author root
 */
public class ConstraintDistance2Points extends Constraint{

    /** Punkty kontrolne */
    /**
     * Point K-id
     */
    int k_id;
    /**
     * Point L-id
     */
    int l_id;
    /**
     * Numer parametru
     */
    int param_id;

    /**
     * Konstruktor pomiedzy 2 punktami
     * rownanie tego wiezu to sqrt[(K-L)'*(K-L)] - d = 0
     *
     * @param constId
     * @param K
     * @param L
     */
    public ConstraintDistance2Points(int constId,Point K,Point L,Parameter param){
        super(constId, GeometricConstraintType.Distance2Points, true);
        k_id=K.id;
        l_id=L.id;
        param_id=param.getId();
    }

    public String toString(){
        MatrixDouble out=getValue();
        double norm=Matrix.constructWithCopy(out.getArray()).norm1();
        return "Constraint-Distance2Points"+constraintId+"*s"+size()+" = "+norm+" { K ="+ dbPoint.get(k_id)+"  ,L ="+ dbPoint.get(l_id)+" , Parametr-"+ dbParameter.get(param_id).getId()+" = "+ dbParameter.get(param_id).getValue()+" } \n";

    }

    @Override
    public MatrixDouble getJacobian(){
        //macierz 2 wierszowa
        MatrixDouble out=MatrixDouble.fill(1,dbPoint.size()*2,0.0);
        //zerujemy cala macierz + wstawiamy na odpowiednie miejsce Jacobian wiezu
        int j=0;
        Vector vLK=((Vector) dbPoint.get(l_id)).sub((Vector) dbPoint.get(k_id)).unit();
        for(Integer i: dbPoint.keySet()){

            Point pointI = dbPoint.get(i);

            //a tu wstawiamy macierz dla tego wiezu

            if(k_id== pointI.id){
                out.m[0][j*2]=-vLK.x;
                out.m[0][j*2+1]=-vLK.y;
            }
            if(l_id== pointI.id){
                out.m[0][j*2]=vLK.x;
                out.m[0][j*2+1]=vLK.y;
            }
            j++;
        }

        return out;
    }

    @Override
    public boolean isJacobianConstant(){
        return true;

    }

    @Override
    public MatrixDouble getValue(){

        Double vLK=((Vector) dbPoint.get(l_id)).sub((Vector) dbPoint.get(k_id)).length();

        MatrixDouble mt=new MatrixDouble(1,1);
        mt.m[0][0]=vLK-dbParameter.get(param_id).getRadians();
        return mt;
    }

    @Override
    public MatrixDouble getHessian(double alfa){
        return null;
    }

    @Override
    public boolean isHessianConst(){
        return true;
    }

    @Override
    public int getK(){
        return k_id;
    }

    @Override
    public int getL(){
        return l_id;
    }

    @Override
    public int getM(){
        return -1;
    }

    @Override
    public int getN(){
        return -1;
    }

    @Override
    public int getParametr(){
        return param_id;
    }

    /**
     * @param args
     */
    public static void main(String[] args){

        Point p1=new Point(Point.nextId(),0.0,0.1);
        Point p2=new Point(Point.nextId(),1.0,0.2);
        Parameter par=new Parameter(1.0);
        ConstraintDistance2Points con=new ConstraintDistance2Points(Constraint.nextId(),p1,p2,new Parameter(0.8));
        ConstraintDistance2Points con2=new ConstraintDistance2Points(Constraint.nextId(),p1,p2,par);
        System.out.println(con);


    }

    @Override
    public double getNorm(){

        Double vLK=((Vector) dbPoint.get(l_id)).sub((Vector) dbPoint.get(k_id)).length();

        return (vLK-dbParameter.get(param_id).getRadians());
    }

}
