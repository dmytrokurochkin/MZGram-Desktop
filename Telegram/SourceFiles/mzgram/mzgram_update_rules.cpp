/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_update_rules.h"

#include "base/basic_types.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>

#include <cstdlib>

namespace MZGram::Update {

qint64 StableCode(const QString &tag) {
	static const auto expression = QRegularExpression(
		u"^v(0|[1-9]\\d*)\\.(0|[1-9]\\d?)\\.(0|[1-9]\\d?)(-beta\\.([1-9]\\d?))?$"_q);
	const auto match = expression.match(tag);
	if (!match.hasMatch() || !match.captured(4).isEmpty()) {
		return -1;
	}
	return match.captured(1).toLongLong() * 1000000
		+ match.captured(2).toLongLong() * 10000
		+ match.captured(3).toLongLong() * 100
		+ 99;
}

std::optional<Release> ParseLatest(const QByteArray &json) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(json, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return std::nullopt;
	}
	const auto object = document.object();
	if (object.value(u"draft"_q).toBool()
		|| object.value(u"prerelease"_q).toBool()) {
		return std::nullopt;
	}
	const auto tag = object.value(u"tag_name"_q).toString();
	const auto code = StableCode(tag);
	if (code < 0) {
		return std::nullopt;
	}
	auto url = object.value(u"html_url"_q).toString();
	// Only the MZGram releases page is ever opened
	if (!url.startsWith(QString::fromLatin1(kReleasesPagePrefix))) {
		url = QString::fromLatin1(kReleasesPagePrefix) + u"tag/"_q + tag;
	}
	return Release{ .tag = tag, .url = url, .code = code };
}

bool Due(const State &state, qint64 now, bool enabled, bool force) {
	if (force) {
		return true;
	}
	return enabled && (std::llabs(now - state.lastCheck) >= kCheckIntervalMs);
}

void Apply(State &state, int status, const QByteArray &body, const QString &etag) {
	if (status == 200) {
		const auto release = ParseLatest(body);
		state.etag = etag;
		state.latestTag = release ? release->tag : QString();
		state.latestUrl = release ? release->url : QString();
	} else if (status == 404) {
		state.etag = QString();
		state.latestTag = QString();
		state.latestUrl = QString();
	}
}

std::optional<Release> Offer(
		const State &state,
		qint64 installedCode,
		bool enabled,
		bool force) {
	if ((!enabled && !force) || installedCode <= 0) {
		return std::nullopt;
	}
	const auto code = StableCode(state.latestTag);
	if (code <= installedCode || state.latestUrl.isEmpty()) {
		return std::nullopt;
	}
	if (!force && state.latestTag == state.dismissedTag) {
		return std::nullopt;
	}
	return Release{ .tag = state.latestTag, .url = state.latestUrl, .code = code };
}

} // namespace MZGram::Update
