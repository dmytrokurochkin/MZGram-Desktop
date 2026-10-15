/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

#include <vector>

namespace MZGram::Branding {

// The app's own name wherever it names itself: window titles, About, the
// tray, notifications, menus, the exe. The Telegram service keeps its name
// (account, server, Premium texts), and nothing sent to the server changes
// (api_id, app_version, device_model, the lang pack id stay Telegram's).
// No app dependencies, so mzgram_tests checks these rules.

inline constexpr auto kAppName = "MZGram";
inline constexpr auto kDesktopName = "MZGram Desktop";
inline constexpr auto kRepositoryUrl = "https://github.com/dmytrokurochkin/MZGram-Desktop";
inline constexpr auto kLicenseUrl
	= "https://github.com/dmytrokurochkin/MZGram-Desktop/blob/mzgram/LICENSE";
inline constexpr auto kReleasesUrl
	= "https://github.com/dmytrokurochkin/MZGram-Desktop/releases";

// The MZGram version of a release code (X * 1000000 + Y * 10000 + Z * 100
// + N, N 99 for a stable release, a beta number below): "1.0.0",
// "1.1.0 beta 2"; empty for 0 (a build without a version).
[[nodiscard]] QString VersionText(qint64 code);

// Lang keys whose text names the app itself ("Quit Telegram", "Launch
// Telegram when system starts"): the name in them, in any language of the
// cloud packs, becomes MZGram's.
[[nodiscard]] const std::vector<const char*> &RenamedKeys();

// Lang keys whose text is replaced as a whole by an MZGram phrase (key in
// mzgram_lang_table.cpp): they speak of the official app.
struct Replaced {
	const char *langKey = nullptr;
	const char *phrase = nullptr;
};
[[nodiscard]] const std::vector<Replaced> &ReplacedKeys();

// "Telegram Desktop" -> "MZGram Desktop", then "Telegram" -> "MZGram".
[[nodiscard]] QString Rename(QString text);

} // namespace MZGram::Branding
