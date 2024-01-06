/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011-2017 The plumed team
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
#ifndef __PLUMED_core_AverageBase_h
#define __PLUMED_core_AverageBase_h
#include "ActionPilot.h"
#include "ActionAtomistic.h"
#include "ActionWithValue.h"
#include "ActionWithArguments.h"
#include "tools/RMSD.h"

namespace PLMD {

class AverageBase :
  public ActionPilot,
  public ActionAtomistic,
  public ActionWithValue,
  public ActionWithArguments {
private:
  bool clearnextstep;
  Tensor rot;
  PLMD::RMSD myrmsd;
  std::string rmsd_type;
  Matrix<std::vector<Vector> > DRotDPos;
  // std::vector<double> data;
  std::vector<std::vector<Vector> > direction;
  std::vector<Vector> der, centeredpos, centeredreference;
protected:
  bool firststep;
  double starttime;
  std::vector<Vector> atom_pos;
  bool clearnorm;
  unsigned clearstride;
  unsigned n_real_args;
  std::vector<double> align, displace;
/// Get the number of atoms that we are averaging
  unsigned getNumberOfAtomsToAverage() const ;
/// Get the position of the ith atom in the reference configuration
  Vector getReferencePosition(const unsigned& i );
public:
  static void registerKeywords( Keywords& keys );
  explicit AverageBase( const ActionOptions& );
  void clearDerivatives( const bool& force=false );
  bool hasClear() const ;
  unsigned getNumberOfDerivatives() const ;
/// These are required because we inherit from both ActionAtomistic and ActionWithArguments
  void lockRequests();
  void unlockRequests();
  void calculateNumericalDerivatives( ActionWithValue* a=NULL ) { plumed_error(); }
/// We override this here so as not to get errors  
  void apply() {}
  void update();
  virtual void setReferenceConfig();
  virtual void accumulate( const std::vector<std::vector<Vector> >& dir ) = 0;
  std::string getStrideClearAndWeights() const ;
  std::string getAtomsData() const ;
};

inline
unsigned AverageBase::getNumberOfAtomsToAverage() const {
  return atom_pos.size();
}

inline
Vector AverageBase::getReferencePosition(const unsigned& i ) {
  return myrmsd.getReference()[i];
}

inline
bool AverageBase::hasClear() const {
  return (clearstride>0);
}

}
#endif
