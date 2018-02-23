/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2017 The plumed team
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
#include "HistogramBase.h"

namespace PLMD {
namespace gridtools {

void HistogramBase::shortcutKeywords( Keywords& keys ) {
  keys.add("optional","HEIGHTS","this keyword takes the label of an action that calculates a vector of values.  The elements of this vector "
                                "are used as weights for the Gaussians.");
  keys.addFlag("UNORMALIZED",false,"calculate the unormalized distribution of colvars");
}

void HistogramBase::resolveNormalizationShortcut( const std::string& lab, const std::vector<std::string>& words,
                                                  const std::map<std::string,std::string>& keys,
                                                  std::vector<std::vector<std::string> >& actions ) {
  std::vector<std::string> inp;
  if( keys.count("HEIGHTS") && !keys.count("UNORMALIZED") ) {
      std::vector<std::string> norm_input; norm_input.push_back( lab + "_hsum:");
      norm_input.push_back("COMBINE"); norm_input.push_back( "ARG=" + keys.find("HEIGHTS")->second );
      norm_input.push_back("PERIODIC=NO"); actions.push_back( norm_input );
      inp.push_back( lab + "_unorm:" ); inp.push_back(words[0]); inp.push_back("UNORMALIZED");
  } else {
      inp.push_back( lab + ":" ); inp.push_back(words[0]);
      if( keys.count("UNORMALIZED") ) inp.push_back("UNORMALIZED");
  }
  for(unsigned i=1;i<words.size();++i) inp.push_back( words[i] );
  if( keys.count("HEIGHTS") ) inp.push_back( "HEIGHTS=" + keys.find("HEIGHTS")->second );
  actions.push_back( inp );
  if( keys.count("HEIGHTS") && !keys.count("UNORMALIZED") ) {
      std::vector<std::string> ninp; ninp.push_back( lab + ":" ); ninp.push_back("MATHEVAL");
      ninp.push_back("ARG1=" + lab + "_unorm"); ninp.push_back("ARG2=" + lab + "_hsum");
      ninp.push_back("FUNC=x/y"); ninp.push_back("PERIODIC=NO"); actions.push_back( ninp );
  }
}

void HistogramBase::registerKeywords( Keywords& keys ) {
  Action::registerKeywords( keys ); ActionWithValue::registerKeywords( keys );
  ActionWithArguments::registerKeywords( keys ); keys.use("ARG"); 
  keys.add("optional","HEIGHTS","this keyword takes the label of an action that calculates a vector of values.  The elements of this vector "
                                "are used as weights for the Gaussians.");
  keys.addFlag("UNORMALIZED",false,"calculate the unormalized distribution of colvars");
}

HistogramBase::HistogramBase(const ActionOptions&ao):
  Action(ao),
  ActionWithValue(ao),
  ActionWithArguments(ao),
  heights_index(1),
  one_kernel_at_a_time(false)
{
  // Check all the values have the right size
  unsigned nvals = 1; 
  if( arg_ends.size()>0 ) { 
      nvals=0; for(unsigned i=arg_ends[0];i<arg_ends[1];++i) nvals += getPntrToArgument(i)->getNumberOfValues( getLabel() );
      for(unsigned i=1;i<arg_ends.size()-1;++i) {
          unsigned tvals=0; for(unsigned j=arg_ends[i];j<arg_ends[i+1];++j) tvals += getPntrToArgument(j)->getNumberOfValues( getLabel() );
          if( nvals!=tvals ) error("mismatch between numbers of values in input arguments");
      }
  } else {
      arg_ends.push_back(0); for(unsigned i=0;i<getNumberOfArguments();++i) arg_ends.push_back(i+1);
  }
  // Get the heights if need be
  std::vector<std::string> weight_str; parseVector("HEIGHTS",weight_str);
  if( weight_str.size()>0 ) {
      std::vector<Value*> weight_args; interpretArgumentList( weight_str, weight_args ); 
      heights_index=2; std::vector<Value*> args( getArguments() ); unsigned tvals=0; 
      log.printf("  quantities used for weights are : %s ", weight_str[0].c_str() );
      for(unsigned i=1;i<weight_args.size();++i) log.printf(", %s", weight_str[i].c_str() );
      log.printf("\n");

      for(unsigned i=0;i<weight_args.size();++i) {
          tvals += weight_args[i]->getNumberOfValues( getLabel() );
          args.push_back( weight_args[i] ); 
      }
      if( nvals!=tvals ) error("mismatch between numbers of values in input arguments and HEIGHTS");
      arg_ends.push_back( args.size() ); requestArguments( args, true );
  } 

  parseFlag("UNORMALIZED",unorm);
  if( unorm ) log.printf("  calculating unormalized distribution \n");
  else log.printf("  calculating normalized distribution \n");

  // Now construct the tasks
  if( distinct_arguments.size()>0 ) {
      plumed_assert( getNumberOfArguments()>0 ); unsigned ntasks = 1; 
      if( getPntrToArgument(0)->getRank()>0 ) ntasks = getPntrToArgument(0)->getShape()[0];
      for(unsigned i=1;i<getNumberOfArguments();++i) {
          if( arg_ends[i]!=i ) error("not sure if this sort of reshaping works");
          if( getPntrToArgument(0)->getRank()==0 && getPntrToArgument(i)->getRank()!=0 ) error("all arguments should have same shape");
          else if( getPntrToArgument(i)->getShape()[0]!=ntasks ) error("all arguments should have same shape");
      }
      for(unsigned i=0;i<ntasks;++i) addTaskToList(i);
      std::vector<std::string> alabels;
      for(unsigned i=0;i<getNumberOfArguments();++i){
          bool found=false; std::string mylab = (getPntrToArgument(i)->getPntrToAction())->getLabel();
          for(unsigned j=0;j<alabels.size();++j){
              if( alabels[j]==mylab ){ found=true; break; }
          }
          if( !found ) alabels.push_back( mylab );
      }

      bool added=false;
      for(unsigned i=0;i<getNumberOfArguments();++i){
          // Add this function to jobs to do in recursive loop in previous action
          if( getPntrToArgument(i)->getRank()>0 ){
              if( (getPntrToArgument(i)->getPntrToAction())->addActionToChain( alabels, this ) ){ added=true; break; }
          }
      }
      plumed_massert(added, "could not add action " + getLabel() + " to chain of any of its arguments");
  } else {
      bool hasrank = getPntrToArgument(0)->getRank()>0;
      if( hasrank ) { 
          for(unsigned i=0;i<getNumberOfArguments();++i){ getPntrToArgument(i)->buildDataStore( getLabel() ); plumed_assert( getPntrToArgument(i)->getRank()>0 ); }
          for(unsigned i=0;i<nvals;++i) addTaskToList(i);
      } else {
          one_kernel_at_a_time=true; for(unsigned i=0;i<arg_ends.size();++i){ if( arg_ends[i]!=i ){ one_kernel_at_a_time=false; break; } }
          if( !one_kernel_at_a_time ) for(unsigned i=0;i<nvals;++i) addTaskToList(i);
      }
  }
}

void HistogramBase::addValueWithDerivatives( const std::vector<unsigned>& shape ){ 
  ActionWithValue::addValueWithDerivatives( shape ); setNotPeriodic();
  if( one_kernel_at_a_time ) {
      for(unsigned i=0;i<gridobject.getNumberOfPoints();++i) addTaskToList(i);
  }
}

unsigned HistogramBase::getNumberOfDerivatives() const {
  return arg_ends.size()-heights_index;
}

void HistogramBase::getGridPointIndicesAndCoordinates( const unsigned& ind, std::vector<unsigned>& indices, std::vector<double>& coords ) const {
  gridobject.getGridPointCoordinates( ind, indices, coords );
}

void HistogramBase::getGridPointAsCoordinate( const unsigned& ind, const bool& setlength, std::vector<double>& coords ) const {
  if( setlength ) gridobject.putCoordinateAtValue( ind, getPntrToOutput(0)->get(ind), coords );
  else  gridobject.putCoordinateAtValue( ind, 1.0, coords );
}

void HistogramBase::calculate(){
  // Everything is done elsewhere
  if( actionInChain() ) return;
  // This is done if we are calculating a function of multiple cvs
  runAllTasks(); 
}

void HistogramBase::buildCurrentTaskList( std::vector<unsigned>& tflags ) {
  if( !one_kernel_at_a_time ){ tflags.assign(tflags.size(),1); norm = static_cast<double>( tflags.size() ); }
  else {
     std::vector<double> args( getNumberOfDerivatives() ); 
     double height=1.0; if( heights_index==2 ) height = getPntrToArgument(arg_ends[args.size()])->get(); 
     for(unsigned i=0;i<args.size();++i) args[i]=getPntrToArgument(i)->get();
     buildSingleKernel( tflags, height, args );
  }
}

void HistogramBase::performTask( const unsigned& current, MultiValue& myvals ) const {
  if( one_kernel_at_a_time ) { 
      std::vector<double> args( getNumberOfDerivatives() ), der( getNumberOfDerivatives() ); 
      unsigned valout = getPntrToOutput(0)->getPositionInStream(); 
      gridobject.getGridPointCoordinates( current, args ); double vv = calculateValueOfSingleKernel( args, der ); 
      myvals.setValue( valout, vv ); 
      for(unsigned i=0;i<der.size();++i){ myvals.addDerivative( valout, i, der[i] ); myvals.updateIndex( valout, i ); }
  }
}

void HistogramBase::gatherGridAccumulators( const unsigned& code, const MultiValue& myvals,
                                            const unsigned& bufstart, std::vector<double>& buffer ) const {
  if( one_kernel_at_a_time ) {
      unsigned istart = bufstart + (1+getNumberOfDerivatives())*code;
      unsigned valout = getPntrToOutput(0)->getPositionInStream(); buffer[istart] += myvals.get( valout );
      for(unsigned i=0;i<getNumberOfDerivatives();++i) buffer[istart+1+i] += myvals.getDerivative( valout, i );
      return;
  }
  std::vector<double> argsh( arg_ends.size()-1 ), args( getNumberOfDerivatives() );
  if( getPntrToArgument(0)->getRank()==2 ) {
      unsigned matind=getPntrToArgument(0)->getPositionInMatrixStash();
#ifdef DNDEBUG
      for(unsigned k=0;k<args.size();++k){
           unsigned amtind = getPntrToArgument(k)->getPositionInMatrixStash();
           plumed_dbg_assert( myvals.getNumberOfStashedMatrixElements(amtind)==myvals.getNumberOfStashedMatrixElements(matind) ); 
      }
#endif      
      for(unsigned j=0;j<myvals.getNumberOfStashedMatrixElements(matind);++j){
          unsigned jind = myvals.getStashedMatrixIndex(matind,j); 
          for(unsigned k=0;k<args.size();++k){
              unsigned amtind = getPntrToArgument(k)->getPositionInMatrixStash();
              plumed_assert( amtind==myvals.getStashedMatrixIndex(matind,j) );
              args[k] = myvals.getStashedMatrixElement( amtind, jind );
          }
          double height=1.0; 
          if( heights_index==2 ) {
              unsigned amtind = getPntrToArgument( argsh.size()-1 )->getPositionInMatrixStash();
              height = myvals.getStashedMatrixElement( amtind, jind );
          }
          if( !unorm ) height = height / norm;
          addKernelToGrid( height, args, bufstart, buffer );
      }
  } else { 
      retrieveArguments( myvals, argsh ); 
      double height=1.0; if( heights_index==2 ) height = argsh[ argsh.size()-1 ];
      if( !unorm ) height = height / norm;
      for(unsigned i=0;i<args.size();++i) args[i]=argsh[i];
      if( fabs(height)>epsilon ) {
          addKernelToGrid( height, args, bufstart, buffer );
      }
  }
}

void HistogramBase::apply() { }

}
}