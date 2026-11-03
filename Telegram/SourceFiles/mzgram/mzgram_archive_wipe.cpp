/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_archive_wipe.h"

namespace MZGram {

// Not done yet: the tests come first.

QStringList SetAsideForWipe(const QStringList &folders) {
	return {};
}

WipedFiles RemoveWiped(const QStringList &folders) {
	return { .complete = false };
}

QStringList LeftFromWipe(const QStringList &folders) {
	return {};
}

} // namespace MZGram
