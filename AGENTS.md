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
- When using a new library and a module facade has not been exported for it yet, query the user for instructions on how to treat it.

## MANDATORY: Git Rules

NEVER stage, commit, pull or push any changes EVER. This will only be required on request from the user.

**Note**: When the user has asked you to make commits, then the `graphify` post-commit hook will run after a commit has been made. Always commit these `graphify` artifacts as a new, separate commit with message `chore: commit graph`.

NEVER use squash commit merges unless explicitly asked to do so by the user.
