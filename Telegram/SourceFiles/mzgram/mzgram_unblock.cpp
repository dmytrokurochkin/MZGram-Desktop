/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_unblock.h"
#include "mzgram/mzgram_unblock.h"

namespace MZGram {

// The tests come first: the files are not unblocked yet.
bool HasInternetMark(const QString &path) {
	Q_UNUSED(path);
	return false;
}

int MarkedFiles(const QString &folder) {
	Q_UNUSED(folder);
	return 0;
}

int UnblockFolder(const QString &folder) {
	Q_UNUSED(folder);
	return 0;
}

} // namespace MZGram
