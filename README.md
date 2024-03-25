# Get-Me - Source Code Navigation Tool

Finds the fastest way to get what you need.

```c++
struct Apple {};
struct Kiwi {};
struct Berry {};
struct FruitSalad {};

FruitSalad makeFruitSalad(Apple, Kiwi, Berry);
```

*I want a `FruitSalad`, but I don't know what the best way to get it is.*

```console
$ ./bin/get-me -p . -t FruitSalad ../source.cpp

|Transitions|: 5
Graph size: |V| = 9, |E| = 14
path length distribution: [(1, 1), (1, 4)]
generated 2 paths
path #0: FruitSalad FruitSalad() -> remaining: {}
path #1: FruitSalad makeFruitSalad(Apple, Kiwi, Berry), Kiwi Kiwi(), Apple Apple(), Berry Berry() -> remaining: {}
```

Sometimes it is hard to figure out how to get an instance of a specific type from documentation or code alone.
`get-me` is a tool that analyses dependencies based on what is *acquired* (e.g., return type) and what is *required* (e.g., parameter types) from function signatures, constructors, member variables and static class variables.
Given the name of a type (`-t <name>`), `get-me` builds a graph of these dependencies to determine the easiest way to acquire an instance of that type.
In the above example, this is the `FruitSalad`, which can be acquired in two ways:

- the default constructor of `FruitSalad`
- calling the function `makeFruitSalad`, and acquiring the parameters via their default constructors

Note: I wanted to try out `ranges` in this project, so the only `for`-loop you will find is in `benchmarks/`.

## Open Issues

`get-me` is missing some features to become generally usable.

- Context awareness

  When querying for a type, the user may already have some types available because the location where they want that type is inside a function scope.
  Providing the source location where the user is requesting the type will increase the quality of the results considerably by using the already existing instances of types (e.g., parameters, local variables).

- Scoring of the found paths

  Currently, the output limits the number of paths that are printed through configuration options.
  These paths are taken from all paths, sorted ascending by their length and then the number of remaining types that were not resolved.
  Together with context awareness, this improves the quality of the output considerably.
  For example, score a transition (function-like thing) with a better value if it re-uses a local variable.

- ...

## Installation

1. Install [Conan](https://conan.io/)
2. Install clang trunk (clang-18 may also work)
3. If you do not have a default profile, run `CC=clang CXX=clang++ conan profile detect` to make sure it is using clang, and add to your .conan2/profiles/default:

```text
 [conf]
 tools.build:compiler_executables={'c': 'clang', 'cpp': 'clang++'}
```
4.  Get dependencies, build the tool and run the tests
```shell
conan test . ./conanfile.py --build missing
```

## Usage

CLI arguments:

```text
$ ./bin/get-me --help

USAGE: get-me [options] <source0> [... <sourceN>]

OPTIONS:

Generic Options:

  --help                      - Display available options (--help-hidden for more)
  --help-list                 - Display list of available options (--help-list-hidden for more)
  --version                   - Display the version of this program

get-me:

  --config=<value>            - Config file path
  --dump-config               - Dump the current configuration
  --extra-arg=<string>        - Additional argument to append to the compiler command line
  --extra-arg-before=<string> - Additional argument to prepend to the compiler command line
  -i                          - Run with interactive gui
  -p <string>                 - Build path
  --query-all                 - Query every type available (that has a transition)
  -t <string>                 - Name of the type to get
  -v                          - Verbose output
```

Default config:

```console
$ ./bin/get-me --dump-config

Loaded configuration:
---
EnableFilterOverloads: false
EnablePropagateInheritance: true
EnablePropagateTypeAlias: true
EnableTruncateArithmetic: true
EnableFilterArithmeticTransitions: true
EnableFilterStd: false
EnableGraphBackwardsEdge: true
EnableVerboseTransitionCollection: false
MaxGraphDepth:   4
MaxRemainingTypes: 18446744073709551615
MaxPathLength:   4
MaxPathOutputCount: 10
...
```

Note that the tool automatically dumps the graph that was built for the queried type to `graph.dot` in the working directory.
