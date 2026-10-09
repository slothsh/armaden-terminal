## MANDATORY: Local Code Search/Traversal

Always use the `graphify` skill to query and search for any local code if the graphify-out/ directory has been initialized. Only revert to manual grepping as a last resort.

NEVER SELF INITIALIZE THE GRAPHIFY DIRECTORY UNLESS EXPLICITLY INSTRUCTED TO DO SO.

Fallback to normal grep usage if it doesn't exist.

## MANDATORY: Use td for Task Management

Run td usage --new-session at conversation start (or after /clear). This tells you what to work on next.

Sessions are automatic (based on terminal/agent context). Optional:
- td session "name" to label the current session
- td session --new to force a new session in the same context

Use td usage -q after first read.

When the user asks you to create an epic in `td`, **always** add the epic and its tasks with these guidelines:

- Use **VERY** elaborate descriptions for each task on the epic using the --description flag.
- Add relevant labels to each task using the --label task. 2-5 is more than enough.
- Keep the name of the epic concise and relevant to the overall theme of the tasks.

## MANDATORY: Worktree Management

Whenever the user gives you an instruction to work on a git worktree, use the `wt` (worktrunk) command to manage worktrees.

Use the `wt-switch-create` skill for managing worktrees and/or the `worktrunk` skill for more detailed information on how to use the `wt` tool.

## MANDATORY: Code Comment Conventions

Do NOT add documentation comments to ANY code. This will only be required on request from the user.

## MANDATORY: Code Style Conventions

- Always use east-const convention for everything.
- Prefer import modules where possible. Only include headers for unsupported libraries.
- Always prefix member function calls with `this->` and static member function calls with the class's name (`Foo::static_member()`).
- Attach the pointer or reference symbol to the `const` qualifier when one is available, otherwise to the type. For a const pointer to a const type, attach the second `const` qualifier to the pointer. Examples: `Foo const* foo`, `Foo const& foo`, `Foo const *const foo`, `Foo* foo`, `Foo& foo`.
- Always use braces on `if` / `else if` / `while` / `for` / `do` bodies. Never write single-line or brace-less control statements.
- Always favour trailing return types and use them on everything, including free functions, member functions, Qt signals, slots, `Q_INVOKABLE` methods, and overrides. Constructors, destructors, conversion operators and lambdas are exempt (constructors/destructors/conversion operators cannot have one; lambdas may keep their implicit return type).
- Use `static_cast` / `reinterpret_cast` (or `const_cast` / `dynamic_cast` where genuinely appropriate) instead of raw C-style casts such as `(Type)value` or `(void)value`. Function-style casts like `Type(value)` are fine.
- Group chunks of code by concern and separate the chunks with a blank line.
- For function declarations and function calls with long or large parameter/argument lists, put each expression on its own line and align the closing parenthesis with the indentation level of the statement/declaration that opened it:

    ```
    void foo(
        int,
        float,
        string
    ) {
        // ...
    }

    foo(
        foo + bar + baz,
        foo + bar + baz,
        bar(
            foo + bar + baz,
            123
        )
    );
    ```

- When a namespace access exceeds two qualifiers, consider introducing a namespace alias for it, and keep the alias name consistent across files.
- Namespace formatting: put a single blank line after the opening brace, keep the contents at the same indentation as the `namespace` declaration, put a blank line before the closing brace, and annotate the closing brace with an `END OF NAMESPACE` marker followed by the namespace's qualified name in backticks (use `anonymous` for an anonymous namespace):

    ```
    namespace mts::map {

    void foo() {
        // ...
    }

    } // END OF NAMESPACE `mts::map`
    ```

- Module partitions: split an oversized module by concern. The primary module interface unit stays thin and re-exports the public partitions; internal partitions are imported but never re-exported.
  - File naming: `<module>_<partition>.cppm`, declaring `export module <module>:<partition>;` (e.g. `map_loader.cppm` → `export module map:loader;`).
  - Public surface: declare the partition as `export module M:P;` and re-export it from the primary with `export import :P;`.
  - Internal helpers: declare the partition as `export module M:P;` but import it in the primary with a plain `import :P;` (never `export`). Partitions are only importable inside their own module, so the primary's failure to re-export keeps the helpers out of the public surface.

- Logging injection (deliberate force-include): every translation unit is force-included with a generated header that defines the per-TU `ModuleScope` (its `LIBRARY_NAME`/`MODULE_NAME`) and then includes `logging.hpp`. Calls are `mts::log::info(...)` etc. with `Scope = ModuleScope` defaulted, so logs carry rich per-TU module info without an explicit template argument. This requires the logging API to be a force-included header (a module is parsed once and cannot see the caller's `ModuleScope`); do not replace it with an explicit `mts::log::info<ModuleScope>(...)` or move the API into a module.

- When using a new library and a module facade has not been exported for it yet, query the user for instructions on how to treat it.

## MANDATORY: Design Conventions

- Prefer static polymorphism as much as possible. Model interfaces/protocols with concepts and type traits rather than inheritance hierarchies.
- Only use dynamic dispatch (virtual functions / abstract base classes) when it is genuinely the most suitable option.

## MANDATORY: Git Rules

NEVER stage, commit, pull or push any changes EVER. This will only be required on request from the user.

**Note**: When the user has asked you to make commits, then the `graphify` post-commit hook will run after a commit has been made. Always commit these `graphify` artifacts as a new, separate commit with message `chore: commit graph`.

NEVER use squash commit merges unless explicitly asked to do so by the user.
