/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_self_update.h"

#include "mzgram/mzgram_self_update_rules.h"
#include "mzgram/mzgram_update.h"
#include "mzgram/mzgram_update_install.h"
#include "base/platform/base_platform_file_utilities.h"
#include "core/application.h"
#include "core/update_checker.h"
#include "settings.h"

#ifdef Q_OS_WIN
#include "base/zlib_help.h"
#endif // Q_OS_WIN

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace MZGram::Update {
namespace {

[[nodiscard]] System CurrentSystem() {
#if defined Q_OS_WIN
	return System::Windows;
#elif defined Q_OS_LINUX // Q_OS_WIN
	return System::Linux;
#else // Q_OS_WIN || Q_OS_LINUX
	return System::Other;
#endif // Q_OS_WIN || Q_OS_LINUX
}

[[nodiscard]] Arch CurrentArch() {
#if defined Q_PROCESSOR_X86_64
	return Arch::X64;
#elif defined Q_PROCESSOR_ARM_64 // Q_PROCESSOR_X86_64
	return Arch::Arm64;
#else // Q_PROCESSOR_X86_64 || Q_PROCESSOR_ARM_64
	return Arch::Other;
#endif // Q_PROCESSOR_X86_64 || Q_PROCESSOR_ARM_64
}

[[nodiscard]] QString UpdatesFolder() {
	return cWorkingDir() + u"tupdates/"_q;
}

[[nodiscard]] QString ReadyFolder() {
	return UpdatesFolder() + u"temp"_q;
}

[[nodiscard]] QString DownloadFolder() {
	return UpdatesFolder() + u"mzgram_download/"_q;
}

// The files every release archive has: the app and the Updater.
[[nodiscard]] QStringList RequiredFiles() {
#ifdef Q_OS_WIN
	return { u"MZGram.exe"_q, u"Updater.exe"_q };
#else // Q_OS_WIN
	return { u"MZGram"_q, u"Updater"_q };
#endif // Q_OS_WIN
}

[[nodiscard]] bool HasRequiredFiles(const QString &ready) {
	for (const auto &name : RequiredFiles()) {
		if (!QFile::exists(ready + '/' + name)) {
			LOG(("MZGram Update Error: the archive has no '%1'.").arg(name));
			return false;
		}
	}
	return true;
}

// The Updater replaces the files in the app folder: it must be writable,
// the binary too.
[[nodiscard]] bool AppFolderWritable() {
	const auto folder = cExeDir();
	if (folder.isEmpty() || cExeName().isEmpty()) {
		return false;
	}
	auto probe = QFile(folder + u".mzgram_write_test"_q);
	if (!probe.open(QIODevice::WriteOnly)) {
		return false;
	}
	probe.close();
	probe.remove();
	return QFileInfo(folder + cExeName()).isWritable();
}

[[nodiscard]] QString FileSha256(const QString &path) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return QString();
	}
	auto hash = QCryptographicHash(QCryptographicHash::Sha256);
	if (!hash.addData(&file)) {
		return QString();
	}
	return QString::fromLatin1(hash.result().toHex());
}

[[nodiscard]] bool WriteFile(const QString &path, const QByteArray &content) {
	QDir().mkpath(QFileInfo(path).absolutePath());
	auto file = QFile(path);
	return file.open(QIODevice::WriteOnly)
		&& (file.write(content) == content.size());
}

#ifdef Q_OS_WIN

// The zip of the release into the ready folder: only entries under MZGram/
// that stay inside it (EntryPath).
[[nodiscard]] bool Unpack(const QString &archive, const QString &ready) {
	auto input = QFile(archive);
	if (!input.open(QIODevice::ReadOnly)) {
		return false;
	}
	auto zip = zlib::FileToRead(input.readAll());
	input.close();
	if (zip.goToFirstFile() != UNZ_OK) {
		return false;
	}
	auto files = 0;
	while (true) {
		const auto name = zip.getCurrentFileName();
		const auto path = EntryPath(name);
		if (path.isEmpty()) {
			LOG(("MZGram Update Error: refused archive entry '%1'.").arg(name));
			return false;
		} else if (path.endsWith('/')) {
			QDir().mkpath(ready + '/' + path);
		} else {
			const auto content = zip.readCurrentFileContent(int(kMaxArchiveSize));
			if (zip.error() != UNZ_OK
				|| !WriteFile(ready + '/' + path, content)) {
				return false;
			}
			++files;
		}
		if (zip.goToNextFile() != UNZ_OK) {
			break;
		}
	}
	return (zip.error() == UNZ_END_OF_LIST_OF_FILE) && (files > 0);
}

#else // Q_OS_WIN

// The lines tar lists the archive with, empty when it fails.
[[nodiscard]] QStringList ListArchive(
		const QString &archive,
		const QString &option) {
	auto list = QProcess();
	list.start(u"tar"_q, { option, archive });
	if (!list.waitForFinished(120000)
		|| list.exitStatus() != QProcess::NormalExit
		|| list.exitCode() != 0) {
		LOG(("MZGram Update Error: tar could not list the archive."));
		return {};
	}
	return QString::fromUtf8(list.readAllStandardOutput())
		.split('\n', Qt::SkipEmptyParts);
}

// The tar.xz of the release with the system's tar: every entry is checked
// first (EntryPath, PlainArchiveEntry), then it is unpacked next to the
// ready folder and moved.
[[nodiscard]] bool Unpack(const QString &archive, const QString &ready) {
	const auto entries = ListArchive(archive, u"-tJf"_q);
	if (entries.isEmpty()) {
		return false;
	}
	for (const auto &entry : entries) {
		if (EntryPath(entry).isEmpty()) {
			LOG(("MZGram Update Error: refused archive entry '%1'.").arg(entry));
			return false;
		}
	}
	const auto listing = ListArchive(archive, u"-tvJf"_q);
	if (listing.size() != entries.size()) {
		return false;
	}
	for (const auto &line : listing) {
		if (!PlainArchiveEntry(line)) {
			LOG(("MZGram Update Error: refused archive entry '%1'.").arg(line));
			return false;
		}
	}
	const auto unpack = UpdatesFolder() + u"mzgram_unpack"_q;
	base::Platform::DeleteDirectory(unpack);
	QDir().mkpath(unpack);
	auto extract = QProcess();
	extract.start(u"tar"_q, { u"-xJf"_q, archive, u"-C"_q, unpack });
	if (!extract.waitForFinished(300000)
		|| extract.exitStatus() != QProcess::NormalExit
		|| extract.exitCode() != 0) {
		LOG(("MZGram Update Error: tar could not unpack the archive."));
		base::Platform::DeleteDirectory(unpack);
		return false;
	}
	const auto moved = QDir().rename(unpack + u"/MZGram"_q, ready);
	base::Platform::DeleteDirectory(unpack);
	return moved;
}

#endif // Q_OS_WIN

class Installer final {
public:
	rpl::producer<Status> status() const {
		return _status.value();
	}

	void reset() {
		if (!_running) {
			_status = Status();
		}
	}

	void rolledBack() {
		const auto marker = cWorkingDir()
			+ QString::fromLatin1(UpdateInstall::kFailedMarker);
		if (QFile::exists(marker)) {
			QFile::remove(marker);
			LOG(("MZGram Update: the Updater rolled the update back."));
			_status = Status{ .step = Status::Step::RolledBack };
		}
	}

	void install(const Release &release) {
		if (_running || !CheckUpdates()) {
			return;
		}
		if (CurrentSystem() == System::Other
			|| CurrentArch() == Arch::Other
			|| !AppFolderWritable()) {
			_status = Status{ .step = Status::Step::NotWritable };
			return;
		}
		_running = true;
		_release = release;
		_status = Status{ .step = Status::Step::Downloading };
		base::Platform::DeleteDirectory(DownloadFolder());
		const auto url = QUrl(
			QString::fromLatin1(kReleaseByTagUrl) + release.tag,
			QUrl::StrictMode);
		get(url, 0, [=](QByteArray body) {
			gotRelease(body);
		});
	}

private:
	void fail(const char *failure, const QString &reason) {
		LOG(("MZGram Update Error: %1 (%2)."
			).arg(QString::fromLatin1(failure), reason));
		_running = false;
		base::Platform::DeleteDirectory(DownloadFolder());
		_status = Status{ .step = Status::Step::Failed, .failure = failure };
	}

	void fail(const Failure &failure) {
		fail(failure.key, failure.reason);
	}

	[[nodiscard]] static Answer ReadAnswer(not_null<QNetworkReply*> reply) {
		return {
			.status = reply->attribute(
				QNetworkRequest::HttpStatusCodeAttribute).toInt(),
			.error = int(reply->error()),
			.errorText = reply->errorString(),
			.location = reply->rawHeader("Location"),
		};
	}

	// The release has no archive that can be checked: its page instead
	void fallback() {
		_running = false;
		_status = Status();
		OpenReleasePage(_release);
	}

	// GET with every redirect checked against the allowed hosts; the body
	// is kept in memory (the release answer, SHA256SUMS).
	// The address stays a QUrl from the Location header to the request
	// (mzgram_self_update_rules.cpp, RedirectTarget).
	void get(QUrl url, int redirects, Fn<void(QByteArray)> done) {
		if (const auto failure = RequestFailure(url, redirects)) {
			fail(*failure);
			return;
		}
		const auto reply = _network.get(request(url));
		QObject::connect(reply, &QNetworkReply::finished, [=] {
			reply->deleteLater();
			const auto outcome = AnswerOutcome(url, ReadAnswer(reply));
			if (outcome.redirect) {
				get(*outcome.redirect, redirects + 1, done);
			} else if (outcome.failure) {
				fail(*outcome.failure);
			} else {
				const auto body = reply->read(kMaxBodySize + 1);
				if (const auto failure = BodyFailure(body.size())) {
					fail(*failure);
				} else {
					done(body);
				}
			}
		});
	}

	[[nodiscard]] QNetworkRequest request(const QUrl &url) const {
		auto result = QNetworkRequest(url);
		result.setAttribute(
			QNetworkRequest::RedirectPolicyAttribute,
			QNetworkRequest::ManualRedirectPolicy);
		result.setRawHeader("User-Agent", "MZGram-Desktop");
		result.setTransferTimeout(30000);
		return result;
	}

	void gotRelease(const QByteArray &body) {
		if (const auto failure = ReleaseFailure(
				body,
				_release.tag,
				InstalledCode())) {
			fail(*failure);
			return;
		}
		const auto assets = ParseAssets(body);
		const auto archive = ChooseArchive(
			assets,
			_release.version(),
			CurrentSystem(),
			CurrentArch());
		const auto sums = FindAsset(assets, QString::fromLatin1(kSumsName));
		if (!archive
			|| !sums
			|| !ReleaseFile(archive->url, _release.tag)
			|| !ReleaseFile(sums->url, _release.tag)
			|| archive->size > kMaxArchiveSize) {
			fallback();
			return;
		}
		_archive = *archive;
		get(QUrl(sums->url, QUrl::StrictMode), 0, [=](QByteArray text) {
			const auto all = ParseSums(text);
			const auto i = all.find(_archive.name);
			if (i == end(all)) {
				fallback();
				return;
			}
			_expected = i->second;
			download(QUrl(_archive.url, QUrl::StrictMode), 0);
		});
	}

	// The archive, into a file, with the progress in the bar.
	void download(QUrl url, int redirects) {
		if (const auto failure = RequestFailure(url, redirects)) {
			fail(*failure);
			return;
		}
		QDir().mkpath(DownloadFolder());
		const auto path = DownloadFolder() + _archive.name;
		const auto file = std::make_shared<QFile>(path);
		if (!file->open(QIODevice::WriteOnly)) {
			fail("update_failed_network", u"could not open %1: %2"_q.arg(
				path,
				file->errorString()));
			return;
		}
		const auto reply = _network.get(request(url));
		const auto written = std::make_shared<qint64>(0);
		// Why the download was stopped while it was going.
		const auto stopped = std::make_shared<std::optional<Failure>>();
		QObject::connect(reply, &QNetworkReply::readyRead, [=] {
			const auto status = reply->attribute(
				QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (status != 200 || *stopped) {
				return;
			}
			const auto chunk = reply->readAll();
			*written += chunk.size();
			if (*written > kMaxArchiveSize) {
				*stopped = Failure{
					"update_failed_network",
					u"an archive larger than %1 bytes"_q.arg(kMaxArchiveSize),
				};
				reply->abort();
			} else if (file->write(chunk) != chunk.size()) {
				*stopped = Failure{
					"update_failed_network",
					u"could not write %1: %2"_q.arg(path, file->errorString()),
				};
				reply->abort();
			}
		});
		QObject::connect(reply, &QNetworkReply::downloadProgress, [=](
				qint64 received,
				qint64 total) {
			if (total > 0 && _status.current().step == Status::Step::Downloading) {
				const auto percent = int(std::min(qint64(100), received * 100 / total));
				if (percent != _status.current().percent) {
					_status = Status{
						.step = Status::Step::Downloading,
						.percent = percent,
					};
				}
			}
		});
		QObject::connect(reply, &QNetworkReply::finished, [=] {
			reply->deleteLater();
			const auto outcome = AnswerOutcome(url, ReadAnswer(reply));
			if (*stopped) {
				file->close();
				fail(**stopped);
				return;
			} else if (outcome.redirect) {
				file->close();
				file->remove();
				download(*outcome.redirect, redirects + 1);
				return;
			} else if (outcome.failure) {
				file->close();
				fail(*outcome.failure);
				return;
			}
			const auto rest = reply->readAll();
			const auto saved = (file->write(rest) == rest.size());
			const auto error = file->errorString();
			file->close();
			if (!saved) {
				fail("update_failed_network", u"could not write %1: %2"_q.arg(
					path,
					error));
				return;
			}
			verifyAndUnpack(path);
		});
	}

	// Off the main thread: the hash, then the archive into tupdates/temp.
	void verifyAndUnpack(const QString &path) {
		_status = Status{ .step = Status::Step::Verifying };
		const auto expected = _expected;
		const auto digest = _archive.digest;
		const auto code = _release.code;
		crl::async([=] {
			const auto actual = FileSha256(path);
			const auto matches = HashMatches(actual, expected, digest);
			auto unpackFailure = QString();
			if (matches) {
				const auto ready = ReadyFolder();
				base::Platform::DeleteDirectory(ready);
				if (!Unpack(path, ready)) {
					unpackFailure = u"could not unpack %1"_q.arg(path);
				} else if (!HasRequiredFiles(ready)) {
					unpackFailure = u"the archive misses a file"_q;
				} else if (!WriteFile(
						ready + '/' + QString::fromLatin1(kPreparedCodeFile),
						QByteArray::number(code))
					|| !WriteFile(ready + u"/ready"_q, "1")) {
					unpackFailure = u"could not write into %1"_q.arg(ready);
				}
				if (!unpackFailure.isEmpty()) {
					base::Platform::DeleteDirectory(ready);
				}
			}
			QFile::remove(path);
			crl::on_main([=] {
				if (!matches) {
					fail("update_failed_checksum", u"SHA-256 %1, SHA256SUMS %2, "
						"GitHub %3"_q.arg(actual, expected, digest));
				} else if (!unpackFailure.isEmpty()) {
					fail("update_failed_unpack", unpackFailure);
				} else {
					restart();
				}
			});
		});
	}

	// Telegram Desktop's own path: the Updater moves into place, the app
	// quits, the Updater copies the files and starts MZGram again.
	void restart() {
		_status = Status{ .step = Status::Step::Installing };
		if (!Core::checkReadyUpdate()) {
			fail(
				"update_failed_unpack",
				u"the prepared update was refused (Update Error above)"_q);
			return;
		}
		cSetRestartingUpdate(true);
		Core::Quit();
	}

	QNetworkAccessManager _network;
	rpl::variable<Status> _status;
	Release _release;
	Asset _archive;
	QString _expected;
	bool _running = false;

};

// Never destroyed: the network manager must not outlive the application
// object at exit.
[[nodiscard]] Installer &Instance() {
	static const auto result = new Installer();
	return *result;
}

} // namespace

rpl::producer<Status> StatusValue() {
	return Instance().status();
}

void Install(const Release &release) {
	Instance().install(release);
}

void ResetStatus() {
	Instance().reset();
}

void CheckRolledBack() {
	Instance().rolledBack();
}

bool PreparedReady() {
	return QFile::exists(ReadyFolder() + u"/ready"_q)
		&& (PreparedCode(ReadyFolder()) > 0);
}

} // namespace MZGram::Update
