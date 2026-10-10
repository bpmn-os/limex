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
#include <cstddef>
#include <limits>
#include <unordered_map>
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
  enum class Type { NUMBER, VARIABLE, OPERATOR, SEPARATOR, GROUP, SET, SEQUENCE, FUNCTION_CALL, AGGREGATION, PATH, FIELD, INDEX };
  Token(Category category, Type type, std::string value = "") : category(category), type(type), value(std::move(value)) {}
  Category category;
  Type type;
  std::string value;
  std::vector<Token> children; // for nested tokens (GROUP, SET, FUNCTION_CALL, AGGREGATION, PATH, INDEX)
  inline std::string stringify(int indent = 0) const;
};

/**
 * @brief Read-only view of the values of an array.
 *
 * A view refers to values held elsewhere and copies none of them, so that an array is handed to an
 * expression without being copied. It is either contiguous, referring to an array of values by its address
 * and length, or computed, determining each value by an accessor called with the position of the value, which
 * lets a caller present values that are not stored contiguously or not stored as `T`. The values must outlive
 * the view.
 *
 * @tparam T The type of the values.
 */
template <typename T>
class View {
public:
  /// @brief Iterator over the values of a view, yielding each value by value.
  class iterator {
  public:
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    iterator() = default;
    iterator(const View* owner, size_t position) : view(owner), index(position) {}
    T operator*() const { return (*view)[index]; }
    iterator& operator++() { ++index; return *this; }
    iterator operator++(int) { iterator previous = *this; ++index; return previous; }
    bool operator==(const iterator& other) const { return index == other.index; }
  private:
    const View* view = nullptr;
    size_t index = 0;
  };

  /// @brief Constructs an empty view.
  View() = default;
  /// @brief Constructs a contiguous view of `count` values beginning at `first`.
  View(const T* first, size_t count) : data(first), length(count) {}
  /// @brief Constructs a contiguous view of the values of a vector, which must outlive the view.
  View(const std::vector<T>& values) : data(values.data()), length(values.size()) {}
  /// @brief Deleted, since the view would refer to a vector destroyed at the end of the full expression.
  View(std::vector<T>&& values) = delete;
  /// @brief Constructs a computed view of `count` values, the value at position `k` being `valueAt(k)`.
  View(size_t count, std::function<T(size_t)> valueAt) : length(count), accessor(std::move(valueAt)) {}

  size_t size() const { return length; }
  bool empty() const { return length == 0; }
  /// @brief Returns the value at position `k`, counting from zero.
  T operator[](size_t k) const { return accessor ? accessor(k) : data[k]; }
  iterator begin() const { return iterator(this, 0); }
  iterator end() const { return iterator(this, length); }

private:
  const T* data = nullptr;
  size_t length = 0;
  std::function<T(size_t)> accessor;
};

/**
 * @brief Description of an access path `name ( .field | [expression] )*` in an expression.
 *
 * Each step is either a field, given by its name, or an index, given by `std::nullopt`. The values of the index
 * expressions are handed to the resolver in the order of the index steps.
 */
struct Path {
  std::string name;
  std::vector< std::optional<std::string> > steps;
};

/**
 * @brief Interface by which the caller of an evaluation resolves the access paths of an expression.
 *
 * `value(path, indices)` returns the value a path addresses and `array(path, indices)` the array a path
 * addresses, where `path` is the position of the path in `Expression::getPaths()` and `indices` holds the
 * values of its index expressions in order. The indices are passed as they are evaluated, so that their
 * numbering and their bounds are the caller's to check. A path is resolved as an array where an array is
 * expected, i.e. as the argument of an aggregator, on the right of `in` and in a binding of an aggregation,
 * and as a value everywhere else; a name used as an array is a path without steps.
 *
 * The caller implements both methods in a class of its own and passes an instance, typically constructed for
 * the evaluation on the stack, by reference, so that resolving allocates nothing.
 *
 * @tparam T The type of the values.
 * @tparam A The type of an array, a view of values by default.
 */
template <typename T, typename A = View<T> >
class Resolver {
public:
  virtual ~Resolver() = default;
  /// @brief Returns the value the path with the given position addresses for the given values of its indices.
  virtual T value(size_t path, const std::vector<T>& indices) const = 0;
  /// @brief Returns the array the path with the given position addresses for the given values of its indices.
  virtual A array(size_t path, const std::vector<T>& indices) const = 0;
};

/**
 * @brief The values an evaluation reads: those supplied by the caller and the values of the iterators.
 */
template <typename T, typename A = View<T> >
struct Context {
  const std::vector<T>& variableValues;
  /// The resolver of the paths, absent for an expression evaluated without one
  const Resolver<T,A>* resolver;
  std::vector<T> iteratorValues;
};

enum class Type; /// Types of nodes in the abstract syntax tree

template <typename T, typename A = View<T> > class Expression;

/**
 * @brief Represents a node in the abstract syntax tree of an expression.
 * 
 * Each node can represent either a literal value, a variable, or a complex expression with operands. 
 * It supports evaluation based on provided
 * variable values and on the arrays and values the caller resolves, and can generate a string representation
 * of the expression.
 * 
 * @tparam T The type of the value held by the node (e.g., double).
 */
template <typename T, typename A = View<T> >
class Node {
public:
  Expression<T,A>* expression;
  Type type;
  std::vector< std::variant<double, size_t, Node> > operands;
  // Constructor for a literal node
  Node(Expression<T,A>* expression, double value);
  // Constructor for a variable node
  Node(Expression<T,A>* expression, Type type, std::string name);
  // Constructor for a node with multiple operands
  Node(Expression<T,A>* expression, Type type, std::vector< std::variant<double, size_t, Node> > operands);
  // Templated deep copy constructor
  template <typename U>
  Node(Expression<T,A>* expression, const Node<U>& other);
  // Evaluate the node
  inline T evaluate( Context<T,A>& context ) const;
  // Evaluate the index expressions of a path node
  inline std::vector<T> evaluateIndices( Context<T,A>& context ) const;
  // Evaluate a path node resolved as an array to the array it denotes
  inline A evaluateArray( Context<T,A>& context ) const;
  // Returns true if the node or one of its descendants reads the iterator with the given slot
  inline bool usesIterator( size_t slot ) const;
  std::string stringify() const;
};

template <typename T, typename A = View<T> >
class Handle {
friend class Node<T,A>;
friend class Expression<T,A>;
public:
  Handle() { initialize(); };
  inline void addFunction(const std::string& name, std::function<T(const std::vector<T>&)> implementation);
  inline void addAggregator(const std::string& name, std::function<T(const View<T>&)> implementation);
  inline bool isAggregator(const std::string& name) const;
  inline const std::vector<std::string>& getFunctionNames() const { return functionNames; }
  inline const std::vector<std::string>& getAggregatorNames() const { return aggregatorNames; }
  inline size_t getFunctionIndex(const std::string& name) const;
  inline size_t getAggregatorIndex(const std::string& name) const;
  inline T aggregateEvaluation( const std::string& name, const A& array ) const;
  inline T membershipEvaluation(
    const std::string& name,
    const T& element,
    const A& array
  ) const;
private:
  inline void initialize();
  std::vector<std::string> functionNames;
  std::vector<std::function<T(const std::vector<T>&)>> functionImplementations;
  std::vector<std::string> aggregatorNames;
  std::vector<std::function<T(const View<T>&)>> aggregatorImplementations;
};

/**
 * @brief Represents a mathematical expression that can be evaluated for different values.
 * 
 * This class is used to parse a string representing a mathemtical expression, build an abstract syntax tree, 
 * and evaluate it based on the provided variable values and the paths the caller resolves. The expression can include 
 * variables, functions, operators, and other mathematical operations, which are processed and executed 
 * during evaluation. Users can defined custom functions and set operators to be used when evaluating an 
 * expression. Several built-in functions and set operators must be made available through an appropriate 
 * @ref `Handle`.
 * 
 * @tparam T The type of the value used in the expression (e.g., double).
 */
template <typename T, typename A >
class Expression {
friend class Node<T,A>;
public:
  Expression(const std::string& expression, const Handle<T,A>& handle);
  /// The names of the variables, whose values the caller passes in this order
  inline const std::vector<std::string>& getVariables() const { return variables; }
  /// The access paths, which the caller resolves by their position in this vector
  inline const std::vector<Path>& getPaths() const { return paths; }
  /// The name of the variable or path an assignment assigns to
  inline const std::optional<std::string>& getTarget() const { return target; }
  /// The position of the path an assignment assigns to, if the target is a path
  inline const std::optional<size_t>& getTargetPath() const { return targetPath; }
  /// Evaluates the expression for the given values of its variables, which must have no paths
  inline T evaluate( const std::vector<T>& variableValues = {}) const;
  /// Evaluates the expression for the given values of its variables, the resolver resolving its paths
  inline T evaluate( const std::vector<T>& variableValues, const Resolver<T,A>& resolver) const;
  /// Evaluates the indices of the path an assignment assigns to, which the caller writes
  inline std::vector<T> evaluateTargetIndices( const std::vector<T>& variableValues = {}) const;
  /// @copydoc evaluateTargetIndices(const std::vector<T>&) const
  inline std::vector<T> evaluateTargetIndices( const std::vector<T>& variableValues, const Resolver<T,A>& resolver) const;
  inline const Node<T,A>& getRoot() const { return root; }
  const std::string input;
  inline std::string stringify() const;
private:
  const Handle<T,A>& handle;
  std::vector<std::string> variables;
  std::vector<Path> paths;
  std::vector<std::string> iterators; ///< The names of the iterators, one slot per binding
  std::vector< std::unordered_map<std::string,size_t> > scopes; ///< The iterators bound while parsing an aggregation
  std::optional<std::string> target;
  std::optional<size_t> targetPath;
  Node<T,A> root;
  inline Node<T,A> parse();
  inline static Token tokenize(const std::string& input);
  inline static Token readPath(const std::string& input, size_t& pos, std::string name);
  inline static bool isNameStart(char c) { return (std::isalpha( (unsigned char)c ) || c == '_'); };
  inline std::optional<size_t> boundIterator(const std::string& name) const;
  /// Evaluates the indices of the target path in the given context
  inline std::vector<T> targetIndices( Context<T,A> context ) const;
  inline static bool isnumeric(char c) { return (std::isdigit( c ) || c == '.'); }; 
  inline static bool isalphanumeric(char c) { return (std::isalnum(c) || c == '_'); }; 
  inline size_t getIndex(std::vector<std::string>& container, std::string name);
  inline static size_t skipWhitespaces(const std::string& input, size_t& pos);
  inline static bool startsWith(const std::string& input, size_t pos, std::string_view candidate);
  template <size_t N>
  inline static std::string fetch(const std::string& input, size_t start, const std::array<std::string_view, N>& prefix);
  Node<T,A> buildTree( Type type, const std::vector<Token>& tokens, std::optional<size_t> index = std::nullopt );
  Node<T,A> buildAggregationOver( const Token& token );
};

enum class Type {
    literal, // a given number
    variable, // a named variable
    group, // a block encapsulated in '(' and ')'
    set, // a block encapsulated in '{' and '}'
    sequence, // a block encapsulated in '[' and ']'
    function_call,  // a function call of the form '<function_name>(...)'
    aggregation, // an aggregate operation of the form '<operation_name>{...}'
    aggregation_over, // an aggregation of the form '<operation_name>{ <expression> | <bindings>, <conditions> }'
    range_binding, // a binding '<iterator> in <lower>..<upper>' of an aggregation, with its conditions
    array_binding, // a binding '<iterator> in <array>' of an aggregation, with its conditions
    such_that, // helper for aggregations, '|' separator
    range, // helper for bindings, '..' separator
    iterator, // the iterator of a binding
    path, // an access path '<name>( .<field> | [<expression>] )*' resolved as a value
    array_path, // an access path resolved as an array
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
    "group", // a block encapsulated in '(' and ')'
    "set", // a block encapsulated in '{' and '}'
    "sequence", // a block encapsulated in '[' and ']'
    "function_call",  // a function call of the form '<function_name>(...)'
    "aggregation", // an aggregate operation of the form '<operation_name>{...}'
    "aggregation_over", // an aggregation of the form '<operation_name>{ <expression> | <bindings>, <conditions> }'
    "range_binding", // a binding '<iterator> in <lower>..<upper>' of an aggregation, with its conditions
    "array_binding", // a binding '<iterator> in <array>' of an aggregation, with its conditions
    "such_that", // helper for aggregations, '|' separator
    "range", // helper for bindings, '..' separator
    "iterator", // the iterator of a binding
    "path", // an access path '<name>( .<field> | [<expression>] )*' resolved as a value
    "array_path", // an access path resolved as an array
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
constexpr auto infix = std::to_array<std::string_view>({ ",", "..", "==", "!=", "<=", ">=", "<", ">", ":=", "≔", "+=", "-=", "*=", "/=", "+", "-", "*", "/", "^",  "&&", "||", "?", ":", "and", "or", "in", "not in", "≠", "≤", "≥", "∧", "∨", "∈", "∉", "|" });
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
    {"..", Type::range},
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
    case Type::OPERATOR: result += "OPERATOR"; break;
    case Type::SEPARATOR: result += "SEPARATOR"; break;
    case Type::GROUP: result += "GROUP"; break;
    case Type::SET: result += "SET"; break;
    case Type::SEQUENCE: result += "SEQUENCE"; break;
    case Type::FUNCTION_CALL: result += "FUNCTION_CALL"; break;
    case Type::AGGREGATION: result += "AGGREGATION"; break;
    case Type::PATH: result += "PATH"; break;
    case Type::FIELD: result += "FIELD"; break;
    case Type::INDEX: result += "INDEX"; break;
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

template <typename T, typename A>
Node<T,A>::Node(Expression<T,A>* expression, double value)
: expression(expression), type(Type::literal)
{
  operands.emplace_back(std::move(value));
}

// Constructor for a variable node
template <typename T, typename A>
Node<T,A>::Node(Expression<T,A>* expression, Type type, std::string name)
: expression(expression), type(type)
{
  if ( type == Type::variable ) {
    operands.emplace_back(expression->getIndex(expression->variables,name));
  }
  else {
    throw std::logic_error("LIMEX: Unexpected node type");
  }
}

template <typename T, typename A>
Node<T,A>::Node(Expression<T,A>* expression, Type type, std::vector< std::variant< double, size_t, Node<T,A> > > operands)
: expression(expression), type(type), operands(std::move(operands)) {}

template <typename T, typename A>
template <typename U>
Node<T,A>::Node(Expression<T,A>* expression, const Node<U>& other)
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

template <typename T, typename A>
inline T Node<T,A>::evaluate( Context<T,A>& context ) const {
//std::cerr << "Type: "<< typeName[(int)type] << std::endl;
  switch (type) {
    case Type::group:
      return std::get<Node>(operands[0]).evaluate(context);
    case Type::set:
      throw std::runtime_error("LIMEX: Sets cannot be evaluated");
    case Type::sequence:
      throw std::runtime_error("LIMEX: Sequences cannot be evaluated");
    case Type::literal:
      return std::get<double>(operands[0]);
    case Type::variable: {
      return context.variableValues[std::get<size_t>(operands[0])];
    }
    case Type::negate: {
      auto operandValue = std::get<Node>(operands[0]).evaluate(context);
      return -operandValue;
    }

    case Type::logical_not: {
      auto operandValue = std::get<Node>(operands[0]).evaluate(context);
      return !operandValue;
    }

    case Type::logical_and: {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left && right;
    }

    case Type::logical_or: {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left || right;
    }
    case Type::add: {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left + right;
    }
    case Type::subtract: {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left - right;
    }

    case Type::multiply: {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left * right;
    }
    case Type::divide: {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      
      if constexpr (std::is_arithmetic_v<T>) {
        if (right == 0) {
          throw std::runtime_error("LIMEX: Division by zero");
        }
      }

      return left / right;
    }
    case Type::square: {
      auto value = std::get<Node>(operands[0]).evaluate(context);
      return value * value;
    }
    case Type::cube: {
      auto value = std::get<Node>(operands[0]).evaluate(context);
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
          std::get<Node>(operands[i]).evaluate(context)
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
          std::get<Node>(operands[i]).evaluate(context)
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
        std::get<Node>(operands[1]).type == Type::array_path
      ) {
        // argument is a path resolved as an array
        A array = std::get<Node>(operands[1]).evaluateArray(context);

        if constexpr (std::is_same_v< A, View<T> >) {
          // array type A is View<T>, which the aggregator takes as it is
          return expression->handle.aggregatorImplementations[index](array);
        }
        else if constexpr (std::is_same_v< A, T >) {
          // array type A is T
          return expression->handle.aggregateEvaluation(expression->handle.getAggregatorNames()[index], array);
        }
        else {
          static_assert([]{ return false; }(), "LIMEX: unexpected array type");
        }
      }
      else {
        // explicitly collect all evaluated arguments
        std::vector<T> arguments;
        for (size_t i = 1; i < operands.size(); ++i) {
          arguments.push_back(
            std::get<Node>(operands[i]).evaluate(context)
          );
        }
        return expression->handle.aggregatorImplementations[index](arguments);
      }
    }
    case Type::aggregation_over:
    {
      if constexpr (std::is_arithmetic_v<T>) {
        size_t index = std::get<size_t>(operands[0]);
        if (index >= expression->handle.getAggregatorNames().size()) {
          throw std::runtime_error("LIMEX: Aggregator index out of range");
        }
        const Node& body = std::get<Node>(operands[1]);
        // the values of the expression for every combination of iterator values satisfying the conditions,
        // the bindings being nested in their order and each condition checked once the iterators it reads are
        // bound
        std::vector<T> values;
        std::function<void(size_t)> iterate = [&](size_t position) {
          if (position == operands.size()) {
            values.push_back(body.evaluate(context));
            return;
          }
          const Node& binding = std::get<Node>(operands[position]);
          size_t slot = std::get<size_t>(binding.operands[0]);
          size_t firstCondition = ( binding.type == Type::range_binding ? 3 : 2 );
          auto proceed = [&](T value) {
            context.iteratorValues[slot] = value;
            for (size_t i = firstCondition; i < binding.operands.size(); ++i) {
              T satisfied = std::get<Node>(binding.operands[i]).evaluate(context);
              // an undefined condition, which is NaN, is not satisfied
              if ( std::isnan((double)satisfied) || !satisfied ) {
                return;
              }
            }
            iterate(position + 1);
          };
          if (binding.type == Type::range_binding) {
            T lower = std::get<Node>(binding.operands[1]).evaluate(context);
            T upper = std::get<Node>(binding.operands[2]).evaluate(context);
            if constexpr (std::is_floating_point_v<T>) {
              if ( std::isnan(lower) || std::isnan(upper) ) {
                // an undefined bound gives an empty range
                return;
              }
              lower = std::ceil(lower);
            }
            for (T value = lower; value <= upper; value += 1) {
              proceed(value);
            }
          }
          else {
            A array = std::get<Node>(binding.operands[1]).evaluateArray(context);
            for (T value : array) {
              if constexpr (std::is_floating_point_v<T>) {
                if ( std::isnan(value) ) {
                  // undefined elements are skipped
                  continue;
                }
              }
              proceed(value);
            }
          }
        };
        iterate(2);
        return expression->handle.aggregatorImplementations[index](View<T>(values));
      }
      else {
        throw std::logic_error("LIMEX: Aggregation over an expression requires an arithmetic value type");
      }
    }
    case Type::range_binding:
    case Type::array_binding:
    case Type::range:
    case Type::such_that:
      throw std::logic_error("LIMEX: Bindings are evaluated by their aggregation");
    case Type::iterator:
      return context.iteratorValues[std::get<size_t>(operands[0])];
    case Type::path:
    {
      size_t path = std::get<size_t>(operands[0]);
      if (!context.resolver) {
        throw std::runtime_error("LIMEX: No resolver provided for path '" + expression->paths[path].name + "'");
      }
      return context.resolver->value(path, evaluateIndices(context));
    }
    case Type::array_path:
      throw std::runtime_error("LIMEX: Arrays cannot be evaluated");
    case Type::element_of: {
      T element = std::get<Node>(operands[0]).evaluate(context);
      auto& rhs = std::get<Node>(operands[1]);
      if (rhs.type == Type::array_path) {
        // a path resolved as an array - use membershipEvaluation
        return expression->handle.membershipEvaluation("element_of", element, rhs.evaluateArray(context));
      }
      else {
        // Inline set - collect arguments and call aggregator
        std::vector<T> arguments = { element };
        for (auto& elem : rhs.operands) {
          arguments.push_back(std::get<Node>(elem).evaluate(context));
        }
        return expression->handle.aggregatorImplementations[
          expression->handle.getAggregatorIndex("element_of")](arguments);
      }
    }
    case Type::not_element_of: {
      T element = std::get<Node>(operands[0]).evaluate(context);
      auto& rhs = std::get<Node>(operands[1]);
      if (rhs.type == Type::array_path) {
        // a path resolved as an array - use membershipEvaluation
        return expression->handle.membershipEvaluation("not_element_of", element, rhs.evaluateArray(context));
      }
      else {
        // Inline set - collect arguments and call aggregator
        std::vector<T> arguments = { element };
        for (auto& elem : rhs.operands) {
          arguments.push_back(std::get<Node>(elem).evaluate(context));
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
          std::get<Node>(operands[i]).evaluate(context)
        );
      }
      // Call the function
      return expression->handle.functionImplementations[index](arguments);
    }
    case Type::less_than:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left < right;
    }
    case Type::less_or_equal:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left <= right;
    }
    case Type::greater_than:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left > right;
    }
    case Type::greater_or_equal:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left >= right;
    }
    case Type::equal_to:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left == right;
    }
    case Type::not_equal_to:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left != right;
    }
    case Type::assign:
    {
      auto result = std::get<Node>(operands[0]).evaluate(context);
      return result;
    }
    case Type::add_assign:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left + right;
    }
    case Type::subtract_assign:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left - right;
    }
    case Type::multiply_assign:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left * right;
    }
    case Type::divide_assign:
    {
      auto left = std::get<Node>(operands[0]).evaluate(context);
      auto right = std::get<Node>(operands[1]).evaluate(context);
      return left / right;
    }
    default:
      throw std::runtime_error("LIMEX: Unsupported type '" + std::string(typeName[(int)type]) + "'in evaluate");
  }
};

template <typename T, typename A>
inline std::vector<T> Node<T,A>::evaluateIndices( Context<T,A>& context ) const {
  std::vector<T> indices;
  indices.reserve(operands.size() - 1);
  for (size_t i = 1; i < operands.size(); ++i) {
    indices.push_back( std::get<Node>(operands[i]).evaluate(context) );
  }
  return indices;
}

template <typename T, typename A>
inline A Node<T,A>::evaluateArray( Context<T,A>& context ) const {
  if (type == Type::array_path) {
    size_t path = std::get<size_t>(operands[0]);
    if (!context.resolver) {
      throw std::runtime_error("LIMEX: No resolver provided for array '" + expression->paths[path].name + "'");
    }
    return context.resolver->array(path, evaluateIndices(context));
  }
  throw std::logic_error("LIMEX: Node is not an array");
}

template <typename T, typename A>
inline bool Node<T,A>::usesIterator( size_t slot ) const {
  if (type == Type::iterator && std::get<size_t>(operands[0]) == slot) {
    return true;
  }
  for (const auto& operand : operands) {
    if (std::holds_alternative<Node>(operand) && std::get<Node>(operand).usesIterator(slot)) {
      return true;
    }
  }
  return false;
}

template <typename T, typename A>
inline std::string Node<T,A>::stringify() const {
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
      else if ( type == Type::path || type == Type::array_path ) {
        result += expression->paths.at(std::get<size_t>(operand)).name + ", ";
      }
      else if ( type == Type::iterator || type == Type::range_binding || type == Type::array_binding ) {
        result += expression->iterators.at(std::get<size_t>(operand)) + ", ";
      }
      else if ( type == Type::function_call ) {
        result += expression->handle.getFunctionNames().at(std::get<size_t>(operand)) + ", ";
      }
      else if ( type == Type::aggregation || type == Type::aggregation_over ) {
        result += expression->handle.getAggregatorNames().at(std::get<size_t>(operand)) + ", ";
      }
      else {
        throw std::logic_error("LIMEX: Unexpected size_t operand in stringify for type '" + std::string(typeName[(int)type]) + "'");
      }
    }
    else if (std::holds_alternative< Node<T,A> >(operand)) {
      result += std::get<Node<T,A>>(operand).stringify() + ", ";
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

template <typename T, typename A>
Expression<T,A>::Expression(const std::string& expression, const Handle<T,A>& handle)
  : input(expression)
  , handle(handle) 
  , root(parse()) 
{
}

template <typename T, typename A>
inline T Expression<T,A>::evaluate( const std::vector<T>& variableValues) const {
  Context<T,A> context{ variableValues, nullptr, std::vector<T>(iterators.size()) };
  return root.evaluate(context);
}

template <typename T, typename A>
inline T Expression<T,A>::evaluate( const std::vector<T>& variableValues, const Resolver<T,A>& resolver) const {
  Context<T,A> context{ variableValues, &resolver, std::vector<T>(iterators.size()) };
  return root.evaluate(context);
}

template <typename T, typename A>
inline std::vector<T> Expression<T,A>::evaluateTargetIndices( const std::vector<T>& variableValues) const {
  return targetIndices( Context<T,A>{ variableValues, nullptr, std::vector<T>(iterators.size()) } );
}

template <typename T, typename A>
inline std::vector<T> Expression<T,A>::evaluateTargetIndices( const std::vector<T>& variableValues, const Resolver<T,A>& resolver) const {
  return targetIndices( Context<T,A>{ variableValues, &resolver, std::vector<T>(iterators.size()) } );
}

template <typename T, typename A>
inline std::vector<T> Expression<T,A>::targetIndices( Context<T,A> context ) const {
  if (!targetPath.has_value()) {
    return {};
  }
  // the root is a group holding the assignment, which keeps the target path as its last operand for ':=' and
  // as its first operand for a compound assignment
  const Node<T,A>& assignment = std::get< Node<T,A> >(root.operands[0]);
  const Node<T,A>& path = std::get< Node<T,A> >( assignment.type == Type::assign ? assignment.operands[1] : assignment.operands[0] );
  return path.evaluateIndices(context);
}

template <typename T, typename A>
inline std::optional<size_t> Expression<T,A>::boundIterator(const std::string& name) const {
  // the innermost binding of the name, if it is bound by an enclosing aggregation
  for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
    if (auto it = scope->find(name); it != scope->end()) {
      return it->second;
    }
  }
  return std::nullopt;
}

template <typename T, typename A>
inline Node<T,A> Expression<T,A>::parse() {
  auto rootToken = tokenize(input);
//std::cerr << rootToken.stringify() << std::endl;
  return buildTree( Type::group, rootToken.children, std::nullopt );
}

template <typename T, typename A>
inline Token Expression<T,A>::tokenize(const std::string& input) {
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
        while (pos < input.length() && isnumeric( input[pos] ) && !startsWith(input,pos,"..") ) {
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
          // an array named explicitly, which is a path without steps
          pos += 2;
          expected = Token::Category::OPERAND;
          groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::PATH, name);
        }
        else if (
          pos < input.length() &&
          ( input[pos] == '[' || ( input[pos] == '.' && pos + 1 < input.length() && isNameStart(input[pos+1]) ) )
        ) {
          // a name followed by an index or a field begins an access path
          groupStack.top().first->children.push_back( readPath(input, pos, name) );
          expected = Token::Category::POSTFIX;
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
          // a name following element_of or not_element_of is an array, which is a path without steps
          auto& children = groupStack.top().first->children;
          bool afterInOperator = !children.empty() &&
              children.back().category == Token::Category::INFIX &&
              children.back().type == Token::Type::OPERATOR &&
              (infixTypes.at(children.back().value) == Type::element_of ||
               infixTypes.at(children.back().value) == Type::not_element_of);
          if (afterInOperator) {
            groupStack.top().first->children.emplace_back(Token::Category::OPERAND, Token::Type::PATH, name);
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

template <typename T, typename A>
inline Token Expression<T,A>::readPath(const std::string& input, size_t& pos, std::string name) {
  Token path(Token::Category::OPERAND, Token::Type::PATH, std::move(name));
  while (pos < input.length()) {
    if (input[pos] == '[') {
      // an index, whose expression is tokenized on its own
      size_t start = ++pos;
      size_t depth = 1;
      while (pos < input.length() && depth > 0) {
        if (input[pos] == '[') {
          ++depth;
        }
        else if (input[pos] == ']') {
          --depth;
        }
        ++pos;
      }
      if (depth > 0) {
        throw std::runtime_error("LIMEX: Unbalanced brackets in path: " + input.substr(0,pos));
      }
      Token index = tokenize(input.substr(start, pos - 1 - start));
      if (index.children.empty()) {
        throw std::runtime_error("LIMEX: Empty index in path: " + input.substr(0,pos));
      }
      index.type = Token::Type::INDEX;
      path.children.push_back(std::move(index));
    }
    else if (input[pos] == '.' && pos + 1 < input.length() && isNameStart(input[pos+1])) {
      // a field
      std::string field;
      ++pos;
      while (pos < input.length() && isalphanumeric(input[pos])) {
        field += input[pos++];
      }
      path.children.emplace_back(Token::Category::OPERAND, Token::Type::FIELD, std::move(field));
    }
    else {
      break;
    }
  }
  return path;
}

template <typename T, typename A>
inline size_t Expression<T,A>::skipWhitespaces(const std::string& input, size_t& pos) {
  // Skip whitespaces
  while ( pos < input.length() && std::isspace( input[pos] ) ) {
    ++pos;     
  }
  return pos;
}

template <typename T, typename A>
inline bool Expression<T,A>::startsWith(const std::string& input, size_t start, std::string_view candidate) {
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


template <typename T, typename A>
template <size_t N>
inline std::string Expression<T,A>::fetch(const std::string& input, size_t start, const std::array<std::string_view, N>& prefix) {
  for (const auto& candidate : prefix) {
    if ( startsWith(input,start,candidate) ) {
      return std::string(candidate);
    }
  }
  return ""; // No match found
}

template <typename T, typename A>
inline size_t Expression<T,A>::getIndex(std::vector<std::string>& container, std::string name) {
  for ( size_t i = 0; i < container.size(); i++) {
    if ( container[i] == name ) {
      return i;
    }
  }
  container.push_back(name);
  return container.size()-1;
}
 
template <typename T, typename A>
inline Node<T,A> Expression<T,A>::buildTree( Type type, const std::vector<Token>& tokens, std::optional<size_t> index ) {
  std::vector< std::variant< double, size_t, Node<T,A> > > operands;
  if ( index.has_value() ) {
    operands.emplace_back( index.value() );
  }

  std::stack< Node<T,A> > nodeStack;
  std::stack< Type > operatorStack;

  std::function<Node<T,A>(const Token&)> createNode = [&](const Token& token) -> Node<T,A> {
    switch (token.type) {
      case Token::Type::NUMBER:
        return Node<T,A>(this, std::stod(token.value));
      case Token::Type::VARIABLE:
        if ( auto slot = boundIterator(token.value) ) {
          // the iterator of an enclosing aggregation
          return Node<T,A>(this, Type::iterator, std::vector<std::variant<double, size_t, Node<T,A>>>{ slot.value() });
        }
        return Node<T,A>(this, Type::variable, token.value);
      case Token::Type::PATH:
      {
        if ( boundIterator(token.value) ) {
          throw std::runtime_error("LIMEX: Iterator '" + token.value + "' cannot be indexed or have fields");
        }
        // the index expressions are built first, so that the paths they contain precede this one
        Path description{ token.value, {} };
        std::vector<std::variant<double, size_t, Node<T,A>>> indices;
        for (const auto& step : token.children) {
          if (step.type == Token::Type::FIELD) {
            description.steps.emplace_back(step.value);
          }
          else {
            description.steps.emplace_back(std::nullopt);
            indices.emplace_back( buildTree(Type::group, step.children) );
          }
        }
        paths.push_back(std::move(description));
        std::vector<std::variant<double, size_t, Node<T,A>>> pathOperands{ paths.size() - 1 };
        for (auto& indexNode : indices) {
          pathOperands.push_back(std::move(indexNode));
        }
        return Node<T,A>(this, Type::path, std::move(pathOperands));
      }
      case Token::Type::GROUP:
        return buildTree(Type::group, token.children);
      case Token::Type::SET:
        return buildTree(Type::set, token.children);    
      case Token::Type::SEQUENCE:
        return buildTree(Type::sequence, token.children);    
      case Token::Type::FUNCTION_CALL:
        if (handle.isAggregator(token.value)) {
          // aggr(X) - aggregation over the array a path addresses, a name being a path without steps
          if (
            token.children.size() != 1 ||
            ( token.children[0].type != Token::Type::VARIABLE && token.children[0].type != Token::Type::PATH ) ||
            boundIterator(token.children[0].value)
          ) {
            throw std::runtime_error("LIMEX: Aggregator with () requires single array");
          }
          size_t aggrIdx = handle.getAggregatorIndex(token.value);
          Token array = token.children[0];
          array.type = Token::Type::PATH;
          Node<T,A> collNode = createNode(array);
          collNode.type = Type::array_path;
          return Node<T,A>(this, Type::aggregation, std::vector<std::variant<double, size_t, Node<T,A>>>{ aggrIdx, std::move(collNode) });
        }
        return buildTree(Type::function_call, token.children, handle.getFunctionIndex(token.value));    
      case Token::Type::AGGREGATION:
      {
        const auto& children = token.children;
        bool over = std::ranges::any_of(children, [](const Token& child) {
          return child.type == Token::Type::OPERATOR && child.value == "|";
        });
        if (over) {
          // aggr{ expression | bindings, conditions }
          return buildAggregationOver(token);
        }
        return buildTree(Type::aggregation, token.children, handle.getAggregatorIndex(token.value));
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
      Node<T,A> else_result = std::move(nodeStack.top());
      nodeStack.pop();
      Node<T,A> then_result = std::move(nodeStack.top());
      then_result.type = Type::group;
      nodeStack.pop();
      Node<T,A> condition = std::move(nodeStack.top());
      if ( condition.type == Type::if_ ) {
        condition.type = Type::group;
      }
      nodeStack.pop();

      nodeStack.push(Node<T,A>(this,Type::if_then_else, {std::move(condition), std::move(then_result), std::move(else_result) }));
      return;
    }

    // process all types of assignments
    if ( (int)operatorType >= (int)Type::assign ) {
      if (nodeStack.size() < 2) {
        throw std::runtime_error("LIMEX: Insufficient operands for assignment");
      }
      Node<T,A> right = std::move(nodeStack.top());
      nodeStack.pop();
      Node<T,A> left = std::move(nodeStack.top());
      nodeStack.pop();
      if ( left.type != Type::variable && left.type != Type::path ) {
        throw std::runtime_error("LIMEX: illegal target for assignment");
      }
      if ( operatorType == Type::assign ) {
        if ( left.type == Type::path ) {
          // the target path is kept for the evaluation of its indices, but not read
          nodeStack.push(Node<T,A>(this,Type::assign, {std::move(right), std::move(left) }));
        }
        else {
          nodeStack.push(Node<T,A>(this,Type::assign, {std::move(right) }));
        }
      }
      else {
        // add_assign, subtract_assign, multiply_assign, or divide_assign
        nodeStack.push(Node<T,A>(this,operatorType, {std::move(left), std::move(right) }));
      }
      return;
    }

    if (nodeStack.size() < 2) {
      throw std::logic_error("LIMEX: Insufficient operands for infix operator");
    }
    // Pop two operands and apply the operator
    Node<T,A> right = std::move(nodeStack.top());
    nodeStack.pop();
    Node<T,A> left = std::move(nodeStack.top());
    nodeStack.pop();

    if ( ( operatorType == Type::element_of || operatorType == Type::not_element_of ) && right.type == Type::path ) {
      // a path on the right of 'in' addresses an array
      right.type = Type::array_path;
    }
    nodeStack.push(Node<T,A>(this,operatorType, {std::move(left), std::move(right)}));
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
        node = Node<T,A>(this, postfixTypes.at(tokens[i+1].value), { std::move(node) });
      }
      if ( i > 0 && tokens[i-1].category == Token::Category::PREFIX ) {
        node = Node<T,A>(this, prefixTypes.at(tokens[i-1].value), { std::move(node) });
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
      if ( operatorType == Type::such_that ) {
        throw std::runtime_error("LIMEX: '|' is only allowed in an aggregation");
      }
      if ( operatorType == Type::range ) {
        throw std::runtime_error("LIMEX: A range '..' is only allowed in a binding of an aggregation");
      }
      if ( (int)operatorType >= (int)Type::assign ) {
        if ( i != 1 || ( tokens[0].type != Token::Type::VARIABLE && tokens[0].type != Token::Type::PATH ) ) {
          throw std::runtime_error("LIMEX: Assignment must start with a variable or path followed by the assignment operator");
        }
        if ( tokens[0].type == Token::Type::PATH ) {
          // the target path is the last path built, after the paths in its indices; the variables in its indices
          // remain inputs
          targetPath = paths.size() - 1;
        }
        else if ( operatorType == Type::assign ) {
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
  return Node<T,A>(this, type, operands);
}

template <typename T, typename A>
inline Node<T,A> Expression<T,A>::buildAggregationOver( const Token& token ) {
  const auto& children = token.children;
  size_t aggregator = handle.getAggregatorIndex(token.value);
  auto bar = std::ranges::find_if(children, [](const Token& child) {
    return child.type == Token::Type::OPERATOR && child.value == "|";
  });
  std::vector<Token> bodyTokens(children.begin(), bar);
  if (bodyTokens.empty()) {
    throw std::runtime_error("LIMEX: Aggregation requires an expression before '|'");
  }
  // the parts after '|', separated by ','
  std::vector< std::vector<Token> > parts(1);
  for (auto it = bar + 1; it != children.end(); ++it) {
    if (it->type == Token::Type::SEPARATOR && it->value == ",") {
      parts.emplace_back();
    }
    else {
      parts.back().push_back(*it);
    }
  }

  scopes.emplace_back();
  std::vector< Node<T,A> > bindings;
  std::vector<size_t> slots;
  size_t part = 0;
  // the leading parts of the form '<name> in ...' whose name is not bound are bindings
  for (; part < parts.size(); ++part) {
    const auto& tokens = parts[part];
    if (
      tokens.size() < 3 ||
      tokens[0].type != Token::Type::VARIABLE ||
      tokens[1].type != Token::Type::OPERATOR ||
      !infixTypes.contains(tokens[1].value) ||
      infixTypes.at(tokens[1].value) != Type::element_of ||
      boundIterator(tokens[0].value)
    ) {
      break;
    }
    const std::string& name = tokens[0].value;
    if ( std::ranges::contains(variables, name) || std::ranges::any_of(paths, [&name](const Path& path) { return path.name == name; }) ) {
      throw std::runtime_error("LIMEX: Iteration variable '" + name + "' shadows existing identifier");
    }
    std::vector<Token> domain(tokens.begin() + 2, tokens.end());
    auto range = std::ranges::find_if(domain, [](const Token& t) {
      return t.type == Token::Type::OPERATOR && t.value == "..";
    });
    // the domain is built before the iterator is bound, so that it may read the iterators of preceding bindings
    size_t slot = iterators.size();
    iterators.push_back(name);
    if (range != domain.end()) {
      std::vector<Token> lower(domain.begin(), range);
      std::vector<Token> upper(range + 1, domain.end());
      if (!lower.empty() && lower.front().type == Token::Type::PATH && lower.front().children.empty()) {
        // the tokenizer takes a name following 'in' for an array, but a bound is a value
        lower.front().type = Token::Type::VARIABLE;
      }
      if (lower.empty() || upper.empty()) {
        throw std::runtime_error("LIMEX: A range requires a lower and an upper bound: '" + name + " in ..'");
      }
      bindings.emplace_back(this, Type::range_binding, std::vector<std::variant<double, size_t, Node<T,A>>>{ slot, buildTree(Type::group, lower), buildTree(Type::group, upper) });
    }
    else if (domain.size() == 1 && domain[0].type == Token::Type::PATH) {
      Node<T,A> array = buildTree(Type::group, domain);
      // the group holds the path node, which addresses an array
      Node<T,A> path = std::get< Node<T,A> >(array.operands[0]);
      path.type = Type::array_path;
      bindings.emplace_back(this, Type::array_binding, std::vector<std::variant<double, size_t, Node<T,A>>>{ slot, std::move(path) });
    }
    else {
      throw std::runtime_error("LIMEX: Binding of '" + name + "' requires a range 'm..n' or an array");
    }
    scopes.back()[name] = slot;
    slots.push_back(slot);
  }
  if (bindings.empty()) {
    throw std::runtime_error("LIMEX: Aggregation requires syntax: aggr{ expression | var in range-or-array, ..., conditions }");
  }
  // the remaining parts are conditions, each checked by the last binding whose iterator it reads
  for (; part < parts.size(); ++part) {
    if (parts[part].empty()) {
      throw std::runtime_error("LIMEX: Empty condition in aggregation");
    }
    Node<T,A> condition = buildTree(Type::group, parts[part]);
    size_t level = 0;
    for (size_t b = 0; b < slots.size(); ++b) {
      if (condition.usesIterator(slots[b])) {
        level = b;
      }
    }
    bindings[level].operands.emplace_back(std::move(condition));
  }
  Node<T,A> body = buildTree(Type::group, bodyTokens);
  scopes.pop_back();

  std::vector<std::variant<double, size_t, Node<T,A>>> operands{ aggregator, std::move(body) };
  for (auto& binding : bindings) {
    operands.emplace_back(std::move(binding));
  }
  return Node<T,A>(this, Type::aggregation_over, std::move(operands));
}

template <typename T, typename A>
inline std::string Expression<T,A>::stringify() const {
  return root.stringify();
}

/*******************************
 ** Handle
 *******************************/

template <typename T, typename A>
inline size_t Handle<T,A>::getFunctionIndex(const std::string& name) const {
  for (size_t i = 0; i < functionNames.size(); i++) {
    if (functionNames[i] == name) {
      return i;
    }
  }
  throw std::logic_error("LIMEX: Unknown function '" + name + "'");
}

template <typename T, typename A>
inline size_t Handle<T,A>::getAggregatorIndex(const std::string& name) const {
  for (size_t i = 0; i < aggregatorNames.size(); i++) {
    if (aggregatorNames[i] == name) {
      return i;
    }
  }
  throw std::logic_error("LIMEX: Unknown aggregator '" + name + "'");
}

template <typename T, typename A>
inline bool Handle<T,A>::isAggregator(const std::string& name) const {
  return std::ranges::contains(aggregatorNames, name);
}

template <typename T, typename A>
inline void Handle<T,A>::addFunction(const std::string& name, std::function<T(const std::vector<T>&)> implementation) {
  if (std::ranges::contains(functionNames, name)) {
    throw std::runtime_error("LIMEX: Function '" + name + "' already exists");
  }
  if (std::ranges::contains(aggregatorNames, name)) {
    throw std::runtime_error("LIMEX: '" + name + "' is already registered as aggregator");
  }
  functionNames.push_back(name);
  functionImplementations.emplace_back(std::move(implementation));
}

template <typename T, typename A>
inline void Handle<T,A>::addAggregator(const std::string& name, std::function<T(const View<T>&)> implementation) {
  if (std::ranges::contains(aggregatorNames, name)) {
    throw std::runtime_error("LIMEX: Aggregator '" + name + "' already exists");
  }
  if (std::ranges::contains(functionNames, name)) {
    throw std::runtime_error("LIMEX: '" + name + "' is already registered as function");
  }
  aggregatorNames.push_back(name);
  aggregatorImplementations.emplace_back(std::move(implementation));
}

template <typename T, typename A>
inline T Handle<T,A>::membershipEvaluation(
  const std::string& name,
  const T& element,
  const A& array
) const {
  if constexpr (std::is_arithmetic_v<T>) {
    // the elements are compared where they are, without collecting them
    bool found = false;
    for (const T& elem : array) {
      if (elem == element) {
        found = true;
        break;
      }
    }
    if (name == "element_of") {
      return found;
    }
    if (name == "not_element_of") {
      return !found;
    }
    throw std::logic_error("LIMEX: Unknown membership '" + name + "'");
  }
  else {
    std::vector<T> args = { element };
    for (const T& elem : array) {
      args.push_back(elem);
    }
    return aggregatorImplementations.at(getAggregatorIndex(name))(View<T>(args));
  }
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

  // Aggregators, which skip undefined values, represented by NaN, except for size
  addAggregator(
    std::string("sum"),
    [](const LIMEX::View<double>& args) -> double
    {
      double result = 0.0;
      for ( double value : args ) {
        if ( !std::isnan(value) ) {
          result += value;
        }
      }
      return result;
    }
  );

  addAggregator(
    std::string("avg"),
    [](const LIMEX::View<double>& args) -> double
    {
      double result = 0.0;
      size_t count = 0;
      for ( double value : args ) {
        if ( !std::isnan(value) ) {
          result += value;
          ++count;
        }
      }
      if (count == 0) throw std::runtime_error("LIMEX: avg{} requires at least one defined argument");
      return result / (double)count;
    }
  );

  addAggregator(
    std::string("count"),
    [](const LIMEX::View<double>& args) -> double
    {
      size_t count = 0;
      for ( double value : args ) {
        if ( !std::isnan(value) ) {
          ++count;
        }
      }
      return (double)count;
    }
  );

  addAggregator(
    std::string("size"),
    [](const LIMEX::View<double>& args) -> double
    {
      return (double)args.size();
    }
  );

  addAggregator(
    std::string("min"),
    [](const LIMEX::View<double>& args) -> double
    {
      double result = DBL_MAX;
      bool defined = false;
      for ( double value : args ) {
        if ( !std::isnan(value) && result > value ) {
          result = value;
          defined = true;
        }
        else if ( !std::isnan(value) ) {
          defined = true;
        }
      }
      if (!defined) throw std::runtime_error("LIMEX: min{} requires at least one defined argument");
      return result;
    }
  );

  addAggregator(
    std::string("max"),
    [](const LIMEX::View<double>& args) -> double
    {
      double result = -DBL_MAX;
      bool defined = false;
      for ( double value : args ) {
        if ( !std::isnan(value) && result < value ) {
          result = value;
          defined = true;
        }
        else if ( !std::isnan(value) ) {
          defined = true;
        }
      }
      if (!defined) throw std::runtime_error("LIMEX: max{} requires at least one defined argument");
      return result;
    }
  );

  addAggregator(
    std::string("element_of"),
    [](const LIMEX::View<double>& args) -> double
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
    [](const LIMEX::View<double>& args) -> double
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

