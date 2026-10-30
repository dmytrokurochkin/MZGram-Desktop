/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_self_update_rules.h"

#include "mzgram/mzgram_update_rules.h"
#include "base/basic_types.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>

#include <array>

namespace MZGram::Update {

bool AllowedUrl(const QString &url) {
	// github.com and the hosts its release downloads redirect to
	static const auto hosts = std::array{
		u"github.com"_q,
		u"api.github.com"_q,
		u"objects.githubusercontent.com"_q,
		u"release-assets.githubusercontent.com"_q,
		u"github-releases.githubusercontent.com"_q,
	};
	const auto parsed = QUrl(url, QUrl::StrictMode);
	if (!parsed.isValid()
		|| parsed.scheme() != u"https"_q
		|| !parsed.userInfo().isEmpty()
		|| (parsed.port() != -1 && parsed.port() != 443)) {
		return false;
	}
	const auto host = parsed.host().toLower();
	for (const auto &allowed : hosts) {
		if (host == allowed) {
			return true;
		}
	}
	return false;
}

bool AllowedUrl(const QUrl &url) {
	return AllowedUrl(url.toString());
}

std::optional<Failure> RequestFailure(const QUrl &url, int redirects) {
	if (redirects > kMaxRedirects) {
		return Failure{
			"update_failed_network",
			u"more than %1 redirects"_q.arg(kMaxRedirects),
		};
	}
	if (!AllowedUrl(url)) {
		return Failure{
			"update_failed_network",
			u"not an allowed address: %1"_q.arg(LogAddress(url)),
		};
	}
	return std::nullopt;
}

QUrl RedirectTarget(const QUrl &from, const QByteArray &location) {
	return from.resolved(QUrl(QString::fromUtf8(location)));
}

namespace {

[[nodiscard]] QString AnswerReason(const Answer &answer) {
	if (answer.status != 0 && answer.status != 200) {
		return u"HTTP %1"_q.arg(answer.status);
	} else if (answer.error != 0) {
		// Qt puts the address into the text: never its query
		auto text = answer.errorText;
		text.replace(QRegularExpression(u"\\?\\S*"_q), u"?..."_q);
		return u"network error %1: %2"_q.arg(answer.error).arg(text);
	}
	return u"no answer"_q;
}

} // namespace

Outcome AnswerOutcome(const QUrl &url, const Answer &answer) {
	if (answer.status >= 300 && answer.status < 400) {
		return { .redirect = RedirectTarget(url, answer.location) };
	} else if (answer.error != 0 || answer.status != 200) {
		return {
			.failure = Failure{ "update_failed_network", AnswerReason(answer) },
		};
	}
	return {};
}

std::optional<Failure> BodyFailure(qint64 size) {
	if (size > kMaxBodySize) {
		return Failure{
			"update_failed_network",
			u"an answer larger than %1 bytes"_q.arg(kMaxBodySize),
		};
	}
	return std::nullopt;
}

std::optional<Failure> ReleaseFailure(
		const QByteArray &body,
		const QString &tag,
		qint64 installedCode) {
	const auto again = ParseLatest(body);
	if (!again) {
		return Failure{
			"update_failed_network",
			u"the answer of GitHub is not a stable release"_q,
		};
	} else if (again->tag != tag) {
		return Failure{
			"update_failed_network",
			u"GitHub answered with %1, not %2"_q.arg(again->tag, tag),
		};
	} else if (again->code <= installedCode) {
		return Failure{
			"update_failed_network",
			u"%1 (code %2) is not newer than this build (code %3)"_q
				.arg(again->tag)
				.arg(again->code)
				.arg(installedCode),
		};
	}
	return std::nullopt;
}

QString LogAddress(const QUrl &url) {
	return url.adjusted(QUrl::RemoveQuery
		| QUrl::RemoveFragment
		| QUrl::RemoveUserInfo).toString(QUrl::FullyEncoded);
}

bool ReleaseFile(const QString &url, const QString &tag) {
	return !tag.isEmpty()
		&& url.startsWith(QString::fromLatin1(kDownloadPrefix) + tag + '/')
		&& AllowedUrl(url);
}

std::vector<Asset> ParseAssets(const QByteArray &json) {
	auto result = std::vector<Asset>();
	const auto document = QJsonDocument::fromJson(json);
	for (const auto &value : document.object().value(u"assets"_q).toArray()) {
		const auto object = value.toObject();
		result.push_back(Asset{
			.name = object.value(u"name"_q).toString(),
			.url = object.value(u"browser_download_url"_q).toString(),
			.digest = object.value(u"digest"_q).toString(),
			.size = qint64(object.value(u"size"_q).toDouble(-1)),
		});
	}
	return result;
}

QString ArchiveName(const QString &version, System system, Arch arch) {
	const auto archName = (arch == Arch::X64)
		? u"x64"_q
		: (arch == Arch::Arm64)
		? u"arm64"_q
		: QString();
	if (archName.isEmpty() || version.isEmpty()) {
		return QString();
	}
	switch (system) {
	case System::Windows:
		return u"MZGram-Desktop-%1-windows-%2.zip"_q.arg(version, archName);
	case System::Linux:
		return u"MZGram-Desktop-%1-linux-%2.tar.xz"_q.arg(version, archName);
	case System::Other:
		break;
	}
	return QString();
}

std::optional<Asset> FindAsset(
		const std::vector<Asset> &assets,
		const QString &name) {
	for (const auto &asset : assets) {
		if (!name.isEmpty() && asset.name == name) {
			return asset;
		}
	}
	return std::nullopt;
}

std::optional<Asset> ChooseArchive(
		const std::vector<Asset> &assets,
		const QString &version,
		System system,
		Arch arch) {
	return FindAsset(assets, ArchiveName(version, system, arch));
}

std::map<QString, QString> ParseSums(const QByteArray &text) {
	static const auto hex = QRegularExpression(u"^[0-9a-f]{64}$"_q);
	auto result = std::map<QString, QString>();
	for (const auto &raw : QString::fromUtf8(text).split('\n')) {
		const auto line = raw.trimmed();
		if (line.size() < 66 || line[64] != ' ') {
			continue;
		}
		const auto hash = line.left(64).toLower();
		auto name = line.mid(65).trimmed();
		if (name.startsWith('*')) {
			name = name.mid(1);
		}
		if (hex.match(hash).hasMatch() && !name.isEmpty()) {
			result[name] = hash;
		}
	}
	return result;
}

bool HashMatches(
		const QString &actual,
		const QString &expected,
		const QString &digest) {
	if (actual.isEmpty() || expected.isEmpty()) {
		return false;
	}
	const auto lower = actual.toLower();
	if (lower != expected.toLower()) {
		return false;
	}
	if (digest.startsWith(u"sha256:"_q)) {
		return lower == digest.mid(7).toLower();
	}
	return true;
}

qint64 PreparedCode(const QString &readyFolder) {
	auto file = QFile(readyFolder + '/' + QString::fromLatin1(kPreparedCodeFile));
	if (!file.open(QIODevice::ReadOnly)) {
		return -1;
	}
	auto ok = false;
	const auto code = file.read(32).trimmed().toLongLong(&ok);
	return (ok && code > 0) ? code : -1;
}

QString EntryPath(const QString &entry) {
	auto path = entry;
	path.replace('\\', '/');
	const auto prefix = u"MZGram/"_q;
	if (!path.startsWith(prefix)) {
		return QString();
	}
	path = path.mid(prefix.size());
	if (path.isEmpty()) {
		return u"/"_q;
	}
	const auto directory = path.endsWith('/');
	const auto parts = (directory ? path.chopped(1) : path).split('/');
	for (const auto &part : parts) {
		if (part.isEmpty()
			|| part == u"."_q
			|| part == u".."_q
			|| part.contains(':')) {
			return QString();
		}
	}
	// The user's data is never part of an update
	if (!parts.front().compare(u"tdata"_q, Qt::CaseInsensitive)) {
		return QString();
	}
	return path;
}

bool PlainArchiveEntry(const QString &listing) {
	return listing.startsWith('-') || listing.startsWith('d');
}

} // namespace MZGram::Update
