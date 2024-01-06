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
#ifndef __PLUMED_matrixtools_FunctionOfMatrix_h
#define __PLUMED_matrixtools_FunctionOfMatrix_h

#include "function/FunctionOfVector.h"
#include "function/Sum.h"
#include "adjmat/MatrixProductBase.h"
#include "tools/Matrix.h"

namespace PLMD {
namespace matrixtools {

template <class T>
class FunctionOfMatrix : public adjmat::MatrixProductBase {
private:
/// The function that is being computed
  T myfunc;
/// The number of derivatives for this action
  unsigned nderivatives;
/// The list of actiosn in this chain
  std::vector<std::string> actionsLabelsInChain;
public:
  static void registerKeywords(Keywords&);
  explicit FunctionOfMatrix(const ActionOptions&);
/// Get the shape of the output matrix
  std::vector<unsigned> getValueShapeFromArguments() override ;
/// Get the label to write in the graph
  std::string writeInGraph() const override { return myfunc.getGraphInfo( getName() ); }
/// Make sure the derivatives are turned on
  void turnOnDerivatives() override;
/// Get the number of derivatives for this action
  unsigned getNumberOfDerivatives() const override ;
/// This gets the number of columns
  unsigned getNumberOfColumns() const override ;
/// This checks for tasks in the parent class
  void buildTaskListFromArgumentRequests( const unsigned& ntasks, bool& reduce, std::set<AtomNumber>& otasks ) override ;
/// This is not used
  double computeVectorProduct( const unsigned& index1, const unsigned& index2,
                               const std::vector<double>& vec1, const std::vector<double>& vec2,
                               std::vector<double>& dvec1, std::vector<double>& dvec2, MultiValue& myvals ) const { plumed_error(); }
/// Calculate the full matrix
  bool performTask( const std::string& controller, const unsigned& index1, const unsigned& index2, MultiValue& myvals ) const override ;
/// This updates the indices for the matrix
  void updateCentralMatrixIndex( const unsigned& ind, const std::vector<unsigned>& indices, MultiValue& myvals ) const override ;
};

template <class T>
void FunctionOfMatrix<T>::registerKeywords(Keywords& keys ) {
  Action::registerKeywords(keys); ActionWithValue::registerKeywords(keys); ActionWithArguments::registerKeywords(keys); keys.use("ARG");
  keys.add("hidden","NO_ACTION_LOG","suppresses printing from action on the log");
  keys.reserve("compulsory","PERIODIC","if the output of your function is periodic then you should specify the periodicity of the function.  If the output is not periodic you must state this using PERIODIC=NO");
  T tfunc; tfunc.registerKeywords( keys );
}

template <class T>
FunctionOfMatrix<T>::FunctionOfMatrix(const ActionOptions&ao):
Action(ao),
MatrixProductBase(ao),
nderivatives(getNumberOfScalarArguments())
{
  if( myfunc.getArgStart()>0 ) error("this has not beeen implemented -- if you are interested email gareth.tribello@gmail.com");
  // Get the shape of the output
  std::vector<unsigned> shape( getValueShapeFromArguments() );
  // Check if the output matrix is symmetric
  bool symmetric=true; unsigned argstart=myfunc.getArgStart();
  for(unsigned i=argstart;i<getNumberOfArguments();++i) {
      if( getPntrToArgument(i)->getRank()==2 ) {
          if( !getPntrToArgument(i)->isSymmetric() ){ symmetric=false;  }
      }                        
  }
  // Read the input and do some checks
  myfunc.read( this ); unsigned ntasks = shape[0];
  // Check we are not calculating a sum
  if( myfunc.zeroRank() ) shape.resize(0);
  // Get the names of the components
  std::vector<std::string> components( keywords.getAllOutputComponents() );
  // Create the values to hold the output
  std::vector<std::string> str_ind( myfunc.getComponentsPerLabel() );
  if( components.size()==0 && myfunc.zeroRank() && str_ind.size()==0 ) {
      addValueWithDerivatives( shape ); getPntrToOutput(0)->setNumberOfTasks( ntasks );
  } else if( components.size()==0 && myfunc.zeroRank() ) {
     for(unsigned j=0;j<str_ind.size();++j) {
         addComponentWithDerivatives( str_ind[j], shape ); getPntrToOutput(j)->setNumberOfTasks( ntasks );
     }
  } else if( components.size()==0 && str_ind.size()==0 ) { 
     addValue( shape ); getPntrToOutput(0)->setSymmetric( symmetric );
  } else if ( components.size()==0 ) {
    for(unsigned j=0;j<str_ind.size();++j) { addComponent( str_ind[j], shape ); getPntrToOutput(j)->setSymmetric( symmetric ); }
  } else {
    for(unsigned i=0;i<components.size();++i) {
        if( str_ind.size()>0 ) {
            for(unsigned j=0;j<str_ind.size();++j) { addComponent( components[i] + str_ind[j], shape ); getPntrToOutput(i*str_ind.size()+j)->setSymmetric( symmetric ); }
        } else if( components[i].find_first_of("_")!=std::string::npos ) {
            if( getNumberOfArguments()-argstart==1 ) { addValue( shape ); getPntrToOutput(0)->setSymmetric( symmetric ); }
            else { 
              for(unsigned j=argstart; j<getNumberOfArguments(); ++j) {
                 addComponent( getPntrToArgument(j)->getName() + components[i], shape );
                 getPntrToOutput(i*(getNumberOfArguments()-argstart)+j-argstart)->setSymmetric( symmetric );
              } 
            }
        } else { addComponent( components[i], shape ); getPntrToOutput(i)->setSymmetric( symmetric ); } 
    }
  }
  // Check if this is a timeseries
  for(unsigned i=argstart; i<getNumberOfArguments();++i) {
    if( getPntrToArgument(i)->isTimeSeries() ) { 
        for(unsigned i=0; i<getNumberOfComponents(); ++i) getPntrToOutput(i)->makeHistoryDependent(); 
        break;
    }
  }
  // Check if this can be sped up
  if( myfunc.getDerivativeZeroIfValueIsZero() )  {
      for(unsigned i=0; i<getNumberOfComponents(); ++i) getPntrToComponent(i)->setDerivativeIsZeroWhenValueIsZero();
  }
  // Set the periodicities of the output components
  myfunc.setPeriodicityForOutputs( this ); bool doNotChain=false;
  // We can't do this with if we are dividing a stack by some a product v.v^T product as we need to store the vector
  // In order to do this type of calculation.  There should be a neater fix than this but I can't see it.
  for(unsigned i=argstart; i<getNumberOfArguments();++i) {
      if( (getPntrToArgument(i)->getPntrToAction())->getName()=="VSTACK" ) { doNotChain=true; break; }
      if( getPntrToArgument(i)->getRank()==0 ) {
          function::FunctionOfVector<function::Sum>* as = dynamic_cast<function::FunctionOfVector<function::Sum>*>( getPntrToArgument(i)->getPntrToAction() );
          if(as) doNotChain=true;
      }  
  }
  // Now setup the action in the chain if we can
  if( !doNotChain && distinct_arguments.size()>0 ) nderivatives = setupActionInChain(0); 
  else { for(unsigned i=argstart; i<getNumberOfArguments(); ++i) getPntrToArgument(i)->buildDataStore( getLabel() ); }
}

template <class T>
void FunctionOfMatrix<T>::turnOnDerivatives() {
  if( !myfunc.derivativesImplemented() ) error("derivatives have not been implemended for " + getName() );
  ActionWithValue::turnOnDerivatives(); myfunc.setup(this); 
}

template <class T>
unsigned FunctionOfMatrix<T>::getNumberOfDerivatives() const {
  return nderivatives;
}

template <class T>
unsigned FunctionOfMatrix<T>::getNumberOfColumns() const {
  if( getPntrToOutput(0)->getRank()==2 ) {
     unsigned argstart=myfunc.getArgStart();
     for(unsigned i=argstart;i<getNumberOfArguments();++i) {
         if( getPntrToArgument(i)->getRank()==2 ) return getPntrToArgument(i)->getNumberOfColumns();
     }
  }
  plumed_error(); return 0;
}

template <class T>
void FunctionOfMatrix<T>::buildTaskListFromArgumentRequests( const unsigned& ntasks, bool& reduce, std::set<AtomNumber>& otasks ) {
  // Check if this is the first element in a chain
  if( actionInChain() ) return;
  // If it is computed outside a chain get the tassks the daughter chain needs
  propegateTaskListsForValue( 0, ntasks, reduce, otasks );
}

template <class T>
bool FunctionOfMatrix<T>::performTask( const std::string& controller, const unsigned& index1, const unsigned& index2, MultiValue& myvals ) const {
  unsigned argstart=myfunc.getArgStart(); std::vector<double> args( getNumberOfArguments() - argstart ); 
  unsigned ind2 = index2; 
  if( getPntrToOutput(0)->getRank()==2 && index2>=getPntrToOutput(0)->getShape()[0] ) ind2 = index2 - getPntrToOutput(0)->getShape()[0];
  else if( index2>=getPntrToArgument(0)->getShape()[0] ) ind2 = index2 - getPntrToArgument(0)->getShape()[0];
  if( actionInChain() ) {
      for(unsigned i=argstart;i<getNumberOfArguments();++i) {
          if( getPntrToArgument(i)->getRank()==0 ) args[i-argstart] = getPntrToArgument(i)->get(); 
          else if( !getPntrToArgument(i)->valueHasBeenSet() ) args[i-argstart] = myvals.get( getPntrToArgument(i)->getPositionInStream() ); 
          else args[i-argstart] = getPntrToArgument(i)->get( getPntrToArgument(i)->getShape()[1]*index1 + ind2 );
      }
  } else {
      for(unsigned i=argstart;i<getNumberOfArguments();++i) {
          if( getPntrToArgument(i)->getRank()==2 ) args[i-argstart]=getPntrToArgument(i)->get( getPntrToArgument(i)->getShape()[1]*index1 + ind2 );
          else args[i-argstart] = getPntrToArgument(i)->get();
      }
  }
  // Calculate the function and its derivatives
  std::vector<double> vals( getNumberOfComponents() ); Matrix<double> derivatives( getNumberOfComponents(), getNumberOfArguments()-argstart );
  myfunc.calc( this, args, vals, derivatives );
  // And set the values
  for(unsigned i=0;i<vals.size();++i) myvals.addValue( getPntrToOutput(i)->getPositionInStream(), vals[i] );
  // Return if we are not computing derivatives
  if( doNotCalculateDerivatives() ) return true;

  if( actionInChain() ) {
      for(unsigned i=0;i<getNumberOfComponents();++i) {
          unsigned ostrn=getPntrToOutput(i)->getPositionInStream();
          for(unsigned j=argstart;j<getNumberOfArguments();++j) {
              if( getPntrToArgument(j)->getRank()==2 ) {
                  unsigned istrn = getArgumentPositionInStream( j, myvals ); 
                  for(unsigned k=0; k<myvals.getNumberActive(istrn); ++k) {
                      unsigned kind=myvals.getActiveIndex(istrn,k);
                      myvals.addDerivative( ostrn, arg_deriv_starts[j] + kind, derivatives(i,j)*myvals.getDerivative( istrn, kind ) );
                  }
              }  
          }
      }
      // If we are computing a matrix we need to update the indices here so that derivatives are calcualted correctly in functions of these
      if( getPntrToOutput(0)->getRank()==2 ) { 
          for(unsigned i=0;i<getNumberOfComponents();++i) {
              unsigned ostrn=getPntrToOutput(i)->getPositionInStream();
              for(unsigned j=argstart;j<getNumberOfArguments();++j) {
                  if( getPntrToArgument(j)->getRank()==0 ) continue ;
                  // Ensure we only store one lot of derivative indices
                  bool found=false;
                  for(unsigned k=0; k<j; ++k) {
                      if( arg_deriv_starts[k]==arg_deriv_starts[j] ) { found=true; break; }
                  }
                  if( found ) continue;
                  unsigned istrn = getPntrToArgument(j)->getPositionInStream();
                  for(unsigned k=0; k<myvals.getNumberActive(istrn); ++k) {
                      unsigned kind=myvals.getActiveIndex(istrn,k); 
                      myvals.updateIndex( ostrn, arg_deriv_starts[j] + kind );
                  }
              }
          }
      }
  } else {
      unsigned base=0; ind2 = index2;
      if( index2>=getPntrToOutput(0)->getShape()[0] ) ind2 = index2 - getPntrToOutput(0)->getShape()[0];
      for(unsigned j=argstart;j<getNumberOfArguments();++j) {
          if( getPntrToArgument(j)->getRank()==2 ) {
              for(unsigned i=0;i<getNumberOfComponents();++i) {
                  unsigned ostrn=getPntrToOutput(i)->getPositionInStream();
                  unsigned myind = base + getPntrToOutput(i)->getShape()[1]*index1 + ind2;
                  myvals.addDerivative( ostrn, myind, derivatives(i,j) );
                  myvals.updateIndex( ostrn, myind ); 
              }
          } else {
              for(unsigned i=0;i<getNumberOfComponents();++i) {
                  unsigned ostrn=getPntrToOutput(i)->getPositionInStream();
                  myvals.addDerivative( ostrn, base, derivatives(i,j) );
                  myvals.updateIndex( ostrn, base );
              }  
          }
          base += getPntrToArgument(j)->getNumberOfValues();
      }
  }
  return true;
}

template <class T>
void FunctionOfMatrix<T>::updateCentralMatrixIndex( const unsigned& ind, const std::vector<unsigned>& indices, MultiValue& myvals ) const {
  unsigned argstart=myfunc.getArgStart();
  if( actionInChain() && getPntrToOutput(0)->getRank()==2 ) {
      // This is triggered if we are outputting a matrix
      for(unsigned vv=0; vv<getNumberOfComponents(); ++vv) {
          unsigned nmat = getPntrToOutput(vv)->getPositionInMatrixStash();
          std::vector<unsigned>& mat_indices( myvals.getMatrixIndices( nmat ) ); unsigned ntot_mat=0;
          if( mat_indices.size()<getNumberOfDerivatives() ) mat_indices.resize( getNumberOfDerivatives() );
          for(unsigned i=argstart; i<getNumberOfArguments(); ++i) {
            if( getPntrToArgument(i)->getRank()==0 ) continue ;
            // Ensure we only store one lot of derivative indices
            bool found=false;
            for(unsigned j=0; j<i; ++j) {
                if( arg_deriv_starts[j]==arg_deriv_starts[i] ) { found=true; break; }
            } 
            if( found ) continue;
            unsigned istrn = getPntrToArgument(i)->getPositionInMatrixStash();
            std::vector<unsigned>& imat_indices( myvals.getMatrixIndices( istrn ) );
            for(unsigned k=0; k<myvals.getNumberOfMatrixIndices( istrn ); ++k) mat_indices[ntot_mat + k] = arg_deriv_starts[i] + imat_indices[k];
            ntot_mat += myvals.getNumberOfMatrixIndices( istrn ); 
          }
          myvals.setNumberOfMatrixIndices( nmat, ntot_mat );
      }
  } else if( actionInChain() ) {
      // This is triggered if we are calculating a single scalar in the function
      for(unsigned i=argstart; i<getNumberOfArguments(); ++i) {
          bool found=false;
          for(unsigned j=0; j<i; ++j) {
              if( arg_deriv_starts[j]==arg_deriv_starts[i] ) { found=true; break; }
          } 
          if( found ) continue; 
          unsigned istrn = getPntrToArgument(i)->getPositionInMatrixStash();
          std::vector<unsigned>& mat_indices( myvals.getMatrixIndices( istrn ) );  
          for(unsigned k=0; k<myvals.getNumberOfMatrixIndices( istrn ); ++k) {
              for(unsigned j=0; j<getNumberOfComponents(); ++j) {
                  unsigned ostrn = getPntrToOutput(j)->getPositionInStream();
                   myvals.updateIndex( ostrn, arg_deriv_starts[i] + mat_indices[k] );
              }
          }
      }
  } else if( getPntrToOutput(0)->getRank()==2 ) {
      for(unsigned vv=0; vv<getNumberOfComponents(); ++vv) {
          unsigned nmat = getPntrToOutput(vv)->getPositionInMatrixStash();
          std::vector<unsigned>& mat_indices( myvals.getMatrixIndices( nmat ) ); unsigned ntot_mat=0;
          if( mat_indices.size()<getNumberOfDerivatives() ) mat_indices.resize( getNumberOfDerivatives() );
          for(unsigned i=argstart; i<getNumberOfArguments(); ++i) {
            if( getPntrToArgument(i)->getRank()==0 ) continue ;
            unsigned ss = getPntrToArgument(i)->getShape()[1]; unsigned tbase = ss*myvals.getTaskIndex();
            for(unsigned k=0; k<ss; ++k) mat_indices[ntot_mat + k] = tbase + k;
            ntot_mat += ss;
          }
          myvals.setNumberOfMatrixIndices( nmat, ntot_mat );
      }
  } 
}

template <class T>
std::vector<unsigned> FunctionOfMatrix<T>::getValueShapeFromArguments() { 
  unsigned argstart=myfunc.getArgStart(); std::vector<unsigned> shape(2); shape[0]=shape[1]=0;
  for(unsigned i=argstart; i<getNumberOfArguments(); ++i) {
      plumed_assert( getPntrToArgument(i)->getRank()==2 || getPntrToArgument(i)->getRank()==0 );
      if( getPntrToArgument(i)->getRank()==2 ) {
          if( shape[0]>0 && (getPntrToArgument(i)->getShape()[0]!=shape[0] || getPntrToArgument(i)->getShape()[1]!=shape[1]) ) error("all matrices input should have the same shape");
          else if( shape[0]==0 ) { shape[0]=getPntrToArgument(i)->getShape()[0]; shape[1]=getPntrToArgument(i)->getShape()[1]; }
          plumed_assert( !getPntrToArgument(i)->hasDerivatives() );
      }   
  }
  myfunc.setPrefactor( this, 1.0 ); return shape;
}

}
}
#endif
