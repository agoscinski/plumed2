/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2017-2020 The plumed team
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
#ifndef __PLUMED_core_ActionToPutData_h
#define __PLUMED_core_ActionToPutData_h

#include "ActionForInterface.h"
#include "DataPassingObject.h"
#include "tools/Units.h"

namespace PLMD {

class ActionToPutData : 
public ActionForInterface 
{
friend class PlumedMain;
friend class TimeStep;
friend class DomainDecomposition;
private:
/// Are we not applying forces on this values
  bool noforce;
/// Is this quantity fixed
  bool fixed;
/// Are we allowed to set data at this time
  bool dataCanBeSet;
/// The unit of the value that has been passed to plumed
  enum{n,e,l,m,q,t} unit;
/// The unit to to use for the force
  enum{d,eng} funit;
/// This holds the pointer that we getting data from
  std::unique_ptr<DataPassingObject> mydata;
protected:
/// Setup the units of the input value
  void setUnit( const std::string& unitstr, const std::string& funitstr );
/// Conver a void pointer that is passed by the MD code to a double
  double MD2double( void* val ) const ;
public:
  static void registerKeywords(Keywords& keys);
  explicit ActionToPutData(const ActionOptions&ao);
/// This resets the stride in the collection object
  void setStride( const std::string& name, const unsigned& sss ) override;
/// Update the units on the input data
  void updateUnits( const Units& MDUnits, const Units& units );
/// This is called at the start of the step
  void resetForStepStart() override { dataCanBeSet = true; }
/// These are the actions that set the pointers to the approrpiate values 
  virtual bool setValuePointer( const std::string& name, void* val ) override ;
  bool setForcePointer( const std::string& name, void* val ) override ;
/// There are no atoms n local to set here
  void Set_comm(Communicator& comm) override {}
  void setAtomsNlocal(int n) override {}
  void setAtomsGatindex(int*g,bool fortran) override {}
  void setAtomsContiguous(int start) override {}
/// And this gets the number of forces that need to be rescaled
  unsigned getNumberOfForcesToRescale() const override ;
/// This transfers a constant value across from the MD code
  virtual void transferFixedValue( const double& unit );
/// Share the data
  void share() override {}
  void shareAll() override {}
/// Get the data to share
  virtual void wait();
/// Actually set the values for the output
  virtual void apply();
  void rescaleForces( const double& alpha );
/// For replica exchange
  void writeBinary(std::ostream&o) override;
  virtual void readBinary(std::istream&i) override;
  bool onStep() const override { return false; }
};

}
#endif
