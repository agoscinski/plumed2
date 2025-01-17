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
#include "core/ActionShortcut.h"
#include "core/PlumedMain.h"
#include "core/ActionRegister.h"

namespace PLMD {
namespace colvar {

//+PLUMEDOC COLVAR ENERGY
/*
Calculate the total potential energy of the simulation box.

The potential energy can be biased e.g. with umbrella sampling \cite bart-karp98jpcb or with well-tempered metadynamics \cite Bonomi:2009p17935.

Notice that this CV could be unavailable with some MD code. When
it is available, and when also replica exchange is available,
metadynamics applied to ENERGY can be used to decrease the
number of required replicas.

\bug This \ref ENERGY does not include long tail corrections.
Thus when using e.g. LAMMPS `"pair_modify tail yes"` or GROMACS `"DispCorr Ener"` (or `"DispCorr EnerPres"`),
the potential energy from \ref ENERGY will be slightly different form the one of the MD code.
You should still be able to use \ref ENERGY and then reweight your simulation with the correct MD energy value.

\bug Acceptance for replica exchange when \ref ENERGY is biased
is computed correctly only if all the replicas have the same
potential energy function. This is for instance not true when
using GROMACS with lambda replica exchange or with plumed-hrex branch.

\par Examples

The following input instructs plumed to print the energy of the system
\plumedfile
ene: ENERGY
PRINT ARG=ene
\endplumedfile

*/
//+ENDPLUMEDOC


class Energy : public ActionShortcut {

public:
  explicit Energy(const ActionOptions&);
// active methods:
  static void registerKeywords( Keywords& keys );
};


PLUMED_REGISTER_ACTION(Energy,"ENERGY")

Energy::Energy(const ActionOptions&ao):
  Action(ao),
  ActionShortcut(ao)
{
  log<<"  Bibliography ";
  log<<plumed.cite("Bartels and Karplus, J. Phys. Chem. B 102, 865 (1998)");
  log<<plumed.cite("Bonomi and Parrinello, J. Comp. Chem. 30, 1615 (2009)");
  log<<"\n";
  readInputLine( getShortcutLabel() + ": COMBINE ARG=Energy PERIODIC=NO");
}

void Energy::registerKeywords( Keywords& keys ) {
  ActionShortcut::registerKeywords( keys );
}

}
}



