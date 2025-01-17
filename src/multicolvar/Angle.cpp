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
#include "tools/Angle.h"

namespace PLMD {
namespace multicolvar {

//+PLUMEDOC COLVAR ANGLE
/*
Calculate an angle.

This command can be used to compute the angle between three atoms. Alternatively
if four atoms appear in the atom
specification it calculates the angle between
two vectors identified by two pairs of atoms.

If _three_ atoms are given, the angle is defined as:
\f[
\theta=\arccos\left(\frac{ {\bf r}_{21}\cdot {\bf r}_{23}}{
|{\bf r}_{21}| |{\bf r}_{23}|}\right)
\f]
Here \f$ {\bf r}_{ij}\f$ is the distance vector among the
\f$i\f$th and the \f$j\f$th listed atom.

If _four_ atoms are given, the angle is defined as:
\f[
\theta=\arccos\left(\frac{ {\bf r}_{21}\cdot {\bf r}_{34}}{
|{\bf r}_{21}| |{\bf r}_{34}|}\right)
\f]

Notice that angles defined in this way are non-periodic variables and
their value is limited by definition between 0 and \f$\pi\f$.

The vectors \f$ {\bf r}_{ij}\f$ are by default evaluated taking
periodic boundary conditions into account.
This behavior can be changed with the NOPBC flag.

\par Examples

This command tells plumed to calculate the angle between the vector connecting atom 1 to atom 2 and
the vector connecting atom 2 to atom 3 and to print it on file COLVAR1. At the same time,
the angle between vector connecting atom 1 to atom 2 and the vector connecting atom 3 to atom 4 is printed
on file COLVAR2.
\plumedfile

a: ANGLE ATOMS=1,2,3
# equivalently one could state:
# a: ANGLE ATOMS=1,2,2,3

b: ANGLE ATOMS=1,2,3,4

PRINT ARG=a FILE=COLVAR1
PRINT ARG=b FILE=COLVAR2
\endplumedfile


*/
//+ENDPLUMEDOC

class Angle : public MultiColvarBase {
public:
  explicit Angle(const ActionOptions&);
// active methods:
  virtual void compute( const std::vector<Vector>& pos, MultiValue& myvals ) const override;
  static void registerKeywords( Keywords& keys );
};

PLUMED_REGISTER_ACTION(Angle,"ANGLE")

void Angle::registerKeywords( Keywords& keys ) {
  MultiColvarBase::registerKeywords(keys);
}

Angle::Angle(const ActionOptions&ao):
  Action(ao),
  MultiColvarBase(ao)
{
  if(getNumberOfAtomsInEachCV()==3 ) useFourAtomsForEachCV();
  if( getNumberOfAtomsInEachCV()!=4 ) error("Number of specified atoms should be 3 or 4");

  addValueWithDerivatives(); setNotPeriodic();
}

// calculator
void Angle::compute( const std::vector<Vector>& pos, MultiValue& myvals ) const {
  Vector dij,dik; dij=delta(pos[2],pos[3]); dik=delta(pos[1],pos[0]);
  Vector ddij,ddik; PLMD::Angle a;
  double angle=a.compute(dij,dik,ddij,ddik);
  addAtomsDerivatives(0,0,ddik,myvals);
  addAtomsDerivatives(0,1,-ddik,myvals);
  addAtomsDerivatives(0,2,-ddij,myvals);
  addAtomsDerivatives(0,3,ddij,myvals);
  setBoxDerivativesNoPbc(0,pos,myvals);
  setValue(0,angle,myvals);
}

}
}



