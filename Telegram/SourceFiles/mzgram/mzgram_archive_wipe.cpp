/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_archive_wipe.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace MZGram {
namespace {

const auto kAside = QString::fromLatin1(".mzgram-deleting-");

[[nodiscard]] QString Clean(const QString &folder) {
	return QDir::cleanPath(folder);
}

} // namespace

QStringList SetAsideForWipe(const QStringList &folders) {
	auto result = QStringList();
	for (const auto &folder : folders) {
		const auto path = Clean(folder);
		if (path.isEmpty() || !QFileInfo(path).isDir()) {
			continue;
		}
		const auto stamp = QString::number(QDateTime::currentMSecsSinceEpoch());
		auto aside = path + kAside + stamp;
		for (auto n = 1; QFileInfo::exists(aside); ++n) {
			aside = path + kAside + stamp + '-' + QString::number(n);
		}
		result.push_back(QDir().rename(path, aside) ? aside : path);
	}
	return result;
}

WipedFiles RemoveWiped(const QStringList &folders) {
	auto result = WipedFiles();
	for (const auto &folder : folders) {
		const auto path = Clean(folder);
		if (path.isEmpty() || !QFileInfo::exists(path)) {
			continue;
		}
		for (auto i = QDirIterator(
				path,
				QDir::Files | QDir::Hidden | QDir::System,
				QDirIterator::Subdirectories); i.hasNext();) {
			const auto file = i.next();
			const auto size = i.fileInfo().size();
			if (QFile::remove(file)) {
				++result.files;
				result.bytes += size;
			} else {
				QFile::setPermissions(file, QFile::WriteOwner | QFile::ReadOwner);
				if (QFile::remove(file)) {
					++result.files;
					result.bytes += size;
				} else {
					result.complete = false;
				}
			}
		}
		if (!QDir(path).removeRecursively() && QFileInfo::exists(path)) {
			result.complete = false;
		}
	}
	return result;
}

QStringList LeftFromWipe(const QStringList &folders) {
	auto result = QStringList();
	for (const auto &folder : folders) {
		const auto path = Clean(folder);
		if (path.isEmpty()) {
			continue;
		}
		const auto info = QFileInfo(path);
		const auto parent = info.absoluteDir();
		const auto prefix = info.fileName() + kAside;
		for (const auto &name : parent.entryList(
				{ prefix + '*' },
				QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
			result.push_back(parent.filePath(name));
		}
	}
	return result;
}

namespace {

[[nodiscard]] qint64 SizeOf(const QString &path) {
	const auto info = QFileInfo(path);
	if (!info.isDir()) {
		return info.exists() ? info.size() : 0;
	}
	auto result = qint64(0);
	auto it = QDirIterator(
		path,
		QDir::Files | QDir::Hidden | QDir::System,
		QDirIterator::Subdirectories);
	while (it.hasNext()) {
		it.next();
		result += it.fileInfo().size();
	}
	return result;
}

[[nodiscard]] QString CleanPath(const QString &path) {
	return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

} // namespace

ArchiveSizes MeasureArchive(
		const QString &database,
		const QString &folder,
		const QString &media,
		const QString &attachments) {
	auto result = ArchiveSizes();
	const auto base = QFileInfo(database);
	const auto name = base.fileName();
	if (!name.isEmpty()) {
		const auto list = base.dir().entryInfoList(
			QDir::Files | QDir::Hidden | QDir::System);
		for (const auto &file : list) {
			if (file.fileName().startsWith(name)) {
				result.database += file.size();
			}
		}
	}
	const auto mediaPath = CleanPath(media);
	const auto attachmentsPath = CleanPath(attachments);
	if (!folder.isEmpty()) {
		const auto list = QDir(folder).entryInfoList(
			QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
		for (const auto &entry : list) {
			const auto path = CleanPath(entry.absoluteFilePath());
			if (entry.fileName().contains(u".mzgram-deleting-"_q)
				|| path == CleanPath(database)
				|| path == attachmentsPath) {
				continue;
			} else if (path == mediaPath) {
				result.media += SizeOf(path);
			} else if (!entry.fileName().startsWith(name) || name.isEmpty()) {
				result.other += SizeOf(path);
			}
		}
	}
	if (!attachments.isEmpty()) {
		result.attachments += SizeOf(attachments);
	}
	return result;
}

} // namespace MZGram
