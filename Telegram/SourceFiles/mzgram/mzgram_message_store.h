/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtSql/QSqlDatabase>

#include <rpl/event_stream.h>
#include <rpl/variable.h>

#include <optional>
#include <set>
#include <vector>

namespace MZGram {

// Ids are taken as raw integers so this header stays free of the data layer.
// Callers pass PeerId::value and MsgId::bare.
struct MessageKey {
	uint64 account = 0;
	uint64 peer = 0;
	int64 msg = 0;

	friend inline auto operator<=>(const MessageKey&, const MessageKey&)
		= default;
};

struct StoredEdit {
	int64 replacedAt = 0;
	QString text;
};

struct StoredMessage {
	int64 msg = 0;
	QByteArray raw;
	QString mediaPath;
};

// Plain SQLite on purpose: the user chose readability over sitting behind
// the local passcode, so nothing here is encrypted.
class MessageStore final {
public:
	[[nodiscard]] static MessageStore &Instance();

	// The chats the user picked. Messages are kept only there.
	[[nodiscard]] bool isTracked(uint64 account, uint64 peer);
	void setTracked(uint64 account, uint64 peer, bool tracked);
	[[nodiscard]] std::vector<uint64> trackedPeers(uint64 account);
	[[nodiscard]] rpl::producer<> trackedChanges() const;

	// Messages of tracked chats exactly as the server sent them, so a deleted
	// one can be rebuilt after a restart. Writes are batched per event loop.
	// replaceExisting is false when the new copy lost media the old one has.
	void storeRaw(
		const MessageKey &key,
		bool channel,
		int64 date,
		QByteArray raw,
		bool replaceExisting);
	[[nodiscard]] std::optional<StoredMessage> stored(const MessageKey &key);
	void setMediaPath(const MessageKey &key, const QString &path);
	[[nodiscard]] QString mediaPath(const MessageKey &key);

	// Deleted messages with ids strictly between the bounds, newest first.
	[[nodiscard]] std::vector<StoredMessage> deletedBetween(
		uint64 account,
		uint64 peer,
		int64 above,
		int64 below);

	void recordDeleted(
		const MessageKey &key,
		uint64 sender,
		int64 date,
		const QString &text);
	void markDeleted(
		uint64 account,
		uint64 peer,
		const std::vector<int64> &ids);
	// Private chats and basic groups share one id sequence per account, so
	// such a deletion names only ids and the peer is looked up here.
	void markDeletedNonChannel(uint64 account, const std::vector<int64> &ids);
	// Drops a message the user deleted, together with its saved media.
	void forget(const MessageKey &key);

	void recordEdit(const MessageKey &key, const QString &previousText);

	// Answered from memory: it is queried while painting every message.
	[[nodiscard]] bool isDeleted(const MessageKey &key);
	[[nodiscard]] std::vector<StoredEdit> edits(const MessageKey &key);

	// In bytes, 0 for no limit. Applies to videos and files only.
	[[nodiscard]] int64 mediaSizeLimit();
	[[nodiscard]] rpl::producer<int64> mediaSizeLimitValue();
	void setMediaSizeLimit(int64 bytes);

	[[nodiscard]] QString mediaFolder(uint64 account, uint64 peer) const;

private:
	struct PendingRaw {
		MessageKey key;
		bool channel = false;
		int64 date = 0;
		QByteArray raw;
		bool replaceExisting = false;
	};

	MessageStore() = default;

	bool ensureOpen();
	void loadCaches();
	void flushRaw();
	void markKeyDeleted(const MessageKey &key, qint64 now);
	[[nodiscard]] QString setting(const QString &key);
	void setSetting(const QString &key, const QString &value);

	QSqlDatabase _db;
	std::set<MessageKey> _deleted;
	std::set<std::pair<uint64, uint64>> _tracked;
	rpl::event_stream<> _trackedChanges;
	rpl::variable<int64> _mediaSizeLimit;
	std::vector<PendingRaw> _pendingRaw;
	bool _flushScheduled = false;
	bool _opened = false;
	bool _failed = false;

};

} // namespace MZGram
