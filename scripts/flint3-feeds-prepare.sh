#!/bin/sh
# Prepare the pinned feeds and reapply this fork's source-only overrides.
# Never reset, clean, stash, or change an existing feed to a different revision.
set -eu

task_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$task_root"

if [ -f feeds.conf ]; then
	echo 'feeds.conf overrides the pinned defaults; review it before proceeding.' >&2
	exit 1
fi

check_feed_revisions() {
	for task_feed in packages luci routing telephony video; do
		task_uri=$(awk -v name="$task_feed" '$1 == "src-git" && $2 == name { print $3 }' feeds.conf.default)
		case "$task_uri" in
			*^*) task_expected=${task_uri##*^} ;;
			*) echo "Missing pinned revision for $task_feed" >&2; return 1 ;;
		esac
		if [ -e "feeds/$task_feed" ]; then
			if [ ! -e "feeds/$task_feed/.git" ]; then
				echo "feeds/$task_feed is not a Git checkout; leaving it untouched." >&2
				return 1
			fi
			task_actual=$(git -C "feeds/$task_feed" rev-parse HEAD)
			if [ "$task_actual" != "$task_expected" ]; then
				echo "Feed $task_feed is at $task_actual, expected $task_expected." >&2
				echo 'Save local changes and switch the feed explicitly; no automatic reset was performed.' >&2
				return 1
			fi
		fi
	done
}

check_feed_revisions
# The feeds tool may delete and re-clone an existing checkout when its saved
# source URL differs (for example after adding a revision pin). Matching
# checkouts already contain every required source; only fetch missing feeds.
for task_feed in packages luci routing telephony video; do
	if [ ! -e "feeds/$task_feed" ]; then
		./scripts/feeds update "$task_feed"
	fi
done
check_feed_revisions

for task_feed in packages luci; do
	case "$task_feed" in
		packages) task_file=001-preserve-libevdev-dependencies.patch ;;
		luci) task_file=001-preserve-babeld-removal.patch ;;
	esac
	task_patch="$task_root/patches/feeds/$task_feed/$task_file"
	if git -C "feeds/$task_feed" apply --reverse --check "$task_patch" 2>/dev/null; then
		echo "Local $task_feed override is already applied."
	elif git -C "feeds/$task_feed" apply --check "$task_patch"; then
		git -C "feeds/$task_feed" apply "$task_patch"
		echo "Reapplied local $task_feed override."
	else
		echo "$task_feed override conflicts with local changes; leaving those changes untouched." >&2
		exit 1
	fi
done

# Re-index after applying the override, then refresh package-definition links.
./scripts/feeds update -i -a
./scripts/feeds install -a
