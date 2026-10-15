/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace MZGram::UpdateInstall {

// How the Updater (_other/updater_win.cpp, _other/updater_linux.cpp) puts
// an update in place: every file it replaces is backed up first, and when a
// copy fails everything is put back, so a failed update leaves the old
// version as it was and the app still starts. The user's data is never
// part of the steps: the Updater skips tdata of the update, and the app
// folder holds only the program files.
//
// Header only and on top of the Updaters' own file functions: the Updater
// has no Qt, the Linux one is built with the old libstdc++ string ABI, and
// mzgram_tests checks the same code with its own files.

// Where the replaced files wait until the update is complete, inside the
// working folder (cleared with the rest of tupdates).
inline constexpr auto kBackupFolder = "tupdates/mzgram_backup";

// Left in the working folder when an update was rolled back; the app reads
// it on the next start, says so and removes it.
inline constexpr auto kFailedMarker = "mzgram_update_failed";

template <typename Path>
struct Step {
	Path from;
	Path to;
};

template <typename Path>
struct Files {
	// A file is at path
	std::function<bool(const Path &path)> exists;
	// Copies from over to (to may exist), with the Updater's retries while
	// the quitting app still holds a file
	std::function<bool(const Path &from, const Path &to)> copy;
	std::function<bool(const Path &path)> remove;
	// Where the file of the step with this index is backed up
	std::function<Path(std::size_t index)> backup;
	std::function<void(const char *message, const Path &path)> log;
};

// Copies every step. Before a file is replaced it is copied to its backup;
// when a step fails, every file replaced so far (and the one that failed)
// is put back from its backup and every file the update added is removed.
// True when the whole update is in place.
template <typename Path>
[[nodiscard]] bool Install(
		const std::vector<Step<Path>> &steps,
		const Files<Path> &files) {
	struct Done {
		Path to;
		Path saved;
		bool replaced = false;
	};
	auto done = std::vector<Done>();
	const auto rollback = [&] {
		for (auto i = done.rbegin(); i != done.rend(); ++i) {
			if (!i->replaced) {
				files.remove(i->to);
				files.log("Rollback: removed the new file", i->to);
			} else if (files.copy(i->saved, i->to)) {
				files.log("Rollback: restored", i->to);
			} else {
				files.log("Rollback error: could not restore", i->to);
			}
		}
	};
	for (auto i = std::size_t(); i != steps.size(); ++i) {
		const auto &step = steps[i];
		auto entry = Done();
		entry.to = step.to;
		if (files.exists(step.to)) {
			entry.saved = files.backup(i);
			if (!files.copy(step.to, entry.saved)) {
				files.log("Error: could not back up", step.to);
				rollback();
				return false;
			}
			entry.replaced = true;
		}
		// Before the copy: a file left half-written is put back too
		done.push_back(entry);
		if (!files.copy(step.from, step.to)) {
			files.log("Error: could not copy", step.to);
			rollback();
			return false;
		}
	}
	return true;
}

} // namespace MZGram::UpdateInstall
