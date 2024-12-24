#include <iostream>
#include <cassert>

#include "limex.h"

#include "test.h"

int main()
{
  LIMEX::Expression<double>::createBuiltInCallables();
  test();
  std::string input = "z -= √(x²+y²)";
  std::cout << "=== " << input << " ===" << std::endl; 
  LIMEX::Expression<double> expression(input);
  std::cout << input << " = " << expression.evaluate({{5.0},{3.0},{4.0}}) << std::endl; 
  std::cout << expression.stringify() << std::endl; // Print the entire AST
  return 0;
}

