/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_types.h"

class History;
class HistoryItem;
class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace MZGram {

// The chats the user picked in the chat menu or in Settings > MZGram.
// Anti-recall, edit history and view-once keeping work only there.
[[nodiscard]] bool IsTracked(not_null<PeerData*> peer);
void SetTracked(not_null<PeerData*> peer, bool tracked);

// Called for every message the server sends, from History::createItem and
// Data::Session::updateEditedMessage. Stores the ones of tracked chats and
// saves their media to disk.
void CaptureMessage(not_null<HistoryItem*> item, const MTPMessage &message);

// History::createItem is the one place every server message passes through
// on its way to becoming an item: the chat list, history slices, replies and
// pinned bars all end there. Returns message itself, or the copy kept before
// its view-once media was opened, stored in storage.
[[nodiscard]] const MTPMessage &PreferKeptCopy(
	not_null<History*> history,
	MsgId id,
	const MTPMessage &message,
	std::optional<MTPMessage> &storage);

// Points a new item at its media saved on disk when the server no longer
// has the file: kept deleted messages and view-once media.
void RestoreSavedMedia(not_null<HistoryItem*> item);

// True when an edition would only strip view-once media that is kept.
[[nodiscard]] bool KeepsMediaAgainst(
	not_null<HistoryItem*> item,
	const MTPMessage &edition);

// Server deletions, including ones for messages not loaded right now, such
// as those deleted while the app was closed.
void RecordRemoteDeletion(
	not_null<Main::Session*> session,
	PeerId peerId,
	const QVector<MTPint> &ids);
void RecordRemoteDeletion(
	not_null<Main::Session*> session,
	const QVector<MTPint> &ids);

// The user deleted the message, so it must not come back after a restart.
void ForgetDeletedByUser(not_null<HistoryItem*> item);

// Returns slice itself, or storage filled with slice plus the kept deleted
// messages whose ids fall into the range the slice covers. Newest first, the
// order the server uses.
[[nodiscard]] const QVector<MTPMessage> &MergePreserved(
	not_null<History*> history,
	const QVector<MTPMessage> &slice,
	bool older,
	QVector<MTPMessage> &storage);

} // namespace MZGram
