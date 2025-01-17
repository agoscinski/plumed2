/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2014-2017 The plumed team
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
#include "core/ActionWithValue.h"
#include "core/ActionWithArguments.h"
#include "core/ActionRegister.h"
#include "core/PlumedMain.h"
#include "core/ActionSet.h"
#include "core/Atoms.h"
#include "multicolvar/MultiColvarBase.h"
#include "ClusteringBase.h"

//+PLUMEDOC CONCOMP CLUSTER_PROPERTIES
/*
Calculate properties of the distribution of some quantities that are part of a connected component

This collective variable was developed for looking at nucleation phenomena, where you are
interested in using studying the behavior of atoms in small aggregates or nuclei.  In these sorts of
problems you might be interested in the degree the atoms in a nucleus have adopted their crystalline
structure or (in the case of heterogenous nucleation of a solute from a solvent) you might be
interested in how many atoms are present in the largest cluster \cite tribello-clustering.

\par Examples

The input below calculates the coordination numbers of atoms 1-100 and then computes the an adjacency
matrix whose elements measures whether atoms \f$i\f$ and \f$j\f$ are within 0.55 nm of each other.  The action
labelled dfs then treats the elements of this matrix as zero or ones and thus thinks of the matrix as defining
a graph.  This dfs action then finds the largest connected component in this graph.  The sum of the coordination
numbers for the atoms in this largest connected component are then computed and this quantity is output to a colvar
file.  The way this input can be used is described in detail in \cite tribello-clustering.

\plumedfile
lq: COORDINATIONNUMBER SPECIES=1-100 SWITCH={CUBIC D_0=0.45  D_MAX=0.55} LOWMEM
cm: CONTACT_MATRIX ATOMS=lq  SWITCH={CUBIC D_0=0.45  D_MAX=0.55}
dfs: DFSCLUSTERING MATRIX=cm
clust1: CLUSTER_PROPERTIES CLUSTERS=dfs CLUSTER=1 SUM
PRINT ARG=clust1.* FILE=colvar
\endplumedfile

*/
//+ENDPLUMEDOC

namespace PLMD {
namespace clusters {

class ClusterWeights :
  public ActionWithArguments,
  public ActionWithValue {
private:
/// The cluster we are looking for
  unsigned clustr;
/// The forces
  std::vector<double> forcesToApply;
public:
/// Create manual
  static void registerKeywords( Keywords& keys );
/// Constructor
  explicit ClusterWeights(const ActionOptions&);
/// The number of derivatives
  unsigned getNumberOfDerivatives() const ;
/// Work out what needs to be done in this action
  void buildCurrentTaskList( bool& forceAllTasks, std::vector<std::string>& actionsThatSelectTasks, std::vector<unsigned>& tflags );
/// Do the calculation
  void calculate();
/// We can use ActionWithVessel to run all the calculation
  void performTask( const unsigned&, MultiValue& ) const ;
  void apply() {}
};

PLUMED_REGISTER_ACTION(ClusterWeights,"CLUSTER_WEIGHTS")

void ClusterWeights::registerKeywords( Keywords& keys ) {
  Action::registerKeywords( keys );
  ActionWithArguments::registerKeywords( keys ); keys.remove("ARG");
  ActionWithValue::registerKeywords( keys ); keys.remove("NUMERICAL_DERIVATIVES");
  keys.add("compulsory","CLUSTERS","the label of the action that does the clustering");
  keys.add("compulsory","CLUSTER","1","which cluster would you like to look at 1 is the largest cluster, 2 is the second largest, 3 is the the third largest and so on.");
  // keys.add("hidden","FROM_PROPERTIES","indicates that this is created from CLUSTER_PROPERTIES shortcut");
}

ClusterWeights::ClusterWeights(const ActionOptions&ao):
  Action(ao),
  ActionWithArguments(ao),
  ActionWithValue(ao)
{
  // Read in the clustering object
  std::vector<Value*> clusters; parseArgumentList("CLUSTERS",clusters);
  if( clusters.size()!=1 ) error("should pass only one matrix to clustering base");
  ClusteringBase* cc = dynamic_cast<ClusteringBase*>( clusters[0]->getPntrToAction() );
  if( !cc ) error("input to CLUSTERS keyword should be a clustering action");
  // Request the arguments
  requestArguments( clusters, false );
  // Now create the value
  std::vector<unsigned> shape(1); shape[0]=clusters[0]->getShape()[0];
  addValue( shape ); setNotPeriodic(); getPntrToOutput(0)->alwaysStoreValues();
  // And the tasks
  for(unsigned i=0; i<shape[0]; ++i) addTaskToList(i);
  // Find out which cluster we want
  parse("CLUSTER",clustr);
  if( clustr<1 ) error("cannot look for a cluster larger than the largest cluster");
  if( clustr>clusters[0]->getShape()[0] ) error("cluster selected is invalid - too few atoms in system");
  log.printf("  atoms in %dth largest cluster calculated by %s are equal to one \n",clustr, cc->getLabel().c_str() );
}

void ClusterWeights::buildCurrentTaskList( bool& forceAllTasks, std::vector<std::string>& actionsThatSelectTasks, std::vector<unsigned>& tflags ) {
  plumed_assert( getPntrToArgument(0)->valueHasBeenSet() ); actionsThatSelectTasks.push_back( getLabel() );
  for(unsigned i=0; i<getPntrToArgument(0)->getShape()[0]; ++i) {
    if( fabs(getPntrToArgument(0)->get(i)-clustr)<epsilon ) tflags[i]=1;
  }
}

unsigned ClusterWeights::getNumberOfDerivatives() const {
  return 0;
}

void ClusterWeights::calculate() {
  runAllTasks();
}

void ClusterWeights::performTask( const unsigned& current, MultiValue& myvals ) const {
  myvals.addValue( getPntrToOutput(0)->getPositionInStream(), 1.0 );
}

}
}
