#!/usr/bin/env bash
set -euo pipefail

TARGET_BRANCH="main"
REMOTE_NAME="origin"
STATE_FILE=".git/NK_LAST_PUBLISH_COMMIT"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 \"commit message\""
  exit 1
fi

COMMIT_MESSAGE="$*"
START_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

if [[ -z "$(git status --porcelain)" ]]; then
  echo "No local changes to commit."
  exit 1
fi

git add -A
git commit -m "$COMMIT_MESSAGE"
COMMIT_SHA="$(git rev-parse HEAD)"
echo "$COMMIT_SHA" > "$STATE_FILE"

echo "Created commit: $COMMIT_SHA"

if [[ "$START_BRANCH" == "$TARGET_BRANCH" ]]; then
  git pull --rebase "$REMOTE_NAME" "$TARGET_BRANCH"
else
  git switch "$TARGET_BRANCH"
  git pull --rebase "$REMOTE_NAME" "$TARGET_BRANCH"
  git cherry-pick "$COMMIT_SHA"
fi

git push "$REMOTE_NAME" "$TARGET_BRANCH"

if [[ "$START_BRANCH" != "$TARGET_BRANCH" ]]; then
  git switch "$START_BRANCH"
fi

echo "Done: pushed $COMMIT_SHA to $TARGET_BRANCH"
