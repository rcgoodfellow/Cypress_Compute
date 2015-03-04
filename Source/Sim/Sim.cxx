#include "Cypress/Sim/Sim.hxx"
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <stdexcept>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <array>

using namespace cypress;
using std::vector;
using std::runtime_error;
using std::make_shared;
using std::string;
using std::static_pointer_cast;
using std::stringstream;
using std::unordered_map;
using std::find_if;
using std::to_string;
using std::pair;
using std::array;

//Cypress::Sim ----------------------------------------------------------------

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
      
    //psys.push_back(cpy);
    psys.insert({c, cpy});
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

  for(auto eqtn_p: psys)
  {
    liftControlledVars(eqtn_p.second, under_control);
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

  addCVarResiduals();
}

void Sim::buildPhysics()
{
  buildSystemEquations();
}

SimEx Sim::buildSimEx()
{
  SimEx sx{psys.size(), 1e-4, 1e-6}; 
  //sx.residualClosureSource = buildResidualClosure();
  sx.computeNodes = buildComputeTopology(1);
  for(ComputeNode &c: sx.computeNodes) 
    sx.computeNodeSources.push_back(c.emitSource());

  return sx;
}

vector<RVar> Sim::mapVariables(size_t N)
{
  vector<RVar> m;
  vector<RVar> c;

  EqtnVarCollector evc;
  //for(EquationSP eqtn: psys) evc.run(eqtn);
  for(ComponentSP cp: exp->components) evc.run(cp);

  if(N > evc.vars.size())
    throw runtime_error(
        "The number of compute nodes is greater than" 
        "the number of system variables");


  //unordered_map<string, vector<MetaVar>> vref_map;
  //for(MetaVar v: evc.vars)
  //{
  //  vref_map[v.name].push_back(v);
  //}
  
  //size_t L = ceil(static_cast<double>(vref_map.size()) / N);
  size_t L = ceil(static_cast<double>(evc.vars.size()) / N);

  size_t i{0};
  for(auto v: evc.vars)
  {
    m.push_back( RVar{v->qname(), DCoordinate{i/L, i, i%L}} );
    ++i;
  }

  return m;
}

vector<REqtn> Sim::mapEquations(size_t N)
{
  vector<REqtn> m;

  size_t L = ceil(static_cast<double>(psys.size()) / N);

  size_t i{0};
  for(auto eqtn_p: psys)
  {
    m.push_back( REqtn{eqtn_p.second->clone(), eqtn_p.first, DCoordinate{i/L, i, i%L}} );
    ++i;
  }

  return m;
}

void addRVars(ComputeNode &n, vector<RVar> &rvars)
{
  EqtnVarCollector evc;
  for(auto p: n.eqtns) evc.run(p.first);

  for(VarRefSP v: evc.vars)
  {
    if(v->kind() == VarRef::Kind::Derivative) continue;
    auto lit =
      find_if(n.vars.begin(), n.vars.end(),
          [v](string s){ return s == v->qname(); });

    if(lit != n.vars.end()) continue;

    auto rit =
      find_if(rvars.begin(), rvars.end(),
        [v](RVar r){ return r.name == v->qname(); });

    if(rit == rvars.end()) throw runtime_error("Hocus Pocus! " + v->qname());

    n.rvars.push_back(*rit);
  }
}

void Sim::addCVarResiduals()
{
  CVarExtractor cvx;
  //for(auto eqtn_p: psys) eqtn_p.second->accept(cvx);
  for(auto eqtn_p: psys) cvx.run(eqtn_p.first, eqtn_p.second);

  std::cout << "--" << std::endl;

  std::cout << "system controlled variables:" << std::endl;
  for(auto p: cvx.cvars)
  {
    std::cout << p.second << std::endl;
    //-1 indicates generated code e.g., there is no source line
    static constexpr int nosrc{-1};
    EquationSP eq = make_shared<Equation>(nosrc, nosrc); 
    eq->lhs = make_shared<Real>(0, nosrc, nosrc);
    eq->rhs = 
      make_shared<Subtract>(
        make_shared<CVar>(make_shared<Symbol>(p.second, nosrc, nosrc)),
        make_shared<CCVar>(make_shared<Symbol>(p.second, nosrc, nosrc)),
        nosrc, nosrc);
    psys.insert({p.first, eq});
  }
  
  std::cout << "--" << std::endl;
}

vector<ComputeNode> Sim::buildComputeTopology(size_t N)
{
  vector<ComputeNode> topo(N);

  vector<RVar> vars = mapVariables(N);
  std::cout << "VC=" << vars.size() << std::endl;

  unordered_map<string, Initials> initials;
  for(ComponentSP c : exp->components)
  {
    for(auto p : c->initials)
    {
      if(p.first->kind() == VarRef::Kind::Normal)
      {
        initials[p.first->qname()].v = p.second->value;
      }
      else //kind == Derivative
      {
        initials[p.first->qname()].d = p.second->value;
      }
    }
  }

  /*
  for(auto p : vref_map) 
  {
    for(MetaVar m: p.second)
    {
      if(!m.derivative)
        m.initial = initials[p.first].v;
      else
        m.initial = initials[p.first].d;
    }
  }
  */


  vector<REqtn> eqtns = mapEquations(N);
  std::cout << "EC=" << eqtns.size() << std::endl;

  for(REqtn e: eqtns)
  {
    if(e.coord.px >= N) 
      throw runtime_error("Equation Balderdashery! " + to_string(e.coord.px));
    topo[e.coord.px].eqtns.insert({e.component, e.eqtn});
  }

  for(RVar v: vars)
  {
    if(v.coord.px >= N) throw runtime_error("Variable Balderdashery!");
    topo[v.coord.px].vars.push_back(v.name);
    topo[v.coord.px].initials[v.coord.lx] = initials[v.name];
  }

  size_t i{0};
  for(ComputeNode &n: topo)
  {
    n.N = vars.size();
    n.expInfo = exp->name->value;
    addRVars(n, vars);
    n.id = i++;
    std::cout << n << std::endl;
  }

  return topo;
}

string qdif(string s)
{
  if(boost::starts_with(s, "d_")) return "d";
  return "";
};

string vax(string s)
{
  return s + "_ax";
}

string sysinclude(string file)
{
  return "#include <"+file+">";
}

string use(string name)
{
  return "using " + name + ";";
}

string use_namespace(string name)
{
  return "using namespace " + name + ";";
}

string rcname(ExperimentSP ex)
{
  return ex->name->value + "RC";
}

/*
string Sim::buildResidualClosure()
{
  using std::endl;
  stringstream ss;

  EqtnVarCollector evc;
  for(EquationSP eqtn: psys) evc.run(eqtn);

  ss 
    << sysinclude("Cypress/Sim/ResidualClosure.hxx") << endl
    << sysinclude("cmath") << endl;
  ss << endl;

  ss << use_namespace("cypress") << endl;
  ss << use("std::string") << endl;
  ss << endl;

  ss 
    << "struct " << rcname(exp) << " : ResidualClosure" << endl
    << "{" << endl;

  size_t ax{0};
  for(MetaVar v: evc.vars) 
    ss << "  " 
      << "static constexpr size_t " << vax(v.name) << "{" << ax++ << "};" 
      << endl;
  ss << endl;
  
  for(MetaVar v: evc.vars) 
    ss << "  realtype " << v.name << "()" << endl
       << "  {" << endl
       << "    return " 
        << qdif(v.name) << "yresolve(varmap[" << vax(v.name) << "]);" << endl
       << "  }" << endl << endl;

  ss << "  " << "void compute(realtype *r) override" << endl;
  ss << "  " << "{" << endl;
  CxxResidualFuncBuilder rcb;
  size_t idx{0};
  for(EquationSP eqtn: psys) ss << "    " << rcb.run(eqtn, idx++) << endl;
  ss << "  " << "}" << endl;

  ss << "  " << "string experimentInfo() override" << endl
     << "  " << "{" << endl
     << "  " << "  " << "return \"" << exp->name->value << "\";" << endl
     << "  " << "}" << endl;

  ss << "};" << endl << endl;

  ss << rcname(exp) << " *rc = new " << rcname(exp) << ";" << endl << endl;


  return ss.str();
}
*/


//Cypress::EqtnVarCollector ---------------------------------------------------

void EqtnVarCollector::run(ComponentSP c)
{
  component = c;
  for(EquationSP eqtn : c->element->eqtns) eqtn->accept(*this);
}

//void EqtnVarCollector::run(EquationSP eqtn)
//{
//  eqtn->accept(*this);
//}

void EqtnVarCollector::in(SymbolSP s)
{
  //string sname = s->value;
  //boost::replace_all(sname, ".", "_");
  //vars.insert({sname, in_derivative, in_cvar});
  if(in_derivative)
    vars.insert(make_shared<DVarRef>(component, s->value, derivative_order));
  else
    vars.insert(make_shared<VarRef>(component, s->value));
}

void EqtnVarCollector::visit(DifferentiateSP)
{
  in_derivative = true;
  ++derivative_order;
}

void EqtnVarCollector::leave(DifferentiateSP)
{
  in_derivative = false;
  --derivative_order;
}
  
void EqtnVarCollector::visit(CVarSP)
{
  in_cvar = true;
}

void EqtnVarCollector::leave(CVarSP)
{
  in_cvar = false;
}
