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
#ifndef __PLUMED_function_FunctionTemplateBase_h
#define __PLUMED_function_FunctionTemplateBase_h

#include "core/ActionWithValue.h"
#include "core/ActionWithArguments.h"
#include "tools/Matrix.h"

namespace PLMD {
namespace function {

class FunctionTemplateBase { 
protected:
/// Are we using derivatives
  bool noderiv;
/// Parse a keyword from the input as a value
  template<class T>
  void parse( Action* action, const std::string&key, T&t );
/// Parse a keyword from the input as a vector
  template<class T>
  void parseVector( Action* action, const std::string&key,std::vector<T>&t);
/// Parse a keyword from the input as a flag
  void parseFlag( Action* action, const std::string&key, bool&t );
public:
/// Override this function if you have not implemented the derivatives
  virtual bool derivativesImplemented() { return true; }
////
  virtual std::vector<std::string> getComponentsPerLabel() const ;
  virtual bool getDerivativeZeroIfValueIsZero() const { return false; } 
  virtual void buildTaskList( const std::string& name, const std::set<AtomNumber>& tflags, ActionWithValue* av ) const ; 
  virtual std::string getGraphInfo( const std::string& lab ) const ;
  virtual void registerKeywords( Keywords& keys ) = 0;
  virtual void read( ActionWithArguments* action ) = 0;
  virtual bool doWithTasks() const { return true; }
  virtual bool zeroRank() const { return false; }
  virtual void setPeriodicityForOutputs( ActionWithValue* action );
  virtual void setPrefactor( ActionWithArguments* action, const double pref ) {}
  virtual unsigned getArgStart() const { return 0; }
  virtual void setup( const ActionWithValue* action ); 
  virtual void calc( const ActionWithArguments* action, const std::vector<double>& args, std::vector<double>& vals, Matrix<double>& derivatives ) const = 0;
};

template<class T>
void FunctionTemplateBase::parse( Action* action, const std::string&key, T&t ) {
  action->parse(key,t);
}

template<class T>
void FunctionTemplateBase::parseVector( Action* action, const std::string&key,std::vector<T>&t) {
  action->parseVector(key,t);
}

inline
void FunctionTemplateBase::parseFlag( Action* action, const std::string&key, bool&t ) {
  action->parseFlag(key,t);
}

inline
std::vector<std::string> FunctionTemplateBase::getComponentsPerLabel() const {
  std::vector<std::string> comps; return comps;
}

inline
void FunctionTemplateBase::setup( const ActionWithValue* action ) {
  noderiv=action->doNotCalculateDerivatives();
  // Check for grids in input
  const ActionWithArguments* aarg=dynamic_cast<const ActionWithArguments*>( action ); plumed_assert( aarg ); 
  for(unsigned i=0; i<aarg->getNumberOfArguments(); ++i) {
      if( aarg->getPntrToArgument(i)->getRank()>0 && aarg->getPntrToArgument(i)->hasDerivatives() ) { noderiv=false; break; }
  }
}

inline
void FunctionTemplateBase::setPeriodicityForOutputs( ActionWithValue* action ) {
  plumed_massert( action->getNumberOfComponents()==1,"you must defined a setPeriodicityForOutputs function in your function class");
  if( action->keywords.exists("PERIODIC") ) { 
      std::vector<std::string> period; parseVector(action,"PERIODIC",period);
      if( period.size()==1 ) {
        if( period[0]!="NO") action->error("input to PERIODIC keyword does not make sense");
        action->setNotPeriodic(); return;
      } else if( period.size()!=2 ) action->error("input to PERIODIC keyword does not make sense");
      action->setPeriodic( period[0], period[1] );
  } else action->setNotPeriodic();
}

inline
std::string FunctionTemplateBase::getGraphInfo( const std::string& name ) const {
  std::size_t und = name.find_last_of("_"); return name.substr(0,und); 
}

inline
void FunctionTemplateBase::buildTaskList( const std::string& name, const std::set<AtomNumber>& tflags, ActionWithValue* av ) const {
  for(unsigned i=0;i<av->getNumberOfComponents();++i) (av->copyOutput(i))->addTasksToCurrentList( tflags ); 
}

}
}
#endif
