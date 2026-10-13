/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/object_ptr.h"
#include "mzgram/mzgram_update_rules.h"

namespace Ui {
class RpWidget;
} // namespace Ui

namespace MZGram::Update {

// The bar at the bottom of the chat list: "MZGram X.Y.Z update available",
// Update (opens the release page) and Close (hides that version for good).
[[nodiscard]] object_ptr<Ui::RpWidget> CreateBar(
	not_null<QWidget*> parent,
	const Release &release);

} // namespace MZGram::Update
