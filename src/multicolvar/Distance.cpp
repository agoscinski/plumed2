/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011-2020 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "MultiColvarBase.h"
#include "core/ActionRegister.h"
#include "tools/Pbc.h"

namespace PLMD {
namespace multicolvar {

//+PLUMEDOC COLVAR DISTANCE
/*
Calculate the distance between a pair of atoms.

By default the distance is computed taking into account periodic
boundary conditions. This behavior can be changed with the NOPBC flag.
Moreover, single components in Cartesian space (x,y, and z, with COMPONENTS)
or single components projected to the three lattice vectors (a,b, and c, with SCALED_COMPONENTS)
can be also computed.

Notice that Cartesian components will not have the proper periodicity!
If you have to study e.g. the permeation of a molecule across a membrane,
better to use SCALED_COMPONENTS.

\par Examples

The following input tells plumed to print the distance between atoms 3 and 5,
the distance between atoms 2 and 4 and the x component of the distance between atoms 2 and 4.
\plumedfile
d1:  DISTANCE ATOMS=3,5
d2:  DISTANCE ATOMS=2,4
d2c: DISTANCE ATOMS=2,4 COMPONENTS
PRINT ARG=d1,d2,d2c.x
\endplumedfile

The following input computes the end-to-end distance for a polymer
of 100 atoms and keeps it at a value around 5.
\plumedfile
WHOLEMOLECULES ENTITY0=1-100
e2e: DISTANCE ATOMS=1,100 NOPBC
RESTRAINT ARG=e2e KAPPA=1 AT=5
\endplumedfile

Notice that NOPBC is used
to be sure that if the end-to-end distance is larger than half the simulation
box the distance is compute properly. Also notice that, since many MD
codes break molecules across cell boundary, it might be necessary to
use the \ref WHOLEMOLECULES keyword (also notice that it should be
_before_ distance). The list of atoms provided to \ref WHOLEMOLECULES
here contains all the atoms between 1 and 100. Strictly speaking, this
is not necessary. If you know for sure that atoms with difference in
the index say equal to 10 are _not_ going to be farther than half cell
you can e.g. use
\plumedfile
WHOLEMOLECULES ENTITY0=1,10,20,30,40,50,60,70,80,90,100
e2e: DISTANCE ATOMS=1,100 NOPBC
RESTRAINT ARG=e2e KAPPA=1 AT=5
\endplumedfile
Just be sure that the ordered list provide to \ref WHOLEMOLECULES has the following
properties:
- Consecutive atoms should be closer than half-cell throughout the entire simulation.
- Atoms required later for the distance (e.g. 1 and 100) should be included in the list

The following example shows how to take into account periodicity e.g.
in z-component of a distance
\plumedfile
# this is a center of mass of a large group
c: COM ATOMS=1-100
# this is the distance between atom 101 and the group
d: DISTANCE ATOMS=c,101 COMPONENTS
# this makes a new variable, dd, equal to d and periodic, with domain -10,10
# this is the right choise if e.g. the cell is orthorombic and its size in
# z direction is 20.
dz: COMBINE ARG=d.z PERIODIC=-10,10
# metadynamics on dd
METAD ARG=dz SIGMA=0.1 HEIGHT=0.1 PACE=200
\endplumedfile

Using SCALED_COMPONENTS this problem should not arise because they are always periodic
with domain (-0.5,+0.5).




*/
//+ENDPLUMEDOC

class Distance : public MultiColvarBase {
private:
  bool components;
  bool scaled_components;

public:
  static void registerKeywords( Keywords& keys );
  explicit Distance(const ActionOptions&);
// active methods:
  void compute( const std::vector<Vector>& pos, MultiValue& myvals ) const override;
};

PLUMED_REGISTER_ACTION(Distance,"DISTANCE")

void Distance::registerKeywords( Keywords& keys ) {
  MultiColvarBase::registerKeywords( keys );
  keys.addFlag("COMPONENTS",false,"calculate the x, y and z components of the distance separately and store them as label.x, label.y and label.z");
  keys.addFlag("SCALED_COMPONENTS",false,"calculate the a, b and c scaled components of the distance separately and store them as label.a, label.b and label.c");
  keys.addOutputComponent("x","COMPONENTS","the x-component of the vector connecting the two atoms");
  keys.addOutputComponent("y","COMPONENTS","the y-component of the vector connecting the two atoms");
  keys.addOutputComponent("z","COMPONENTS","the z-component of the vector connecting the two atoms");
  keys.addOutputComponent("a","SCALED_COMPONENTS","the normalized projection on the first lattice vector of the vector connecting the two atoms");
  keys.addOutputComponent("b","SCALED_COMPONENTS","the normalized projection on the second lattice vector of the vector connecting the two atoms");
  keys.addOutputComponent("c","SCALED_COMPONENTS","the normalized projection on the third lattice vector of the vector connecting the two atoms");
}

Distance::Distance(const ActionOptions&ao):
  Action(ao),
  MultiColvarBase(ao),
  components(false),
  scaled_components(false)
{
  if(getNumberOfAtomsInEachCV()!=2) error("Number of specified atoms should be 2");
  parseFlag("COMPONENTS",components);
  parseFlag("SCALED_COMPONENTS",scaled_components);
  checkRead();

  if(components && scaled_components) error("COMPONENTS and SCALED_COMPONENTS are not compatible");

  if(components) {
    addComponentWithDerivatives("x"); componentIsNotPeriodic("x");
    addComponentWithDerivatives("y"); componentIsNotPeriodic("y");
    addComponentWithDerivatives("z"); componentIsNotPeriodic("z");
    log<<"  WARNING: components will not have the proper periodicity - see manual\n";
  } else if(scaled_components) {
    addComponentWithDerivatives("a"); componentIsPeriodic("a","-0.5","+0.5");
    addComponentWithDerivatives("b"); componentIsPeriodic("b","-0.5","+0.5");
    addComponentWithDerivatives("c"); componentIsPeriodic("c","-0.5","+0.5");
  } else {
    addValueWithDerivatives(); setNotPeriodic();
  }
}


// calculator
void Distance::compute( const std::vector<Vector>& pos, MultiValue& myvals ) const {

  Vector distance=delta(pos[0],pos[1]);
  const double value=distance.modulo();
  const double invvalue=1.0/value;

  if(components) {
    addAtomsDerivatives(0,0,Vector(-1,0,0),myvals);
    addAtomsDerivatives(0,1,Vector(+1,0,0),myvals);
    setBoxDerivativesNoPbc( 0, pos, myvals);
    setValue( 0, distance[0], myvals );

    addAtomsDerivatives(1,0,Vector(0,-1,0),myvals);
    addAtomsDerivatives(1,1,Vector(0,+1,0),myvals);
    setBoxDerivativesNoPbc( 1, pos, myvals);
    setValue( 1, distance[1], myvals );

    addAtomsDerivatives(2,0,Vector(0,0,-1),myvals);
    addAtomsDerivatives(2,1,Vector(0,0,+1),myvals);
    setBoxDerivativesNoPbc( 2, pos, myvals );
    setValue( 2, distance[2], myvals );
  } else if(scaled_components) {
    Vector d=getPbc().realToScaled(distance);
    addAtomsDerivatives(0,0,matmul(getPbc().getInvBox(),Vector(-1,0,0)),myvals);
    addAtomsDerivatives(0,1,matmul(getPbc().getInvBox(),Vector(+1,0,0)),myvals);
    setValue(0,Tools::pbc(d[0]),myvals);

    addAtomsDerivatives(1,0,matmul(getPbc().getInvBox(),Vector(0,-1,0)),myvals);
    addAtomsDerivatives(1,1,matmul(getPbc().getInvBox(),Vector(0,+1,0)),myvals);
    setValue(1,Tools::pbc(d[1]),myvals);

    addAtomsDerivatives(2,0,matmul(getPbc().getInvBox(),Vector(0,0,-1)),myvals);
    addAtomsDerivatives(2,1,matmul(getPbc().getInvBox(),Vector(0,0,+1)),myvals);
    setValue(2,Tools::pbc(d[2]),myvals);
  } else {
    addAtomsDerivatives(0,0,-invvalue*distance,myvals);
    addAtomsDerivatives(0,1,invvalue*distance,myvals);
    setBoxDerivativesNoPbc(0,pos,myvals);
    setValue(0,value,myvals);
  }

}

}
}



