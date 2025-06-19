# LIMEX

LIMEX is a header-only C++ library for parsing and evaluating mathematical expressions at runtime.

## Features

- Header-only: no compilation or linking needed
- Support for simple variables and indexed variables, i.e. arrays
- Customizable evaluation through custom handlers allowing to build, e.g., [constraint programming](https://github.com/bpmn-os/cp) models. 

## Getting started

### Include LIMEX

Simply include the header in your project:

```cpp
#include "limex.h"
```

No linking is required since LIMEX is header-only.

### Basic usage

```cpp
#include "limex.h"
#include <iostream>

int main() {
  std::string input = "1 + 2 * 3";
  LIMEX::Handle<double> handle;
  LIMEX::Expression<double> expression(input, handle);
  std::cout << "Result: " << expression.evaluate() << std::endl; // prints 7
}
```

### Using variables and collections

```cpp
std::string input = "x + a[i]";
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

// Retrieve collections and respective values in expression
std::vector< std::vector<double> > collectionValues;
for ( auto collectionName : expression.getCollections() ) {
  std::cout << "Enter comma separated values for collection '" << collectionName << "': ";
  std::vector<double> values;
  std::getline( std::cin, input );
  auto parts = std::ranges::views::split( input, ',' );
  for ( auto value : std::ranges::to<std::vector<std::string>>(parts) ) {
    values.push_back( std::stod(value) );
  } 
  collectionValues.push_back(values);
} 

// Evaluate expression with given values
std::cout << "Result: " << expression.evaluate(variableValues,collectionValues) << std::endl;
```

## Supported operators and symbols

LIMEX supports a wide range of operators and symbols for mathematical and logical expression parsing, including both symbolic and textual forms.

### Boolean literals

- `true`, `false`

### Ternary expressions

- `if ... then ... else ...`
- `?`, `:` (interpreted as `then`, `else`)

### Prefix operators

- `-` (negation)
- `!`, `¬` (logical NOT)

### Postfix operators

- `²`, `³` (square, cube)

### Infix operators

#### Arithmetic
- `+`, `-`, `*`, `/`, `^` (addition, subtraction, multiplication, division, exponentiation)

#### Logical
- `&&`, `||` (AND, OR)
- `and`, `or`
- `∧`, `∨` (Unicode AND, OR)

#### Membership
- `in`, `not in`
- `∈`, `∉` (Unicode equivalents)

#### Comparison
- `==`, `!=`, `<`, `<=`, `>`, `>=`
- `≠`, `≤`, `≥` (Unicode equivalents)

#### Assignment
- `:=`, `≔` (assignment)
- `+=`, `-=`, `*=`, `/=` (compound assignments)

#### Sets
- Operands within `{` and ¸`}` and separated by `,` can be used to define sets.

### Functions and aggregators

Functions can be applied on `,` separated arguments and aggregators can be used on sets.

| Symbol | Alias   |
|--------|---------|
| `∑`    | `sum`   |
| `√`    | `sqrt`  |
| `∛`    | `cbrt`  |

You can use either the symbol or its textual alias in expressions.

## Built-in functions and aggregators

LIMEX includes a number of built-in functions and aggregators:

| Function/Aggregator |Description                                                                      |
|---------------------|----------------------------------------------------------------------------------|
| `abs(x)`            | Returns the absolute value of `x`.                                               |
| `pow(x, y)`         | Computes `x` raised to the power of `y`.                                         |
| `sqrt(x)`           | Square root of `x`.                                                              |
| `cbrt(x)`           | Cube root of `x`.                                                                |
| `sum{a, b, ...}`    | Returns the sum of all arguments.                                                |
| `avg{a, b, ...}`    | Returns the average of all arguments.                                            |
| `count{a, b, ...}`  | Returns the number of arguments.                                                 |
| `min{a, b, ...}`    | Returns the smallest value.                                                      |
| `max{a, b, ...}`    | Returns the largest value.                                                       |
| `x ∈ { a, b, ...}`  | Returns true if `x` is in the set.                                               |
| `x ∉ { a, b, ...}`  | Returns true if `x` is not in the set.                                           | 

Custom function and aggregator definitions can be added.

## Build test example

Compile and run tests:
```
make clean; make; ./test
```

## License

MIT License

Copyright (c) 2025 Asvin Goel
