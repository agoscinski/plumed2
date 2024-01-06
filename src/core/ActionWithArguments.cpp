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
#include "ActionWithArguments.h"
#include "ActionWithValue.h"
#include "ActionForInterface.h"
#include "ActionAtomistic.h"
#include "ActionSetup.h"
#include "tools/PDB.h"
#include "PlumedMain.h"
#include "ActionSet.h"
#include "AverageBase.h"
#include "ReweightBase.h"
#include <iostream>
#ifdef __PLUMED_HAS_CREGEX
#include <cstring>
#include <regex.h>
#endif

namespace PLMD {

void ActionWithArguments::registerKeywords(Keywords& keys) {
  keys.reserve("numbered","ARG","the input for this action is the scalar output from one or more other actions. The particular scalars that you will use "
               "are referenced using the label of the action. If the label appears on its own then it is assumed that the Action calculates "
               "a single scalar value.  The value of this scalar is thus used as the input to this new action.  If * or *.* appears the "
               "scalars calculated by all the proceeding actions in the input file are taken.  Some actions have multi-component outputs and "
               "each component of the output has a specific label.  For example a \\ref DISTANCE action labelled dist may have three components "
               "x, y and z.  To take just the x component you should use dist.x, if you wish to take all three components then use dist.*."
               "More information on the referencing of Actions can be found in the section of the manual on the PLUMED \\ref Syntax.  "
               "Scalar values can also be "
               "referenced using POSIX regular expressions as detailed in the section on \\ref Regex. To use this feature you you must compile "
               "PLUMED with the appropriate flag.");
}

void ActionWithArguments::parseArgumentList(const std::string&key,std::vector<Value*>&arg) {
  std::string def; std::vector<std::string> c; arg.clear(); parseVector(key,c);
  if( c.size()==0 && (keywords.style(key,"compulsory") || keywords.style(key,"hidden")) ) {
    if( keywords.getDefaultValue(key,def) ) c.push_back( def );
    else return;
  }
  interpretArgumentList(c,plumed.getActionSet(),this,arg);
}

bool ActionWithArguments::parseArgumentList(const std::string&key,int i,std::vector<Value*>&arg) {
  std::vector<std::string> c;
  arg.clear();
  if(parseNumberedVector(key,i,c)) {
    interpretArgumentList(c,plumed.getActionSet(),this,arg);
    return true;
  } else return false;
}

void ActionWithArguments::interpretArgumentList(const std::vector<std::string>& c, const ActionSet& action_set, Action* action, std::vector<Value*>&arg) {
  for(unsigned i=0; i<c.size(); i++) {
    // Skip this option if this is a refernece configuration that only gives the positions of atoms
    ActionSetup* setup=action_set.selectWithLabel<ActionSetup*>(c[i]);
    if( setup ) { 
        ActionWithValue* avs=dynamic_cast<ActionWithValue*>( setup );
        if( avs->getNumberOfComponents()==0 && action->getName()=="PRINT") continue; 
    }
    // is a regex? then just interpret it. The signal is ()
    if(!c[i].compare(0,1,"(")) {
      unsigned l=c[i].length();
      if(!c[i].compare(l-1,1,")")) {
        // start regex parsing
#ifdef __PLUMED_HAS_CREGEX
        // take the string enclosed in quotes and put in round brackets
        std::string myregex=c[i];
        //log<<"  Evaluating regexp for this action: "<<myregex<<"\n";

        regex_t reg; // regular expression

        int errcode=regcomp(&reg, myregex.c_str(),REG_EXTENDED|REG_NEWLINE); // compile the regular expression
        if(errcode) {
          // one can check the errors asking to regerror
          size_t errbuf_size = regerror(errcode, &reg, NULL, 0);
          std::vector<char> errbuf(errbuf_size);
          regerror(errcode, &reg, errbuf.data(), errbuf_size);
          plumed_error()<<"Error parsing regular expression: "<<errbuf.data();
        }

        // call regfree when reg goes out of scope
        auto deleter=[](regex_t* r) { regfree(r); };
        std::unique_ptr<regex_t,decltype(deleter)> reg_deleter(&reg,deleter);

        plumed_massert(reg.re_nsub==1,"I can parse with only one subexpression");
        regmatch_t match;
        // select all the actions that have a value
        std::vector<ActionWithValue*> all=action_set.select<ActionWithValue*>();
        if( all.empty() ) action->error("your input file is not telling plumed to calculate anything");
        bool found_something=false;
        for(unsigned j=0; j<all.size(); j++) {
          std::vector<std::string> ss=all[j]->getComponentsVector();
          for(unsigned  k=0; k<ss.size(); ++k) {
            unsigned ll=strlen(ss[k].c_str())+1;
            std::vector<char> str(ll);
            strcpy(&str[0],ss[k].c_str());
            const char *ppstr=&str[0];
            if(!regexec(&reg, ppstr, reg.re_nsub, &match, 0)) {
              //log.printf("  Something matched with \"%s\" : ",ss[k].c_str());
              do {
                if (match.rm_so != -1) {	/* The regex is matching part of a string */
                  size_t matchlen = match.rm_eo - match.rm_so;
                  std::vector<char> submatch(matchlen+1);
                  strncpy(submatch.data(), ppstr+match.rm_so, matchlen+1);
                  submatch[matchlen]='\0';
                  //log.printf("  subpattern %s\n", submatch.data());
                  // this is the match: try to see if it is a valid action
                  std::string putativeVal(submatch.data());
                  if( all[j]->exists(putativeVal) ) {
                    all[j]->copyOutput(putativeVal)->use( action, arg );
                    found_something=true;
                    // log.printf("  Action %s added! \n",putativeVal.c_str());
                  }
                }
                ppstr += match.rm_eo;	/* Restart from last match */
              } while(!regexec(&reg,ppstr,reg.re_nsub,&match,0));
            }
          }
        }
        if(!found_something) plumed_error()<<"There isn't any action matching your regex " << myregex;
#else
        plumed_merror("Regexp support not compiled!");
#endif
      } else {
        plumed_merror("did you want to use regexp to input arguments? enclose it between two round braces (...) with no spaces!");
      }
    } else {
      std::size_t dot=c[i].find_first_of('.');
      std::string a=c[i].substr(0,dot);
      std::string name=c[i].substr(dot+1);
      if(c[i].find(".")!=std::string::npos) {   // if it contains a dot:
        if(a=="*" && name=="*") {
          // Take all values from all actions
          std::vector<ActionWithValue*> all=action_set.select<ActionWithValue*>();
          if( all.empty() ) action->error("your input file is not telling plumed to calculate anything");
          for(unsigned j=0; j<all.size(); j++) {
            ActionForInterface* ap=dynamic_cast<ActionForInterface*>( all[j] ); if( ap ) continue;
            for(int k=0; k<all[j]->getNumberOfComponents(); ++k) all[j]->copyOutput(k)->use( action, arg ); 
          }
        } else if ( name=="*") {
          unsigned carg=arg.size();
          // Take all the values from an action with a specific name 
          ActionShortcut* shortcut=action_set.getShortcutActionWithLabel(a);
          if( shortcut ) shortcut->interpretDataLabel( a + "." + name, action, arg );
          if( arg.size()==carg ) { 
              ActionWithValue* avalue=action_set.selectWithLabel<ActionWithValue*>(a);
              if(!avalue) {
                std::string str=" (hint! the actions with value in this ActionSet are: ";
                str+=action_set.getLabelList<ActionWithValue*>()+")";
                action->error("cannot find action named " + a + str);
              }
              if( avalue->getNumberOfComponents()==0 ) action->error("found " + a +".* indicating use all components calculated by action with label " + a + " but this action has no components");
              for(int k=0; k<avalue->getNumberOfComponents(); ++k) avalue->copyOutput(k)->use( action, arg ); 
          }
        } else if ( a=="*" ) {
          std::vector<ActionShortcut*> shortcuts=action_set.select<ActionShortcut*>();
          // Take components from all actions with a specific name
          std::vector<ActionWithValue*> all=action_set.select<ActionWithValue*>();
          if( all.empty() ) action->error("your input file is not telling plumed to calculate anything");
          unsigned carg=arg.size();
          for(unsigned j=0; j<shortcuts.size(); ++j) {
              shortcuts[j]->interpretDataLabel( shortcuts[j]->getShortcutLabel() + "." + name, action, arg );
          } 
          unsigned nval=0;
          for(unsigned j=0; j<all.size(); j++) {
            std::string flab; flab=all[j]->getLabel() + "." + name;
            bool found=false;
            for(unsigned k=carg; k<arg.size(); ++k) {
                if( flab==arg[k]->getName() ) { found=true; break; }
            }
            if( !found && all[j]->exists(flab) ) { all[j]->copyOutput(flab)->use( action, arg ); nval++; }
          }
          if(nval==0 && arg.size()==carg) action->error("found no actions with a component called " + name );
        } else {
          // Take values with a specific name
          ActionShortcut* shortcut=action_set.getShortcutActionWithLabel(a);
          if( shortcut ) {
              unsigned narg=arg.size(); shortcut->interpretDataLabel( a + "." + name, action, arg );
              if( arg.size()==narg ) action->error("found no element in " + a + " with label " + name );
          } else {
              ActionWithValue* avalue=action_set.selectWithLabel<ActionWithValue*>(a);
              if(!avalue) {
                std::string str=" (hint! the actions with value in this ActionSet are: ";
                str+=action_set.getLabelList<ActionWithValue*>()+")";
                action->error("cannot find action named " + a +str);
              }
              if( !(avalue->exists(c[i])) ) {
                std::string str=" (hint! the components in this actions are: ";
                str+=avalue->getComponentsList()+")";
                action->error("action " + a + " has no component named " + name + str);
              } 
              avalue->copyOutput(c[i])->use( action, arg ); 
          }
        }
      } else {    // if it doesn't contain a dot
        if(c[i]=="*") {
          // This outputs values from shortcuts
          std::vector<ActionWithValue*> all=action_set.select<ActionWithValue*>();
          std::vector<ActionShortcut*> shortcuts=action_set.select<ActionShortcut*>();
          if( all.empty() ) action->error("your input file is not telling plumed to calculate anything");
          for(unsigned j=0; j<all.size(); j++) {
            bool found=false;
            ActionForInterface* ap=dynamic_cast<ActionForInterface*>( all[j] ); 
            if( ap && ap->getName()!="ENERGY" ) continue;
            // Find an action created from the shortcut with this label that has a single action
            for(unsigned k=0; k<shortcuts.size(); ++k) {
                if( shortcuts[k]->matchWildcard() && all[j]->getLabel()==shortcuts[k]->getShortcutLabel() && all[j]->getNumberOfComponents()==1 ) { found=true; break; }
            }
            if( found ) {
                for(int k=0; k<all[j]->getNumberOfComponents(); ++k) all[j]->copyOutput(k)->use( action, arg ); 
            } else {
                // Don't print the working out that gets us to shortcut output
                for(unsigned k=0; k<shortcuts.size(); ++k) {
                    if( all[j]->getLabel().find(shortcuts[k]->getShortcutLabel())!=std::string::npos ) { found=true; break; }
                }
                if( !found ) {
                    for(int k=0; k<all[j]->getNumberOfComponents(); ++k) all[j]->copyOutput(k)->use( action, arg ); 
                }
            }
          }
          for(unsigned j=0; j<shortcuts.size(); ++j) shortcuts[j]->interpretDataLabel( shortcuts[j]->getShortcutLabel() + ".*", action, arg );
        } else {
          ActionWithValue* avalue=action_set.selectWithLabel<ActionWithValue*>(c[i]);
          if(!avalue) {
            std::string str=" (hint! the actions with value in this ActionSet are: ";
            str+=action_set.getLabelList<ActionWithValue*>()+")";
            action->error("cannot find action named " + c[i] + str );
          }
          if( !(avalue->exists(c[i])) ) {
            std::string str=" (hint! the components in this actions are: ";
            str+=avalue->getComponentsList()+")";
            action->error("action " + c[i] + " has no component named " + c[i] +str);
          };
          avalue->copyOutput(c[i])->use( action, arg );
        }
      }
    }
  } 
}

void ActionWithArguments::expandArgKeywordInPDB( const PDB& pdb ) {
  std::vector<std::string> arg_names = pdb.getArgumentNames();
  if( arg_names.size()>0 ) {
    std::vector<Value*> arg_vals;
    interpretArgumentList( arg_names, plumed.getActionSet(), this, arg_vals );
  }
}

void ActionWithArguments::requestArguments(const std::vector<Value*> &arg, const bool& allow_streams, const unsigned& argstart ) {
  plumed_massert(!lockRequestArguments,"requested argument list can only be changed in the prepare() method");
  thisAsActionWithValue = dynamic_cast<const ActionWithValue*>(this);
  bool firstcall=(arguments.size()==0);
  arguments=arg; clearDependencies();
  if( arguments.size()==0 ) return ;
  distinct_arguments.resize(0); done_over_stream=false;
  bool storing=false, allconstant=true;
  // Now check if we need to store data
  for(unsigned i=argstart; i<arguments.size(); ++i) {
    if( !allow_streams ) { storing=true; break; }
    if( arguments[i]->alwaysstore ) { 
        ActionSetup* as = dynamic_cast<ActionSetup*>( arguments[i]->getPntrToAction() );
        if( arguments[i]->isConstant() || as ) { arguments[i]->buildDataStore( getLabel() ); } else { storing=true; break; }
    } else if( arguments[i]->isConstant() ) arguments[i]->buildDataStore( getLabel() );
    if( !arguments[i]->isConstant() ) { allconstant=false; }
  }
  if( allconstant ) storing=true;
  std::string fullname,name;
  std::vector<ActionWithValue*> f_actions;
  for(unsigned i=0; i<arguments.size(); i++) {
    fullname=arguments[i]->getName();
    if(fullname.find(".")!=std::string::npos) {
      std::size_t dot=fullname.find_first_of('.');
      name=fullname.substr(0,dot);
    } else {
      name=fullname;
    }
    AverageBase* av = dynamic_cast<AverageBase*>( arguments[i]->getPntrToAction() );
    ReweightBase* rb = dynamic_cast<ReweightBase*>( arguments[i]->getPntrToAction() );
    if( av ) { 
        plumed_assert( !rb ); theAverageInArguments=av; arguments[i]->buildDataStore( getLabel() );
    } else if( rb ) {
        theReweightBase=rb;
    } else {
        ActionWithArguments* aa = dynamic_cast<ActionWithArguments*>( arguments[i]->getPntrToAction() );
        if( aa ) {
            if( aa->theAverageInArguments ) theAverageInArguments=aa->theAverageInArguments;
            if( aa->theReweightBase ) theReweightBase=aa->theReweightBase;
        }
    } 
    ActionWithValue* action=plumed.getActionSet().selectWithLabel<ActionWithValue*>(name);
    plumed_massert(action,"cannot find action named (in requestArguments - this is weird)" + name);
    if( i<argstart ) {
        // We don't add the depedency if we the previous one is an average
        AverageBase* av=dynamic_cast<AverageBase*>( action ); 
        if( !av ) addDependency(action);
    }
    if( i<argstart ) continue;
    // Check that we are not adding a dependency to an action that appears after this one
    bool add_depend=false;
    for(const auto & pp : plumed.getActionSet() ) {
        Action* p(pp.get()); if( p==this ) { break; } else if( p==action ) { add_depend=true; break; }
    }
    if( add_depend ) addDependency(action);

    if( storing ) arguments[i]->buildDataStore( getLabel() );
    if( arguments[i]->getRank()>0 ) {
      // This checks if we have used the data in this value to calculate any of the other arguments in the input
      for(unsigned j=0; j<arguments[i]->store_data_for.size(); ++j) {
        bool found=false;
        for(unsigned k=0; k<arguments.size(); ++k) {
          if( !arguments[k]->getPntrToAction() ) continue;
          if( arguments[i]->store_data_for[j].first==(arguments[k]->getPntrToAction())->getLabel() && 
              arguments[k]->getPntrToAction()->getName()!="NEIGHBORS" ) { found=true; break; }
        }
        if( found ) { arguments[i]->buildDataStore( getLabel() ); break; }
      }
      // Check if we already have this argument in the stream
      ActionWithValue* myact = (arguments[i]->getPntrToAction())->getActionThatCalculates();
      ActionSetup* as = dynamic_cast<ActionSetup*>( myact ); bool found=false;
      if( !as && !arguments[i]->isConstant() && myact->getName()!="READ" ) {
          for(unsigned k=0; k<f_actions.size(); ++k) {
            if( f_actions[k]==myact ) { found=true; break; }
          }
          if( !found ) f_actions.push_back( myact );
      }
    } else arguments[i]->buildDataStore( getLabel() );
  }
  // This is a way of checking if we are in an ActionWithValue by looking at the keywords -- is there better fix?
  if( firstcall && !thisAsActionWithValue ) {
    if( !keywords.exists("SERIAL") ) {
      for(unsigned i=0; i<arg.size(); ++i) { if( arg[i]->getRank()>0 ) arg[i]->buildDataStore( getLabel() ); }
      return;
    }
  } else if(!thisAsActionWithValue) return;

  if( storing ) {
    done_over_stream=false;
  } else if( f_actions.size()>1 ) {
    done_over_stream=true;
    // Get the number of tasks and check this is matched for all in f_action
    unsigned ntasks = (f_actions[0]->getPntrToOutput(0))->ntasks;
    for(unsigned i=1;i<f_actions[0]->getNumberOfComponents();++i) plumed_assert( ntasks==(f_actions[0]->getPntrToOutput(i))->ntasks ); 
    for(unsigned i=1; i<f_actions.size(); ++i) {
      // Check for mismatched tasks in values
      for(unsigned j=0;j<f_actions[i]->getNumberOfComponents();++j) {
          if( ntasks!=(f_actions[i]->getPntrToOutput(j))->ntasks ) { done_over_stream=false; break; }
      }
      // This checks we are not creating cicular recursive loops
      if( !done_over_stream || f_actions[0]->checkForDependency(f_actions[i]) ) { done_over_stream=false; break; }
      // Check everything for second action is computed before f_actions[0]
      const std::vector<Action*> depend( f_actions[i]->getDependencies() ); 
      for(unsigned j=0; j<depend.size(); ++j) {
          bool found=false;
          for(const auto & pp : plumed.getActionSet()) {
              Action* p(pp.get());
              // Check if this is the dependency
              if( pp->getLabel()==depend[j]->getLabel() ) { found=true; break; }
              // Check if this is the first of the distinct_arguments that will appear in the chain
              else if( pp->getLabel()==f_actions[0]->getLabel() ) break; 
          }
          if( !found ) { done_over_stream=false; break; }
      }
    }
    if( !done_over_stream ) {
      for(unsigned i=0; i<arg.size(); ++i) { if( arg[i]->getRank()>0 ) arg[i]->buildDataStore( getLabel() );  }
    }
  } else if( f_actions.size()==1 ) {
    // Get the number of tasks and check this is matched for all in f_action
    unsigned ntasks = (f_actions[0]->getPntrToOutput(0))->ntasks;
    for(unsigned i=1;i<f_actions[0]->getNumberOfComponents();++i) plumed_assert( ntasks==(f_actions[0]->getPntrToOutput(i))->ntasks );
    done_over_stream=true;
  }

  if( done_over_stream ) {
    // Get the action where this argument should be applied
    ActionWithArguments* aa=dynamic_cast<ActionWithArguments*>( arguments[argstart]->getPntrToAction() );
    bool distinct_but_stored=(arguments[argstart]->getRank()==0);
    for(unsigned i=0; i<arguments[argstart]->store_data_for.size(); ++i) {
      if( arguments[argstart]->store_data_for[i].first==getLabel() ) { distinct_but_stored=true; break; }
    }
    AverageBase* ab=dynamic_cast<AverageBase*>( arguments[argstart]->getPntrToAction() ); 
    if( !aa || aa->mustBeTreatedAsDistinctArguments() ) {
      if( !distinct_but_stored && !ab ) distinct_arguments.push_back( std::pair<ActionWithValue*,unsigned>(arguments[argstart]->getPntrToAction(),0) );
      else if( ab ) distinct_arguments.push_back( std::pair<ActionWithValue*,unsigned>(arguments[argstart]->getPntrToAction(),2) );
      else distinct_arguments.push_back( std::pair<ActionWithValue*,unsigned>(arguments[argstart]->getPntrToAction(),1) );
    } else {
      if( !distinct_but_stored && !ab ) distinct_arguments.push_back( std::pair<ActionWithValue*,unsigned>(aa->getFirstNonStream(),0) );
      else if( ab ) distinct_arguments.push_back(std::pair<ActionWithValue*,unsigned>(aa->getFirstNonStream(),2) );
      else distinct_arguments.push_back(std::pair<ActionWithValue*,unsigned>(aa->getFirstNonStream(),1) );
    }
    // Build vector with locations to keep derivatives of arguments
    arg_deriv_starts.clear(); arg_deriv_starts.resize(0);
    arg_deriv_starts.push_back(0); unsigned nder;
    if( !arguments[argstart]->isConstant() && !distinct_but_stored ) nder = distinct_arguments[0].first->getNumberOfDerivatives();
    else if(ab) nder = 1; else nder = arguments[argstart]->getNumberOfValues();

    if( getNumberOfArguments()==1 ) { 
        arg_deriv_starts.push_back( nder );
    } else {
        for(unsigned i=argstart+1; i<getNumberOfArguments(); ++i) {
          // Work out what action applies forces
          ActionWithValue* myval; ActionWithArguments* aa=dynamic_cast<ActionWithArguments*>( arguments[i]->getPntrToAction() );
          if( !aa || aa->mustBeTreatedAsDistinctArguments() ) myval = arguments[i]->getPntrToAction();
          else myval = aa->getFirstNonStream();

          distinct_but_stored=(arguments[i]->getRank()==0);
          for(unsigned j=0; j<arguments[i]->store_data_for.size(); ++j) {
            if( arguments[i]->store_data_for[j].first==getLabel() ) { distinct_but_stored=true; break; }
          }

          // Check we haven't already dealt with this argument
          int argno=-1;;
          for(unsigned j=0; j<distinct_arguments.size(); ++j) {
            if( myval==distinct_arguments[j].first ) { argno=j; break; }
          }
          if( argno>=0 ) {
            arg_deriv_starts.push_back( arg_deriv_starts[argno] );
          } else {
            arg_deriv_starts.push_back( nder ); 
            ActionSetup* as=dynamic_cast<ActionSetup*>( myval );
            AverageBase* ab=dynamic_cast<AverageBase*>( myval );
            if( !arguments[i]->isConstant() && !as && !distinct_but_stored && !ab ) {
              distinct_arguments.push_back( std::pair<ActionWithValue*,unsigned>(myval,0) );
              nder += myval->getNumberOfDerivatives();
            } else if( ab ) {
              distinct_arguments.push_back( std::pair<ActionWithValue*,unsigned>(myval,2) );
              nder++;
            } else {
              distinct_arguments.push_back( std::pair<ActionWithValue*,unsigned>(myval,1) );
              nder += arguments[i]->getNumberOfValues();
            }
          }
        }
    }
  } else {
    for(unsigned i=argstart; i<getNumberOfArguments(); ++i) { if( arg[i]->getRank()>0 ) arg[i]->buildDataStore( getLabel() ); }
  }
}

unsigned ActionWithArguments::setupActionInChain( const unsigned& argstart ) {
  plumed_assert( done_over_stream ); 
  std::vector<std::string> alabels; std::vector<ActionWithValue*> f_actions;
  for(unsigned i=argstart; i<getNumberOfArguments(); ++i) {
    bool found=false; std::string mylab = (arguments[i]->getPntrToAction())->getLabel();
    for(unsigned j=0; j<alabels.size(); ++j) {
      if( alabels[j]==mylab ) { found=true; break; }
    }
    if( !found ) alabels.push_back( mylab );

    found=false; ActionWithValue* myact = (arguments[i]->getPntrToAction())->getActionThatCalculates();
    ActionSetup* as = dynamic_cast<ActionSetup*>( myact ); // If this is calculated in setup we never need to add to chain 
    if( !as && !arguments[i]->isConstant() ) {
        for(unsigned j=0; j<f_actions.size(); ++j) {
          if( f_actions[j]==myact ) { found=true; break; }
        }
        if( !found ) {
          bool storing_for_this=false;
          for(unsigned j=0;j<arguments[i]->store_data_for.size();++j) {
              if( arguments[i]->getRank()==0 || arguments[i]->store_data_for[j].first==getLabel() ) { storing_for_this=true; break; }
          }   
          if( f_actions.size()==0 || !storing_for_this ) f_actions.push_back( myact );
        }
    }
  }

  if( f_actions.size()>0 ) {
      // Now make sure that everything we need is in the chain
      std::vector<std::string> empty(1); empty[0] = f_actions[0]->getLabel();
      for(unsigned i=1; i<f_actions.size(); ++i) f_actions[0]->addActionToChain( empty, f_actions[i] );
  }

  // Now add this argument to the chain
  bool added=false, all_setup=true; 
  ActionWithValue* av = dynamic_cast<ActionWithValue*>( this ); plumed_assert( av );
  for(unsigned i=argstart; i<getNumberOfArguments(); ++i) {
    // Ensures we don't add actions to setup chains
    // ActionSetup* as = dynamic_cast<ActionSetup*>( arguments[i]->getPntrToAction() );
    // Add this function to jobs to do in recursive loop in previous action
    if( (arguments[i]->getPntrToAction())->canChainFromThisAction() && arguments[i]->getRank()>0 ) {
      if( (arguments[i]->getPntrToAction())->addActionToChain( alabels, av ) ) { added=true; break; }
    }
    if( (arguments[i]->getPntrToAction())->canChainFromThisAction() ) all_setup=false;
  }
  if( !all_setup ) plumed_massert(added, "could not add action " + getLabel() + " to chain of any of its arguments");

  // Now make sure we have the derivative size correct
  unsigned nderivatives=0;
  for(unsigned i=0; i<distinct_arguments.size(); ++i) {
    if( distinct_arguments[i].second==0 ) nderivatives += distinct_arguments[i].first->getNumberOfDerivatives();
    else {
      if( distinct_arguments[i].first->getNumberOfComponents()==1 ) nderivatives += (distinct_arguments[i].first->getPntrToComponent(0))->getNumberOfValues();
      else {
        unsigned nd=0;
        for(unsigned j=0;j<getNumberOfArguments();++j) {
            if( (arguments[j]->getPntrToAction())->getLabel()==distinct_arguments[i].first->getLabel() ) { 
                 if( nd>0 ) error("cannot use more than one argument from an action at once in this way");
                 if( distinct_arguments[i].second==2 ) nd = 1; else nd = arguments[j]->getNumberOfValues();
            }
        }
        plumed_assert( nd > 0 ); nderivatives += nd;
      }
    }
  }
  return nderivatives;
}

ActionWithArguments::ActionWithArguments(const ActionOptions&ao):
  Action(ao),
  lockRequestArguments(false),
  theAverageInArguments(NULL),
  theReweightBase(NULL),
  thisAsActionWithValue(NULL),
  done_over_stream(false)
{
  if( keywords.exists("ARG") ) {
    std::vector<Value*> arg;
    parseArgumentList("ARG",arg);

    if(!arg.empty()) {
      log.printf("  with arguments");
      for(unsigned i=0; i<arg.size(); i++) log.printf("%s",arg[i]->getOutputDescription().c_str());
      log.printf("\n");
    } else if( keywords.numbered("ARG") ) {
      unsigned narg=0;
      for(unsigned i=1;; ++i) {
        std::vector<Value*> argn; parseArgumentList("ARG",i,argn);
        if( argn.size()==0 ) break;
        unsigned nargt=0; log.printf("  %dth set of arguments",i);
        for(unsigned j=0; j<argn.size(); ++j) {
          log.printf(" %s",argn[j]->getOutputDescription().c_str());
          arg.push_back( argn[j] ); nargt += argn[j]->getNumberOfValues();
        }
        log.printf("\n");
        if( i==1 ) narg = nargt;
        else if( narg!=nargt && getName()!="CONCATENATE" && getName()!="MATHEVAL" && getName()!="CUSTOM" && getName()!="DIFFERENCE" && getName()!="DOT" && getName()!="TORSIONS_MATRIX" && getName()!="RMSD_CALC" ) {
           error("mismatch between number of arguments specified for different numbered ARG values");
        }
      }
    }
    if( keywords.numbered("ARG" ) ) requestArguments(arg,true);
    else requestArguments(arg,false);
  }
}

bool ActionWithArguments::mustBeTreatedAsDistinctArguments() {
//  if( done_over_stream || arguments.size()>1 ) return true;
  if( !done_over_stream ) return true;
  if( arguments.size()==1 ) {
      ActionWithValue* av=dynamic_cast<ActionWithValue*>(this);
      plumed_assert( av ); ActionWithValue* cal = av->getActionThatCalculates();
      ActionAtomistic* aa = dynamic_cast<ActionAtomistic*>(cal);
      if( !aa ) return true;
      return false;
  }

  std::vector<const ActionWithValue*> allvals; (arguments[0]->getPntrToAction())->getAllActionsRequired( allvals );
  for(unsigned j=1;j<arguments.size();++j) {
      std::vector<const ActionWithValue*> tvals; (arguments[j]->getPntrToAction())->getAllActionsRequired( tvals );
      if( allvals.size()!=tvals.size() ) return true;

      for(unsigned k=0;k<tvals.size();++k) {
          if( tvals[k]!=allvals[k] ) return true;
      }
  } 
  return false;
} 

ActionWithValue* ActionWithArguments::getFirstNonStream() {
  ActionWithArguments* aa=dynamic_cast<ActionWithArguments*>( getPntrToArgument(0)->getPntrToAction() );
  if( !aa || aa->mustBeTreatedAsDistinctArguments() ){ 
      ActionWithValue* av=dynamic_cast<ActionWithValue*>(this); 
      plumed_assert( av ); return av; 
  }

  for(unsigned i=1;i<arguments.size();++i) {
      ActionWithArguments* aaa=dynamic_cast<ActionWithArguments*>( getPntrToArgument(i)->getPntrToAction() );
      if( aa!=aaa ){ 
          ActionWithValue* av=dynamic_cast<ActionWithValue*>(this); 
          plumed_assert( av ); return av;
      }
  }
  return aa->getFirstNonStream();
}

void ActionWithArguments::calculateNumericalDerivatives( ActionWithValue* a ) {
  if( done_over_stream ) error("cannot use numerical derivatives if calculation is done over stream");
  if(!a) {
    a=dynamic_cast<ActionWithValue*>(this);
    plumed_massert(a,"cannot compute numerical derivatives for an action without values");
  }

  unsigned nargs=0;  std::vector<Value*> myvals;
  a->retrieveAllScalarValuesInLoop( getLabel(), nargs, myvals );
  const int npar=arguments.size();
  std::vector<double> value (myvals.size()*npar);
  for(int i=0; i<npar; i++) {
    double arg0=arguments[i]->get();
    arguments[i]->set(arg0+sqrt(epsilon));
    a->calculate();
    arguments[i]->set(arg0);
    for(int j=0; j<myvals.size(); j++) {
      value[i*myvals.size()+j]=myvals[j]->get();
    }
  }
  a->calculate();
  a->clearDerivatives();
  for(int j=0; j<myvals.size(); j++) {
    if( myvals[j]->hasDerivatives() ) for(int i=0; i<npar; i++) myvals[j]->addDerivative(i,(value[i*myvals.size()+j]-a->getOutputQuantity(j))/sqrt(epsilon));
  }
}

double ActionWithArguments::getProjection(unsigned i,unsigned j)const {
  plumed_massert(i<arguments.size()," making projections with an index which  is too large");
  plumed_massert(j<arguments.size()," making projections with an index which  is too large");
  plumed_massert(arguments[i]->getRank()==0 && arguments[j]->getRank()==0,"cannot calculate projection for data stream input");
  const Value* v1=arguments[i];
  const Value* v2=arguments[j];
  return Value::projection(*v1,*v2);
}

void ActionWithArguments::buildTaskListFromArgumentRequests( const unsigned& ntasks, bool& reduce, std::set<AtomNumber>& tflags ) {}

void ActionWithArguments::buildTaskListFromArgumentValues( const std::string& name, const std::set<AtomNumber>& tflags ) {
  plumed_merror("should not be in this method.  Something that is not a function is being added to a chain");
}

void ActionWithArguments::setForceOnScalarArgument(const unsigned n, const double& ff) {
  unsigned nt = 0, nn = 0, j=0;
  for(unsigned i=0; i<arguments.size(); ++i) {
    nt += arguments[i]->getNumberOfValues();
    if( n<nt ) { j=i; break ; }
    nn += arguments[i]->getNumberOfValues();
  }
  arguments[j]->addForce( n-nn, ff );
}

void ActionWithArguments::setGradientsForActionChain( Value* myval, unsigned& start, ActionWithValue* av ) { 
  ActionWithArguments* aarg = dynamic_cast<ActionWithArguments*>( av );
  if( aarg ) aarg->setGradients( myval, start ); 
  ActionAtomistic* aat = dynamic_cast<ActionAtomistic*>( av );
  if( aat ) myval->setGradients( aat, start );
}

void ActionWithArguments::setGradients( Value* myval, unsigned& start ) const {
  if( !myval->hasDeriv ) return; plumed_assert( myval->getRank()==0 );

  if( done_over_stream ) {
      for(unsigned i=0; i<distinct_arguments.size(); ++i) {
        if( distinct_arguments[i].second==0 ) setGradientsForActionChain( myval, start, distinct_arguments[i].first );
        else {
          for(unsigned j=0; j<arguments.size(); ++j) {
              bool hasstored=false;
              for(unsigned k=0; k<arguments[j]->store_data_for.size(); ++k) {
                  if( arguments[j]->store_data_for[k].first==getLabel() ) { hasstored=true; break; }
              }
              if( hasstored && arguments[j]->getPntrToAction()==distinct_arguments[i].first ) {
                  if( !arguments[j]->isConstant() ) plumed_merror("cannot use gradients with non-constant values for input " + arguments[j]->getName() );
                  else start += arguments[j]->getNumberOfValues();
              }
          }
        }
      }
  } else {
      bool scalar=true;
      for(unsigned i=0; i<arguments.size(); ++i ) {
          if( arguments[i]->getRank()!=0 ) { scalar=false; break; }
      }
      if( !scalar ) {
           bool constant=true;
           for(unsigned i=0; i<arguments.size(); ++i ) {
               if( !arguments[i]->isConstant() ) { constant=false; break; }
               else start += arguments[i]->getNumberOfValues();
           }
           if( !constant ) error("cannot set gradient as unable to handle non-constant actions that take vectors/matrices/grids in input");
      }
      // Now pass the gradients 
      for(unsigned i=0; i<arguments.size(); ++i ) arguments[i]->passGradients( myval->getDerivative(i), myval->gradients );
  }
}

void ActionWithArguments::setForcesOnActionChain( const std::vector<double>& forces, unsigned& start, ActionWithValue* av ) {
  plumed_dbg_massert( start<=forces.size(), "not enough forces have been saved" );
  ActionWithArguments* aarg = dynamic_cast<ActionWithArguments*>( av );
  if( aarg ) aarg->setForcesOnArguments( 0, forces, start );
  ActionAtomistic* aat = dynamic_cast<ActionAtomistic*>( av );
  if( aat ) aat->setForcesOnAtoms( forces, start );
}

void ActionWithArguments::setForcesOnArguments( const unsigned& argstart, const std::vector<double>& forces, unsigned& start ) {
  if( done_over_stream ) {
    for(unsigned i=0; i<distinct_arguments.size(); ++i) {
      if( distinct_arguments[i].second==0 ) {
        setForcesOnActionChain( forces, start, distinct_arguments[i].first ); 
      } else {
        std::vector<std::string> added_force_on;
        for(unsigned j=argstart; j<arguments.size(); ++j) {
          bool hasstored=false;
          for(unsigned k=0; k<arguments[j]->store_data_for.size(); ++k) {
            if( arguments[j]->store_data_for[k].first==getLabel() ) { hasstored=true; break; }
          }
          if( hasstored && arguments[j]->getPntrToAction()==distinct_arguments[i].first ) {
            // Check for repeatted arguments
            bool already_done=false;
            for(unsigned k=0; k<added_force_on.size(); ++k) {
                if( arguments[j]->getName()==added_force_on[k] ) { already_done=true; break; }
            } 
            if( already_done ) continue;
            // This makes sure we don't add to a vector of all ones multiple times when we do q6.  This is not very good programming Gareth
            if( arguments[j]->getName().find("_ones")!=std::string::npos ) added_force_on.push_back( arguments[j]->getName() ); 
            unsigned narg_v = arguments[j]->getNumberOfValues(); if( distinct_arguments[i].second==2 ) narg_v = 1;
            for(unsigned k=0; k<narg_v; ++k) {
              plumed_dbg_assert( start<forces.size() ); 
              arguments[j]->addForce( k, forces[start] ); start++;
            }
          }
        }
      }
    }
  } else {
    for(unsigned i=argstart; i<arguments.size(); ++i) {
      unsigned narg_v = arguments[i]->getNumberOfValues();
      for(unsigned j=0; j<narg_v; ++j) { plumed_dbg_massert( start<forces.size(), "problem in " + getLabel() ); arguments[i]->addForce( j, forces[start] ); start++; }
    }
  }
}

bool ActionWithArguments::skipCalculate() const {
  if( theAverageInArguments || theReweightBase ) return true;
  return false;
}

bool ActionWithArguments::skipUpdate() const {
  // If there is no average in the arguments calculation is done in calculate
  if( !theAverageInArguments && !theReweightBase ) return true;
  // Redo calculation in update if there is an average in the arguments
  if( theAverageInArguments ) return !theAverageInArguments->isActive();
  // Redo calculation in update if there is an reweight in the arguments
  return !theReweightBase->isActive();
}

void ActionWithArguments::getNumberOfStashedInputArguments( unsigned& nquants ) const {
  const ActionWithValue* av = dynamic_cast<const ActionWithValue*>( this ); plumed_assert( av );
  for(unsigned i=0; i<arguments.size(); ++i) {
    for(unsigned j=0; j<arguments[i]->store_data_for.size(); ++j) {
      if( arguments[i]->store_data_for[j].first==getLabel() ) {
        arguments[i]->store_data_for[j].second=nquants; nquants++; break;
      }
    }
  }
}

unsigned ActionWithArguments::getArgumentPositionInStream( const unsigned& jder, MultiValue& myvals ) const {
  // Check for storage of this argument
  for(unsigned j=0; j<arguments[jder]->store_data_for.size(); ++j) {
    if( arguments[jder]->store_data_for[j].first==getLabel() ) {
      unsigned istrn = arguments[jder]->store_data_for[j].second;
      unsigned task_index = 0; 
      if( arguments[jder]->getRank()>0 && !arguments[jder]->isTimeSeries() ) task_index=myvals.getTaskIndex();
      myvals.addDerivative( istrn, task_index, 1.0 );
      if( myvals.getNumberActive(istrn)==0 ) myvals.updateIndex( istrn, task_index );
      return istrn;
    }
  }
  return arguments[jder]->getPositionInStream();
}

std::vector<unsigned> ActionWithArguments::getValueShapeFromArguments() {
  std::vector<unsigned> r2shape(2); bool argmat=false;
  for(unsigned i=0; i<arguments.size(); ++i) {
      if( arguments[i]->getRank()==2 ) {
          argmat=true; r2shape[0]=arguments[i]->getShape()[0];
          r2shape[1]=arguments[i]->getShape()[1]; break;
      }
  }
  if( !argmat ) {
      for(unsigned i=0; i<arguments.size(); ++i) {
          if( arguments[i]->getRank()==1 ) { r2shape[0]=r2shape[1]=arguments[i]->getShape()[0]; }
      }
  }
  return r2shape;
}

bool ActionWithArguments::calculateConstantValues( const bool& haveatoms ) {
  ActionWithValue* av = dynamic_cast<ActionWithValue*>( this );
  if( !av || arguments.size()==0 ) return false; 
  bool constant = true, atoms=false;
  for(unsigned i=0; i<arguments.size(); ++i) {
      ActionAtomistic* aa=dynamic_cast<ActionAtomistic*>( arguments[i]->getPntrToAction() );
      if( aa ) { atoms=true; }
      if( !arguments[i]->isConstant() ) { constant=false; break; }
  }
  if( constant ) {
      // Set everything constant first as we need to set the shape
      for(unsigned i=0; i<av->getNumberOfComponents(); ++i) (av->copyOutput(i))->setConstant();
      if( !haveatoms ) log.printf("  values stored by this action are computed during startup and stay fixed during the simulation\n");
      if( atoms ) return haveatoms;
  } 
  // Now do the calculation and store the values if we don't need anything from the atoms
  if( constant && !haveatoms ) {
      plumed_assert( !atoms ); activate(); calculate(); deactivate(); 
      for(unsigned i=0; i<av->getNumberOfComponents(); ++i) {
         unsigned nv = av->copyOutput(i)->getNumberOfValues();
         log.printf("  %d values stored in component labelled %s are : ", nv, (av->copyOutput(i))->getName().c_str() );
         for(unsigned j=0; j<nv; ++j) log.printf(" %f", (av->copyOutput(i))->get(j) );
         log.printf("\n");
      }
  }
  return constant;
}

}
