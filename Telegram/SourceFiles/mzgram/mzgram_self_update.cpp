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

constexpr auto kMaxRedirects = 5;
constexpr auto kMaxSumsSize = qint64(1024) * 1024;

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

// The tar.xz of the release with the system's tar: every entry is checked
// first (EntryPath), then it is unpacked next to the ready folder and moved.
[[nodiscard]] bool Unpack(const QString &archive, const QString &ready) {
	auto list = QProcess();
	list.start(u"tar"_q, { u"-tJf"_q, archive });
	if (!list.waitForFinished(120000)
		|| list.exitStatus() != QProcess::NormalExit
		|| list.exitCode() != 0) {
		LOG(("MZGram Update Error: tar could not list the archive."));
		return false;
	}
	const auto entries = QString::fromUtf8(list.readAllStandardOutput())
		.split('\n', Qt::SkipEmptyParts);
	if (entries.isEmpty()) {
		return false;
	}
	for (const auto &entry : entries) {
		if (EntryPath(entry).isEmpty()) {
			LOG(("MZGram Update Error: refused archive entry '%1'.").arg(entry));
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
		get(QString::fromLatin1(kReleaseByTagUrl) + release.tag, 0, [=](
				QByteArray body) {
			gotRelease(body);
		});
	}

private:
	void fail(const char *failure) {
		LOG(("MZGram Update Error: %1.").arg(failure));
		_running = false;
		base::Platform::DeleteDirectory(DownloadFolder());
		_status = Status{ .step = Status::Step::Failed, .failure = failure };
	}

	// The release has no archive that can be checked: its page instead
	void fallback() {
		_running = false;
		_status = Status();
		OpenReleasePage(_release);
	}

	// GET with every redirect checked against the allowed hosts; the body
	// is kept in memory (the release answer, SHA256SUMS).
	void get(QString url, int redirects, Fn<void(QByteArray)> done) {
		if (!AllowedUrl(url) || redirects > kMaxRedirects) {
			fail("update_failed_network");
			return;
		}
		const auto reply = _network.get(request(url));
		QObject::connect(reply, &QNetworkReply::finished, [=] {
			reply->deleteLater();
			const auto status = reply->attribute(
				QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (status >= 300 && status < 400) {
				const auto location = reply->url().resolved(QUrl(
					QString::fromUtf8(reply->rawHeader("Location"))));
				get(location.toString(), redirects + 1, done);
			} else if (reply->error() != QNetworkReply::NoError
				|| status != 200) {
				fail("update_failed_network");
			} else {
				const auto body = reply->read(kMaxSumsSize + 1);
				if (body.size() > kMaxSumsSize) {
					fail("update_failed_network");
				} else {
					done(body);
				}
			}
		});
	}

	[[nodiscard]] QNetworkRequest request(const QString &url) const {
		auto result = QNetworkRequest(QUrl(url));
		result.setAttribute(
			QNetworkRequest::RedirectPolicyAttribute,
			QNetworkRequest::ManualRedirectPolicy);
		result.setRawHeader("User-Agent", "MZGram-Desktop");
		result.setTransferTimeout(30000);
		return result;
	}

	void gotRelease(const QByteArray &body) {
		const auto again = ParseLatest(body);
		if (!again
			|| again->tag != _release.tag
			|| again->code <= InstalledCode()) {
			fail("update_failed_network");
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
		get(sums->url, 0, [=](QByteArray text) {
			const auto all = ParseSums(text);
			const auto i = all.find(_archive.name);
			if (i == end(all)) {
				fallback();
				return;
			}
			_expected = i->second;
			download(_archive.url, 0);
		});
	}

	// The archive, into a file, with the progress in the bar.
	void download(QString url, int redirects) {
		if (!AllowedUrl(url) || redirects > kMaxRedirects) {
			fail("update_failed_network");
			return;
		}
		QDir().mkpath(DownloadFolder());
		const auto path = DownloadFolder() + _archive.name;
		const auto file = std::make_shared<QFile>(path);
		if (!file->open(QIODevice::WriteOnly)) {
			fail("update_failed_network");
			return;
		}
		const auto reply = _network.get(request(url));
		const auto written = std::make_shared<qint64>(0);
		QObject::connect(reply, &QNetworkReply::readyRead, [=] {
			const auto status = reply->attribute(
				QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (status != 200) {
				return;
			}
			const auto chunk = reply->readAll();
			*written += chunk.size();
			if (*written > kMaxArchiveSize
				|| file->write(chunk) != chunk.size()) {
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
			const auto status = reply->attribute(
				QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (status >= 300 && status < 400) {
				file->close();
				file->remove();
				const auto location = reply->url().resolved(QUrl(
					QString::fromUtf8(reply->rawHeader("Location"))));
				download(location.toString(), redirects + 1);
				return;
			}
			file->write(reply->readAll());
			file->close();
			if (reply->error() != QNetworkReply::NoError || status != 200) {
				fail("update_failed_network");
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
			const auto matches = HashMatches(FileSha256(path), expected, digest);
			auto prepared = false;
			if (matches) {
				const auto ready = ReadyFolder();
				base::Platform::DeleteDirectory(ready);
				prepared = Unpack(path, ready)
					&& HasRequiredFiles(ready)
					&& WriteFile(
						ready + '/' + QString::fromLatin1(kPreparedCodeFile),
						QByteArray::number(code))
					&& WriteFile(ready + u"/ready"_q, "1");
				if (!prepared) {
					base::Platform::DeleteDirectory(ready);
				}
			}
			QFile::remove(path);
			crl::on_main([=] {
				if (!matches) {
					fail("update_failed_checksum");
				} else if (!prepared) {
					fail("update_failed_unpack");
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
			fail("update_failed_unpack");
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
