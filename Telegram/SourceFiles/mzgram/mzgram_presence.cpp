/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_presence.h"

#include "base/unixtime.h"
#include "data/data_peer_values.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "mzgram/mzgram_lang.h"
#include "mzgram/mzgram_message_store.h"
#include "mzgram/mzgram_presence_rules.h"

namespace MZGram {
namespace {

[[nodiscard]] HiddenLastSeen Hidden(const Data::LastseenStatus &status) {
	return status.isRecently()
		? HiddenLastSeen::Recently
		: status.isWithinWeek()
		? HiddenLastSeen::WithinWeek
		: status.isWithinMonth()
		? HiddenLastSeen::WithinMonth
		: HiddenLastSeen::None;
}

} // namespace

void RecordSeen(not_null<UserData*> user, TimeId when) {
	if (user->isBot() || user->isServiceUser() || user->isSelf() || when <= 0) {
		return;
	}
	MessageStore::Instance().recordLastSeen(user->id.value, when);
}

std::optional<QString> ApproximateOnlineText(
		not_null<UserData*> user,
		TimeId now) {
	if (user->isBot() || user->isServiceUser() || user->isSelf()) {
		return std::nullopt;
	}
	const auto hidden = Hidden(user->lastseen());
	if (hidden == HiddenLastSeen::None) {
		return std::nullopt;
	}
	const auto seen = MessageStore::Instance().lastSeen(user->id.value);
	if (!ApproximateFits(hidden, seen, now)) {
		return std::nullopt;
	}
	const auto text = Data::OnlineText(
		Data::LastseenStatus::OnlineTill(std::min(seen, now)),
		now);
	return TrNow("last_seen_approx").arg(text);
}

void RecordOutboxRead(not_null<History*> history, int64 maxId) {
	if (maxId <= 0) {
		return;
	}
	MessageStore::Instance().addOutboxRead(
		history->session().uniqueId(),
		history->peer->id.value,
		maxId,
		base::unixtime::now(),
		false);
}

std::pair<TimeId, bool> KeptReadTime(not_null<HistoryItem*> item) {
	return MessageStore::Instance().readTime(
		item->history()->session().uniqueId(),
		item->history()->peer->id.value,
		item->id.bare);
}

void RememberServerReadTime(not_null<HistoryItem*> item, TimeId readAt) {
	MessageStore::Instance().addOutboxRead(
		item->history()->session().uniqueId(),
		item->history()->peer->id.value,
		item->id.bare,
		readAt,
		true);
}

} // namespace MZGram
