#include <iostream>
#include <ranges>
#include <cassert>

#include "limex.h"

#include "test.h"


int main()
{

  std::cout << "Enter expression (or press enter to run all tests): ";
  std::string input;
  std::getline( std::cin, input );

  if ( input.empty() ) {
    test();
    exit(0);
  }

  LIMEX::Handle<double> handle;
  LIMEX::Expression<double> expression(input,handle);

  // Retrieve variables and respective values in expression
  std::vector<double> variableValues;
  for ( auto variableName : expression.getVariables() ) {
    std::cout << "Enter value for variable '" << variableName << "': ";
    double value;
    std::cin >> value;
    variableValues.push_back(value);
  } 

  // Retrieve the arrays the expression names, each a path without steps; the values are held here and
  // passed as views
  std::vector< std::vector<double> > arrays( expression.getPaths().size() );
  for ( size_t path = 0; path < expression.getPaths().size(); path++ ) {
    auto& description = expression.getPaths()[path];
    if ( !description.steps.empty() ) {
      continue;
    }
    std::cout << "Enter comma separated values for array '" << description.name << "': ";
    std::getline( std::cin, input );
    auto parts = std::ranges::views::split( input, ',' );
    for ( auto value : std::ranges::to<std::vector<std::string>>(parts) ) {
      arrays[path].push_back( std::stod(value) );
    }
  }
  LIMEX::Resolver<double> resolver;
  resolver.collection = [&arrays](size_t path, const std::vector<double>&) -> LIMEX::View<double> {
    return LIMEX::View<double>( arrays[path] );
  };

  // Evaluate expression with given values
  std::cout << "Result: " << expression.evaluate(variableValues,resolver) << std::endl;

/*
  LIMEX::Handle<double> handle;

  std::string input = "z -= √(x²+y²)";
  std::cout << "=== " << input << " ===" << std::endl; 
  LIMEX::Expression<double> expression(input,handle);
  std::cout << input << " = " << expression.evaluate({{5.0},{3.0},{4.0}}) << std::endl; 
  std::cout << expression.stringify() << std::endl; // Print the entire AST

  LIMEX::Expression<double> c("x in [1,2,3,y]",handle);
  std::cout << c.stringify() << std::endl; 
  std::cout << c.evaluate({{4.0},{4.0}}) << std::endl; 

  LIMEX::Expression<double> bounds("3 <= x < y",handle);
  std::cout << bounds.stringify() << std::endl; 
  std::cout << bounds.evaluate({{4.0},{4.0}}) << std::endl; 
*/

  return 0;
}

