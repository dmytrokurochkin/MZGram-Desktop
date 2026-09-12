/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mzgram/mzgram_message_store.h"

namespace Ui {
class GenericBox;
} // namespace Ui

namespace MZGram {

void EditHistoryBox(
	not_null<Ui::GenericBox*> box,
	MessageKey key,
	QString currentText);

} // namespace MZGram
