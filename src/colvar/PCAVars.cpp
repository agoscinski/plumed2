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
#include "core/ActionShortcut.h"
#include "core/ActionRegister.h"
#include "core/PlumedMain.h"
#include "tools/PDB.h"
#include "RMSD.h"

using namespace std;

namespace PLMD {
namespace colvar {

//+PLUMEDOC FUNCTION PCAVARS 
/*

\par Examples

*/
//+ENDPLUMEDOC


class PCAVars : public ActionShortcut {
public:
  static void registerKeywords(Keywords& keys);
  explicit PCAVars(const ActionOptions&);
};


PLUMED_REGISTER_ACTION(PCAVars,"PCAVARS")

void PCAVars::registerKeywords( Keywords& keys ) {
  ActionShortcut::registerKeywords( keys );
  keys.add("compulsory","REFERENCE","a pdb file containing the set of reference configurations");
  keys.add("compulsory","TYPE","OPTIMAL-FAST","the manner in which distances are calculated. More information on the different "
           "metrics that are available in PLUMED can be found in the section of the manual on "
           "\\ref dists");
  keys.addFlag("NOPBC",false,"do not use periodic boundary conditions when computing this quantity");
}

PCAVars::PCAVars( const ActionOptions& ao ):
  Action(ao),
  ActionShortcut(ao)
{
  std::string reference; parse("REFERENCE",reference); 
  // Create the reference object
  RMSD::createReferenceConfiguration( getShortcutLabel() + "_ref", reference, plumed, 1 ); 
  // Create the object that holds the atomic positions by reading the first frame
  FILE* fp=std::fopen(reference.c_str(),"r"); PDB pdb; if(!fp) error("could not open reference file " + reference );
  bool do_read=pdb.readFromFilepointer(fp,false,0.1); if( !do_read ) plumed_merror("missing file " + reference );
  RMSD::createPosVector( getShortcutLabel() + "_pos", pdb, this );
  // And now create the rmsd object
  std::string rmsd_line =  getShortcutLabel() + ": RMSD_CALC DISPLACEMENT SQUARED ARG2=" + getShortcutLabel() + "_ref ARG1=" + getShortcutLabel() + "_pos";
  // Now create the RMSD object
  std::string mtype; parse("TYPE",mtype); readInputLine( rmsd_line + " TYPE=" + mtype );
  // Get the displace stuff
  std::vector<double> displace( pdb.getBeta() ); double dtot = 0;
  for(unsigned i=0;i<displace.size();++i) dtot += displace[i];
  for(unsigned i=0;i<displace.size();++i) displace[i] = displace[i] / dtot;  

  // Now read in the directions and create matheval objects to compute the pca components
  unsigned nfram=1;
  while( do_read ) {
    PDB mypdb; do_read=mypdb.readFromFilepointer(fp,plumed.usingNaturalUnits(),0.1/plumed.getUnits().getLength());
    if( do_read ) {
        std::string num; Tools::convert( nfram, num ); nfram++;
        // Normalize the eigenvector in the input
        double norm=0;
        for(unsigned i=0;i<mypdb.getPositions().size();++i) {
            norm += mypdb.getPositions()[i][0]*mypdb.getPositions()[i][0];
            norm += mypdb.getPositions()[i][1]*mypdb.getPositions()[i][1];
            norm += mypdb.getPositions()[i][2]*mypdb.getPositions()[i][2];
        }
        norm = sqrt( norm ); std::vector<double> normed_coeffs( 3*mypdb.getPositions().size() );
        for(unsigned i=0;i<mypdb.getPositions().size();++i) {
            if( mtype=="SIMPLE" ) {
                normed_coeffs[0*mypdb.getPositions().size()+i] = mypdb.getPositions()[i][0] / norm;
                normed_coeffs[1*mypdb.getPositions().size()+i] = mypdb.getPositions()[i][1] / norm;
                normed_coeffs[2*mypdb.getPositions().size()+i] = mypdb.getPositions()[i][2] / norm;
            } else {
                normed_coeffs[0*mypdb.getPositions().size()+i] = sqrt(displace[i])*mypdb.getPositions()[i][0] / norm;
                normed_coeffs[1*mypdb.getPositions().size()+i] = sqrt(displace[i])*mypdb.getPositions()[i][1] / norm;
                normed_coeffs[2*mypdb.getPositions().size()+i] = sqrt(displace[i])*mypdb.getPositions()[i][2] / norm;
            }
        }
        std::string coeff1, pvec; Tools::convert( normed_coeffs[0], pvec );
        for(unsigned i=1;i<normed_coeffs.size();++i) {
            Tools::convert( normed_coeffs[i], coeff1 );
            pvec += "," + coeff1;
        }
        // Read in eigenvector
        readInputLine( getShortcutLabel() + "_peig-" + num + ": CONSTANT_VALUE VALUES=" + pvec );
        // And calculate dot product
        readInputLine( getShortcutLabel()  + "_eig-" + num + ": DOT DIAGONAL_ELEMENTS_ONLY ARG2=" + getShortcutLabel() + "_peig-" + num + " ARG1=" + getShortcutLabel() + ".disp");
    } else { break; }
  }
  std::fclose(fp);
  std::string resid_inp = getShortcutLabel() + "_residual_2: COMBINE PERIODIC=NO ARG=" + getShortcutLabel() + ".dist";
  for(unsigned i=0;i<nfram-1;++i) {
      std::string num; Tools::convert( i+1, num ); resid_inp += "," + getShortcutLabel() + "_eig-" + num;
  }
  resid_inp += " COEFFICIENTS=1"; for(unsigned i=0;i<nfram-1;++i) resid_inp +=",-1";
  resid_inp += " POWERS=1"; for(unsigned i=0;i<nfram-1;++i) resid_inp += ",2";
  readInputLine( resid_inp );
  readInputLine( getShortcutLabel() + "_residual: MATHEVAL ARG=" + getShortcutLabel() + "_residual_2 FUNC=sqrt(x) PERIODIC=NO");
}

}
}


