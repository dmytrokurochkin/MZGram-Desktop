/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <optional>

class History;
class HistoryItem;
class UserData;

namespace MZGram {

// An approximate "last seen" for people who hide it. The server then only
// says "recently", "within a week" or "within a month"; this device notes
// the last sign of the person being online (UserData::madeAction: a new
// message, typing, reading the owner's messages; or a status change the
// server sends) and the status line shows that time, marked as
// approximate, while it fits what the server says. Kept in the archive
// database, so it stays after a restart.
void RecordSeen(not_null<UserData*> user, TimeId when);
[[nodiscard]] std::optional<QString> ApproximateOnlineText(
	not_null<UserData*> user,
	TimeId now);

// When other people read the owner's own messages, for the message
// details: the time this device got each read event, kept in the archive
// database; the server's time where it tells is asked for on showing.
void RecordOutboxRead(not_null<History*> history, int64 maxId);
// {readAt, true if the server's time}; {0, false} when unknown.
[[nodiscard]] std::pair<TimeId, bool> KeptReadTime(
	not_null<HistoryItem*> item);
void RememberServerReadTime(not_null<HistoryItem*> item, TimeId readAt);

} // namespace MZGram
