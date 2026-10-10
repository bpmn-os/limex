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
std::vector< std::vector<double> > collections;
for ( auto collectionName : expression.getCollections() ) {
  std::cout << "Enter comma separated values for collection '" << collectionName << "': ";
  std::vector<double> values;
  std::getline( std::cin, input );
  auto parts = std::ranges::views::split( input, ',' );
  for ( auto value : std::ranges::to<std::vector<std::string>>(parts) ) {
    values.push_back( std::stod(value) );
  } 
  collections.push_back(values);
} 

// Pass each collection as a view of the values held above
std::vector< LIMEX::View<double> > collectionValues( collections.begin(), collections.end() );

// Evaluate expression with given values
std::cout << "Result: " << expression.evaluate(variableValues,collectionValues) << std::endl;
```

### Views of collections

A collection is passed to an expression as a `LIMEX::View<T>`, a read-only view of values held elsewhere,
so that no collection is copied when an expression is evaluated. A view is either contiguous, constructed from
a `std::vector<T>` or from the address and the number of its values, or computed, constructed from the number
of values and an accessor returning the value at a given position, counting from zero. A computed view lets
the caller present values that are not stored contiguously or not stored as `T`. The values must outlive the
view; a view cannot be constructed from a temporary vector.

```cpp
std::vector<double> stored = { 1.0, 2.0, 3.0 };
LIMEX::View<double> contiguous( stored );
LIMEX::View<double> computed( 5, [](size_t k) { return 2.0 * (double)(k + 1); } ); // 2, 4, 6, 8, 10
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
| `log(x)`            | Natural logarithm of `x`.                                                        |
| `exp(x)`            | Euler's number *e* raised to the power of `x`.                                   |
| `sum{a, b, ...}`    | Returns the sum of all arguments.                                                |
| `avg{a, b, ...}`    | Returns the average of all arguments.                                            |
| `count{a, b, ...}`  | Returns the number of arguments.                                                 |
| `min{a, b, ...}`    | Returns the smallest value.                                                      |
| `max{a, b, ...}`    | Returns the largest value.                                                       |
| `sum{ i in X \| i >= a, ... }` | Returns the sum of elements in `X` satisfying conditions.                     |
| `avg{ i in X \| i >= a, ... }` | Returns the average of elements in `X` satisfying conditions.                 |
| `count{ i in X \| i >= a, ... }` | Returns the count of elements in `X` satisfying conditions.                 |
| `min{ i in X \| i >= a, ... }` | Returns the minimum of elements in `X` satisfying conditions.                 |
| `max{ i in X \| i >= a, ... }` | Returns the maximum of elements in `X` satisfying conditions.                 |
| `x ∈ { a, b, ...}`  | Returns true if `x` is in the set.                                               |
| `x ∉ { a, b, ...}`  | Returns true if `x` is not in the set.                                           |
| `x in X`            | Returns true if `x` is in the collection `X`.                                    |
| `x not in X`        | Returns true if `x` is not in the collection `X`.                                |

Custom function and aggregator definitions can be added. A function receives its evaluated arguments as a
`std::vector<T>`, an aggregator receives its values as a `const LIMEX::View<T>&`, whether they are the values
of a collection or the evaluated arguments of an aggregation such as `second{a, b, c}`:

```cpp
LIMEX::Handle<double> handle;
handle.addFunction( "half", [](const std::vector<double>& args) { return args.at(0) / 2; } );
handle.addAggregator( "second", [](const LIMEX::View<double>& values) { return values[1]; } );
```

## Build test example

Compile and run tests:
```
make clean; make; ./test
```

## License

MIT License

Copyright (c) 2025 Asvin Goel
