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
#include <algorithm>
#include <array>
#include <stack>
#include <set>
#include <ranges>
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
  inline void addFunction(const std::string& name, std::function<T(const std::vector<T>&)> implementation);
  inline void addAggregator(const std::string& name, std::function<T(const std::vector<T>&)> implementation);
  inline bool isAggregator(const std::string& name) const;
  inline const std::vector<std::string>& getFunctionNames() const { return functionNames; }
  inline const std::vector<std::string>& getAggregatorNames() const { return aggregatorNames; }
  inline size_t getFunctionIndex(const std::string& name) const;
  inline size_t getAggregatorIndex(const std::string& name) const;
  inline T indexedEvaluation( const C& collection, const T& index ) const;
  inline T aggregateEvaluation( const std::string& name, const C& collection ) const;
  inline T filteredAggregateEvaluation(
    const std::string& name,
    const C& collection,
    const Node<T,C>& conditions,
    const std::vector<T>& variableValues,
    const std::vector<C>& collectionValues
  ) const;
  inline T membershipEvaluation(
    const std::string& name,
    const T& element,
    const C& collection
  ) const;
private:
  inline void initialize();
  std::vector<std::string> functionNames;
  std::vector<std::function<T(const std::vector<T>&)>> functionImplementations;
  std::vector<std::string> aggregatorNames;
  std::vector<std::function<T(const std::vector<T>&)>> aggregatorImplementations;
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
  inline auto getVariables() const {
    return variables | std::views::filter([this](const auto& var) {
      return !iteratorVariables.contains(var);
    });
  }
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
  std::set<std::string> iteratorVariables;
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
  // Helper to collect variable names from tokens (skips AGGREGATION tokens for nested iterators)
  inline static void collectVariableNames(const std::vector<Token>& tokens, std::set<std::string>& names) {
    for (const auto& token : tokens) {
      if (token.type == Token::Type::VARIABLE) {
        names.insert(token.value);
      }
      // Skip AGGREGATION tokens - they have their own scoped iterators
      if (token.type != Token::Type::AGGREGATION) {
        collectVariableNames(token.children, names);
      }
    }
  }
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
    filtered_aggregation, // a filtered aggregate operation of the form '<operation_name>{ <var> in <collection> | <conditions> }'
    such_that, // helper for filtered aggregation, '|' separator
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
    "filtered_aggregation", // a filtered aggregate operation of the form '<operation_name>{ <var> in <collection> | <conditions> }'
    "such_that", // helper for filtered aggregation, '|' separator
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
constexpr auto infix = std::to_array<std::string_view>({ ",", "==", "!=", "<=", ">=", "<", ">", ":=", "≔", "+=", "-=", "*=", "/=", "+", "-", "*", "/", "^",  "&&", "||", "?", ":", "and", "or", "in", "not in", "≠", "≤", "≥", "∧", "∨", "∈", "∉", "|" });
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
    {"|", Type::such_that},
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

    {Type::such_that, 8},
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
      auto index = expression->handle.getFunctionIndex("pow");
      if (index >= expression->handle.getFunctionNames().size()) {
        throw std::runtime_error("LIMEX: Function index out of range");
      }
      // Collect all evaluated arguments
      std::vector<T> arguments;
      for (size_t i = 0; i < operands.size(); ++i) {
        arguments.push_back(
          std::get<Node>(operands[i]).evaluate(variableValues,collectionValues)
        );
      }
      // Call pow function
      return expression->handle.functionImplementations[index](arguments);
    }
    case Type::function_call:
    {
      size_t index = std::get<size_t>(operands[0]);
      if (index >= expression->handle.getFunctionNames().size()) {
        throw std::runtime_error("LIMEX: Function index out of range");
      }
      // Collect all evaluated arguments
      std::vector<T> arguments;
      for (size_t i = 1; i < operands.size(); ++i) {
        arguments.push_back(
          std::get<Node>(operands[i]).evaluate(variableValues,collectionValues)
        );
      }
      return expression->handle.functionImplementations[index](arguments);
    }
    case Type::aggregation:
    {
      size_t index = std::get<size_t>(operands[0]);
      if (index >= expression->handle.getAggregatorNames().size()) {
        throw std::runtime_error("LIMEX: Aggregator index out of range");
      }
      if (
        operands.size() == 2 &&
        std::holds_alternative<Node>(operands[1]) &&
        std::get<Node>(operands[1]).type == Type::collection
      ) {
        // argument is a collection
        auto collection = std::get<size_t>(std::get<Node>(operands[1]).operands[0]);

        if constexpr (std::is_same_v< C, std::vector<T> >) {
          // collection type C is vector<T>
          return expression->handle.aggregatorImplementations[index](collectionValues[collection]);
        }
        else if constexpr (std::is_same_v< C, T >) {
          // collection type C is T
          return expression->handle.aggregateEvaluation(expression->handle.getAggregatorNames()[index], collectionValues[collection]);
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
        return expression->handle.aggregatorImplementations[index](arguments);
      }
    }
    case Type::filtered_aggregation:
    {
      size_t aggrIndex = std::get<size_t>(operands[0]);
      size_t collIndex = std::get<size_t>(operands[1]);
      const auto& conditions = std::get<Node>(operands[2]);

      const C& collection = collectionValues[collIndex];

      return expression->handle.filteredAggregateEvaluation(
        expression->handle.getAggregatorNames()[aggrIndex],
        collection,
        conditions,
        variableValues,
        collectionValues
      );
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
            auto index = expression->handle.getFunctionIndex("n_ary_if");
            if (index >= expression->handle.getFunctionNames().size()) {
              throw std::runtime_error("LIMEX: Function index out of range");
            }
            // collect arguments for n-ary if statement
            std::vector<T> arguments;
            for ( size_t i = 0; i < collectionValues[collection].size(); i++ ) {
              arguments.emplace_back( value == i+1 );
              arguments.emplace_back( collectionValues[collection][i] );
            }
            arguments.emplace_back( false ); // the else result should never occur
            return expression->handle.functionImplementations[index](arguments);
          }
          else {
            throw std::logic_error("LIMEX: operator== is undefined");
          }
        }
      }
      else if constexpr (std::is_same_v< C, T >) {
        // CP::Expression: call specialized indexedEvaluation()
        auto collectionIndex = std::get<Node>(operands[1]).evaluate(variableValues, collectionValues);
        return expression->handle.indexedEvaluation(collectionValues[collection], collectionIndex);
      }
      else {
        static_assert([]{ return false; }(), "LIMEX: unexpected collection type");
      }
    }
    case Type::element_of: {
      T element = std::get<Node>(operands[0]).evaluate(variableValues, collectionValues);
      auto& rhs = std::get<Node>(operands[1]);
      if (rhs.type == Type::collection) {
        // External collection reference - use membershipEvaluation
        size_t collIdx = std::get<size_t>(rhs.operands[0]);
        return expression->handle.membershipEvaluation("element_of", element, collectionValues[collIdx]);
      }
      else {
        // Inline set - collect arguments and call aggregator
        std::vector<T> arguments = { element };
        for (auto& elem : rhs.operands) {
          arguments.push_back(std::get<Node>(elem).evaluate(variableValues, collectionValues));
        }
        return expression->handle.aggregatorImplementations[
          expression->handle.getAggregatorIndex("element_of")](arguments);
      }
    }
    case Type::not_element_of: {
      T element = std::get<Node>(operands[0]).evaluate(variableValues, collectionValues);
      auto& rhs = std::get<Node>(operands[1]);
      if (rhs.type == Type::collection) {
        // External collection reference - use membershipEvaluation
        size_t collIdx = std::get<size_t>(rhs.operands[0]);
        return expression->handle.membershipEvaluation("not_element_of", element, collectionValues[collIdx]);
      }
      else {
        // Inline set - collect arguments and call aggregator
        std::vector<T> arguments = { element };
        for (auto& elem : rhs.operands) {
          arguments.push_back(std::get<Node>(elem).evaluate(variableValues, collectionValues));
        }
        return expression->handle.aggregatorImplementations[
          expression->handle.getAggregatorIndex("not_element_of")](arguments);
      }
    }
    case Type::if_then_else: {
      auto index = expression->handle.getFunctionIndex("if_then_else");
      if (index >= expression->handle.getFunctionNames().size()) {
        throw std::runtime_error("LIMEX: Function index out of range");
      }
      // Collect all evaluated arguments
      std::vector<T> arguments;
      for (size_t i = 0; i < operands.size(); ++i) {
        arguments.push_back(
          std::get<Node>(operands[i]).evaluate(variableValues,collectionValues)
        );
      }
      // Call the function
      return expression->handle.functionImplementations[index](arguments);
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
  for ( size_t i = 0; i < operands.size(); ++i ) {
    auto& operand = operands[i];
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
      else if ( type == Type::filtered_aggregation && i == 1 ) {
        // operands[1] is collection index for filtered_aggregation
        result += expression->collections.at(std::get<size_t>(operand)) + ", ";
      }
      else if ( type == Type::function_call ) {
        result += expression->handle.getFunctionNames().at(std::get<size_t>(operand)) + ", ";
      }
      else if ( type == Type::aggregation || type == Type::filtered_aggregation ) {
        result += expression->handle.getAggregatorNames().at(std::get<size_t>(operand)) + ", ";
      }
      else {
        throw std::logic_error("LIMEX: Unexpected size_t operand in stringify for type '" + std::string(typeName[(int)type]) + "'");
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
          // Check if previous token was element_of/not_element_of operator - if so, this is a collection
          auto& children = groupStack.top().first->children;
          bool afterInOperator = !children.empty() &&
              children.back().category == Token::Category::INFIX &&
              children.back().type == Token::Type::OPERATOR &&
              (infixTypes.at(children.back().value) == Type::element_of ||
               infixTypes.at(children.back().value) == Type::not_element_of);
          if (afterInOperator) {
            groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::COLLECTION, name);
          }
          else {
            groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::VARIABLE, name);
          }
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
        throw std::runtime_error("LIMEX: Unexpected operand at: '" + input.substr(0,pos) + "' >>> '" + input.substr(pos) + "'" );
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
        if (handle.isAggregator(token.value)) {
          // aggr(X) - aggregation over collection
          if (token.children.size() != 1 || token.children[0].type != Token::Type::VARIABLE) {
            throw std::runtime_error("LIMEX: Aggregator with () requires single collection");
          }
          size_t aggrIdx = handle.getAggregatorIndex(token.value);
          size_t collIdx = getIndex(collections, token.children[0].value);
          Node<T,C> collNode(this, Type::collection, std::vector<std::variant<double, size_t, Node<T,C>>>{ collIdx });
          return Node<T,C>(this, Type::aggregation, std::vector<std::variant<double, size_t, Node<T,C>>>{ aggrIdx, std::move(collNode) });
        }
        return buildTree(Type::function_call, token.children, handle.getFunctionIndex(token.value));    
      case Token::Type::AGGREGATION:
      {
        const auto& children = token.children;

        // Detect filtered aggregation pattern: '<var> in <collection> |'
        if ( 
          children.size() >= 5 &&
          children[0].type == Token::Type::VARIABLE &&
          children[1].type == Token::Type::OPERATOR && infixTypes.at(children[1].value) == Type::element_of &&
          children[2].type == Token::Type::COLLECTION &&
          children[3].type == Token::Type::OPERATOR && infixTypes.at(children[3].value) == Type::such_that
        ) {
          std::string iteratorName = children[0].value;

          // Disallow shadowing (but allow iterator reuse)
          bool isIteratorReuse = iteratorVariables.contains(iteratorName);
          auto inVariables = std::find(variables.begin(), variables.end(), iteratorName) != variables.end();
          auto inCollections = std::find(collections.begin(), collections.end(), iteratorName) != collections.end();
          if ((inVariables && !isIteratorReuse) || inCollections) {
            throw std::runtime_error("LIMEX: Iteration variable '" + iteratorName + "' shadows existing identifier");
          }

          // Collect condition tokens (everything after '|', split by ',')
          std::vector<std::vector<Token>> conditionTokenGroups;
          conditionTokenGroups.emplace_back();
          for (size_t i = 4; i < children.size(); ++i) {
            if (children[i].type == Token::Type::SEPARATOR && children[i].value == ",") {
              conditionTokenGroups.emplace_back();
            }
            else {
              conditionTokenGroups.back().push_back(children[i]);
            }
          }

          // Pre-scan condition tokens to collect all variable names
          std::set<std::string> conditionVarNames;
          for (const auto& group : conditionTokenGroups) {
            collectVariableNames(group, conditionVarNames);
          }

          // Add external variables FIRST (all except iterator)
          for (const auto& varName : conditionVarNames) {
            if (varName != iteratorName) {
              getIndex(variables, varName);
            }
          }

          // Add iterator LAST and track it
          getIndex(variables, iteratorName);
          iteratorVariables.insert(iteratorName);

          // Build and AND conditions together
          Node<T,C> conditions = buildTree(Type::group, conditionTokenGroups[0]);
          for (size_t i = 1; i < conditionTokenGroups.size(); ++i) {
            Node<T,C> nextCondition = buildTree(Type::group, conditionTokenGroups[i]);
            conditions = Node<T,C>(this, Type::logical_and,
              { std::move(conditions), std::move(nextCondition) });
          }

          // DON'T erase iterator - indices must stay stable

          // Build filtered_aggregation node: { aggrIndex, collIndex, conditions }
          size_t aggrIdx = handle.getAggregatorIndex(token.value);
          size_t collIdx = getIndex(collections, children[2].value);
          return Node<T,C>(this, Type::filtered_aggregation,
            { aggrIdx, collIdx, std::move(conditions) });
        }

        // Check if '|' (such_that) is used but filtered aggregation pattern wasn't matched
        for (const auto& child : children) {
          if (child.type == Token::Type::OPERATOR && infixTypes.count(child.value) && infixTypes.at(child.value) == Type::such_that) {
            throw std::runtime_error("LIMEX: Filtered aggregation requires syntax: aggr{ var in collection | conditions }");
          }
        }

        return buildTree(Type::aggregation, token.children, handle.getAggregatorIndex(token.value));
      }
      case Token::Type::INDEXED_VARIABLE:
        return buildTree(Type::index, token.children, getIndex(collections, token.value));
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
inline size_t Handle<T,C>::getFunctionIndex(const std::string& name) const {
  for (size_t i = 0; i < functionNames.size(); i++) {
    if (functionNames[i] == name) {
      return i;
    }
  }
  throw std::logic_error("LIMEX: Unknown function '" + name + "'");
}

template <typename T, typename C>
inline size_t Handle<T,C>::getAggregatorIndex(const std::string& name) const {
  for (size_t i = 0; i < aggregatorNames.size(); i++) {
    if (aggregatorNames[i] == name) {
      return i;
    }
  }
  throw std::logic_error("LIMEX: Unknown aggregator '" + name + "'");
}

template <typename T, typename C>
inline bool Handle<T,C>::isAggregator(const std::string& name) const {
  return std::ranges::contains(aggregatorNames, name);
}

template <typename T, typename C>
inline void Handle<T,C>::addFunction(const std::string& name, std::function<T(const std::vector<T>&)> implementation) {
  if (std::ranges::contains(functionNames, name)) {
    throw std::runtime_error("LIMEX: Function '" + name + "' already exists");
  }
  if (std::ranges::contains(aggregatorNames, name)) {
    throw std::runtime_error("LIMEX: '" + name + "' is already registered as aggregator");
  }
  functionNames.push_back(name);
  functionImplementations.emplace_back(std::move(implementation));
}

template <typename T, typename C>
inline void Handle<T,C>::addAggregator(const std::string& name, std::function<T(const std::vector<T>&)> implementation) {
  if (std::ranges::contains(aggregatorNames, name)) {
    throw std::runtime_error("LIMEX: Aggregator '" + name + "' already exists");
  }
  if (std::ranges::contains(functionNames, name)) {
    throw std::runtime_error("LIMEX: '" + name + "' is already registered as function");
  }
  aggregatorNames.push_back(name);
  aggregatorImplementations.emplace_back(std::move(implementation));
}

template <typename T, typename C>
inline T Handle<T,C>::filteredAggregateEvaluation(
  const std::string& name,
  const C& collection,
  const Node<T,C>& conditions,
  const std::vector<T>& variableValues,
  const std::vector<C>& collectionValues
) const {
  // Iterator index is implicitly variableValues.size()
  size_t iteratorIndex = variableValues.size();

  // Filter elements
  C filtered;
  std::vector<T> localVars = variableValues;
  localVars.resize(iteratorIndex + 1);
  for (const T& elem : collection) {
    localVars[iteratorIndex] = elem;
    if (conditions.evaluate(localVars, collectionValues)) {
      filtered.push_back(elem);
    }
  }
  // Apply existing aggregate implementation
  return aggregatorImplementations.at(getAggregatorIndex(name))(filtered);
}

template <typename T, typename C>
inline T Handle<T,C>::membershipEvaluation(
  const std::string& name,
  const T& element,
  const C& collection
) const {
  std::vector<T> args = { element };
  for (const T& elem : collection) {
    args.push_back(elem);
  }
  return aggregatorImplementations.at(getAggregatorIndex(name))(args);
}

// Define built-in functions
template <>
inline void Handle<double>::initialize() {
  // Functions
  addFunction(
    std::string("if_then_else"),
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 3) throw std::runtime_error("LIMEX: if_then_else() requires exactly two arguments");
      return args[0] ? args[1] : args[2];
    }
  );

  addFunction(
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

  addFunction(
    std::string("abs"),
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 1) throw std::runtime_error("LIMEX: abs() requires exactly one argument");
      return args[0] >= 0 ? args[0] : -args[0];
    }
  );

  addFunction(
    std::string("pow"),
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 2) throw std::runtime_error("LIMEX: pow() requires exactly two arguments");
      return std::pow(args[0],args[1]);
    }
  );

  addFunction(
    std::string("sqrt"),
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 1) throw std::runtime_error("LIMEX: sqrt() requires exactly one argument");
      return std::sqrt(args[0]);
    }
  );

  addFunction(
    std::string("cbrt"),
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 1) throw std::runtime_error("LIMEX: cbrt() requires exactly one argument");
      return std::cbrt(args[0]);
    }
  );

  addFunction(
    std::string("log"),
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 1) throw std::runtime_error("LIMEX: log() requires exactly one argument");
      return std::log(args[0]);
    }
  );

  addFunction(
    std::string("exp"),
    [](const std::vector<double>& args) -> double
    {
      if (args.size() != 1) throw std::runtime_error("LIMEX: exp() requires exactly one argument");
      return std::exp(args[0]);
    }
  );

  addFunction(
    std::string("at"),
    [](const std::vector<double>& args [[maybe_unused]]) -> double
    {
      throw std::runtime_error("LIMEX: at() not relevant for handle of type double");
    }
  );

  // Aggregators
  addAggregator(
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

  addAggregator(
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

  addAggregator(
    std::string("count"),
    [](const std::vector<double>& args) -> double
    {
      return args.size();
    }
  );

  addAggregator(
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

  addAggregator(
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

  addAggregator(
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

  addAggregator(
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
}

} // namespace LIMEX

#endif // LIMEX_H

