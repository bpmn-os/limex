#include <iostream>
#include <cassert>

#include "limex.h"

#include "test.h"

int main()
{

  test();

  LIMEX::Callables<double> callables;

  std::string input = "z -= √(x²+y²)";
  std::cout << "=== " << input << " ===" << std::endl; 
  LIMEX::Expression<double> expression(input,callables);
  std::cout << input << " = " << expression.evaluate({{5.0},{3.0},{4.0}}) << std::endl; 
  std::cout << expression.stringify() << std::endl; // Print the entire AST

  LIMEX::Expression<double> c("x in [1,2,3,y]",callables);
  std::cout << c.stringify() << std::endl; 
  std::cout << c.evaluate({{4.0},{4.0}}) << std::endl; 

  LIMEX::Expression<double> bounds("3 <= x < y",callables);
  std::cout << bounds.stringify() << std::endl; 
  std::cout << bounds.evaluate({{4.0},{4.0}}) << std::endl; 


  return 0;
}

