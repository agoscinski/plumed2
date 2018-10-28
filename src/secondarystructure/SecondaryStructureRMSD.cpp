/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013-2018 The plumed team
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
#include "SecondaryStructureRMSD.h"
#include "core/PlumedMain.h"
#include "core/ActionSet.h"
#include "core/SetupMolInfo.h"
#include "core/Atoms.h"
#include "reference/MetricRegister.h"
#include "reference/SingleDomainRMSD.h"
#include "tools/SwitchingFunction.h"

namespace PLMD {
namespace secondarystructure {

void SecondaryStructureRMSD::shortcutKeywords( Keywords& keys ) {
  keys.add("compulsory","LESS_THAN","calculate the number of a residue segments that are within a certain target distance of this secondary structure type. "
           "This quantity is calculated using \\f$\\sum_i \\sigma(s_i)\\f$, where \\f$\\sigma(s)\\f$ is a \\ref switchingfunction.");
  keys.add("compulsory","R_0","The r_0 parameter of the switching function.");
  keys.add("compulsory","D_0","0.0","The d_0 parameter of the switching function");
  keys.add("compulsory","NN","8","The n parameter of the switching function");
  keys.add("compulsory","MM","12","The m parameter of the switching function");
}

void SecondaryStructureRMSD::expandShortcut( const std::string& lab, const std::vector<std::string>& words,
    const std::map<std::string,std::string>& keys,
    std::vector<std::vector<std::string> >& actions ) {
  std::vector<std::string> lt_line; lt_line.push_back( lab + "_lt:" );
  lt_line.push_back("LESS_THAN"); lt_line.push_back( "ARG1=" + lab );
  if( keys.count("LESS_THAN") ) {
    lt_line.push_back("SWITCH=" + keys.find("LESS_THAN")->second );
  } else {
    for(const auto & p : keys ) lt_line.push_back( p.first + "=" + p.second );
  }
  actions.push_back( lt_line );
  std::vector<std::string> sum_line; sum_line.push_back( lab + "_lessthan:" );
  sum_line.push_back("COMBINE"); sum_line.push_back("ARG=" + lab + "_lt" );
  sum_line.push_back("PERIODIC=NO"); actions.push_back( sum_line );
}

void SecondaryStructureRMSD::registerKeywords( Keywords& keys ) {
  Action::registerKeywords( keys );
  ActionWithValue::registerKeywords( keys );
  ActionAtomistic::registerKeywords( keys );
  keys.add("residues","RESIDUES","this command is used to specify the set of residues that could conceivably form part of the secondary structure. "
           "It is possible to use residues numbers as the various chains and residues should have been identified else using an instance of the "
           "\\ref MOLINFO action. If you wish to use all the residues from all the chains in your system you can do so by "
           "specifying all. Alternatively, if you wish to use a subset of the residues you can specify the particular residues "
           "you are interested in as a list of numbers. Please be aware that to form secondary structure elements your chain "
           "must contain at least N residues, where N is dependent on the particular secondary structure you are interested in. "
           "As such if you define portions of the chain with fewer than N residues the code will crash.");
  keys.add("compulsory","TYPE","DRMSD","the manner in which RMSD alignment is performed. Should be OPTIMAL, SIMPLE or DRMSD. "
           "For more details on the OPTIMAL and SIMPLE methods see \\ref RMSD. For more details on the "
           "DRMSD method see \\ref DRMSD.");
  keys.reserve("optional","STRANDS_CUTOFF","If in a segment of protein the two strands are further apart then the calculation "
               "of the actual RMSD is skipped as the structure is very far from being beta-sheet like. "
               "This keyword speeds up the calculation enormously when you are using the LESS_THAN option. "
               "However, if you are using some other option, then this cannot be used");
  keys.addFlag("VERBOSE",false,"write a more detailed output");
}

SecondaryStructureRMSD::SecondaryStructureRMSD(const ActionOptions&ao):
  Action(ao),
  ActionAtomistic(ao),
  ActionWithValue(ao),
  nopbc(false),
  align_strands(false),
  s_cutoff2(0),
  align_atom_1(0),
  align_atom_2(0)
{
  parse("TYPE",alignType);
  log.printf("  distances from secondary structure elements are calculated using %s algorithm\n",alignType.c_str() );
  log<<"  Bibliography "<<plumed.cite("Pietrucci and Laio, J. Chem. Theory Comput. 5, 2197 (2009)"); log<<"\n";

  parseFlag("VERBOSE",verbose_output);

  if( keywords.exists("STRANDS_CUTOFF") ) {
    double s_cutoff = 0;
    parse("STRANDS_CUTOFF",s_cutoff); align_strands=true;
    if( s_cutoff>0) log.printf("  ignoring contributions from strands that are more than %f apart\n",s_cutoff);
    s_cutoff2=s_cutoff*s_cutoff;
  }
}

SecondaryStructureRMSD::~SecondaryStructureRMSD() {
// destructor needed to delete forward declarated objects
}

void SecondaryStructureRMSD::setAtomsFromStrands( const unsigned& atom1, const unsigned& atom2 ) {
  align_atom_1=atom1; align_atom_2=atom2;
}

void SecondaryStructureRMSD::readBackboneAtoms( const std::string& moltype, std::vector<unsigned>& chain_lengths ) {
  std::vector<SetupMolInfo*> moldat=plumed.getActionSet().select<SetupMolInfo*>();
  if( moldat.size()==0 ) error("Unable to find MOLINFO in input");

  std::vector<std::string> resstrings; parseVector( "RESIDUES", resstrings );
  if( !verbose_output ) {
    if(resstrings.size()==0) error("residues are not defined, check the keyword RESIDUES");
    else if(resstrings[0]=="all") {
      log.printf("  examining all possible secondary structure combinations\n");
    } else {
      log.printf("  examining secondary structure in residue positions : %s ",resstrings[0].c_str() );
      for(unsigned i=1; i<resstrings.size(); ++i) log.printf(", %s",resstrings[i].c_str() );
      log.printf("\n");
    }
  }
  std::vector< std::vector<AtomNumber> > backatoms;
  moldat[0]->getBackbone( resstrings, moltype, backatoms );

  chain_lengths.resize( backatoms.size() );
  for(unsigned i=0; i<backatoms.size(); ++i) {
    chain_lengths[i]=backatoms[i].size();
    for(unsigned j=0; j<backatoms[i].size(); ++j) all_atoms.push_back( backatoms[i][j] );
  }
  ActionAtomistic::requestAtoms( all_atoms );
  forcesToApply.resize( getNumberOfDerivatives() );
}

void SecondaryStructureRMSD::addColvar( const std::vector<unsigned>& newatoms ) {
  if( colvar_atoms.size()>0 ) plumed_assert( colvar_atoms[0].size()==newatoms.size() );
  if( verbose_output ) {
    log.printf("  Secondary structure segment %u contains atoms : ", static_cast<unsigned>(colvar_atoms.size()+1));
    for(unsigned i=0; i<newatoms.size(); ++i) log.printf("%d ",all_atoms[newatoms[i]].serial() );
    log.printf("\n");
  }
  addTaskToList( colvar_atoms.size() );
  colvar_atoms.push_back( newatoms );
}

void SecondaryStructureRMSD::setSecondaryStructure( std::vector<Vector>& structure, double bondlength, double units ) {
  // If we are in natural units get conversion factor from nm into natural length units
  if( plumed.getAtoms().usingNaturalUnits() ) {
    error("cannot use this collective variable when using natural units");
  }
  plumed_massert( !(align_strands && align_atom_1==0 && align_atom_2==0), "you must use setAtomsFromStrands with strands cutoff");

  // Convert into correct units
  for(unsigned i=0; i<structure.size(); ++i) {
    structure[i][0]*=units; structure[i][1]*=units; structure[i][2]*=units;
  }

  // Set the reference structure
  references.emplace_back( metricRegister().create<SingleDomainRMSD>( alignType ) );
  unsigned nn=references.size()-1;
  std::vector<double> align( structure.size(), 1.0 ), displace( structure.size(), 1.0 );
  references[nn]->setBoundsOnDistances( true, bondlength );   // We always use pbc
  references[nn]->setReferenceAtoms( structure, align, displace );
}

void SecondaryStructureRMSD::setupValues() {
  plumed_assert( references.size()>0 );
  std::vector<unsigned> shape(1); shape[0]=getFullNumberOfTasks();
  if( references.size()==1 ) { addValue( shape ); setNotPeriodic(); }
  else {
    std::string num;
    for(unsigned i=0; i<references.size(); ++i) {
      Tools::convert( i+1, num ); addComponent( "struct-" + num, shape );
      componentIsNotPeriodic( "struct-" + num );
    }
  }
}

void SecondaryStructureRMSD::buildCurrentTaskList( bool& forceAllTasks, std::vector<std::string>& actionsThatSelectTasks, std::vector<unsigned>& tflags ) {
  if( s_cutoff2>0 ) {
    actionsThatSelectTasks.push_back( getLabel() ); 
    for(unsigned i=0; i<tflags.size(); ++i) {
      Vector distance=pbcDistance( ActionAtomistic::getPosition( getAtomIndex(i,align_atom_1) ),
                                   ActionAtomistic::getPosition( getAtomIndex(i,align_atom_2) ) );
      if( distance.modulo2()<s_cutoff2 ) tflags[i]=1;
    }
  } 
}

void SecondaryStructureRMSD::calculate() {
  runAllTasks();
}

void SecondaryStructureRMSD::performTask( const unsigned& current, MultiValue& myvals ) const {
  // Resize the derivatives if need be
  unsigned nderi = 3*getNumberOfAtoms()+9;
  if( myvals.getNumberOfDerivatives()!=nderi ) myvals.resize( myvals.getNumberOfValues(), nderi, 0, 0 );
  // Retrieve the positions
  std::vector<Vector> pos( references[0]->getNumberOfAtoms() );
  const unsigned n=pos.size();
  for(unsigned i=0; i<n; ++i) pos[i]=ActionAtomistic::getPosition( getAtomIndex(current,i) );

  // This aligns the two strands if this is required
  Vector distance=pbcDistance( pos[align_atom_1],pos[align_atom_2] );
  if( alignType!="DRMSD" && align_strands ) {
    Vector origin_old, origin_new; origin_old=pos[align_atom_2];
    origin_new=pos[align_atom_1]+distance;
    for(unsigned i=15; i<30; ++i) {
      pos[i]+=( origin_new - origin_old );
    }
  } else if( alignType!="DRMSD" && !nopbc ) {
    for(unsigned i=0; i<n-1; ++i) {
      const Vector & first (pos[i]);
      Vector & second (pos[i+1]);
      second=first+pbcDistance(first,second);
    }
  }
  // Create a holder for the derivatives
  ReferenceValuePack mypack( 0, pos.size(), myvals ); // mypack.setValIndex( 0 );
  for(unsigned i=0; i<n; ++i) mypack.setAtomIndex( i, getAtomIndex(current,i) );

  // And now calculate the RMSD
  const Pbc& pbc=getPbc(); const unsigned rs = references.size();
  for(unsigned i=0; i<rs; ++i) {
    mypack.setValIndex( i );
    double nr=references[i]->calculate( pos, pbc, mypack, false );
    myvals.setValue( i, nr );

    if( !doNotCalculateDerivatives() && !mypack.virialWasSet() ) {
      Tensor vir; const unsigned cacs = colvar_atoms[current].size();
      for(unsigned i=0; i<cacs; ++i)  vir+=(-1.0*Tensor( pos[i], mypack.getAtomDerivative(i) ));
      mypack.addBoxDerivatives( vir );
    }
  }
  return;
}

void SecondaryStructureRMSD::apply() {
  if( doNotCalculateDerivatives() ) return;
  std::fill(forcesToApply.begin(),forcesToApply.end(),0); unsigned mm=0;
  if( getForcesFromValues( forcesToApply ) ) setForcesOnAtoms( forcesToApply, mm );
}

}
}
