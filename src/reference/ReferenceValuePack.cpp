/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013,2014 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

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
#include "ReferenceValuePack.h"

namespace PLMD {

ReferenceValuePack::ReferenceValuePack( const unsigned& nargs, const unsigned& natoms, MultiValue& vals ):
boxWasSet(false),
numberOfArgs(nargs),
oind(1),
myvals(vals)
{
  atom_indices.resize( natoms ); tmp_derivs.resize( natoms );
}

void ReferenceValuePack::resize( const unsigned& nargs, const unsigned& natoms ){
  numberOfArgs=nargs; atom_indices.resize( natoms ); tmp_derivs.resize( natoms );
}

void ReferenceValuePack::updateDynamicLists(){
  myvals.emptyActiveMembers();
  for(unsigned i=0;i<numberOfArgs;++i) myvals.putIndexInActiveArray( i );
  for(unsigned i=0;i<atom_indices.size();++i){
     unsigned nbase = numberOfArgs + 3*atom_indices[i];
     myvals.putIndexInActiveArray( nbase+0 ); myvals.putIndexInActiveArray( nbase+1 ); myvals.putIndexInActiveArray( nbase+2 );
  }
  unsigned nbase = myvals.getNumberOfDerivatives() - 9;
  // zero is added to all virial components to ensure that these are active in the dynamic list
  // if this is not done there is a problem with secondary structure variables
  if( atom_indices.size()>0 ){
     for(unsigned i=0;i<9;++i) myvals.addDerivative( oind, nbase+i, 0.0 );
  } 
  for(unsigned i=0;i<9;++i) myvals.putIndexInActiveArray( nbase+i ); 
  myvals.completeUpdate();
}

void ReferenceValuePack::clear(){
  if( !myvals.updateComplete() ) updateDynamicLists();
  myvals.clearAll(); boxWasSet=false;
}

void ReferenceValuePack::scaleAllDerivatives( const double& scalef ){
  if( !myvals.updateComplete() ) updateDynamicLists();

  for(unsigned i=0;i<myvals.getNumberActive();++i){
      unsigned ider=myvals.getActiveIndex(i);
      myvals.setDerivative( oind, ider, scalef*myvals.getDerivative( oind, ider ) );
  }
}

void ReferenceValuePack::copyScaledDerivatives( const unsigned& from, const double& scalef, const MultiValue& tvals ){
  plumed_dbg_assert( tvals.getNumberOfDerivatives()==myvals.getNumberOfDerivatives() );
  for(unsigned i=0;i<tvals.getNumberActive();++i){
      unsigned ider=tvals.getActiveIndex(i);
      myvals.addDerivative( oind, ider, scalef*tvals.getDerivative( from, ider ) );
  }
}

void ReferenceValuePack::moveDerivatives( const unsigned& from, const unsigned& to ){
  if( !myvals.updateComplete() ) updateDynamicLists();

  for(unsigned i=0;i<myvals.getNumberActive();++i){
     unsigned ider=myvals.getActiveIndex(i); 
     myvals.setDerivative( to, ider, myvals.getDerivative( from, ider ) );
  } 
}

}
