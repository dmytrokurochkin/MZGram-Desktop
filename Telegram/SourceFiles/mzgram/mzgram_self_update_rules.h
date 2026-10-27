/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <map>
#include <optional>
#include <vector>

namespace MZGram::Update {

// The rules of the self-update (mzgram_self_update.cpp): which file of a
// release is taken, from where, and when it is good enough to install. No
// app dependencies, so mzgram_tests checks them.
//
// Only a release of this repository on GitHub is ever downloaded, over
// https, from github.com or the hosts its downloads redirect to; the archive
// must match SHA256SUMS of the release and, when GitHub gives one, the
// digest of the asset. Telegram's update servers are never asked.

inline constexpr auto kReleaseByTagUrl
	= "https://api.github.com/repos/dmytrokurochkin/MZGram-Desktop/releases/tags/";
inline constexpr auto kDownloadPrefix
	= "https://github.com/dmytrokurochkin/MZGram-Desktop/releases/download/";
inline constexpr auto kSumsName = "SHA256SUMS";
inline constexpr auto kMaxArchiveSize = qint64(512) * 1024 * 1024;

// In tupdates/temp/tdata of a prepared update (the Updater never copies
// tdata): the MZGram code of the release. core/update_checker.cpp checks it
// instead of Telegram's version, which MZGram releases share.
inline constexpr auto kPreparedCodeFile = "tdata/mzgram_version";

enum class System {
	Windows,
	Linux,
	Other,
};

enum class Arch {
	X64,
	Arm64,
	Other,
};

struct Asset {
	QString name;
	QString url;
	QString digest;
	qint64 size = -1;
};

// https on github.com or its download hosts, nothing else.
[[nodiscard]] bool AllowedUrl(const QString &url);

// A file of the release with this tag of this repository.
[[nodiscard]] bool ReleaseFile(const QString &url, const QString &tag);

// The assets of an answer of /releases/tags/<tag>.
[[nodiscard]] std::vector<Asset> ParseAssets(const QByteArray &json);

// MZGram-Desktop-X.Y.Z-windows-x64.zip, -windows-arm64.zip,
// -linux-x64.tar.xz, -linux-arm64.tar.xz; empty for anything else.
[[nodiscard]] QString ArchiveName(
	const QString &version,
	System system,
	Arch arch);

// The archive for this system and architecture, if the release has one.
[[nodiscard]] std::optional<Asset> ChooseArchive(
	const std::vector<Asset> &assets,
	const QString &version,
	System system,
	Arch arch);

[[nodiscard]] std::optional<Asset> FindAsset(
	const std::vector<Asset> &assets,
	const QString &name);

// "<sha256>  <name>" lines, as sha256sum writes them.
[[nodiscard]] std::map<QString, QString> ParseSums(const QByteArray &text);

// The SHA-256 (lowercase hex) of a file matches SHA256SUMS and, when given,
// the GitHub digest "sha256:...". No expected hash: no match.
[[nodiscard]] bool HashMatches(
	const QString &actual,
	const QString &expected,
	const QString &digest);

// The MZGram code of the update prepared in this ready folder
// (tupdates/temp), -1 when it is not an update MZGram prepared.
[[nodiscard]] qint64 PreparedCode(const QString &readyFolder);

// An archive entry as a path inside the app folder: the archive keeps the
// files under MZGram/, which is removed. Nothing outside the folder, nothing
// absolute, and never tdata (the user's data). Empty: the entry is refused;
// "/": the MZGram/ folder itself.
[[nodiscard]] QString EntryPath(const QString &entry);

// A line of "tar -tv" for a regular file or a folder. Links and devices are
// refused: a link could lead a later entry out of the app folder.
[[nodiscard]] bool PlainArchiveEntry(const QString &listing);

} // namespace MZGram::Update
