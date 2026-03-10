#!/usr/bin/env bash
set -euo pipefail

TARGET_BRANCH="RihenAcademicSchoolBeginning"
REMOTE_NAME="origin"
STATE_FILE=".git/NK_LAST_PUBLISH_COMMIT"

if [[ $# -ge 1 ]]; then
  COMMIT_SHA="$1"
elif [[ -f "$STATE_FILE" ]]; then
  COMMIT_SHA="$(cat "$STATE_FILE")"
else
  echo "Usage: $0 <commit-sha>"
  echo "No commit sha found in $STATE_FILE"
  exit 1
fi

START_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

git switch "$TARGET_BRANCH"
git pull --rebase "$REMOTE_NAME" "$TARGET_BRANCH"

if git merge-base --is-ancestor "$COMMIT_SHA" HEAD; then
  echo "Commit already present on $TARGET_BRANCH: $COMMIT_SHA"
else
  git cherry-pick "$COMMIT_SHA"
fi

git push "$REMOTE_NAME" "$TARGET_BRANCH"

if [[ "$START_BRANCH" != "$TARGET_BRANCH" ]]; then
  git switch "$START_BRANCH"
fi

echo "Done: pushed $COMMIT_SHA to $TARGET_BRANCH"
