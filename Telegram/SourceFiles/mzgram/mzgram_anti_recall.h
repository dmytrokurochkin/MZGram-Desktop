/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mzgram/mzgram_message_store.h"

class HistoryItem;
struct TextWithEntities;

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace MZGram {

[[nodiscard]] MessageKey KeyFor(not_null<const HistoryItem*> item);

// Removes from the list every item that should survive a remote deletion,
// recording each one first. Whatever is left is destroyed as usual.
void FilterPreserved(std::vector<not_null<HistoryItem*>> &toDestroy);

[[nodiscard]] bool IsPreservedDeleted(not_null<const HistoryItem*> item);

// Called before an edition overwrites the text, while the old one is intact.
void RecordEditBefore(
	not_null<const HistoryItem*> item,
	const TextWithEntities &incoming);

void AddEditHistoryAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

} // namespace MZGram
