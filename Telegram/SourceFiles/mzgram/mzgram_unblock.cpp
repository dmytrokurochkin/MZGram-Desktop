/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_unblock.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

namespace MZGram {
namespace {

#ifdef _WIN32
[[nodiscard]] std::wstring MarkStream(const QString &path) {
	return QDir::toNativeSeparators(path).toStdWString() + L":Zone.Identifier";
}
#endif // _WIN32

} // namespace

bool HasInternetMark(const QString &path) {
#ifdef _WIN32
	const auto stream = MarkStream(path);
	return GetFileAttributesW(stream.c_str()) != INVALID_FILE_ATTRIBUTES;
#else // _WIN32
	Q_UNUSED(path);
	return false;
#endif // _WIN32
}

int MarkedFiles(const QString &folder) {
#ifdef _WIN32
	auto result = 0;
	auto files = QDirIterator(
		folder,
		QDir::Files | QDir::Hidden | QDir::System,
		QDirIterator::Subdirectories);
	while (files.hasNext()) {
		if (HasInternetMark(files.next())) {
			++result;
		}
	}
	return result;
#else // _WIN32
	Q_UNUSED(folder);
	return 0;
#endif // _WIN32
}

int UnblockFolder(const QString &folder) {
#ifdef _WIN32
	auto result = 0;
	auto files = QDirIterator(
		folder,
		QDir::Files | QDir::Hidden | QDir::System,
		QDirIterator::Subdirectories);
	while (files.hasNext()) {
		const auto path = files.next();
		if (!HasInternetMark(path)) {
			continue;
		}
		// Deleting the stream deletes only the mark, not the file
		const auto stream = MarkStream(path);
		if (DeleteFileW(stream.c_str())) {
			++result;
		}
	}
	return result;
#else // _WIN32
	Q_UNUSED(folder);
	return 0;
#endif // _WIN32
}

} // namespace MZGram
