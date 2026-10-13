/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <optional>

namespace MZGram::Update {

// The MZGram update check, in place of Telegram Desktop's own updater (which
// installs the official app). These rules have no app dependencies, so
// mzgram_tests checks them; mzgram_update.cpp does the requests and the UI.

inline constexpr auto kRepository = "dmytrokurochkin/MZGram-Desktop";
inline constexpr auto kLatestReleaseUrl
	= "https://api.github.com/repos/dmytrokurochkin/MZGram-Desktop/releases/latest";
inline constexpr auto kReleasesPagePrefix
	= "https://github.com/dmytrokurochkin/MZGram-Desktop/releases/";
inline constexpr auto kCheckIntervalMs = qint64(6) * 60 * 60 * 1000;

struct Release {
	QString tag;
	QString url;
	qint64 code = -1;

	[[nodiscard]] QString version() const {
		return tag.mid(1);
	}
};

// What the app keeps between runs.
struct State {
	qint64 lastCheck = 0;
	QString etag;
	QString latestTag;
	QString latestUrl;
	QString dismissedTag;
};

// The code of a stable tag vX.Y.Z (X * 1000000 + Y * 10000 + Z * 100 + 99,
// the scheme of the release workflow), -1 for a beta or anything else.
[[nodiscard]] qint64 StableCode(const QString &tag);

// A release from the answer of /releases/latest, or nothing when it is not
// a stable MZGram release (draft, prerelease, beta tag, broken answer).
[[nodiscard]] std::optional<Release> ParseLatest(const QByteArray &json);

// Whether a request is due: the switch is on (or the user asked) and the
// interval since the last request has passed (or the user asked).
[[nodiscard]] bool Due(const State &state, qint64 now, bool enabled, bool force);

// Puts an answer into the state: 200 replaces the saved release (and the
// ETag), 404 (no release yet) clears it, 304 and errors keep it.
void Apply(State &state, int status, const QByteArray &body, const QString &etag);

// The saved release, when it is newer than the installed version and was
// not closed (force: the user asked, a closed one is shown again).
// installedCode 0 is a build without a version (a local build): nothing.
[[nodiscard]] std::optional<Release> Offer(
	const State &state,
	qint64 installedCode,
	bool enabled,
	bool force);

} // namespace MZGram::Update
