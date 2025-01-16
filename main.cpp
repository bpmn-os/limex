#include <iostream>
#include <cassert>

#include "limex.h"

#include "test.h"

int main()
{
  LIMEX::Expression<double>::initialize();
  test();
  std::string input = "z -= √(x²+y²)";
  std::cout << "=== " << input << " ===" << std::endl; 
  LIMEX::Expression<double> expression(input);
  std::cout << input << " = " << expression.evaluate({{5.0},{3.0},{4.0}}) << std::endl; 
  std::cout << expression.stringify() << std::endl; // Print the entire AST

  LIMEX::Expression<double> c("x in [1,2,3,y]");
  std::cout << c.stringify() << std::endl; 
  std::cout << c.evaluate({{4.0},{4.0}}) << std::endl; 
  LIMEX::Expression<double>::free();

  LIMEX::Expression<double> bounds("3 <= x < y");
  std::cout << bounds.stringify() << std::endl; 
  std::cout << bounds.evaluate({{4.0},{4.0}}) << std::endl; 
  LIMEX::Expression<double>::free();
  
  return 0;
}

