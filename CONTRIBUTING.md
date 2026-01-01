# Contributing Guide

Thank you for your interest in contributing! This guide outlines our step-by-step PR process to ensure code quality and maintainability.

## Step-by-Step PR Logic

We follow an **atomic commit** approach where each PR should build the system incrementally, one logical step at a time. This makes code review easier and preserves the educational value of seeing how the system was constructed.

### Commit Structure

A single PR should **not** be a "code dump." Instead, break your implementation into logical commits:

#### Commit 1: Define Interfaces and Data Structures
- Add header files (`include/`)
- Define function signatures
- Define data structures and their relationships
- Document the API contract

#### Commit 2: Implement Core Logic
- Implement the main algorithms
- Add the primary business logic (e.g., hash table operations, network loop, memory allocator)
- Focus on correctness first

#### Commit 3: Add Tests and Documentation
- Write unit tests (`tests/`)
- Add integration tests if applicable
- Update documentation (`docs/`)
- Ensure all tests pass

### Linear History

We prefer **merge commits** or **rebase-merges** to preserve the step-by-step history of how the system was built. This allows future contributors to understand the evolution of the codebase.

### Before Submitting

- [ ] All commits follow the atomic structure above
- [ ] Code passes all CI checks (formatting, static analysis, tests)
- [ ] Documentation is updated
- [ ] Tests are added and passing
- [ ] PR description explains the changes and rationale

### CI Requirements

All PRs must pass:
- Code formatting checks (clang-format for C/C++, rustfmt for Rust)
- Static analysis (cppcheck for C/C++, clippy for Rust)
- All tests must pass

These checks are enforced via branch protection rules on `main`.
