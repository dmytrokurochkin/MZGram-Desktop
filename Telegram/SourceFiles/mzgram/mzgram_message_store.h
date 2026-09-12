/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtSql/QSqlDatabase>

#include <set>
#include <tuple>

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

// Plain SQLite on purpose: the user chose readability over sitting behind
// the local passcode, so nothing here is encrypted.
class MessageStore final {
public:
	[[nodiscard]] static MessageStore &Instance();

	void recordDeleted(
		const MessageKey &key,
		uint64 sender,
		int64 date,
		const QString &text);
	void recordEdit(const MessageKey &key, const QString &previousText);

	// Answered from memory: it is queried while painting every message.
	[[nodiscard]] bool isDeleted(const MessageKey &key);
	[[nodiscard]] std::vector<StoredEdit> edits(const MessageKey &key);

private:
	MessageStore() = default;

	bool ensureOpen();
	void loadDeletedKeys();

	QSqlDatabase _db;
	std::set<MessageKey> _deleted;
	bool _opened = false;
	bool _failed = false;

};

} // namespace MZGram
