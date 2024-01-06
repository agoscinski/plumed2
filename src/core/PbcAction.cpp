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
#include "PbcAction.h"
#include "tools/Pbc.h"
#include "PlumedMain.h"
#include "ActionSet.h"
#include "ActionRegister.h"

namespace PLMD {

PLUMED_REGISTER_ACTION(PbcAction,"PBC")

void PbcAction::registerKeywords( Keywords& keys ) {
  Action::registerKeywords( keys );
  keys.add("hidden","NO_ACTION_LOG","suppresses printing from action on the log");
}

PbcAction::PbcAction(const ActionOptions&ao):
  Action(ao),
  ActionToPutData(ao),
  interface(NULL)
{
  std::vector<unsigned> shape(2); shape[0]=shape[1]=3; 
  addValue( shape ); setNotPeriodic(); setUnit( "length", "energy" ); 
  getPntrToOutput(0)->alwaysStoreValues();

  std::vector<ActionForInterface*> allput=plumed.getActionSet().select<ActionForInterface*>();
  for(unsigned i=0;i<allput.size();++i) {
      ActionToPutData* ap = dynamic_cast<ActionToPutData*>( allput[i] );
      if( !ap && !interface ) interface = allput[i];
      else if( !ap && interface ) warning("found more than one interface so don't know how to broadcast cell"); 
  }
}


void PbcAction::setPbc() {
  Tensor box; if( interface ) interface->broadcastToDomains( getPntrToOutput(0) );
  for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j) box(i,j) = getPntrToOutput(0)->get(3*i+j);
  pbc.setBox(box);
}

void PbcAction::wait() {
  ActionToPutData::wait(); setPbc();
}

void PbcAction::readBinary(std::istream&i) {
  ActionToPutData::readBinary(i); setPbc();
}

}



