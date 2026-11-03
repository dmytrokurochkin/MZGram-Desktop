/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

#include <vector>

namespace MZGram {

// Settings > MZGram > Interface > "Hide in Telegram's Settings": rows of
// Telegram's own Settings page that MZGram can hide, each with a switch of
// its own, off by default. A hidden row is gone from the page and from the
// settings search; the help title goes too when all of its rows are hidden.
struct HideableRow {
	const char *key = nullptr; // In the code of settings_main.cpp.
	const char *option = nullptr; // base::options id.
	const char *phrase = nullptr; // The row's name in the MZGram table.
	bool help = false; // A row of the help section.
};

// In the order Telegram's Settings shows them.
[[nodiscard]] const std::vector<HideableRow> &HideableRows();

// Whether the row with this key is hidden; false for a key not in the list.
[[nodiscard]] bool RowHidden(const char *key);
void SetRowHidden(const char *key, bool hidden);

// Whether every row of the help section is hidden.
[[nodiscard]] bool HelpHidden();

} // namespace MZGram
