/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mzgram/mzgram_update_rules.h"

#include <rpl/producer.h>

namespace MZGram {

extern const char kOptionCheckUpdates[];
[[nodiscard]] bool CheckUpdates();

namespace Update {

// The version of this build (StableCode of its tag), set by the release
// and push workflows; 0 in a local build, which is never offered anything.
[[nodiscard]] qint64 InstalledCode();

// Starts the checks: soon after start, then every hour (a request at most
// every kCheckIntervalMs). Requests go to GitHub without a token, with the
// ETag of the last answer, off the UI thread; errors are silent.
void Start();

// The release the bar at the bottom of the chat list shows, if any.
[[nodiscard]] rpl::producer<std::optional<Release>> Available();

// Opens the release page in the browser and hides the bar for this run.
void OpenReleasePage(const Release &release);

// Hides the bar for this version for good.
void Dismiss(const Release &release);

} // namespace Update
} // namespace MZGram
