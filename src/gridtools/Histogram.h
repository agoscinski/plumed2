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
#ifndef __PLUMED_gridtools_Histogram_h
#define __PLUMED_gridtools_Histogram_h

#include "core/ActionShortcut.h"

namespace PLMD {
namespace gridtools {

template<class T>
class Histogram : public ActionShortcut {
public:
  static void registerKeywords( Keywords& keys );
  explicit Histogram(const ActionOptions&ao);
};

class HistogramTools {
public:
  static void histogramKeywords( Keywords& keys );
  static void readHistogramKeywords( std::map<std::string,std::string>& keymap, ActionShortcut* action );
  static void createAveragingObject( const std::string& ilab, const std::string& olab,
                                     const std::map<std::string,std::string>& keymap, ActionShortcut* action );
  static void convertBandwiths( const std::string& lab, const std::vector<std::string>& bwidths, ActionShortcut* action );
};

}
}
#endif
