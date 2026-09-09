# Antigravity Workflow Rules

1. **Implementation Plan Workflow**:
   - Whenever the user presents a problem or requests a change, the agent MUST first research, formulate a detailed implementation plan (`implementation_plan.md`), and present it to the user.
   - **STRICT RULE**: The agent is explicitly forbidden from making ANY code edits to any files before presenting the implementation plan and receiving the user's explicit command or approval to proceed (e.g. "proceed", "go ahead"). No matter how small the tweak, a plan must be approved first.
   - Once the command/approval to proceed is given, the agent must start execution immediately.
2. **Implicit Permission / Auto-execution**:
   - Once the user gives the proceed order, the agent should perform all edits, builds, and verifications **completely autonomously**.
   - Do not stop to ask for confirmation or intermediate permissions during implementation. Assume the user approves all actions, commands, and file writes. This does not include git pushes.
3. **Comprehensive Code Analysis**: Always analyze all interconnected files and code layers (UI, C++, JS, logic wrappers) thoroughly before formulating an implementation plan to avoid partial fixes, array size mismatches, or missing logical links.
4. **Build Version Auto-Increment**: Whenever modifying the application's code and preparing for a new build or github push, ALWAYS remember to increment the BUILD_NUMBER in index.html (e.g., from 0.01 to 0.02) to track iterations.
5. **Global Value Syncing**: Whenever changing a hardcoded boundary, maximum array size, or core constant (e.g., changing max partials from 512 to 256), ALWAYS perform a global grep search across all files (.html, .css, .js, .cpp, .h) to ensure every instance of the old value is updated to prevent UI/engine desyncs or partial fixes.
6. **Manual Git Push**: Do NOT automatically commit and push changes to GitHub after completing a task. You must wait for the user's explicit command (e.g., "push the changes", "commit this") before executing `git push`.
