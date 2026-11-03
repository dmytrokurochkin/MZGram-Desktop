/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace MZGram {

// Settings > MZGram > Archive > Clear archive. The archive's files can take
// many gigabytes, and deleting them takes minutes: the window must not wait
// for it. So the folders are first moved aside, next to themselves, which is
// instant, and deleted on a background thread; what an earlier wipe left
// (MZGram closed before it ended) is deleted at the next start.

struct WipedFiles {
	int files = 0;
	qint64 bytes = 0;
	bool complete = true; // false: some files could not be deleted
};

// Moves each existing folder aside as "<name>.mzgram-deleting-<n>" and
// returns where they went. A folder that cannot be moved (a file in it is
// open) is returned as it is, to be deleted where it is.
[[nodiscard]] QStringList SetAsideForWipe(const QStringList &folders);

// Deletes the folders with everything in them. Slow: call it on a
// background thread.
[[nodiscard]] WipedFiles RemoveWiped(const QStringList &folders);

// The folders an earlier wipe set aside and did not delete.
[[nodiscard]] QStringList LeftFromWipe(const QStringList &folders);

} // namespace MZGram
