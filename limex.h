#ifndef LIMEX_H
#define LIMEX_H

#include <iostream>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <variant>
#include <stdexcept>
#include <functional>
#include <array>
#include <stack>
#include <cmath>
#include <cfloat>
/**
 * A library for parsing mathematical expressions
 **/
namespace LIMEX {

/**
 * @brief Represents a token in the analysis of an expression to be parsed.
 * 
 * A token can be of different categories and types, and may contain nested tokens (children). 
 */
struct Token {
  enum class Category { PREFIX, OPERAND, POSTFIX, INFIX }; /// Categories of input to be parsed by lexer
  enum class Type { NUMBER, VARIABLE, COLLECTION, OPERATOR, SEPARATOR, GROUP, SET, SEQUENCE, FUNCTION_CALL, AGGREGATION, INDEXED_VARIABLE };    
  Token(Category category, Type type, std::string value = "") : category(category), type(type), value(std::move(value)) {}
  Category category;
  Type type;
  std::string value;
  std::vector<Token> children; // for nested tokens (GROUP, SET, FUNCTION_CALL, AGGREGATION, INDEXED_VARIABLE)
  inline std::string stringify(int indent = 0) const;
};

enum class Type; /// Types of nodes in the abstract syntax tree

template <typename T, typename C = std::vector<T> > class Expression;

/**
 * @brief Represents a node in the abstract syntax tree of an expression.
 * 
 * Each node can represent either a literal value, a variable, or a complex expression with operands. 
 * It supports evaluation based on provided
 * variable and collection values, and can generate a string representation of the expression.
 * 
 * @tparam T The type of the value held by the node (e.g., double).
 */
template <typename T, typename C = std::vector<T> >
class Node {
public:
  Expression<T,C>* expression;
  Type type;
  std::vector< std::variant<double, size_t, Node> > operands;
  // Constructor for a literal node
  Node(Expression<T,C>* expression, double value);
  // Constructor for a variable or collection node
  Node(Expression<T,C>* expression, Type type, std::string name);
  // Constructor for a node with multiple operands
  Node(Expression<T,C>* expression, Type type, std::vector< std::variant<double, size_t, Node> > operands);
  // Templated deep copy constructor
  template <typename U>
  Node(Expression<T,C>* expression, const Node<U>& other);
  // Evaluate the node
  inline T evaluate( const std::vector<T>& variableValues = {}, const std::vector<C>& collectionValues = {}) const;
  std::string stringify() const;
};

template <typename T, typename C = std::vector<T> >
class Handle {
friend class Node<T,C>;
friend class Expression<T,C>;
public:
  Handle() { initialize(); };
  inline void add(const std::string& name, std::function<T(const std::vector<T>&)> implementation);
  inline const std::vector<std::string>& getNames() const { return names; }
  inline size_t getIndex(const std::string& name) const;
  inline T indexedEvaluation( const C& collection, const T& index ) const; 
  inline T aggregateEvaluation( const std::string& name, const C& collection ) const; 
private:
  inline void initialize();
  std::vector<std::function<T(const std::vector<T>&)>> implementations;
  std::vector<std::string> names;
};

/**
 * @brief Represents a mathematical expression that can be evaluated for different values.
 * 
 * This class is used to parse a string representing a mathemtical expression, build an abstract syntax tree, 
 * and evaluate it based on the provided variable and collection values. The expression can include 
 * variables, functions, operators, and other mathematical operations, which are processed and executed 
 * during evaluation. Users can defined custom functions and set operators to be used when evaluating an 
 * expression. Several built-in functions and set operators must be made available through an appropriate 
 * @ref `Handle`.
 * 
 * @tparam T The type of the value used in the expression (e.g., double).
 */
template <typename T, typename C >
class Expression {
friend class Node<T,C>;
public:
  Expression(const std::string& expression, const Handle<T,C>& handle);
  enum class BUILTIN { IF_THEN_ELSE, N_ARY_IF, ABS, POW, SQRT, CBRT, SUM, AVG, COUNT, MIN, MAX, ELEMENT_OF, NOT_ELEMENT_OF, AT, BUILTINS };
  inline const std::vector<std::string>& getVariables() const { return variables; }
  inline const std::vector<std::string>& getCollections() const { return collections; }
  inline const std::optional<std::string>& getTarget() const { return target; }
  inline T evaluate( const std::vector<T>& variableValues = {}, const std::vector<C>& collectionValues = {}) const;
  inline const Node<T,C>& getRoot() const { return root; }
  const std::string input;
  inline std::string stringify() const;
private:
  const Handle<T,C>& handle;
  std::vector<std::string> variables;
  std::vector<std::string> collections;
  std::optional<std::string> target;
  Node<T,C> root;
  inline Node<T,C> parse();
  inline static Token tokenize(const std::string& input);
  inline static bool isnumeric(char c) { return (std::isdigit( c ) || c == '.'); }; 
  inline static bool isalphanumeric(char c) { return (std::isalnum(c) || c == '_'); }; 
  inline size_t getIndex(std::vector<std::string>& container, std::string name);
  inline static size_t skipWhitespaces(const std::string& input, size_t& pos);
  inline static bool startsWith(const std::string& input, size_t pos, std::string_view candidate);
  template <size_t N>
  inline static std::string fetch(const std::string& input, size_t start, const std::array<std::string_view, N>& prefix);
  Node<T,C> buildTree( Type type, const std::vector<Token>& tokens, std::optional<size_t> index = std::nullopt );
};

enum class Type {
    literal, // a given number
    variable, // a named variable
    collection, // a named collection
    group, // a block encapsulated in '(' and ')'
    set, // a block encapsulated in '{' and '}'
    sequence, // a block encapsulated in '[' and ']'
    function_call,  // a function call of the form '<function_name>(...)'
    aggregation, // an aggregate operation of the form '<operation_name>{...}'
    index,  // an indexing operation of the form '<variable_name>[...]'
    negate,
    logical_not,
    logical_and,
    logical_or,
    add,
    subtract,
    multiply,
    divide,
    exponentiate,
    square,
    cube,
    less_than,
    less_or_equal,
    greater_than,
    greater_or_equal,
    equal_to,
    not_equal_to,
    element_of,
    not_element_of,
    if_, // helper only for parsing, for node type use if_then_else
    _then_, // helper only for parsing, for node type use if_then_else
    _else, // helper only for parsing, for node type use if_then_else
    if_then_else,
    assign,
    add_assign,
    subtract_assign,
    multiply_assign,
    divide_assign
}; /// Types of nodes in the abstract syntax tree

constexpr auto typeName = std::to_array<std::string_view> ({ 
    "literal", // a given number
    "variable", // a named variable
    "collection", // a named collection
    "group", // a block encapsulated in '(' and ')'
    "set", // a block encapsulated in '{' and '}'
    "sequence", // a block encapsulated in '[' and ']'
    "function_call",  // a function call of the form '<function_name>(...)'
    "aggregation", // an aggregate operation of the form '<operation_name>{...}'
    "index",  // an indexing operation of the form '<variable_name>[...]'
    "negate",
    "logical_not",
    "logical_and",
    "logical_or",
    "add",
    "subtract",
    "multiply",
    "divide",
    "exponentiate",
    "square",
    "cube",
    "less_than",
    "less_or_equal",
    "greater_than",
    "greater_or_equal",
    "equal_to",
    "not_equal_to",
    "element_of",
    "not_element_of",
    "if_", // helper only for parsing, for node type use if_then_else
    "_then_", // helper only for parsing, for node type use if_then_else
    "_else", // helper only for parsing, for node type use if_then_else
    "if_then_else",
    "assign",
    "add_assign",
    "subtract_assign",
    "multiply_assign",
    "divide_assign"
});


constexpr auto keywords = std::to_array<std::string_view> ({ "false", "true" });
constexpr auto ternary = std::to_array<std::string_view> ({ "if", "then", "else" });
// Operator lists
constexpr auto prefix = std::to_array<std::string_view>({ "!", "¬", "-" });
constexpr auto infix = std::to_array<std::string_view>({ ",", "==", "!=", "<=", ">=", "<", ">", ":=", "≔", "+=", "-=", "*=", "/=", "+", "-", "*", "/", "^",  "&&", "||", "?", ":", "and", "or", "in", "not in", "≠", "≤", "≥", "∧", "∨", "∈", "∉" });
constexpr auto postfix = std::to_array<std::string_view>({ "²", "³" });
constexpr auto symbolic_names = std::to_array<std::string_view>({ "∑", "√", "∛" });
const std::unordered_map<std::string, std::string> aliases = {
  { "∑", "sum" },
  { "√", "sqrt" },
  { "∛", "cbrt" }
};

const std::unordered_map<std::string, Type> prefixTypes = {
    {"-", Type::negate},
    {"!", Type::logical_not},
    {"¬", Type::logical_not},
    {"if", Type::if_}
};

const std::unordered_map<std::string, Type> infixTypes = {
    {"&&", Type::logical_and},
    {"∧", Type::logical_and},
    {"and", Type::logical_and},
    {"||", Type::logical_or},
    {"∨", Type::logical_or},
    {"or", Type::logical_or},
    {"+", Type::add},
    {"-", Type::subtract},
    {"*", Type::multiply},
    {"/", Type::divide},
    {"^", Type::exponentiate},
    {"<", Type::less_than},
    {"<=", Type::less_or_equal},
    {"≤", Type::less_or_equal},
    {">", Type::greater_than},
    {">=", Type::greater_or_equal},
    {"≥", Type::greater_or_equal},
    {"==", Type::equal_to},
    {"!=", Type::not_equal_to},
    {"≠", Type::not_equal_to},
    {"in", Type::element_of},
    {"∈", Type::element_of},
    {"not in", Type::not_element_of},
    {"∉", Type::not_element_of},
    {std::string(ternary[1]), Type::_then_},
    {std::string(ternary[2]), Type::_else},
    {"?", Type::_then_},
    {":", Type::_else},
    {":=", Type::assign},
    {"≔", Type::assign},
    {"+=", Type::add_assign},
    {"-=", Type::subtract_assign},
    {"*=", Type::multiply_assign},
    {"/=", Type::divide_assign},
};

const std::unordered_map<std::string, Type> postfixTypes = {
    {"²", Type::square},
    {"³", Type::cube},
};

const std::unordered_map<Type, unsigned int> precedences = {
    {Type::group, 1},
    {Type::set, 1},

    {Type::function_call, 1},
    {Type::aggregation, 1},
    {Type::index, 1},

    {Type::square, 2},
    {Type::cube, 2},
    {Type::exponentiate, 2},

    {Type::negate, 3},
    {Type::logical_not, 3},

    {Type::multiply, 4},
    {Type::divide, 4},
    {Type::logical_and, 4},

    {Type::add, 5},
    {Type::subtract, 5},
    {Type::logical_or, 5},

    {Type::if_, 6}, // if-then-else is right associative
    {Type::_then_, 6}, // if-then-else is right associative
    {Type::_else, 6}, // if-then-else is right associative

    {Type::less_than, 7},
    {Type::greater_than, 7},
    {Type::less_or_equal, 7},
    {Type::greater_or_equal, 7},
    {Type::equal_to, 7},
    {Type::not_equal_to, 7},
    {Type::element_of, 7},
    {Type::not_element_of, 7},

    {Type::assign, 8},
    {Type::add_assign, 8},
    {Type::subtract_assign, 8},
    {Type::multiply_assign, 8},
    {Type::divide_assign, 8},
};

/*******************************
 ** Token
 *******************************/

inline std::string Token::stringify(int indent) const {
  std::string result;
  for (int i = 0; i < indent; ++i) result += "  ";
        
  result += "Category: ";
  result += (
    category == Category::PREFIX ? "PREFIX" : 
    category == Category::OPERAND ? "OPERAND" : 
    category == Category::POSTFIX ? "POSTFIX" : 
    "INFIX"
  );
  result += ", Type: ";
  switch (type) {
    case Type::NUMBER: result += "NUMBER"; break;
    case Type::VARIABLE: result += "VARIABLE"; break;
    case Type::COLLECTION: result += "COLLECTION"; break;
    case Type::OPERATOR: result += "OPERATOR"; break;
    case Type::SEPARATOR: result += "SEPARATOR"; break;
    case Type::GROUP: result += "GROUP"; break;
    case Type::SET: result += "SET"; break;
    case Type::SEQUENCE: result += "SEQUENCE"; break;
    case Type::FUNCTION_CALL: result += "FUNCTION_CALL"; break;
    case Type::AGGREGATION: result += "AGGREGATION"; break;
    case Type::INDEXED_VARIABLE: result += "INDEXED_VARIABLE"; break;
  }
  result += ", Value: " + value + '\n';

  if (!children.empty()) {
    for (const auto& child : children) {
      result += child.stringify(indent + 1);
    }
  }
  return result;
}


/*******************************
 ** Node
 *******************************/

template <typename T, typename C>
Node<T,C>::Node(Expression<T,C>* expression, double value)
: expression(expression), type(Type::literal)
{
  operands.emplace_back(std::move(value));
}

// Constructor for a variable or collection node
template <typename T, typename C>
Node<T,C>::Node(Expression<T,C>* expression, Type type, std::string name)
: expression(expression), type(type)
{
  if ( type == Type::variable ) {
    operands.emplace_back(expression->getIndex(expression->variables,name));
  }
  else if ( type == Type::collection ) {
    operands.emplace_back(expression->getIndex(expression->collections,name));
  }
  else {
    throw std::logic_error("LIMEX: Unexpected node type");
  }
}

template <typename T, typename C>
Node<T,C>::Node(Expression<T,C>* expression, Type type, std::vector< std::variant< double, size_t, Node<T,C> > > operands)
: expression(expression), type(type), operands(std::move(operands)) {}

template <typename T, typename C>
template <typename U>
Node<T,C>::Node(Expression<T,C>* expression, const Node<U>& other)
: expression(expression), type(other.type) 
{
  operands.reserve(other.operands.size());
  for (const auto& operand : other.operands) {
    if (std::holds_alternative<double>(operand)) {
      operands.emplace_back(std::get<double>(operand));
    }
    else if (std::holds_alternative<size_t>(operand)) {
      operands.emplace_back(std::get<size_t>(operand));
    }
    else if (std::holds_alternative< Node<U> >(operand)) {
      const auto& other_node = std::get< Node<U> >(operand);
      operands.emplace_back(other_node);
    }
  }
}

template <typename T, typename C>
inline T Node<T,C>::evaluate( const std::vector<T>& variableValues, const std::vector<C>& collectionValues) const {
//std::cerr << "Type: "<< typeName[(int)type] << std::endl;
  switch (type) {
    case Type::group:
      return std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
    case Type::set:
      throw std::runtime_error("LIMEX: Sets cannot be evaluated");
    case Type::sequence:
      throw std::runtime_error("LIMEX: Sequences cannot be evaluated");
    case Type::literal:
      return std::get<double>(operands[0]);
    case Type::variable: {
      return variableValues[std::get<size_t>(operands[0])];
    }
    case Type::collection: {
      throw std::runtime_error("LIMEX: Collections cannot be evaluated");
    }
    case Type::negate: {
      auto operandValue = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      return -operandValue;
    }

    case Type::logical_not: {
      auto operandValue = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      return !operandValue;
    }

    case Type::logical_and: {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left && right;
    }

    case Type::logical_or: {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left || right;
    }
    case Type::add: {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left + right;
    }
    case Type::subtract: {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left - right;
    }

    case Type::multiply: {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left * right;
    }
    case Type::divide: {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      
      if constexpr (std::is_arithmetic_v<T>) {
        if (right == 0) {
          throw std::runtime_error("LIMEX: Division by zero");
        }
      }

      return left / right;
    }
    case Type::square: {
      auto value = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      return value * value;
    }
    case Type::cube: {
      auto value = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      return value * value * value;
    }
    case Type::exponentiate: {
      auto index = (size_t)Expression<T,C>::BUILTIN::POW;
      if ( index >= expression->handle.getNames().size()) {
        throw std::runtime_error("LIMEX: Callable index out of range");
      }
      // Collect all evaluated arguments
      std::vector<T> arguments;
      for (size_t i = 0; i < operands.size(); ++i) {
        arguments.push_back(
          std::get<Node>(operands[i]).evaluate(variableValues,collectionValues)
        );
      }
      // Call the custom callable
      return expression->handle.implementations[index](arguments);
    }
    case Type::function_call: 
    case Type::aggregation: 
    {
      size_t index = std::get<size_t>(operands[0]);
      if (index >= expression->handle.getNames().size()) {
        throw std::runtime_error("LIMEX: Callable index out of range");
      }
      if ( index == (size_t)Expression<T,C>::BUILTIN::AT ) {
        if constexpr (std::is_same_v< C, std::vector<T> >) {
          throw std::logic_error("LIMEX: unexpected use of built-in 'at' for double");
        }
        else if constexpr (std::is_same_v< C, T >) {
          // custom index operation
          auto& collection = std::get<size_t>( std::get<Node>(operands[1]).operands[0] );
          auto collectionIndex = std::get<Node>(operands[2]).evaluate(variableValues,collectionValues);
          return expression->handle.indexedEvaluation( collectionValues[collection], collectionIndex );
        }
        else {
          static_assert([]{ return false; }(), "LIMEX: unexpected collection type");
        }
      }          
      else if (
        operands.size() == 2 && 
        std::holds_alternative<Node>(operands[1]) &&
        std::get<Node>(operands[1]).type == Type::collection
      ) {
        // argument is a collection
        auto collection = std::get<size_t>(std::get<Node>(operands[1]).operands[0]);
        
        if constexpr (std::is_same_v< C, std::vector<T> >) {
          // collection type C is vector<T>
          return expression->handle.implementations[index](collectionValues[collection]);
        }
        else if constexpr (std::is_same_v< C, T >) {
          // collection type C is T
          return expression->handle.aggregateEvaluation( expression->handle.getNames()[index], collectionValues[collection] );
        }
        else {
          static_assert([]{ return false; }(), "LIMEX: unexpected collection type");
        }
      }
      else {
        // explicitly collect all evaluated arguments
        std::vector<T> arguments;
        for (size_t i = 1; i < operands.size(); ++i) {
           arguments.push_back(
            std::get<Node>(operands[i]).evaluate(variableValues,collectionValues)
          );
        }
        // Call the custom callable
        return expression->handle.implementations[index](arguments);
      }
    }
    case Type::index: 
    {
      size_t collection = std::get<size_t>(operands[0]);
      if (collection >= expression->getCollections().size()) {
        throw std::runtime_error("LIMEX: Illegal reference to collection");
      }
      if (collection >= collectionValues.size()) {
        throw std::runtime_error("LIMEX: Insufficient collections provided");
      }
      if (!std::holds_alternative<Node>(operands[1])) {
        throw std::logic_error("LIMEX: Unexpected operand");
      }
      if constexpr (std::is_same_v< C, std::vector<T> >) {
        // collection type C is vector<T>
        if ( std::get<Node>(operands[1]).type == Type::literal ) {
          // index is given as a literal 
          auto value = std::get<double>(std::get<Node>(operands[1]).operands[0]);        
          auto index = (size_t)value - 1;
          if (index >= collectionValues[collection].size()) {
            throw std::runtime_error("LIMEX: Illegal index for collection");
          }
          return collectionValues[collection][index];
        }
        else {
          // index is not given as a literal and has to be determined through evaluation
          auto value = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues); 
                 
          if constexpr (std::is_arithmetic_v<T>) {
            // arithmetic value can be cast to index 
            auto index = (size_t)value - 1;
            if (index >= collectionValues[collection].size()) {
              throw std::runtime_error("LIMEX: Illegal index for collection");
            }
            return collectionValues[collection][index];
          }
          else if constexpr ( requires { std::declval<T>() == std::declval<T>(); } ) {
            // operator== is available for T and n-ary if statement can be constructed
            auto index = (size_t)Expression<T,C>::BUILTIN::N_ARY_IF;
            if ( index >= expression->handle.getNames().size()) {
              throw std::runtime_error("LIMEX: Callable index out of range");
            }
            // collect arguments for n-ary if statement
            std::vector<T> arguments;
            for ( size_t i = 0; i < collectionValues[collection].size(); i++ ) {
              arguments.emplace_back( value == i+1 );
              arguments.emplace_back( collectionValues[collection][i] );
            }
            arguments.emplace_back( false ); // the else result should never occur
            return expression->handle.implementations[index](arguments);
          }
          else {
            throw std::logic_error("LIMEX: operator== is undefined");
          }
        }
      }
      else if constexpr (std::is_same_v< C, T >) {
        throw std::logic_error("LIMEX: unexpected use of built-in 'index' for collection type");
      }
      else {
        static_assert([]{ return false; }(), "LIMEX: unexpected collection type");
      }
    }
    case Type::element_of: {
      auto index = (size_t)Expression<T>::BUILTIN::ELEMENT_OF;
      if ( index >= expression->handle.getNames().size()) {
        throw std::runtime_error("LIMEX: Callable index out of range");
      }
      // Collect all evaluated arguments
      std::vector<T> arguments = { std::get<Node>(operands[0]).evaluate(variableValues,collectionValues) };
      auto& set = std::get<Node>(operands[1]);
      for ( auto& element : set.operands ) {
        arguments.push_back(
          std::get<Node>(element).evaluate(variableValues,collectionValues)
        );
      }
      // Call the custom callable
      return expression->handle.implementations[index](arguments);
    }
    case Type::not_element_of: {
      auto index = (size_t)Expression<T,C>::BUILTIN::NOT_ELEMENT_OF;
      if ( index >= expression->handle.getNames().size()) {
        throw std::runtime_error("LIMEX: Callable index out of range");
      }
      // Collect all evaluated arguments
      std::vector<T> arguments = { std::get<Node>(operands[0]).evaluate(variableValues,collectionValues) };
      auto& set = std::get<Node>(operands[1]);
      for ( auto& element : set.operands ) {
        arguments.push_back(
          std::get<Node>(element).evaluate(variableValues,collectionValues)
        );
      }
      // Call the custom callable
      return expression->handle.implementations[index](arguments);
    }
    case Type::if_then_else: {
      auto index = (size_t)Expression<T,C>::BUILTIN::IF_THEN_ELSE;
      if ( index >= expression->handle.getNames().size()) {
        throw std::runtime_error("LIMEX: Callable index out of range");
      }
      // Collect all evaluated arguments
      std::vector<T> arguments;
      for (size_t i = 0; i < operands.size(); ++i) {
        arguments.push_back(
          std::get<Node>(operands[i]).evaluate(variableValues,collectionValues)
        );
      }
      // Call the custom callable
      return expression->handle.implementations[index](arguments);
    }
    case Type::less_than:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left < right;
    }
    case Type::less_or_equal:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left <= right;
    }
    case Type::greater_than:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left > right;
    }
    case Type::greater_or_equal:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left >= right;
    }
    case Type::equal_to:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left == right;
    }
    case Type::not_equal_to:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left != right;
    }
    case Type::assign:
    {
      auto result = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      return result;
    }
    case Type::add_assign:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left + right;
    }
    case Type::subtract_assign:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left - right;
    }
    case Type::multiply_assign:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left * right;
    }
    case Type::divide_assign:
    {
      auto left = std::get<Node>(operands[0]).evaluate(variableValues,collectionValues);
      auto right = std::get<Node>(operands[1]).evaluate(variableValues,collectionValues);
      return left / right;
    }
    default:
      throw std::runtime_error("LIMEX: Unsupported type '" + std::string(typeName[(int)type]) + "'in evaluate");
  }
};

template <typename T, typename C>
inline std::string Node<T,C>::stringify() const {
  std::string result;
//  result += " ";
  result += std::string(typeName[(int)type]) + "( ";
  for ( auto& operand : operands ) {
    if (std::holds_alternative<double>(operand)) {
      result += std::to_string(std::get<double>(operand)) + ", ";
    }
    else if (std::holds_alternative<size_t>(operand)) {
      if ( type == Type::variable ) {
        result += expression->variables.at(std::get<size_t>(operand)) + ", ";
      }
      else if ( type == Type::collection ) {
        result += expression->collections.at(std::get<size_t>(operand)) + ", ";
      }
      else if ( type == Type::index ) {
        result += expression->collections.at(std::get<size_t>(operand)) + ", ";
      }
      else {
        result += expression->handle.names.at(std::get<size_t>(operand)) + ", ";
      }
    }
    else if (std::holds_alternative< Node<T,C> >(operand)) {
      result += std::get<Node<T,C>>(operand).stringify() + ", ";
    }
  }
  result.pop_back(); // remove trailing ' '
  result.back() = ' '; // replace trailing ',' with ' '
  result += ")";
  return result;
}

/*******************************
 ** Expression
 *******************************/

template <typename T, typename C>
Expression<T,C>::Expression(const std::string& expression, const Handle<T,C>& handle)
  : input(expression)
  , handle(handle) 
  , root(parse()) 
{
}

template <typename T, typename C>
inline T Expression<T,C>::evaluate( const std::vector<T>& variableValues, const std::vector<C>& collectionValues) const {
  return root.evaluate(variableValues,collectionValues);
}

template <typename T, typename C>
inline Node<T,C> Expression<T,C>::parse() {
  auto rootToken = tokenize(input);
//std::cerr << rootToken.stringify() << std::endl;
  return buildTree( Type::group, rootToken.children, std::nullopt );
}

template <typename T, typename C>
inline Token Expression<T,C>::tokenize(const std::string& input) {
  Token root = Token( Token::Category::OPERAND, Token::Type::GROUP, "" );
  std::stack< std::pair<Token*,std::string_view> > groupStack;
  groupStack.emplace(&root,"#");
  size_t pos = 0;
  Token::Category expected = Token::Category::PREFIX;

  while (pos < input.length()) {
    skipWhitespaces(input,pos);
    if ( pos == input.length() ) break; 

    if ( expected == Token::Category::PREFIX ) {
      // Consume prefix
      std::string match = fetch(input, pos, prefix);
      if ( !match.empty() ) {
        pos += match.size();
        if ( pos == input.length() ) {
          throw std::runtime_error("LIMEX: Prefix operator must be followed by operand: " + input.substr(0,pos));
        }; 
        groupStack.top().first->children.emplace_back(Token::Category::PREFIX, Token::Type::OPERATOR, std::move(match));
      }
      expected = Token::Category::OPERAND;
    }

    if ( expected == Token::Category::OPERAND ) {
      if ( auto match = fetch(input, pos, keywords); !match.empty() ) {
//std::cerr << "keywords" << std::endl;
        pos += match.size();
        if ( match == "true" ) {
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::NUMBER, std::to_string(true));
        }
        else if ( match == "false" ) {
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::NUMBER, std::to_string(false));
        }
        expected = Token::Category::INFIX;
        continue;
      }
      else if ( startsWith(input,pos,ternary[0]) ) {
        // ternary keyowrd "if"
        pos += 2;
        expected = Token::Category::PREFIX;
        groupStack.top().first->children.emplace_back(Token::Category::PREFIX, Token::Type::GROUP, std::string(ternary[0]));
        groupStack.emplace(&groupStack.top().first->children.back(),ternary[1]); // groub closed by "then"
        continue;
      }
      else if ( startsWith(input,pos,ternary[1]) || startsWith(input,pos,ternary[2]) ) {
        // close current group upon then and else
        expected = Token::Category::INFIX;
      }
      else if ( isnumeric( input[pos] ) ) {
        // Consume numbers
        std::string number;
        while (pos < input.length() && isnumeric( input[pos] ) ) {
          number += input[pos++];
        }
        expected = Token::Category::POSTFIX;
        groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::NUMBER, number);
      }
      else if ( isalphanumeric( input[pos] ) ) {
        // Consume name
        std::string name;
        while (pos < input.length() && isalphanumeric( input[pos] ) ) {
          name += input[pos++];
        }
        if ( pos < input.length() && input[pos] == '(' ) {
          ++pos;
          expected = Token::Category::PREFIX;
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::FUNCTION_CALL, name);
          groupStack.emplace(&groupStack.top().first->children.back(),")");
          continue;
        }
        else if ( startsWith(input,pos,"[]") ) {
          pos += 2;
          expected = Token::Category::OPERAND;
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::COLLECTION, name);
        }
        else if ( pos < input.length() && input[pos] == '[' ) {
          ++pos;
          expected = Token::Category::PREFIX;
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::INDEXED_VARIABLE, name);
          groupStack.emplace(&groupStack.top().first->children.back(),"]");
          continue;
        }
        else if ( pos < input.length() && input[pos] == '{' ) {
          ++pos;
          expected = Token::Category::PREFIX;
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::AGGREGATION, name);
          groupStack.emplace(&groupStack.top().first->children.back(),"}");
          continue;
        }
        else {
          expected = Token::Category::POSTFIX;
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::VARIABLE, name);
        }
      }
      else if ( auto match = fetch(input, pos, symbolic_names); !match.empty() ) {
        pos += match.size();
        if ( pos < input.length() && input[pos] == '(' ) {
          ++pos;
          expected = Token::Category::PREFIX;
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::FUNCTION_CALL, aliases.at(match));
          groupStack.emplace(&groupStack.top().first->children.back(),")");
          continue;
        }
        else if ( pos < input.length() && input[pos] == '{' ) {
          ++pos;
          expected = Token::Category::PREFIX;
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::AGGREGATION, aliases.at(match));
          groupStack.emplace(&groupStack.top().first->children.back(),"}");
          continue;
        }
        throw std::runtime_error("LIMEX: Symbolic names must be followed by parenthesis or braces: " + input.substr(0,pos));
      }
      else if ( input[pos] == '(' ) {
        ++pos;
        expected = Token::Category::PREFIX;
        groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::GROUP);
        groupStack.emplace(&groupStack.top().first->children.back(),")");
        continue;
      }
      else if ( input[pos] == '{' ) {
        ++pos;
        expected = Token::Category::PREFIX;
        groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::SET);
        groupStack.emplace(&groupStack.top().first->children.back(),"}");
        continue;
      }
      else if ( input[pos] == '[' ) {
        ++pos;
        expected = Token::Category::PREFIX;
        groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::SEQUENCE);
        groupStack.emplace(&groupStack.top().first->children.back(),"]");
        continue;
      }
      else {
//std::cerr << "IF" << startsWith(input,pos,"if") << "/" << pos << std::endl;
        throw std::runtime_error("LIMEX: Unexpected operand at: " + input.substr(0,pos) );
      }
    }

    if ( expected == Token::Category::POSTFIX ) {
      // Consume postfix
      std::string match = fetch(input, pos, postfix);
      if ( !match.empty() ) {
        pos += match.size();
        groupStack.top().first->children.emplace_back(Token::Category::POSTFIX, Token::Type::OPERATOR, std::move(match));
      }
      expected = Token::Category::INFIX;
    }

    skipWhitespaces(input,pos);
    if ( pos == input.length() ) break; 

    if ( startsWith(input,pos,groupStack.top().second) ) {
      if ( groupStack.top().second == "then" ) {
//std::cerr << " ... then ..." << std::endl;
        auto match = groupStack.top().second;
        // Consume closure of group and open next group
        pos += match.size();
        expected = Token::Category::PREFIX;
        groupStack.pop();
        groupStack.top().first->children.emplace_back(Token::Category::INFIX, Token::Type::GROUP, std::string(ternary[1]));
        groupStack.emplace(&groupStack.top().first->children.back(), ternary[2]);
        continue;
      }
      if ( groupStack.top().second == ":" || groupStack.top().second == ternary[2] ) {
//std::cerr << " ... else ..." << std::endl;
        auto match = groupStack.top().second;
        // Consume closure of group
        pos += groupStack.top().second.size();
        expected = Token::Category::PREFIX;
        groupStack.pop();
        groupStack.top().first->children.emplace_back(Token::Category::INFIX, Token::Type::OPERATOR, std::string(match));
        continue;
      }
      // Consume closure of group
      pos += groupStack.top().second.size();
      expected = Token::Category::POSTFIX;
      groupStack.pop();
      continue;
    }

    if ( expected == Token::Category::INFIX ) {
      // Consume separator
      if ( input[pos] == ',' ) {
        groupStack.top().first->children.emplace_back(Token::Category::INFIX, Token::Type::SEPARATOR, ",");
        ++pos;
        expected = Token::Category::PREFIX;
        continue;
      }
      
      // Consume ternary
      if ( input[pos] == '?' ) {
//std::cerr << "TERNARY" << std::endl;
        ++pos;
        expected = Token::Category::PREFIX;
        groupStack.top().first->children.emplace_back(Token::Category::INFIX, Token::Type::GROUP, "?");
        groupStack.emplace(&groupStack.top().first->children.back(), ":");
        continue;
      }
      // Consume infix
      std::string match = fetch(input, pos, infix);
      if ( !match.empty() ) {
        pos += match.size();
        groupStack.top().first->children.emplace_back(Token::Category::INFIX, Token::Type::OPERATOR, std::move(match));
        expected = Token::Category::PREFIX;
        continue;
      }
    }

    // Invalid character
    throw std::runtime_error("LIMEX: Unexpected character: " + input.substr(0,pos));
  }
  if (groupStack.size() != 1) {
    throw std::runtime_error("LIMEX: Unbalanced parentheses, brackets, or braces:" + input.substr(0,pos));
  }
  return root;
}

template <typename T, typename C>
inline size_t Expression<T,C>::skipWhitespaces(const std::string& input, size_t& pos) {
  // Skip whitespaces
  while ( pos < input.length() && std::isspace( input[pos] ) ) {
    ++pos;     
  }
  return pos;
}

template <typename T, typename C>
inline bool Expression<T,C>::startsWith(const std::string& input, size_t start, std::string_view candidate) {
  if ( input.compare(start, candidate.size(), candidate) == 0 ) {
    // `input` at `start` begins with candidate
    if ( !isalphanumeric( candidate.back() ) ) {
//std::cerr << "Found " << candidate << " after " << input.substr(0,start) << std::endl;
      return true;
    }
    if ( start + candidate.size() == input.size() || !isalphanumeric( input[start + candidate.size()] ) ) {
//std::cerr << "Found " << candidate << " after " << input.substr(0,start) << std::endl;
      return true;
    }
  }
  return false;
}


template <typename T, typename C>
template <size_t N>
inline std::string Expression<T,C>::fetch(const std::string& input, size_t start, const std::array<std::string_view, N>& prefix) {
  for (const auto& candidate : prefix) {
    if ( startsWith(input,start,candidate) ) {
      return std::string(candidate);
    }
  }
  return ""; // No match found
}

template <typename T, typename C>
inline size_t Expression<T,C>::getIndex(std::vector<std::string>& container, std::string name) {
  for ( size_t i = 0; i < container.size(); i++) {
    if ( container[i] == name ) {
      return i;
    }
  }
  container.push_back(name);
  return container.size()-1;
}
 
template <typename T, typename C>
inline Node<T,C> Expression<T,C>::buildTree( Type type, const std::vector<Token>& tokens, std::optional<size_t> index ) {
  std::vector< std::variant< double, size_t, Node<T,C> > > operands;
  if ( index.has_value() ) {
    operands.emplace_back( index.value() );
  }

  std::stack< Node<T,C> > nodeStack;
  std::stack< Type > operatorStack;

  auto createNode = [&](const Token& token) {
    switch (token.type) {
      case Token::Type::NUMBER:
        return Node<T,C>(this, std::stod(token.value));
      case Token::Type::VARIABLE:
        return Node<T,C>(this, Type::variable, token.value);
      case Token::Type::COLLECTION:
        return Node<T,C>(this, Type::collection, token.value);
      case Token::Type::GROUP:
        return buildTree(Type::group, token.children);
      case Token::Type::SET:
        return buildTree(Type::set, token.children);    
      case Token::Type::SEQUENCE:
        return buildTree(Type::sequence, token.children);    
      case Token::Type::FUNCTION_CALL:
        return buildTree(Type::function_call, token.children, handle.getIndex(token.value));    
      case Token::Type::AGGREGATION:
        return buildTree(Type::aggregation, token.children, handle.getIndex(token.value));    
      case Token::Type::INDEXED_VARIABLE:
        if constexpr (std::is_same_v< C, std::vector<T> >) {
          return buildTree(Type::index, token.children, getIndex(collections,token.value));
        }
        else if constexpr (std::is_same_v< C, T >) {
          std::vector<Token> children;
          children.emplace_back(Token::Category::OPERAND, Token::Type::COLLECTION, token.value);
          children.emplace_back(Token::Category::INFIX, Token::Type::SEPARATOR, ",");
          children.insert(children.end(), token.children.begin(), token.children.end());
          return buildTree(Type::function_call, children, handle.getIndex("at"));
        }
        else {
          throw std::logic_error("LIMEX:Collection type not supported");
        }
      default:
        throw std::logic_error("LIMEX: Unexpected token type for operand");
    }
  };

  auto applyOperator = [&](Type operatorType) {
    // process ternary operator
    if ( operatorType == Type::_else ) {
      if ( operatorStack.size() < 2) {
        throw std::logic_error("LIMEX: Insufficient operators for ternary operator");
      }
      operatorStack.pop();
      if ( operatorStack.top() != Type::_then_ ) {
        throw std::logic_error("LIMEX: Wrong operators for ternary operator");
      }
      if (nodeStack.size() < 3) {
        throw std::logic_error("LIMEX: Insufficient operands for ternary operator");
      }
      // Pop three operands and apply the operator
      Node<T,C> else_result = std::move(nodeStack.top());
      nodeStack.pop();
      Node<T,C> then_result = std::move(nodeStack.top());
      then_result.type = Type::group;
      nodeStack.pop();
      Node<T,C> condition = std::move(nodeStack.top());
      if ( condition.type == Type::if_ ) {
        condition.type = Type::group;
      }
      nodeStack.pop();

      nodeStack.push(Node<T,C>(this,Type::if_then_else, {std::move(condition), std::move(then_result), std::move(else_result) }));
      return;
    }

    // process all types of assignments
    if ( (int)operatorType >= (int)Type::assign ) {
      if (nodeStack.size() < 2) {
        throw std::runtime_error("LIMEX: Insufficient operands for assignment");
      }
      Node<T,C> right = std::move(nodeStack.top());
      nodeStack.pop();
      Node<T,C> left = std::move(nodeStack.top());
      nodeStack.pop();
      if ( left.type != Type::variable ) {
        throw std::runtime_error("LIMEX: illegal target for assignment");
      }
      if ( operatorType == Type::assign ) {
        nodeStack.push(Node<T,C>(this,Type::assign, {std::move(right) }));
      }
      else {
        // add_assign, subtract_assign, multiply_assign, or divide_assign
        nodeStack.push(Node<T,C>(this,operatorType, {std::move(left), std::move(right) }));
      }
      return;
    }

    if (nodeStack.size() < 2) {
      throw std::logic_error("LIMEX: Insufficient operands for infix operator");
    }
    // Pop two operands and apply the operator
    Node<T,C> right = std::move(nodeStack.top());
    nodeStack.pop();
    Node<T,C> left = std::move(nodeStack.top());
    nodeStack.pop();

    nodeStack.push(Node<T,C>(this,operatorType, {std::move(left), std::move(right)}));
    return;
  };

  for ( size_t i = 0; i < tokens.size(); i++ ) {
//std::cerr << "Token: '" << tokens[i].value << "', " << (i+1) << "/" << tokens.size() << std::endl;
    if ( tokens[i].category == Token::Category::PREFIX && tokens[i].type == Token::Type::GROUP ) {
      // keyword "if" starts a group, other prefix operators are treated separately
      auto node = buildTree(Type::if_ , tokens[i].children); 
      nodeStack.push(std::move(node));
      continue;
    }
    if ( tokens[i].category == Token::Category::INFIX && tokens[i].type == Token::Type::GROUP ) {
      // operators '?' and keyword "then" start a group, other infix operators are treated separately
      auto node = buildTree(Type::_then_ , tokens[i].children); 
      nodeStack.push(std::move(node));
      operatorStack.push(Type::_then_);
      continue;
    }
    if ( tokens[i].category == Token::Category::OPERAND ) {
      auto node = createNode( tokens[i] );
      // apply postfix and prefix operators
      if ( i + 1 < tokens.size() && tokens[i+1].category == Token::Category::POSTFIX ) {
        node = Node<T,C>(this, postfixTypes.at(tokens[i+1].value), { std::move(node) });
      }
      if ( i > 0 && tokens[i-1].category == Token::Category::PREFIX ) {
        node = Node<T,C>(this, prefixTypes.at(tokens[i-1].value), { std::move(node) });
      }
      nodeStack.push(std::move(node));
    }
    else if ( tokens[i].type == Token::Type::SEPARATOR ) {
      // separators create separate operand nodes
      // clear stack and apply all operators on stack
      while ( !operatorStack.empty() ) {
        applyOperator(operatorStack.top());
        operatorStack.pop();
      }
      if (nodeStack.size() != 1) {
        throw std::runtime_error("LIMEX: Invalid expression - unmatched operators or operands");
      }
      operands.emplace_back( std::move(nodeStack.top()) );
      nodeStack.pop();
      continue;
    }
    else if ( tokens[i].category == Token::Category::INFIX ) {
      Type operatorType = infixTypes.at(tokens[i].value);
      if ( (int)operatorType >= (int)Type::assign ) {
        if ( i != 1 ) {
          throw std::runtime_error("LIMEX: Assignment must start with a variable followed by the assignment operator");
        }
        if ( operatorType == Type::assign ) {
          // remove target from the list of variables 
          variables.clear();
        }
        target = tokens[0].value;
      }
      // apply operators on stack with smaller or equal precedence number
      while ( 
       !operatorStack.empty() && 
       precedences.at(operatorStack.top()) <= precedences.at(operatorType) && 
       operatorStack.top() != Type::_then_ 
     ) {
//std::cerr << "apply prior operator: " << (int)operatorStack.top() << "/" << (int)Type::if_then_else << std::endl;
        applyOperator(operatorStack.top());
        operatorStack.pop();
      }
//std::cerr << "add operator: " << (int)operatorType << "/" << (int)Type::if_then_else << std::endl;
      operatorStack.push(operatorType);
    }
    // skip prefix and postfix tokens
  }

  // apply all operators on stack
//std::cerr << "flush" << std::endl;
  while ( !operatorStack.empty() ) {
    applyOperator(operatorStack.top());
    operatorStack.pop();
  }

  if (nodeStack.size() != 1) {
    throw std::logic_error("LIMEX: Invalid expression - unmatched operators or operands");
  }

  operands.emplace_back( std::move(nodeStack.top()) );
  return Node<T,C>(this, type, operands);
}

template <typename T, typename C>
inline std::string Expression<T,C>::stringify() const {
  return root.stringify();
}

/*******************************
 ** Handle
 *******************************/

template <typename T, typename C>
inline size_t Handle<T,C>::getIndex(const std::string& name) const {
  for ( size_t i = 0; i < names.size(); i++) {
    if ( names[i] == name ) {
      return i;
    }
  }
  throw std::logic_error("LIMEX: Unknown callable '" + name + "'");
}

template <typename T, typename C>
inline void Handle<T,C>::add(const std::string& name, std::function<T(const std::vector<T>&)> implementation) {
  if (std::ranges::find(names, name) != names.end()) {
    throw std::runtime_error("LIMEX: Callable with name '" + name + "' already exists");
  }
  names.push_back(name);
  implementations.emplace_back(std::move(implementation));
}

// Define built-in functions
template <>
inline void Handle<double>::initialize() {
  add(
    std::string("if_then_else"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 3) throw std::runtime_error("LIMEX: if_then_else() requires exactly two arguments");
      return args[0] ? args[1] : args[2];
    }
  );

  add(
    std::string("n_ary_if"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.empty() || args.size()%2 != 1) throw std::runtime_error("LIMEX: n_ary_if() requires an unconditional argument");
      for ( size_t i = 0; i < args.size()/2; i++ ) {
        if ( args[2*i] ) return args[2*i+1];
      }
      return args.back();
    }
  );

  add(
    std::string("abs"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 1) throw std::runtime_error("LIMEX: abs() requires exactly one argument");
      return args[0] >= 0 ? args[0] : -args[0];
    }
  );

  add(
    std::string("pow"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 2) throw std::runtime_error("LIMEX: pow() requires exactly two arguments");
      return std::pow(args[0],args[1]);
    }
  );

  add(
    std::string("sqrt"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 1) throw std::runtime_error("LIMEX: sqrt() requires exactly one argument");
      return std::sqrt(args[0]);
    }
  );

  add(
    std::string("cbrt"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 1) throw std::runtime_error("LIMEX: cbrt() requires exactly one argument");
      return std::cbrt(args[0]);
    }
  );

  add(
    std::string("sum"), 
    [](const std::vector<double>& args) -> double
    {
      double result = 0.0;
      for ( double value : args ) {
        result += value;
      }
      return result;
    }
  );

  add(
    std::string("avg"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.empty()) throw std::runtime_error("LIMEX: avg{} requires at least one argument");
      double result = 0.0;
      for ( double value : args ) {
        result += value;
      }
      return result / args.size();
    }
  );

  add(
    std::string("count"), 
    [](const std::vector<double>& args) -> double
    {
      return args.size();
    }
  );

  add(
    std::string("min"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.empty()) throw std::runtime_error("LIMEX: min{} requires at least one argument");
      double result = DBL_MAX;
      for ( double value : args ) {
        if ( result > value ) {
          result = value;
        };
      }
      return result;
    }
  );

  add(
    std::string("max"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.empty()) throw std::runtime_error("LIMEX: max{} requires at least one argument");
      double result = -DBL_MAX;
      for ( double value : args ) {
        if ( result < value ) {
          result = value;
        };
      }
      return result;
    }
  );

  add(
    std::string("element_of"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.empty()) throw std::runtime_error("LIMEX: ∈ {...} requires at least one argument");
      for ( size_t i = 1; i < args.size(); i++ ) {
        if (args[0] == args[i]) {
          return true;
        }
      }
      return false;
    }
  );

  add(
    std::string("not_element_of"), 
    [](const std::vector<double>& args) -> double
    {
      if (args.empty()) throw std::runtime_error("LIMEX: ∉ {...} requires at least one argument");
      for ( size_t i = 1; i < args.size(); i++ ) {
        if (args[0] == args[i]) {
          return false;
        }
      }
      return true;
    }
  );

  add(
    std::string("at"), 
    [](const std::vector<double>& args [[maybe_unused]]) -> double
    {
      throw std::runtime_error("LIMEX: at() not relevant for handle of type double");
    }
  );
}

} // namespace LIMEX

#endif // LIMEX_H

