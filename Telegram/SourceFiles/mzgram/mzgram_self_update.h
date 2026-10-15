/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mzgram/mzgram_update_rules.h"

#include <rpl/producer.h>

namespace MZGram::Update {

// The self-update behind Update in the bar (mzgram_update_bar.cpp):
//
// 1. The release is read again by its tag from the MZGram-Desktop
//    repository on GitHub and must still be newer than this build.
// 2. The archive for this system and architecture and SHA256SUMS are
//    downloaded from that release only (mzgram_self_update_rules.h).
// 3. The archive must match SHA256SUMS and the GitHub digest; otherwise it is
//    deleted and nothing is installed.
// 4. It is unpacked into tupdates/temp of the working folder, the way
//    Telegram Desktop's own updater prepares an update, with the MZGram code
//    of the release in tupdates/temp/tdata/mzgram_version.
// 5. The app quits and the Updater copies the files over the app folder,
//    backing up every file it replaces and putting them back if a copy
//    fails (mzgram_update_install.h), then starts MZGram again.
//
// The user's data (tdata of the working folder) is never touched. When the
// app folder cannot be written, or the release has no archive for this
// build, the release page is offered instead. Turned off with the bar.

struct Status {
	enum class Step {
		Offer,
		Downloading,
		Verifying,
		Installing,
		Failed,
		NotWritable,
		RolledBack,
	};
	Step step = Step::Offer;
	int percent = 0;
	// The string table key of what went wrong (Failed)
	const char *failure = nullptr;

	friend inline bool operator==(const Status &, const Status &) = default;
};

[[nodiscard]] rpl::producer<Status> StatusValue();

// Downloads, checks and installs the release.
void Install(const Release &release);

// Back to offering the release (the bar was closed).
void ResetStatus();

// At start: the Updater's mark of a rolled back update is read and removed.
void CheckRolledBack();

// Whether tupdates/temp holds an update MZGram prepared (sandbox.cpp and
// launcher.cpp let it install although Telegram's updater is off).
[[nodiscard]] bool PreparedReady();

} // namespace MZGram::Update
