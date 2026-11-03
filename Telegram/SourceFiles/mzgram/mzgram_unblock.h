/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

namespace MZGram {

// Settings > MZGram > Other > "Unblock MZGram files" (Windows): a browser
// marks the files of a downloaded zip as "downloaded from the internet"
// (the Zone.Identifier stream, the Mark of the Web), and Windows asks before
// starting a marked file. This removes the mark from every file in the
// folder and below it, as Unblock-File does; Windows protection stays on.
// Only when the user presses the button; nothing else in MZGram changes any
// protection. Returns how many files had the mark; 0 outside Windows.
int UnblockFolder(const QString &folder);

// Whether the file has the mark (Windows; false elsewhere).
[[nodiscard]] bool HasInternetMark(const QString &path);

// How many files in the folder and below it still have the mark: those
// UnblockFolder could not change (a folder only an administrator writes).
[[nodiscard]] int MarkedFiles(const QString &folder);

} // namespace MZGram
