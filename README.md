# LIMEX

LIMEX is a header-only C++ library for parsing and evaluating mathematical expressions at runtime.

## Features

- Header-only: no compilation or linking needed
- Support for variables, collections, and access paths into arrays and structured values resolved by the caller
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
std::string input = "x + sum(a)";
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

### Access paths

A name followed by indices and fields, `name ( .field | [expression] )*`, is an access path, for instance
`a[i]`, `facilities[i].cost`, `grid[i][j]` or `facilities[location[1]].flags[j]`. LIMEX knows nothing of the
values a path addresses: it evaluates the index expressions and asks the caller to resolve the path. The paths
of an expression are listed by `getPaths()`, each with its name and its steps, a step being a field name or,
for an index, `std::nullopt`. The caller passes a `LIMEX::Resolver<T>` with two callbacks, each receiving the
position of the path in `getPaths()` and the values of its indices in order: `value` returns the value a path
addresses, and `collection` returns the array it addresses as a view. A path is resolved as an array where an
array is expected, namely as the argument of an aggregator (`sum(facilities[i].flags)`), on the right of `in`
and in a binding of an aggregation, and as a value everywhere else. The indices are passed as they are
evaluated, so that their numbering, conventionally from one, and their bounds are the caller's to check.

```cpp
LIMEX::Expression<double> expression("x + a[i]", handle);
std::vector<double> a = { 2.0, 5.0, 3.0 };
LIMEX::Resolver<double> resolver;
resolver.value = [&](size_t path, const std::vector<double>& indices) {
  // the only path is a[i], with a single index counting from one
  return a.at( (size_t)indices[0] - 1 );
};
std::cout << expression.evaluate( { 1.0, 2.0 }, {}, resolver ) << std::endl; // x = 1, i = 2: prints 6
```

### Assignments

An assignment assigns to a variable or to a path, e.g. `x := 3`, `location[i] := j` or
`facilities[i].cost += 5`. LIMEX writes nothing: `evaluate` returns the value to be assigned, `getTarget()`
the name of the target, and for a path target `getTargetPath()` its position in `getPaths()` and
`evaluateTargetIndices(...)` the values of its indices, so that the caller writes the value. For `:=` the
target is not read, whereas the variables in the indices of a target path are read; for a compound assignment
the target is read as well.

### Aggregations over an expression

`aggr{ expression | bindings, conditions }` aggregates the values of an expression over all combinations of
iterator values satisfying the conditions. A binding `i in m..n` iterates over the integers from `m` to `n`,
and a binding `i in X` over the elements of an array, given by a collection or a path. Bindings are nested in
their order, a later binding may use the iterators of earlier ones, and each condition is checked as soon as
all iterators it reads are bound:

```
sum{ facilities[i].cost | i in 1..size(facilities), i < 5 }
sum{ flow[i][j] * distance[location[i]][location[j]] | i in 1..n, j in 1..n }
min{ d | d in departures, d >= timestamp }
```

The leading parts of the form `v in ...` whose name `v` is not yet bound are bindings, the remaining parts are
conditions. A condition testing the membership of a variable that is not bound must therefore not directly
follow the bindings in this form, and is written in parentheses, e.g. `sum{ i | i in 1..3, (y in Y) }`. A
range `m..n` is only allowed in a binding. An iterator must not have the name of a variable or collection of
the expression, and is visible only within its aggregation.

### Undefined values

An undefined value is represented by a quiet NaN. The aggregators skip undefined values, so that `sum`, `avg`,
`min`, `max` and `count` consider the defined values only, whereas `size` counts all values. A binding over an
array skips undefined elements, and membership never finds an undefined value.

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

#### Ranges
- `m..n` (the integers from `m` to `n`, only in a binding of an aggregation)

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
| `count{a, b, ...}`  | Returns the number of defined arguments.                                         |
| `size{a, b, ...}`   | Returns the number of arguments, undefined ones included.                        |
| `min{a, b, ...}`    | Returns the smallest value.                                                      |
| `max{a, b, ...}`    | Returns the largest value.                                                       |
| `sum{ e \| i in X, c, ... }` | Returns the sum of `e` over all `i` in `X` satisfying the conditions.           |
| `avg{ e \| i in X, c, ... }` | Returns the average of `e` over all `i` in `X` satisfying the conditions.       |
| `count{ e \| i in X, c, ... }` | Returns the number of defined values of `e` over all `i` satisfying the conditions. |
| `min{ e \| i in m..n, c, ... }` | Returns the minimum of `e` over all `i` from `m` to `n` satisfying the conditions. |
| `max{ e \| i in m..n, c, ... }` | Returns the maximum of `e` over all `i` from `m` to `n` satisfying the conditions. |
| `sum(X)`, `count(X)`, `size(X)`, ... | Applies the aggregator to the values of a collection or of the array a path addresses. |
| `x ∈ { a, b, ...}`  | Returns true if `x` is in the set.                                               |
| `x ∉ { a, b, ...}`  | Returns true if `x` is not in the set.                                           |
| `x in X`            | Returns true if `x` is in the collection or path `X`.                            |
| `x not in X`        | Returns true if `x` is not in the collection or path `X`.                        |

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
