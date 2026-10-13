/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_local_pins.h"

#include "data/data_folder.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "dialogs/dialogs_entry.h"
#include "history/history.h"
#include "main/main_session.h"
#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QSaveFile>

namespace MZGram::LocalPins {
namespace {

[[nodiscard]] QString Path() {
	return cWorkingDir() + u"tdata/mzgram_local_pins.json"_q;
}

[[nodiscard]] Stored &Pins() {
	static auto result = [] {
		auto file = QFile(Path());
		return file.open(QIODevice::ReadOnly)
			? Parse(file.readAll())
			: Stored();
	}();
	return result;
}

void Save() {
	QDir().mkpath(cWorkingDir() + u"tdata"_q);
	auto file = QSaveFile(Path());
	if (file.open(QIODevice::WriteOnly)) {
		file.write(Serialize(Pins()));
		file.commit();
	}
}

[[nodiscard]] QString Key(not_null<Main::Session*> session, Data::Folder *folder) {
	return QString::number(session->userId().bare)
		+ '_'
		+ QString::number(folder ? folder->id() : 0);
}

[[nodiscard]] const History *HistoryOf(not_null<const Dialogs::Entry*> entry) {
	return entry->asHistory();
}

} // namespace

int Index(not_null<const Dialogs::Entry*> entry) {
	const auto &pins = Pins();
	if (pins.empty()) {
		return -1;
	}
	const auto history = HistoryOf(entry);
	if (!history || !history->folderKnown()) {
		return -1;
	}
	const auto i = pins.find(Key(&history->session(), history->folder()));
	if (i == end(pins)) {
		return -1;
	}
	const auto id = history->peer->id.value;
	const auto j = ranges::find(i->second, id);
	return (j == end(i->second)) ? -1 : int(j - begin(i->second));
}

bool IsPinned(not_null<const Dialogs::Entry*> entry) {
	return Index(entry) >= 0;
}

int Count(not_null<Main::Session*> session, Data::Folder *folder) {
	const auto i = Pins().find(Key(session, folder));
	if (i == end(Pins())) {
		return 0;
	}
	auto result = 0;
	for (const auto id : i->second) {
		if (session->data().historyLoaded(PeerId(id))) {
			++result;
		}
	}
	return result;
}

bool WouldPinLocally(not_null<History*> history) {
	if (!history->folderKnown()
		|| history->isPinnedDialog(FilterId())
		|| IsPinned(history)) {
		return false;
	}
	const auto owner = &history->owner();
	const auto folder = history->folder();
	const auto server = int(owner->pinnedChatsOrder(folder).size());
	return PinsLocally(
		server,
		Count(&history->session(), folder),
		owner->pinnedChatsLimit(folder));
}

void Pin(not_null<History*> history) {
	auto &ids = Pins()[Key(&history->session(), history->folder())];
	const auto id = history->peer->id.value;
	if (ranges::find(ids, id) == end(ids)) {
		ids.push_back(id);
		Save();
	}
	history->updateChatListSortPosition();
	history->owner().notifyPinnedDialogsOrderUpdated();
}

void Unpin(not_null<History*> history) {
	const auto key = Key(&history->session(), history->folder());
	auto &pins = Pins();
	const auto i = pins.find(key);
	if (i != end(pins)) {
		const auto id = history->peer->id.value;
		const auto j = ranges::find(i->second, id);
		if (j != end(i->second)) {
			i->second.erase(j);
			if (i->second.empty()) {
				pins.erase(i);
			}
			Save();
		}
	}
	history->updateChatListSortPosition();
	history->owner().notifyPinnedDialogsOrderUpdated();
}

} // namespace MZGram::LocalPins
