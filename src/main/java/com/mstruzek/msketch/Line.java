package com.mstruzek.msketch;

import com.mstruzek.msketch.matrix.MatrixDouble;


/// FIX Point recalculated form P1 and P2, from Persistence and Serialization!

//FIXME - trzeba zaburzac punkty jezeli sa singularity 
/**
 * Klasa Line - model linii w szkicowniku
 * pomiedzy punktami [a,p1] i [p2,b] -zastosowane sprezyny slabe - springStiffnessLow 
 * pomiedzy pynktami [p1,p2] - sprezyny mocne springStiffnessHigh 
 *
 */
public class Line extends GeometricPrimitive{

	/** fix control points -punkty kontrolne zafixowane */
	Point a = null; 
	Point b = null;
	/** dynamic points - punkty dynamiczne odpowiedzialne za przemiszczanie sie lini*/
	Point p1 =  null;
	Point p2 =  null;
	

	/** Odleglosci pomiedzy wektorami poczatkowe */
	double d_a_p1,d_p1_p2,d_p2_b;
	
	/** wspolczynnik do skalowania wzglednego dla wektorow*/
	double alfa = 0.5;

	/** Numery wiezow  powiazane z a,b*/
	int[] constraintsId = new int[2];
	                              
	public Line(Vector p10,Vector p20){
		this(GeometricPrimitive.nextId(), p10, p20);
	}

	public Line(int id,Vector v10,Vector v20){
		super(id,GeometricPrimitiveType.Line);

		if(v10 instanceof Point && v20 instanceof Point){
			Point p1=(Point) v10;
			Point p2=(Point) v20;
			a=new Point(p1.getId()-1,v10.sub(v20).dot(alfa).add(v10));
			b=new Point(p2.getId()+1,v20.sub(v10).dot(alfa).add(v20));
		}else{
			//ustawienie pozycji dla punktow kontrolnych
			//Kolejnosc inicjalizacji ma znaczenie
			a=new Point(v10.sub(v20).dot(alfa).add(v10));
			p1=new Point(v10);//przepisujemy wartosci
			p2=new Point(v20);
			b=new Point(v20.sub(v10).dot(alfa).add(v20));
		}
		//FIXME  -Trzeba pomyslec o naciagu wstepnym sprezyn
		calculateDistance();
		//ustawiamy wiezy dla punktow a i b , przechowujemy lokalnie numery wiezow
		this.associateConstraintsId = setAssociateConstraints();
	}

	public String toString(){
		String out = type + "" + "*" + this.primitiveId + ": {";
		out+="a=" + a + ",";
		out+="p1=" + p1 + ",";
		out+="p2=" + p2 + ",";
		out+="b=" + b + "}\n";
		return out;
	}
	/** Funkcja oblicza dlugosci poczatkowe pomiedzy wektorami */
	private void calculateDistance() {

		d_a_p1 = Math.abs(p1.sub(a).length());
		d_p1_p2 = Math.abs(p2.sub(p1).length());
		d_p2_b =  Math.abs(b.sub(p2).length());
	}

	@Override
	public void recalculateControlPoints() {
		
		Vector va = (Vector) (p1.sub(p2).dot(alfa).add(p1));
		Vector vb = (Vector) (p2.sub(p1).dot(alfa).add(p2));
		a.setLocation(va.x,va.y);
		b.setLocation(vb.x,vb.y);
		
		calculateDistance();
		
		//uaktulaniamy wiezy
		((ConstraintFixPoint)Constraint.dbConstraint.get(constraintsId[0])).setFixVector(va);
		((ConstraintFixPoint)Constraint.dbConstraint.get(constraintsId[1])).setFixVector(vb);
	}

	
	@Override
	public MatrixDouble getForceJacobian() {
		// a ,p1 ,p2 ,b = 4*2 = 8x8
		MatrixDouble mt = MatrixDouble.fill(8, 8, 0.0);
		/**
		 * k= I*k
		 * [ -ks    ks      0  	  0;
		 *    ks  -ks-kb   kb  	  0;
		 *     0    kb   -ks-kb   ks;
		 *     0  	 0     ks    -ks];
		 */
		// K -mala sztywnosci
		MatrixDouble Ks = MatrixDouble.diagonal(Config.springStiffnessLow,Config.springStiffnessLow);
		// K - duza szytwnosci
		MatrixDouble Kb = MatrixDouble.diagonal(Config.springStiffnessHigh,Config.springStiffnessHigh);
		// -Ks-Kb
		MatrixDouble Ksb = Ks.dotC(-1).addSubMatrix(0, 0, Kb.dotC(-1));
		//wiersz pierwszy
		mt.addSubMatrix(0, 0, Ks.dotC(-1)).addSubMatrix(0, 2, Ks);
		mt.addSubMatrix(2, 0, Ks).addSubMatrix(2, 2, Ksb).addSubMatrix(2, 4, Kb);
		mt.addSubMatrix(4, 2, Kb).addSubMatrix(4, 4, Ksb).addSubMatrix(4, 6, Ks);
		mt.addSubMatrix(6, 4, Ks).addSubMatrix(6, 6, Ks.dotC(-1));
		return mt;
	}

	@Override
	public int[] setAssociateConstraints() {
		ConstraintFixPoint fixPointa = new ConstraintFixPoint(Constraint.nextId(),a);
		ConstraintFixPoint fixPointb = new ConstraintFixPoint(Constraint.nextId(),b);
		constraintsId[0] = fixPointa.constraintId;
		constraintsId[1] = fixPointb.constraintId;
		return constraintsId;
	}

	@Override
	public MatrixDouble getForce() {
		// 8 = 4*2 (4 punkty kontrolne)
		MatrixDouble force = MatrixDouble.fill(8,1,0.0);
		
		//F12 - sily w sprezynach
		Vector f12 = p1.sub(a).unit().dot(Config.springStiffnessLow).dot(p1.sub(a).length()-d_a_p1);	
		//F23
		Vector f23 = p2.sub(p1).unit().dot(Config.springStiffnessHigh).dot(p2.sub(p1).length()-d_p1_p2);
		//F34
		Vector f34 = b.sub(p2).unit().dot(Config.springStiffnessLow).dot(b.sub(p2).length()-d_p2_b);
		
		//F1 - silu na poszczegolne punkty
		force.addSubMatrix(0, 0, new MatrixDouble(f12,true));
		//F2
		force.addSubMatrix(2, 0, new MatrixDouble(f23.sub(f12),true));
		//F3
		force.addSubMatrix(4, 0, new MatrixDouble(f34.sub(f23),true));
		//F4
		force.addSubMatrix(6, 0, new MatrixDouble(f34.dot(-1.0),true));
		
		return force;
	}

	@Override
	public int getNumOfPoints() {
		//a,b,p1,p2
		return 4;
	}

	@Override
	public int getP1() {
		return p1.id;
	}

	@Override
	public int getP2() {
		return p2.id;
	}

	@Override
	public int getP3() {
		return -1;
	}

	@Override
	public int[] getAllPointsId() {
		int[] out = new int[4];
		out[0] = a.getId();
		out[1] = b.getId();
		out[2] = p1.getId();
		out[3] = p2.getId();
		return out;
	}

	@Override
	public int getA() {

		return a.id;
	}

	@Override
	public int getB() {
		return b.id;
	}

	@Override
	public int getC() {
		return -1;
	}

	@Override
	public int getD() {
		return -1;
	}
}
