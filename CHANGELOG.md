# scip-clang ChangeLog

## v0.0.2 (testing)

- Adds support for automatically inferring correct include
  directories for gcc/g++. This means that the indexer will
  correctly find standard library headers even when a
  `compile_commands.json` file refers to gcc/g++ instead of clang.
  (https://github.com/sourcegraph/scip-clang/pull/178)

## v0.0.1 (testing)

- Symbols without hover docs will explicitly show "No documentation available".
  (https://github.com/sourcegraph/scip-clang/pull/173)
- Published binaries should work on Debian Buster and Ubuntu 18.04,
  instead of requiring Debian Bullseye / Ubuntu 20.04 or newer.
  (https://github.com/sourcegraph/scip-clang/pull/174)

## v0.0.0 (testing)

- Initial release with code nav support for various
  language features like macros, #include pragmas, types,
  functions, methods, local variables etc.
