#!/bin/sh
# Installs this repo's git hooks into .git/hooks.
#
# Hooks live in .git/hooks, which git does NOT version - a fresh clone has none.
# So the real copies are kept here and this script puts them in place. Run it once
# after cloning:
#
#   sh Build/GitHooks/install.sh
#
# We copy instead of setting core.hooksPath because that setting REPLACES the whole
# hooks directory, which would disable the four hooks Git LFS installs there
# (post-checkout, post-commit, post-merge, pre-push). Losing pre-push means LFS
# objects never upload and everyone else pulls empty pointers.

set -e

root=$(git rev-parse --show-toplevel)
src="$root/Build/GitHooks"
dst="$(git rev-parse --git-dir)/hooks"

for hook in pre-commit; do
	[ -f "$src/$hook" ] || continue

	if [ -f "$dst/$hook" ] && ! cmp -s "$src/$hook" "$dst/$hook"; then
		cp "$dst/$hook" "$dst/$hook.bak"
		echo "  existing $hook differed - backed up to $hook.bak"
	fi

	cp "$src/$hook" "$dst/$hook"
	chmod +x "$dst/$hook"
	echo "installed: $hook"
done

echo "Done. Hooks are in $dst"
