/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013-2017 The plumed team
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
#ifndef __PLUMED_symfunc_CoordinationNumbers_h
#define __PLUMED_symfunc_CoordinationNumbers_h

#include "core/ActionShortcut.h"

namespace PLMD {
namespace symfunc {

class CoordinationNumbers : public ActionShortcut {
public:
  static void shortcutKeywords( Keywords& keys );
  static void registerKeywords(Keywords& keys);
  static void expandMatrix( const bool& components, const std::string& lab, const std::string& sp_str,
                            const std::string& spa_str, const std::string& spb_str, ActionShortcut* action );
  explicit CoordinationNumbers(const ActionOptions&);
};

}
}
#endif
