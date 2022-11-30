---
name: project-publish-cleanup
description: "Use when cleaning a project repository, removing obsolete files, adding documentation, creating an independent GitHub repository, rewriting commit dates, committing all files, pushing history, or verifying GitHub synchronization."
argument-hint: "Describe the project cleanup, files to keep/remove, target GitHub repository, and desired commit-date range."
user-invocable: true
disable-model-invocation: false
---

# Project Cleanup and Publishing

Use this workflow to turn a local project into a clean, documented Git repository and publish it to GitHub.

## Safety Rules

- Inspect the worktree before changing anything.
- Never delete a repository, branch, file, or commit without explicit confirmation.
- If the target repository is a fork, ask the user whether to delete that fork and recreate it as a new independent repository before taking that action.
- Treat compiled binaries, test output, scratch files, copied reference material, and old experiments as removable only after checking project requirements.
- Never delete the original repository when detaching a fork.
- Rewriting commit dates changes commit IDs. Explain this before rewriting history.
- Use `--force-with-lease`, not `--force`, after a history rewrite.
- Do not claim a push succeeded unless the command output confirms it and local `HEAD` equals `origin/main`.
- Preserve unrelated user changes.

## Procedure

### 0. Ask for publishing decisions

Before destructive cleanup, repository recreation, or history rewriting, ask the user for:

- The exact GitHub owner and repository name
- Confirmation to delete the target repository if it is a fork
- Which files should be kept and which should be removed
- The desired commit date range, including year, start date, end date, and timezone
- The desired number of meaningful commits

Do not guess historical dates. If dates are not provided, use current dates and do not rewrite history.

### 1. Audit the repository

Collect:

- `git status --short --branch`
- `git remote -v`
- `git log --format='%h %ad %cd %s' --date=iso --all`
- A recursive file inventory
- Existing README, assignment requirements, build instructions, and tests

Identify the concrete project files that are required and list proposed removals separately.

### 2. Decide what to keep

Usually keep:

- Source files required to build the project
- Configuration files required at runtime
- README and project documentation
- Assignment or report files when the user asks to include all project files

Usually propose removing:

- Generated executables and object files
- Scratch input/output files
- Obsolete source experiments and backups
- Test binaries or temporary test directories that are not part of the deliverable
- Large reference PDFs or copied material not needed to run the project

Ask for confirmation before destructive cleanup if the requested keep/remove list is unclear.

### 3. Validate before editing

Run the project build or the narrowest available test. For POSIX C++ socket projects on Windows, use WSL or Linux:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -pthread tracker.cpp -o tracker
g++ -std=c++17 -Wall -Wextra -pedantic -pthread client.cpp -o client
```

Read the relevant source and documentation before deciding that a file is useless.

### 4. Make focused changes

- Use the repository's existing style.
- Add or update a README with prerequisites, build commands, run commands, workflows, architecture diagrams, supported commands, troubleshooting, and honest limitations.
- Add `.gitignore` entries for generated build artifacts.
- Keep documentation consistent with the implementation. Do not describe unfinished peer transfer or tracker synchronization as complete.
- Use Mermaid diagrams in Markdown when architecture or sequence diagrams improve understanding.

### 5. Add all requested files

Before committing, run:

```bash
git status --short
git ls-files
```

Confirm that every requested file is tracked. Do not silently omit documentation such as `Assignment.md`, `Report.md`, `RUNNING.md`, or `HowToRun.md` when the user requests all files.

When the user requests everything new, stage the complete intended project explicitly:

```bash
git add --all
git status --short
```

Check that the staged file list contains every required source, configuration, and documentation file.

### 6. Create dated commits

Only rewrite dates when the user explicitly requests it. Use meaningful commits, for example:

- Implement core source changes
- Remove obsolete project artifacts
- Add project README
- Add assignment and run documentation

For a requested historical date range, set both timestamps for each commit:

```bash
GIT_AUTHOR_DATE="2022-10-03T12:00:00+05:30" \\
GIT_COMMITTER_DATE="2022-10-03T12:00:00+05:30" \\
git commit -m "Implement core source changes"
```

On PowerShell:

```powershell
$env:GIT_AUTHOR_DATE = "2022-10-03T12:00:00+05:30"
$env:GIT_COMMITTER_DATE = "2022-10-03T12:00:00+05:30"
git commit -m "Implement core source changes"
Remove-Item Env:GIT_AUTHOR_DATE
Remove-Item Env:GIT_COMMITTER_DATE
```

Use dates supplied by the user. Do not invent dates that imply work occurred at a time the user did not request.

### 7. Handle fork separation

A fork relationship cannot be removed with `git`. If the target repository is reported as a fork and the user confirms deletion:

1. Confirm that the original upstream repository will not be deleted.
2. Delete only the user's fork, using GitHub settings or authenticated GitHub CLI.
3. Create a new empty repository with the same desired owner and name.
4. Add every intended local file with `git add --all`.
5. Keep the local Git history and point `origin` at the new repository.

Check fork status before deletion:

```bash
gh repo view OWNER/REPOSITORY --json nameWithOwner,isFork,url
```

Delete and recreate only after confirmation:

```bash
gh repo delete OWNER/REPOSITORY --yes
gh repo create OWNER/REPOSITORY --public
git remote set-url origin https://github.com/OWNER/REPOSITORY.git
```

With authenticated GitHub CLI, create an independent public repository when no existing fork needs deletion:

```bash
gh repo create OWNER/REPOSITORY --public --source=. --remote=origin
```

If `origin` already exists, push separately:

```bash
git remote set-url origin https://github.com/OWNER/REPOSITORY.git
git push -u origin main
```

### 8. Push and verify

For ordinary commits:

```bash
git push -u origin main
```

After rewriting history:

```bash
git push --force-with-lease origin main
```

Verify all of the following:

```bash
git status --short --branch
git rev-parse HEAD
git rev-parse origin/main
gh repo view OWNER/REPOSITORY --json nameWithOwner,isFork,defaultBranchRef,url
git log --format='%h %ad %cd %s' --date=iso -10
```

The final state should show a clean worktree, equal local and remote commit IDs, the expected default branch, and `isFork: false` when an independent repository was requested.

If the target was a fork, explicitly report whether deletion and recreation occurred. If deletion was not confirmed or the CLI is not authenticated, stop before deleting anything and tell the user what action is required.

## Completion Report

Report concisely:

- Files added, changed, or removed
- Build and smoke-test results
- Commit IDs and date range
- Remote repository URL
- Whether the repository is independent or still a fork
- Any remaining untracked files or limitations
