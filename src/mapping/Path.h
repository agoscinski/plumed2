/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013-2020 The plumed team
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
#ifndef __PLUMED_mapping_Path_h
#define __PLUMED_mapping_Path_h

#include "core/ActionShortcut.h"

namespace PLMD {
namespace mapping {

class Path : public ActionShortcut {
private:
  static std::string fixArgumentName( const std::string& argin );
public:
  static void registerKeywords(Keywords& keys);
  static void registerInputFileKeywords( Keywords& keys );
  static void readArgumentFromPDB( const std::string& argname, const std::string& lab, const std::string& fname, PlumedMain& plmd, const unsigned number=0 );
  static void readPropertyInformation( const std::vector<std::string>& pnames, const std::string& lab, const std::string& refname, ActionShortcut* action );
  static Value* getValueWithLabel( ActionShortcut* action, const std::string& name );
  static void readInputFrames( const std::vector<std::string>& argnames, std::string& refname, const bool& geometric, 
                               ActionShortcut* action, std::string& refargs, std::string& metric );
  explicit Path(const ActionOptions&);
};

}
}


#endif
