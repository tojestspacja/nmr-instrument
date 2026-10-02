# Checkpoint Protocol

## Proposed operating rule

Keep stable mission, changing task state, decisions, issues, source history and test evidence in durable files. A new agent should reconstruct engineering progress without the old account's chat memory. This is a project procedure, not a claim that a provider automatically loads particular filenames.

## Before switching

Finish a small coherent unit or clearly record the stopping point. Pause conflicting writers. Preserve staged, unstaged and untracked source plus important local-only evidence. Review ignored artifacts and secrets separately; do not copy the whole home directory or account configuration.

Update one checkpoint with actual source/config identities and raw logs. Separate completed code, tests executed, missing tests and incomplete outputs. A plan listing requirements is not a record of current implementation state.

Check that the bootstrap file points to the current checkpoint and a finite next task.

## After switching

Read the checkpoint, inspect the live tree, compare identities, resolve mismatches and run targeted smoke tests. Do not clean or regenerate the tree before recording what is there. Distinguish inherited results from checks actually rerun, then continue safe implementation.

## When to checkpoint

After meaningful milestones, interface/design changes, important failed validations, major file moves, and before logout/context reset. If the old session is already blocked, reconstruct from saved files and logs with explicit uncertainty.

## Concurrency

One coordinator owns shared checkpoint/issue state. Other agents use disjoint files or isolated worktrees and include baseline identities in their handbacks.

## Permissions

Use only material authorized for the receiving account. Do not export organization credentials, tokens, session authentication files or unrelated private data. Local-file access does not itself authorize remote pushes or hardware operation.

## Completion standard

The next agent must be able to identify actual source state, WIP, last real test, unresolved physical risks and the next finite action. Do not promise restoration of hidden model state or omitted work.
