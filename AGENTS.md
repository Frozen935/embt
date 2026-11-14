<!--
/****************************************************************************
 *  Copyright (C) 2026 <COPYRIGHT HOLDER>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/
-->

# Agent Submission and Review Rules

These rules apply to agent work in this repository, including code,
configuration, documentation, and repository policy changes. They take
precedence over conflicting inherited upstream submission workflows.

## 1. Work branches and human-only merging

- Prepare changes on a task-specific work branch. Do not create new commits
  on the default branch, protected branches, or shared integration branches.
- When submission is authorized, push only the work branch and open a PR.
  A request to submit or push changes does not authorize integration.
- Do not directly update integration branches through Git, GitHub APIs,
  browser actions, scripts, or other automation.
- Do not merge PRs, enable auto-merge, schedule merges, or approve your own
  delivery on behalf of the human reviewer. Final approval and merging are
  performed by a human.
- Do not bypass these restrictions by changing credentials, branch
  protections, review requirements, or repository permissions.
- Report a submitted PR as awaiting human review and merge. Do not report
  a branch push or PR creation as a completed merge.

### Pre-runtime integration hold

- Keep all open and future bring-up PRs in draft state and unmerged until
  the agreed Bluetooth runtime baseline has been demonstrated and its
  validation evidence recorded.
- Do not merge, enable auto-merge, or mark a PR ready for integration while
  this hold is active. Passing configuration, compilation, or lint checks
  alone does not demonstrate a working runtime.
- Continue development locally on task branches. A follow-up branch may
  start from the required unmerged work branch so the accumulated changes
  remain available for debugging without updating main.
- Unless the user requests a local draft only, record each authorized
  development change in an incremental draft PR and state its base branch
  and dependencies. Do not wait for a main-branch merge to continue work.
- After the runtime baseline is validated, integration still requires
  human review and a human decision to release the hold. Agents never
  perform the final merge.

## 2. Submission scope and documentation

- Include only the files required by the current task.
- Publish project documentation and rule updates through their existing
  documentation PRs. Keep these changes out of code and directory PRs;
  document pending implementation on its work branch without merging it.
- Keep local planning documents and unapproved documentation drafts out of
  code and directory-relocation PRs unless explicitly requested.
- Preserve existing project documentation when the task does not change it.
- Stage explicit paths. Do not use broad staging commands that can include
  unrelated drafts, generated files, or local artifacts.
- For updates to the same unmerged work area, amend or consolidate its
  original task commit and update the existing PR. Do not append a separate
  patch or open another PR for corrections within that work area.
- Use separate commits and PRs for independent work areas. Preserve upstream
  commit messages and provenance when importing community patches.
- A directory-only change must preserve file contents, permissions,
  symbolic links, copyright notices, and upstream comments.

## 3. English-only repository communication

- Write all agent-authored commit titles and bodies in English.
- Write all PR titles and descriptions in English.
- Write all agent-authored code comments, commit comments, PR comments,
  issue comments, and review comments in English.
- Do not include Chinese text in these fields or comments.
- Preserve existing upstream source and provenance. Do not rewrite imported
  files merely to normalize their language.

## 4. Commit titles and detailed descriptions

Every commit must have a descriptive English title and a meaningful body.
The body must explain:

- The problem or purpose of the change.
- The files, components, or behavior changed and the reason for the approach.
- The validation performed and any validation that was not performed.

Do not use an empty body, placeholder text, or a title alone as a submission.
Keep independent changes in separate commits and preserve original upstream
messages and complete SHAs when importing community patches.

## 5. PR titles and detailed descriptions

Every PR must have a descriptive English title and an English description
with enough detail for a reviewer who has not seen the conversation.
The description must cover:

- **Summary:** the concrete problem or goal.
- **Changes:** the final scope and relevant behavior or directory changes.
- **Validation:** checks actually completed and their results.
- **Limitations:** unperformed builds, tests, or hardware verification when
  relevant, plus any dependency on another PR.

Keep the title and description synchronized with the final implementation.
Do not claim that a check passed unless it was run and its result verified.

## 6. Pre-submission verification

- Confirm the work branch, base branch, staged files, and commit scope.
- Verify that commit messages, PR titles and descriptions, and authored
  comments comply with the English-only rule.
- Confirm that documentation drafts and unrelated files are not included.
- After pushing, verify the remote work-branch SHA and the PR file scope.
- Verify that the default branch has not been updated by the agent, that
  the PR remains unmerged, and that auto-merge is not enabled.

## 7. License headers and copyright

- Use [LICENSE_HEADER_TEMPLATE.md](LICENSE_HEADER_TEMPLATE.md) as the
  canonical file-header template. Do not duplicate template definitions
  in these rules.
- Use the full Apache-2.0 banner for new C, C++, and header files. Adapt
  the comment syntax for other file formats while keeping the full notice.
- Use the actual year and applicable copyright attribution for new or
  modified content. Do not hard-code an organization in the template.
- Preserve imported copyright and license notices. Do not bulk-rewrite
  upstream file headers or copy the example's company name and year.
