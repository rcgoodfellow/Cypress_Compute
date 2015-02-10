#ifndef CYPRESS_ELEMENTS
#define CYPRESS_ELEMENTS

#include "Cypress/Core/Common.hxx"
#include "Cypress/Core/Equation.hxx"

#include <vector>
#include <unordered_map>

//Forward Declarations --------------------------------------------------------
namespace cypress
{
  struct Element; using ElementSP = std::shared_ptr<Element>;
  struct Object; using ObjectSP = std::shared_ptr<Object>;
  struct Controller; using ControllerSP = std::shared_ptr<Controller>;
  struct Component; using ComponentSP = std::shared_ptr<Component>;
  struct Linkable; using LinkableSP = std::shared_ptr<Linkable>;
  struct Thing; using ThingSP = std::shared_ptr<Thing>;
  struct SubThing; using SubThingSP = std::shared_ptr<SubThing>;
  struct AtoD; using AtoDSP = std::shared_ptr<AtoD>;
  struct Link; using LinkSP = std::shared_ptr<Link>;
  struct Experiment; using ExperimentSP = std::shared_ptr<Experiment>;
}
namespace cypress 
{

//Core data structures --------------------------------------------------------

struct Element : public Decl
{
  SymbolSP name;
  std::vector<SymbolSP> params;
  std::vector<EquationSP> eqtns; 
  Element(SymbolSP name) : name{name} {}
};

struct Object : public Element
{
  Kind kind() const override { return Kind::Object; }
  using Element::Element;
};

struct Controller : public Element
{
  Kind kind() const override { return Kind::Controller; }
  using Element::Element;
};

struct Component
{
  SymbolSP kind, name;
  std::unordered_map<SymbolSP, RealSP> params;
  ElementSP element;
  Component(SymbolSP kind, SymbolSP name) : kind{kind}, name{name} {}
};

struct Linkable 
{
  enum class Kind { Thing, SubThing, AtoD };
  virtual Kind kind() const = 0;
};

struct Thing : public Linkable
{
  SymbolSP name;
  Kind kind() const override { return Kind::Thing; }
  Thing(SymbolSP name) : name{name} {}
};

struct SubThing : public Linkable
{
  SymbolSP name, subname;
  Kind kind() const override { return Kind::SubThing; }
  SubThing(SymbolSP name, SymbolSP subname) : name{name}, subname{subname} {}
};

struct AtoD : public Linkable
{
  double rate;
  Kind kind() const override { return Kind::AtoD; }
  AtoD(double rate) : rate{rate} {}
};

struct Link
{
  LinkableSP from, to;  
  Link(LinkableSP from, LinkableSP to) : from{from}, to{to} {}
};

struct Experiment : public Decl
{
  SymbolSP name;
  std::vector<ComponentSP> components;
  std::vector<LinkSP> links;
  Kind kind() const override { return Kind::Experiment; }
  Experiment(SymbolSP name) : name{name} {}
};

struct Decls
{
  std::vector<ObjectSP> objects;
  std::vector<ControllerSP> controllers;
  std::vector<ExperimentSP> experiments;
};

//ostream operations ----------------------------------------------------------
std::ostream& operator << (std::ostream &o, const Decls &d);
std::ostream& operator << (std::ostream &o, const Object &obj);
std::ostream& operator << (std::ostream &o, const Controller &controller);
std::ostream& operator << (std::ostream &o, const Experiment &expr);
std::ostream& operator << (std::ostream &o, const Component &cp);
std::ostream& operator << (std::ostream &o, const Link &lnk);
std::ostream& operator << (std::ostream &o, const Linkable &lkb);
void showEqtn(std::ostream &, const Equation &);
void showExpr(size_t indent, std::ostream &o, const Expression &expr);

//Free functions over elements ------------------------------------------------
void
setEqtnsToZero(ElementSP);

ElementSP
qualifyEqtns(ElementSP);

//Element Visitors ------------------------------------------------------------
struct EqtnQualifier : public Visitor
{
  ComponentSP qual{nullptr};
  void setQualifier(ComponentSP);
  void visit(SymbolSP) override;
  void run(EquationSP);
};

}

#endif