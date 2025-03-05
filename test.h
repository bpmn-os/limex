#include <cassert>
#include <iostream>
#include <map>

constexpr const char* RESET_COLOR = "\033[0m";
constexpr const char* GREEN_COLOR = "\033[32m";
constexpr const char* RED_COLOR   = "\033[31m";

void test( std::string input,  double result ) {
  LIMEX::Handle<double> handle;
  try {
    LIMEX::Expression<double> expression(input,handle);
    std::cerr << input << " = " << expression.evaluate() ;
    if (expression.evaluate() == result) {
      std::cerr << GREEN_COLOR << " [pass]" << RESET_COLOR << std::endl;
    }
    else {
      std::cerr << RED_COLOR << " [fail, expected " << result << "]" << RESET_COLOR << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Failed evaluating: " + input << std::endl;
    std::cerr << e.what() << std::endl;
  }
}

void test( std::string input, std::map<std::string,double> valueMap, double result ) {
  LIMEX::Handle<double> handle;
  try {
    LIMEX::Expression<double> expression(input,handle);
    std::vector<double> variableValues;
    for ( auto variable : expression.getVariables() ) {
      std::cerr << variable << " = " << valueMap.at(variable) << " ";    
      variableValues.push_back( valueMap.at(variable) );
    }
    std::cerr << "implies " << input << " = " << expression.evaluate(variableValues) ;
    if (expression.evaluate(variableValues) == result) {
      std::cerr << GREEN_COLOR << " [pass]" << RESET_COLOR << std::endl;
    }
    else {
      std::cerr << RED_COLOR << " [fail, expected " << result << "]" << RESET_COLOR << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Failed evaluating: " + input << std::endl;
    std::cerr << e.what() << std::endl;
  }
}

void test( std::string input, std::map<std::string,double> valueMap, std::map<std::string,std::vector<double>> collectionMap, double result ) {
  LIMEX::Handle<double> handle;
  try {
    LIMEX::Expression<double> expression(input,handle);
    std::vector<double> variableValues;
    for ( auto variable : expression.getVariables() ) {
      std::cerr << variable << " = " << valueMap.at(variable) << " ";    
      variableValues.push_back( valueMap.at(variable) );
    }
    std::vector< std::vector<double> > collectionValues;
    for ( auto collection : expression.getCollections() ) {
      std::cerr << collection << " = [";
      for ( auto value : collectionMap.at(collection) ) {
        std::cerr << value << ", ";    
      }
      std::cerr << "] ";    
      collectionValues.push_back( collectionMap.at(collection) );
    }

    std::cerr << "implies " << input << " = " << expression.evaluate(variableValues,collectionValues) ;
    if (expression.evaluate(variableValues,collectionValues) == result) {
      std::cerr << GREEN_COLOR << " [pass]" << RESET_COLOR << std::endl;
    }
    else {
      std::cerr << RED_COLOR << " [fail, expected " << result << "]" << RESET_COLOR << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Failed evaluating: " + input << std::endl;
    std::cerr << e.what() << std::endl;
  }
}

void test() {
// Literals
  test("3*5", 3*5); // multiply
  test("-3*-5", -3*-5); // multiply and negate
  test("3/5/2", (double)3/5/2); // divide
  test("3 - 5 + 2", 3 - 5 + 2); // add and subtract
  test("2^3^2", std::pow(std::pow(2,3),2)); // exponentiate
  test("2^.5", std::pow(2,0.5)); // exponentiate
  test("3²", 3*3); // square
  test("2³", 2*2*2); // cube
  test("2^.5²", std::pow(2,.5*.5) ); // square and exponentiate
  test("sqrt(9)", std::sqrt(9)); // sqrt and function call
  test("√(9)", std::sqrt(9)); // sqrt and symbolic name
  test("cbrt(8)", std::cbrt(8)); // cbrt and function call
  test("∛(8)", std::cbrt(8)); // cbrt and symbolic name
  test("pow(2,3)", std::pow(2,3)); // function call
  test("sum{1,2,3}", 1+2+3); // set operation
  test("∑{1,2,3}", 1+2+3); // sum and symbolic name
  test("abs(3)", 3); // set operation
  test("abs(-3)", 3); // set operation
  test("avg{1,2,3}", (double)(1+2+3)/3); // set operation
  test("count{1,2,3}", 3); // set operation
  test("min{1,2,3}", 1); // set operation
  test("max{1,2,3}", 3); // set operation
  test("5*4²", 5*4*4); // postfix and infix
  test("!2³", !std::pow(2,3)); // prefix and postfix
  test("-2³ * 2", -std::pow(2,3) * 2 ); // prefix, postfix and infix
  test("8/(2³-(8-2))", 8/(std::pow(2,3)-(8-2))); // group
  //test("{1,2,3}", false); // sets cannot be evaluated
  test("2 in {1,2,3}", true); // element_of
  test("2 ∈ {1,2,3}", true); // element_of
  test("4 ∈ {1,2,3}", false); // element_of
  test("2 not in {1,2,3}", false); // not_element_of
  test("2 ∉ {1,2,3}", false); // not_element_of
  test("4 ∉ {1,2,3}", true); // not_element_of
  test("true and false", false); // logical_and
  test("true && false", false); // logical_and
  test("true ∧ false", false); // logical_and
  test("true and true", true); // logical_and
  test("true && true", true); // logical_and
  test("true ∧ true", true); // logical_and
  test("true or false", true); // logical_or
  test("true || false", true); // logical_or
  test("true ∨ false", true); // logical_or
  test("false or false", false); // logical_or
  test("false || false", false); // logical_or
  test("false ∨ false", false); // logical_or
  test("true ? 1 : -1", 1); // if_then_else
  test("false ? 1 : -1", -1); // if_then_else
  test("true ? 1 : false ? 0 : -1", 1); // if_then_else
  test("true ? 1 : true ? 0 : -1", 1); // if_then_else
  test("false ? 1 : false ? 0 : -1", -1); // if_then_else
  test("false ? 1 : true ? 0 : -1", 0); // if_then_else
  test("if true then 1 else -1", 1); // if_then_else
  test("if true then 1 else if true then 0 else -1", 1); // if_then_else
  test("if true then 1 else if false then 0 else -1", 1); // if_then_else
  test("if false then 1 else if true then 0 else -1", 0); // if_then_else
  test("if false then 1 else if false then 0 else -1", -1); // if_then_else
  test("n_ary_if(1 < 2, 3, 4 < 5, 6, 7)", 3); // n_ary_if(condition1, result1, condition2, result2, elseResult)
  test("n_ary_if(1 > 2, 3, 4 < 5, 6, 7)", 6); // n_ary_if(condition1, result1, condition2, result2, elseResult)
  test("n_ary_if(1 > 2, 3, 4 > 5, 6, 7)", 7); // n_ary_if(condition1, result1, condition2, result2, elseResult)
  
  test("1<2", 1); // less_than
  test("2<2", 0); // less_than
  test("2<=2", 1); // less_or_equal
  test("3<=2", 0); // less_or_equal
  test("3>2", 1); // greater_than
  test("2>2", 0); // greater_than
  test("2>=2", 1); // greater_or_equal
  test("1>=2", 0); // greater_or_equal
  test("2==2", 1); // equal_to
  test("1==2", 0); // equal_to
  test("1!=2", 1); // not_equal_to
  test("2!=2", 0); // not_equal_to

  test("(1>0) ? 1 : 2", 1);
  test("(2<1) ? 1 : -1", -1); 
  test("1>0 ? 1 : 2", 0);
  test("2<3 ? 4 : -1", 1); 
  test("0 ? 4 : 3 > 2", 1);
  test("3 ? 4 : -1 > 2", 1); 
  test("if 1>0 then 1 else 2", 1);
  test("if 2<1 then 1 else -1", -1); 
  test("3 + false ? 4 : -1", 2); 
  test("3 + true ? 4 : -1", 7); 
  test("3 + false ? 4 : -1 * 3", 3 + (false ? 4 : (-1 * 3))); 
  test("6 + true ? 4 : -1 * 3", 6 + (true ? 4 : (-1 * 3))); 
  test("3 + if false then 4 else -1 * 3", 3 + (false ? 4 : (-1 * 3))); 
  test("6 + if true then 4 else -1 * 3", 6 + (true ? 4 : (-1 * 3))); 

// Variables
  test("3*x", { {"x", 5.0} }, 3*5);
  test("x - y + z", { {"z", 2.0}, {"x", 3.0}, {"y", 5.0} }, 3 - 5 + 2); 

// Collections
  test("x[2]", {}, { {"x", { 2.0, 5.0, 3.0} } }, 5.0); 
  test("y + x[5-2]", { {"y", 5.0} }, { {"x", { 2.0, 5.0, 3.0} } }, 8.0);
  test("sum{collection[]}", {}, { {"collection", { 2.0, 5.0, 3.0} } }, 10.0); 
  test("count(collection[])", {}, { {"collection", { 2.0, 5.0, 3.0} } }, 3); 

// Assignments
  test("x := 3", 3);
  test("x += 3", { {"x", 5.0} }, 8);
  test("x -= 3", { {"x", 5.0} }, 2);
  test("x *= 3", { {"x", 5.0} }, 15);
  test("x /= 3", { {"x", 5.0} }, 5.0/3);
  test("x /= 3 + 2", { {"x", 5.0} }, 1);
  test("x /= 3 > 2", { {"x", 5.0} }, 5);
  test("x /= if x > 3 then 2 else 1", { {"x", 5.0} }, 2.5);
  test("x /= if x > 3 then 2 else 1", { {"x", 2.0} }, 2);
}
