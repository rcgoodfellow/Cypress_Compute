#include "Cypress/Sim/Sim.hxx"
#include <stdexcept>

using namespace cypress;
using std::vector;
using std::runtime_error;
using std::make_shared;
using std::string;
using std::static_pointer_cast;

Sim::Sim( vector<ObjectSP> objects, vector<ControllerSP> controllers,
    ExperimentSP exp) 
  : objects{objects}, controllers{controllers}, exp{exp}
{
  elements.insert(elements.end(), objects.begin(), objects.end());
  elements.insert(elements.end(), controllers.begin(), controllers.end());
}

void Sim::addObjectToSim(ComponentSP c)
{
  EqtnQualifier eqq;
  eqq.setQualifier(c);
  for(auto eqtn: c->element->eqtns)
  {
    auto cpy = eqtn->clone();
    eqq.run(cpy);
    setToZero(cpy);

    for(auto p: c->params)
    {
      string sym_name{c->name->value+"."+p.first->value};
      applyParameter(cpy, sym_name, p.second->value);
    }
      
    psys.push_back(cpy);
  }
}

#include <iostream>
void Sim::addControllerToSim(ComponentSP c)
{
  std::cout << "adding controller " 
            << c->name->value << " :: "
            << c->kind->value << std::endl;

  //vector<SubComponentRefSP> uc = findControlledSubComponents(c);  
}

//TODO: This should be a semantic action
string getControlled(ConnectableSP c)
{
  if(c->neighbor != nullptr) return getControlled(c->neighbor);

  if(c->kind() != Connectable::Kind::SubComponent)
    throw runtime_error{"well fuck"};

  auto x = static_pointer_cast<SubComponentRef>(c);
  return x->name->value + "." + x->subname->value;
}

void Sim::addControllerRefToSim(SubComponentRefSP c)
{
  string under_control = getControlled(c);

  for(auto eqtn: psys)
  {
    liftControlledVars(eqtn, under_control);
  }
}

void Sim::buildSystemEquations()
{
  for(auto c: exp->components)
  {
    if(c->element->kind() == Decl::Kind::Object) addObjectToSim(c);
    //if(c->element->kind() == Decl::Kind::Controller) addControllerToSim(c);
  }

  for(ConnectionSP cx : exp->connections)
  {
    if(isa(cx->from, Connectable::Kind::SubComponent))
    {
      auto sc = static_pointer_cast<SubComponentRef>(cx->from);
      if(isa(sc->component->element, Decl::Kind::Controller))
      {
        addControllerRefToSim(sc); 
      }
    }
  }
}

void Sim::buildPhysics()
{
  buildSystemEquations();
}
 
SimEx Sim::buildSimEx()
{
  SimEx sx{psys.size(), 1e-4, 1e-6}; 

  //TODO: you are here output this to the stream in the compiler exec
  //and then input the geenrated file in the SimEx exec and make sure
  //they match up

  return sx;
}

