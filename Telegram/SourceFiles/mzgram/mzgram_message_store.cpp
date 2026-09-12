/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_message_store.h"

#include "base/unixtime.h"
#include "logs.h"
#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QtPlugin>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

// Qt is linked statically, so the SQLite driver has to be pulled in by hand.
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)

namespace MZGram {
namespace {

// SQLite INTEGER is signed; the bit pattern survives the round trip.
[[nodiscard]] qint64 ToSql(uint64 value) {
	return static_cast<qint64>(value);
}

[[nodiscard]] uint64 FromSql(qint64 value) {
	return static_cast<uint64>(value);
}

void BindKey(QSqlQuery &query, const MessageKey &key) {
	query.bindValue(u":account"_q, ToSql(key.account));
	query.bindValue(u":peer"_q, ToSql(key.peer));
	query.bindValue(u":msg"_q, qint64(key.msg));
}

} // namespace

MessageStore &MessageStore::Instance() {
	static auto instance = MessageStore();
	return instance;
}

bool MessageStore::ensureOpen() {
	if (_opened) {
		return true;
	} else if (_failed) {
		return false;
	}
	const auto folder = cWorkingDir() + u"tdata/"_q;
	QDir().mkpath(folder);

	_db = QSqlDatabase::addDatabase(u"QSQLITE"_q, u"mzgram-messages"_q);
	_db.setDatabaseName(folder + u"mzgram_messages.db"_q);
	if (!_db.open()) {
		LOG(("MZGram: could not open message store: %1"
			).arg(_db.lastError().text()));
		_failed = true;
		return false;
	}

	auto query = QSqlQuery(_db);
	const auto schema = {
		u"CREATE TABLE IF NOT EXISTS deleted_messages ("
			"account INTEGER NOT NULL, "
			"peer INTEGER NOT NULL, "
			"msg INTEGER NOT NULL, "
			"sender INTEGER NOT NULL, "
			"date INTEGER NOT NULL, "
			"text TEXT NOT NULL, "
			"deleted_at INTEGER NOT NULL, "
			"PRIMARY KEY (account, peer, msg))"_q,
		u"CREATE TABLE IF NOT EXISTS edit_history ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT, "
			"account INTEGER NOT NULL, "
			"peer INTEGER NOT NULL, "
			"msg INTEGER NOT NULL, "
			"replaced_at INTEGER NOT NULL, "
			"text TEXT NOT NULL)"_q,
		u"CREATE INDEX IF NOT EXISTS edit_history_msg "
			"ON edit_history (account, peer, msg)"_q,
	};
	for (const auto &statement : schema) {
		if (!query.exec(statement)) {
			LOG(("MZGram: message store schema failed: %1"
				).arg(query.lastError().text()));
			_db.close();
			_failed = true;
			return false;
		}
	}
	_opened = true;
	loadDeletedKeys();
	return true;
}

void MessageStore::loadDeletedKeys() {
	auto query = QSqlQuery(_db);
	if (!query.exec(u"SELECT account, peer, msg FROM deleted_messages"_q)) {
		return;
	}
	while (query.next()) {
		_deleted.insert({
			.account = FromSql(query.value(0).toLongLong()),
			.peer = FromSql(query.value(1).toLongLong()),
			.msg = query.value(2).toLongLong(),
		});
	}
}

void MessageStore::recordDeleted(
		const MessageKey &key,
		uint64 sender,
		int64 date,
		const QString &text) {
	if (!ensureOpen()) {
		return;
	}
	auto query = QSqlQuery(_db);
	query.prepare(u"INSERT OR REPLACE INTO deleted_messages "
		"(account, peer, msg, sender, date, text, deleted_at) "
		"VALUES (:account, :peer, :msg, :sender, :date, :text, :deleted_at)"_q);
	BindKey(query, key);
	query.bindValue(u":sender"_q, ToSql(sender));
	query.bindValue(u":date"_q, qint64(date));
	query.bindValue(u":text"_q, text);
	query.bindValue(u":deleted_at"_q, qint64(base::unixtime::now()));
	if (query.exec()) {
		_deleted.insert(key);
	} else {
		LOG(("MZGram: could not record deleted message: %1"
			).arg(query.lastError().text()));
	}
}

void MessageStore::recordEdit(
		const MessageKey &key,
		const QString &previousText) {
	if (!ensureOpen()) {
		return;
	}
	auto query = QSqlQuery(_db);
	query.prepare(u"INSERT INTO edit_history "
		"(account, peer, msg, replaced_at, text) "
		"VALUES (:account, :peer, :msg, :replaced_at, :text)"_q);
	BindKey(query, key);
	query.bindValue(u":replaced_at"_q, qint64(base::unixtime::now()));
	query.bindValue(u":text"_q, previousText);
	if (!query.exec()) {
		LOG(("MZGram: could not record edit: %1"
			).arg(query.lastError().text()));
	}
}

bool MessageStore::isDeleted(const MessageKey &key) {
	return ensureOpen() && _deleted.contains(key);
}

std::vector<StoredEdit> MessageStore::edits(const MessageKey &key) {
	auto result = std::vector<StoredEdit>();
	if (!ensureOpen()) {
		return result;
	}
	auto query = QSqlQuery(_db);
	query.prepare(u"SELECT replaced_at, text FROM edit_history "
		"WHERE account = :account AND peer = :peer AND msg = :msg "
		"ORDER BY id"_q);
	BindKey(query, key);
	if (query.exec()) {
		while (query.next()) {
			result.push_back({
				.replacedAt = query.value(0).toLongLong(),
				.text = query.value(1).toString(),
			});
		}
	}
	return result;
}

} // namespace MZGram
