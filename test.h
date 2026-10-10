#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>

constexpr const char* RESET_COLOR = "\033[0m";
constexpr const char* GREEN_COLOR = "\033[32m";
constexpr const char* RED_COLOR   = "\033[31m";

void testParseError( std::string input ) {
  LIMEX::Handle<double> handle;
  try {
    LIMEX::Expression<double> expression(input,handle);
    std::cerr << "Parse '" << input << "' succeeded" << RED_COLOR << " [fail, expected error]" << RESET_COLOR << std::endl;
  }
  catch (const std::exception& e) {
    std::cerr << "Parse '" << input << "' fails: " << e.what() << GREEN_COLOR << " [pass]" << RESET_COLOR << std::endl;
  }
}

void test( std::string input,  double result ) {
  LIMEX::Handle<double> handle;
  try {
    LIMEX::Expression<double> expression(input,handle);
    std::cerr << input << " = " << expression.evaluate();
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
    // a path without steps names an array of the map, which is passed as a view of the vector held by the
    // map, which outlives the evaluation
    for ( auto& description : expression.getPaths() ) {
      if ( description.steps.empty() ) {
        std::cerr << description.name << " = [";
        for ( auto value : collectionMap.at(description.name) ) {
          std::cerr << value << ", ";
        }
        std::cerr << "] ";
      }
    }

    // a path of one index addresses an element of an array of the map, counting from one
    LIMEX::Resolver<double> resolver;
    resolver.collection = [&](size_t path, const std::vector<double>&) -> LIMEX::View<double> {
      return LIMEX::View<double>( collectionMap.at(expression.getPaths().at(path).name) );
    };
    resolver.value = [&](size_t path, const std::vector<double>& indices) -> double {
      const auto& description = expression.getPaths().at(path);
      if ( description.steps.size() != 1 || description.steps[0].has_value() ) {
        throw std::runtime_error("unsupported path '" + description.name + "'");
      }
      const auto& values = collectionMap.at(description.name);
      if ( indices[0] < 1 || indices[0] > (double)values.size() ) {
        throw std::runtime_error("illegal index for '" + description.name + "'");
      }
      return values[(size_t)indices[0] - 1];
    };

    std::cerr << "implies " << input << " = " << expression.evaluate(variableValues,resolver) ;
    if (expression.evaluate(variableValues,resolver) == result) {
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

// Evaluates an expression whose only array `X` is a computed view of `size` values, the value at
// position k being 2 * (k + 1), so that X = [2, 4, 6, ...] without being stored.
void testComputed( std::string input, size_t size, double result ) {
  LIMEX::Handle<double> handle;
  try {
    LIMEX::Expression<double> expression(input,handle);
    LIMEX::View<double> computed( size, [](size_t k) { return 2.0 * (double)(k + 1); } );
    // a path without steps names X, and a path of one index addresses an element of X
    LIMEX::Resolver<double> resolver;
    resolver.collection = [&](size_t path, const std::vector<double>&) -> LIMEX::View<double> {
      if ( expression.getPaths().at(path).name != "X" ) {
        throw std::runtime_error("unexpected array '" + expression.getPaths().at(path).name + "'");
      }
      return computed;
    };
    resolver.value = [&](size_t path [[maybe_unused]], const std::vector<double>& indices) -> double {
      return computed[(size_t)indices[0] - 1];
    };
    std::cerr << "X computed with " << size << " values implies " << input << " = " << expression.evaluate({},resolver);
    if (expression.evaluate({},resolver) == result) {
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

// Evaluates an expression with a custom aggregator `second`, returning the second value of a view.
void testCustomAggregator( std::string input, std::map<std::string,std::vector<double>> collectionMap, double result ) {
  LIMEX::Handle<double> handle;
  handle.addAggregator( "second", [](const LIMEX::View<double>& values) -> double {
    if ( values.size() < 2 ) throw std::runtime_error("second{} requires at least two values");
    return values[1];
  });
  try {
    LIMEX::Expression<double> expression(input,handle);
    LIMEX::Resolver<double> resolver;
    resolver.collection = [&](size_t path, const std::vector<double>&) -> LIMEX::View<double> {
      return LIMEX::View<double>( collectionMap.at(expression.getPaths().at(path).name) );
    };
    std::cerr << input << " = " << expression.evaluate({},resolver);
    if (expression.evaluate({},resolver) == result) {
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

// The data of the path tests: the arrays `location` and `partial` (with an undefined element), named by paths
// without steps, and, resolved through paths, `location` by index, the array of objects `facilities` with the
// fields `cost` and `flags`, and the matrix `grid` with grid[i][j] = 10 * i + j.
struct PathData {
  std::vector<double> location = { 3.0, 1.0, 2.0 };
  std::vector<double> partial = { 1.0, std::numeric_limits<double>::quiet_NaN(), 3.0 };
  std::vector<double> cost = { 10.0, 20.0, 30.0 };
  std::vector< std::vector<double> > flags = { { 1.0, 0.0 }, { 0.0, 1.0 }, { 1.0, 1.0 } };

  LIMEX::Resolver<double> resolver(const LIMEX::Expression<double>& expression, const std::map<std::string, std::vector<double>>& extra) const {
    LIMEX::Resolver<double> resolver;
    resolver.value = [this, &expression](size_t path, const std::vector<double>& indices) -> double {
      const auto& description = expression.getPaths().at(path);
      std::string shape;
      for ( const auto& step : description.steps ) {
        shape += step.has_value() ? "." + step.value() : "[]";
      }
      auto at = [](const std::vector<double>& values, double index) -> double {
        if ( index < 1 || index > (double)values.size() ) throw std::runtime_error("illegal index");
        return values[(size_t)index - 1];
      };
      if ( description.name == "location" && shape == "[]" ) return at(location, indices[0]);
      if ( description.name == "facilities" && shape == "[].cost" ) return at(cost, indices[0]);
      if ( description.name == "facilities" && shape == "[].flags[]" ) {
        if ( indices[0] < 1 || indices[0] > (double)flags.size() ) throw std::runtime_error("illegal index");
        return at(flags[(size_t)indices[0] - 1], indices[1]);
      }
      if ( description.name == "grid" && shape == "[][]" ) return 10.0 * indices[0] + indices[1];
      throw std::runtime_error("unsupported path '" + description.name + shape + "'");
    };
    resolver.collection = [this, &expression, &extra](size_t path, const std::vector<double>& indices) -> LIMEX::View<double> {
      const auto& description = expression.getPaths().at(path);
      if ( description.steps.empty() ) {
        if ( description.name == "location" ) return LIMEX::View<double>( location );
        if ( description.name == "partial" ) return LIMEX::View<double>( partial );
        return LIMEX::View<double>( extra.at(description.name) );
      }
      if ( description.name == "facilities" && description.steps.size() == 2 && description.steps[1] == "flags" ) {
        if ( indices[0] < 1 || indices[0] > (double)flags.size() ) throw std::runtime_error("illegal index");
        return LIMEX::View<double>( flags[(size_t)indices[0] - 1] );
      }
      throw std::runtime_error("unsupported array '" + description.name + "'");
    };
    return resolver;
  }

};

// Evaluates an expression with paths into the data above, the given variables and further arrays.
void testPaths( std::string input, std::map<std::string,double> valueMap, double result, std::map<std::string,std::vector<double>> collectionMap = {} ) {
  LIMEX::Handle<double> handle;
  PathData data;
  try {
    LIMEX::Expression<double> expression(input,handle);
    std::vector<double> variableValues;
    for ( auto variable : expression.getVariables() ) {
      variableValues.push_back( valueMap.at(variable) );
    }
    auto resolver = data.resolver(expression, collectionMap);
    double value = expression.evaluate(variableValues, resolver);
    std::cerr << input << " = " << value;
    if ( value == result ) {
      std::cerr << GREEN_COLOR << " [pass]" << RESET_COLOR << std::endl;
    }
    else {
      std::cerr << RED_COLOR << " [fail, expected " << result << "]" << RESET_COLOR << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Failed evaluating: " + input << std::endl;
    std::cerr << e.what() << RED_COLOR << " [fail]" << RESET_COLOR << std::endl;
  }
}

// Evaluates an assignment to a path: the value assigned, the target, the indices of the target path and the
// variables the expression reads.
void testTarget( std::string input, std::map<std::string,double> valueMap, double result, std::string target, std::vector<double> indices, std::vector<std::string> variables ) {
  LIMEX::Handle<double> handle;
  PathData data;
  try {
    LIMEX::Expression<double> expression(input,handle);
    std::vector<double> variableValues;
    for ( auto variable : expression.getVariables() ) {
      variableValues.push_back( valueMap.at(variable) );
    }
    std::map<std::string, std::vector<double>> none;
    auto resolver = data.resolver(expression, none);
    double value = expression.evaluate(variableValues, resolver);
    auto targetIndices = expression.evaluateTargetIndices(variableValues, resolver);
    std::cerr << input << " assigns " << value << " to " << expression.getTarget().value_or("?") << " at [";
    for ( auto index : targetIndices ) std::cerr << " " << index;
    std::cerr << " ]";
    if (
      value == result &&
      expression.getTarget() == target &&
      targetIndices == indices &&
      expression.getVariables() == variables &&
      expression.getTargetPath().has_value() == !indices.empty()
    ) {
      std::cerr << GREEN_COLOR << " [pass]" << RESET_COLOR << std::endl;
    }
    else {
      std::cerr << RED_COLOR << " [fail]" << RESET_COLOR << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Failed evaluating: " + input << std::endl;
    std::cerr << e.what() << RED_COLOR << " [fail]" << RESET_COLOR << std::endl;
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
  test("log(1)", std::log(1)); // function call
  test("exp(0)", std::exp(0)); // function call
  test("sum{1,2,3}", 1+2+3); // set operation
  test("∑{1,2,3}", 1+2+3); // sum and symbolic name
  test("abs(3)", 3); // set operation
  test("abs(-3)", 3); // set operation
  test("avg{1,2,3}", (double)(1+2+3)/3); // set operation
  test("count{1,2,3}", 3); // set operation
  test("min{1,2,3}", 1); // set operation
  test("max{1,2,3}", 3); // set operation
  test("sum{x, y, z}", { {"x", 1.0}, {"y", 2.0}, {"z", 3.0} }, 6.0); // set with variables
  testParseError("sum(a, b, c)"); // aggregator with () requires single collection
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
// External collection membership
  test("x in X", { {"x", 3.0} }, { {"X", {1.0, 2.0, 3.0}} }, true); // element_of with collection
  test("x in X", { {"x", 4.0} }, { {"X", {1.0, 2.0, 3.0}} }, false); // element_of with collection
  test("x not in X", { {"x", 3.0} }, { {"X", {1.0, 2.0, 3.0}} }, false); // not_element_of with collection
  test("x not in X", { {"x", 4.0} }, { {"X", {1.0, 2.0, 3.0}} }, true); // not_element_of with collection
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
  test("sum(X)", {}, { {"X", { 2.0, 5.0, 3.0} } }, 10.0);
  test("count(X)", {}, { {"X", { 2.0, 5.0, 3.0} } }, 3);
  test("min(X)", {}, { {"X", {3.0, 1.0, 2.0}} }, 1.0);
  test("max(X)", {}, { {"X", {3.0, 1.0, 2.0}} }, 3.0);
  test("avg(X)", {}, { {"X", {1.0, 2.0, 3.0}} }, 2.0); 

// Assignments
  test("x := 3", 3);
  test("x := !x", { {"x", 1.0} }, 0.0);
  test("x += 3", { {"x", 5.0} }, 8);
  test("x -= 3", { {"x", 5.0} }, 2);
  test("x *= 3", { {"x", 5.0} }, 15);
  test("x /= 3", { {"x", 5.0} }, 5.0/3);
  test("x /= 3 + 2", { {"x", 5.0} }, 1);
  test("x /= 3 > 2", { {"x", 5.0} }, 5);
  test("x /= if x > 3 then 2 else 1", { {"x", 5.0} }, 2.5);
  test("x /= if x > 3 then 2 else 1", { {"x", 2.0} }, 2);

// Aggregations over an expression with a binding over a collection
  test("sum{ i | i in I, i > 1 }", {}, { {"I", { -2.0, 3.0, 1.0, 5.0, 0.0 } } }, 8.0); // 3 + 5
  test("sum{ i | i in I, i >= 1 }", {}, { {"I", { -2.0, 3.0, 1.0, 5.0, 0.0 } } }, 9.0); // 3 + 1 + 5
  test("count{ i | i in I, i > 0 }", {}, { {"I", { -2.0, 3.0, -1.0, 5.0, 0.0 } } }, 2.0); // 3, 5
  test("min{ i | i in I, i > 0 }", {}, { {"I", { -2.0, 3.0, -1.0, 5.0, 0.0 } } }, 3.0);
  test("max{ i | i in I, i < 0 }", {}, { {"I", { -2.0, 3.0, -1.0, 5.0, 0.0 } } }, -1.0);
  test("avg{ i | i in I, i > 0 }", {}, { {"I", { -2.0, 3.0, -1.0, 5.0, 0.0 } } }, 4.0); // (3 + 5) / 2
  // Multiple conditions (AND-ed)
  test("sum{ i | i in I, i > 0, i < 5 }", {}, { {"I", { -2.0, 3.0, -1.0, 5.0, 0.0, 2.0 } } }, 5.0); // 3 + 2
  // With external variable
  test("sum{ i | i in I, i > x }", { {"x", 0.0} }, { {"I", { -2.0, 3.0, -1.0, 5.0 } } }, 8.0); // 3 + 5
  test("sum{ i | i in I, i > x }", { {"x", 3.0} }, { {"I", { -2.0, 3.0, -1.0, 5.0 } } }, 5.0); // 5
  // Iterator reuse in different aggregations
  test("sum{ i | i in I, i > 0 } + min{ i | i in I, i < 0 }", {}, { {"I", { -2.0, 3.0, -1.0, 5.0 } } }, 6.0); // (3+5) + (-2)
  // Nested filtered aggregations
  test("max{ i | i in I, i > min{ j | j in J, j > 0 } }", {}, { {"I", { 1.0, 2.0, 3.0, 4.0 }}, {"J", { -1.0, 2.0, 3.0 }} }, 4.0); // min{J|j>0}=2, max{I|i>2}=4
  test("sum{ i | i in I, i >= min{ j | j in J, j > 0 } }", {}, { {"I", { 1.0, 2.0, 3.0, 4.0 }}, {"J", { -1.0, 2.0, 3.0 }} }, 9.0); // min{J|j>0}=2, sum{I|i>=2}=2+3+4=9
  // Collection membership in conditions
  test("sum{ i | i in I, i in J }", {}, { {"I", { 1.0, 2.0, 3.0, 4.0 }}, {"J", { 2.0, 4.0, 6.0 }} }, 6.0); // 2 + 4
  test("sum{ i | i in I, i not in J }", {}, { {"I", { 1.0, 2.0, 3.0, 4.0 }}, {"J", { 2.0, 4.0, 6.0 }} }, 4.0); // 1 + 3

  // Throw on illegal/unsupported notation, including the former filtered aggregation
  testParseError("min{ d in sqrt(x) | d >= 0 }");
  testParseError("sum{ i in I | i > 1 }");
  testParseError("sum{ i | i in sqrt(x) }");
  testParseError("sum{ i | i > 1 }");
  testParseError("sum{ | i in 1..3 }");
  testParseError("1..3");
  testParseError("x | y");
  testParseError("sum{ i[1] | i in 1..3 }");

// Paths
  testPaths("location[2]", {}, 1.0);
  testPaths("location[i]", { {"i", 3.0} }, 2.0);
  testPaths("facilities[2].cost", {}, 20.0);
  testPaths("facilities[i].flags[j]", { {"i", 3.0}, {"j", 2.0} }, 1.0);
  testPaths("grid[2][3]", {}, 23.0);
  testPaths("grid[1 + 1][location[3]]", {}, 22.0);
  testPaths("facilities[location[1]].cost", {}, 30.0); // location[1] = 3
  testPaths("facilities[2].cost²", {}, 400.0);
  testPaths("sum(facilities[3].flags)", {}, 2.0);
  testPaths("1 in facilities[1].flags", {}, 1.0);
  testPaths("1 not in facilities[2].flags", {}, 0.0);
  testPaths("x + 0.5 * facilities[1].cost", { {"x", 1.0} }, 6.0);

// Aggregations over an expression
  testPaths("sum{ facilities[i].cost | i in 1..3 }", {}, 60.0);
  testPaths("sum{ facilities[i].cost | i in 1..3, i < 3 }", {}, 30.0);
  testPaths("sum{ grid[i][j] | i in 1..2, j in 1..3 }", {}, 102.0); // 11 + 12 + 13 + 21 + 22 + 23
  testPaths("sum{ grid[i][j] | i in 1..2, j in i..3 }", {}, 81.0); // 11 + 12 + 13 + 22 + 23
  testPaths("sum{ grid[i][j] | i in 1..2, j in 1..3, i == j }", {}, 33.0); // 11 + 22
  testPaths("count{ i | i in 1..size(location), location[i] == 1 }", {}, 1.0);
  testPaths("sum{ i * location[i] | i in 1..size(location) }", {}, 11.0); // 3 + 2 + 6
  testPaths("sum{ f | f in facilities[3].flags }", {}, 2.0);
  testPaths("max{ sum{ grid[i][j] | j in 1..3 } | i in 1..2 }", {}, 66.0); // 21 + 22 + 23
  testPaths("sum{ i | i in 1..3, (y in location) }", { {"y", 5.0} }, 0.0);
  testPaths("sum{ i | i in 1..3, (y in location) }", { {"y", 3.0} }, 6.0);
  testPaths("sum{ i | i in 1..2 } + sum{ i | i in 1..3 }", {}, 9.0);
  testPaths("sum{ i | i in I, i > 0 } + y", { {"y", 10.0} }, 18.0, { {"I", { -2.0, 3.0, -1.0, 5.0 }} });
  testPaths("sum{ i | i in n..m }", { {"n", 3.0}, {"m", 2.0} }, 0.0); // empty range
  testPaths("sum{ i | i in 1.5..3 }", {}, 5.0); // 2 + 3

// Undefined elements
  testPaths("sum(partial)", {}, 4.0);
  testPaths("count(partial)", {}, 2.0);
  testPaths("size(partial)", {}, 3.0);
  testPaths("avg(partial)", {}, 2.0);
  testPaths("min(partial)", {}, 1.0);
  testPaths("max(partial)", {}, 3.0);
  testPaths("sum{ p | p in partial }", {}, 4.0);
  testPaths("count{ p | p in partial }", {}, 2.0);
  testPaths("sum{ 1 | p in partial, p }", {}, 2.0);
  testPaths("sum{ 1 | p in partial, p - 1 }", {}, 1.0);

// Assignments to paths
  testTarget("location[i] := j", { {"i", 2.0}, {"j", 7.0} }, 7.0, "location", { 2.0 }, { "i", "j" });
  testTarget("facilities[i].cost += 5", { {"i", 1.0} }, 15.0, "facilities", { 1.0 }, { "i" });
  testTarget("facilities[location[1]].flags[2] := 0", {}, 0.0, "facilities", { 3.0, 2.0 }, {});
  testTarget("x := 3", {}, 3.0, "x", {}, {});

// Views
  // computed views, X = [2, 4, 6, 8, 10]
  testComputed("sum(X)", 5, 30.0);
  testComputed("count(X)", 5, 5.0);
  testComputed("max(X)", 5, 10.0);
  testComputed("X[3]", 5, 6.0);
  testComputed("6 in X", 5, 1.0);
  testComputed("7 in X", 5, 0.0);
  testComputed("7 not in X", 5, 1.0);
  testComputed("sum{ x | x in X, x > 4 }", 5, 24.0); // 6 + 8 + 10
  testComputed("count(X)", 0, 0.0);
  // custom aggregator receiving a view, of a collection and of explicit arguments
  testCustomAggregator("second(X)", { {"X", { 7.0, 8.0, 9.0 }} }, 8.0);
  testCustomAggregator("second{ 3, 4, 5 }", {}, 4.0);
}
