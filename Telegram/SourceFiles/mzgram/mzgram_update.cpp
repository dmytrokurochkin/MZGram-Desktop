/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_update.h"

#include "base/algorithm.h"
#include "base/call_delayed.h"
#include "base/options.h"
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "core/file_utilities.h"
#include "settings.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#ifndef MZGRAM_VERSION_CODE
#define MZGRAM_VERSION_CODE 0
#endif // MZGRAM_VERSION_CODE

namespace MZGram {

const char kOptionCheckUpdates[] = "mzgram-check-updates";

base::options::toggle OptionCheckUpdates({
	.id = kOptionCheckUpdates,
	.name = "Check for MZGram updates",
	.description = "Look for a newer stable MZGram release on GitHub "
		"and show a bar with a link to its page",
	.defaultValue = true,
});

bool CheckUpdates() {
	return OptionCheckUpdates.value();
}

namespace Update {
namespace {

constexpr auto kTimerPeriod = crl::time(60 * 60 * 1000);
constexpr auto kFirstCheckDelay = crl::time(15 * 1000);

class Checker final {
public:
	Checker() {
		_state = load();
		_timer.setCallback([=] { check(false); });
		OptionCheckUpdates.changes() | rpl::on_next([=] {
			if (CheckUpdates()) {
				check(false);
			} else {
				_available = std::nullopt;
			}
		}, _lifetime);
	}

	void start() {
		_timer.callEach(kTimerPeriod);
		base::call_delayed(kFirstCheckDelay, &_guard, [=] { check(false); });
		refresh(false);
	}

	rpl::producer<std::optional<Release>> available() const {
		return _available.value();
	}

	void hide() {
		_available = std::nullopt;
	}

	void dismiss(const Release &release) {
		_state.dismissedTag = release.tag;
		save();
		_available = std::nullopt;
	}

private:
	void check(bool force) {
		const auto now = QDateTime::currentMSecsSinceEpoch();
		if (_reply || !Due(_state, now, CheckUpdates(), force)) {
			return;
		}
		_state.lastCheck = now;
		save();
		auto request = QNetworkRequest(QUrl(QString::fromLatin1(kLatestReleaseUrl)));
		request.setRawHeader("Accept", "application/vnd.github+json");
		request.setRawHeader("User-Agent", "MZGram-Desktop");
		if (!_state.etag.isEmpty()) {
			request.setRawHeader("If-None-Match", _state.etag.toUtf8());
		}
		request.setTransferTimeout(15000);
		_reply = _network.get(request);
		QObject::connect(_reply, &QNetworkReply::finished, [=] {
			const auto reply = base::take(_reply);
			const auto status = reply->attribute(
				QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (reply->error() == QNetworkReply::NoError
				|| status == 304
				|| status == 404) {
				Apply(
					_state,
					status,
					(status == 200) ? reply->readAll() : QByteArray(),
					QString::fromUtf8(reply->rawHeader("ETag")));
				save();
			}
			// Anything else (no network, the rate limit): silent
			reply->deleteLater();
			refresh(force);
		});
	}

	void refresh(bool force) {
		_available = Offer(_state, InstalledCode(), CheckUpdates(), force);
	}

	[[nodiscard]] static QString path() {
		return cWorkingDir() + u"tdata/mzgram_update.json"_q;
	}

	[[nodiscard]] static State load() {
		auto file = QFile(path());
		if (!file.open(QIODevice::ReadOnly)) {
			return State();
		}
		const auto object = QJsonDocument::fromJson(file.readAll()).object();
		return State{
			.lastCheck = qint64(object.value(u"last_check"_q).toDouble()),
			.etag = object.value(u"etag"_q).toString(),
			.latestTag = object.value(u"latest_tag"_q).toString(),
			.latestUrl = object.value(u"latest_url"_q).toString(),
			.dismissedTag = object.value(u"dismissed_tag"_q).toString(),
		};
	}

	void save() const {
		QDir().mkpath(cWorkingDir() + u"tdata"_q);
		auto object = QJsonObject();
		object.insert(u"last_check"_q, double(_state.lastCheck));
		object.insert(u"etag"_q, _state.etag);
		object.insert(u"latest_tag"_q, _state.latestTag);
		object.insert(u"latest_url"_q, _state.latestUrl);
		object.insert(u"dismissed_tag"_q, _state.dismissedTag);
		auto file = QSaveFile(path());
		if (file.open(QIODevice::WriteOnly)) {
			file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
			file.commit();
		}
	}

	State _state;
	QNetworkAccessManager _network;
	QNetworkReply *_reply = nullptr;
	base::Timer _timer;
	base::has_weak_ptr _guard;
	rpl::variable<std::optional<Release>> _available;
	rpl::lifetime _lifetime;

};

// Never destroyed: the network manager must not outlive the application
// object at exit.
[[nodiscard]] Checker &Instance() {
	static const auto result = new Checker();
	return *result;
}

} // namespace

qint64 InstalledCode() {
	return qint64(MZGRAM_VERSION_CODE);
}

void Start() {
	Instance().start();
}

rpl::producer<std::optional<Release>> Available() {
	return Instance().available();
}

void OpenReleasePage(const Release &release) {
	File::OpenUrl(release.url);
	Instance().hide();
}

void Dismiss(const Release &release) {
	Instance().dismiss(release);
}

} // namespace Update
} // namespace MZGram
