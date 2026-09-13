/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_anti_recall.h"

#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "mzgram/mzgram_archive.h"
#include "mzgram/mzgram_edit_history_box.h"
#include "mzgram/mzgram_options.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"

namespace MZGram {
namespace {

// Only the chats the user picked keep deleted messages and edit history.
[[nodiscard]] bool ChatQualifies(not_null<const HistoryItem*> item) {
	return IsTracked(item->history()->peer);
}

} // namespace

MessageKey KeyFor(not_null<const HistoryItem*> item) {
	return {
		.account = item->history()->session().uniqueId(),
		.peer = item->history()->peer->id.value,
		.msg = item->id.bare,
	};
}

void FilterPreserved(std::vector<not_null<HistoryItem*>> &toDestroy) {
	if (!AntiRecall()) {
		return;
	}
	auto &store = MessageStore::Instance();
	std::erase_if(toDestroy, [&](not_null<HistoryItem*> item) {
		if (item->isService() || !ChatQualifies(item)) {
			return false;
		}
		store.recordDeleted(
			KeyFor(item),
			item->from()->id.value,
			item->date(),
			item->originalText().text);
		// The deleted badge is read during layout, so only a resize shows it.
		item->history()->owner().requestItemResize(item);
		return true;
	});
}

bool IsPreservedDeleted(not_null<const HistoryItem*> item) {
	return AntiRecall() && MessageStore::Instance().isDeleted(KeyFor(item));
}

void RecordEditBefore(
		not_null<const HistoryItem*> item,
		const TextWithEntities &incoming) {
	if (!EditHistory() || item->isService() || !ChatQualifies(item)) {
		return;
	}
	const auto &previous = item->originalText().text;
	// Editions that only change media, markup or the hide-edited flag arrive
	// with the same text and are not worth a history row.
	if (previous == incoming.text) {
		return;
	}
	MessageStore::Instance().recordEdit(KeyFor(item), previous);
}

void AddEditHistoryAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller) {
	if (!EditHistory() || !item->Get<HistoryMessageEdited>()) {
		return;
	}
	const auto key = KeyFor(item);
	const auto owner = &item->history()->owner();
	const auto itemId = item->fullId();
	menu->addAction(u"Edit history"_q, [=] {
		const auto item = owner->message(itemId);
		const auto current = item ? item->originalText().text : QString();
		controller->show(Box(EditHistoryBox, key, current));
	}, &st::menuIconEdit);
}

} // namespace MZGram
