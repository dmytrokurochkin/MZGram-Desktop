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
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
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

void LogFailure(const char *what, const QSqlQuery &query) {
	LOG(("MZGram: %1 failed: %2"
		).arg(QString::fromLatin1(what)
		).arg(query.lastError().text()));
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
		// Raw messages get written on every history load.
		u"PRAGMA journal_mode = WAL"_q,
		u"PRAGMA synchronous = NORMAL"_q,
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
		u"CREATE TABLE IF NOT EXISTS messages ("
			"account INTEGER NOT NULL, "
			"peer INTEGER NOT NULL, "
			"msg INTEGER NOT NULL, "
			"channel INTEGER NOT NULL, "
			"date INTEGER NOT NULL, "
			"raw BLOB NOT NULL, "
			"deleted_at INTEGER, "
			"media_path TEXT, "
			"PRIMARY KEY (account, peer, msg))"_q,
		u"CREATE INDEX IF NOT EXISTS messages_deleted "
			"ON messages (account, peer, deleted_at)"_q,
		// No longer read: every chat is saved now. Kept so databases from
		// earlier versions open unchanged.
		u"CREATE TABLE IF NOT EXISTS tracked_chats ("
			"account INTEGER NOT NULL, "
			"peer INTEGER NOT NULL, "
			"added_at INTEGER NOT NULL, "
			"PRIMARY KEY (account, peer))"_q,
		u"CREATE TABLE IF NOT EXISTS settings ("
			"key TEXT PRIMARY KEY, "
			"value TEXT NOT NULL)"_q,
	};
	for (const auto &statement : schema) {
		if (!query.exec(statement)) {
			LogFailure("creating the schema", query);
			_db.close();
			_failed = true;
			return false;
		}
	}
	_opened = true;
	loadCaches();
	return true;
}

void MessageStore::loadCaches() {
	auto query = QSqlQuery(_db);
	const auto readDeleted = [&](const QString &sql) {
		if (!query.exec(sql)) {
			LogFailure("loading deleted messages", query);
			return;
		}
		while (query.next()) {
			_deleted.insert({
				.account = FromSql(query.value(0).toLongLong()),
				.peer = FromSql(query.value(1).toLongLong()),
				.msg = query.value(2).toLongLong(),
			});
		}
	};
	readDeleted(u"SELECT account, peer, msg FROM deleted_messages"_q);
	readDeleted(u"SELECT account, peer, msg FROM messages "
		"WHERE deleted_at IS NOT NULL"_q);

	// Media of any size is kept, with no total quota, and saved files are
	// never deleted on their own: the limits an older version saved go.
	auto drop = QSqlQuery(_db);
	if (!drop.exec(u"DELETE FROM settings "
			"WHERE key IN ('media_size_limit', 'total_media_cap')"_q)) {
		LogFailure("dropping the old media limits", drop);
	}
}

void MessageStore::storeRaw(
		const MessageKey &key,
		bool channel,
		int64 date,
		QByteArray raw,
		bool replaceExisting) {
	_pendingRaw.push_back({
		.key = key,
		.channel = channel,
		.date = date,
		.raw = std::move(raw),
		.replaceExisting = replaceExisting,
	});
	if (!_flushScheduled) {
		_flushScheduled = true;
		crl::on_main([] {
			Instance().flushRaw();
		});
	}
}

void MessageStore::flushRaw() {
	_flushScheduled = false;
	if (_pendingRaw.empty()) {
		return;
	}
	const auto pending = base::take(_pendingRaw);
	if (!ensureOpen()) {
		return;
	}
	_db.transaction();
	auto query = QSqlQuery(_db);
	// A deleted row is frozen: the server copy is gone, ours is the record.
	query.prepare(u"INSERT INTO messages "
		"(account, peer, msg, channel, date, raw) "
		"VALUES (:account, :peer, :msg, :channel, :date, :raw) "
		"ON CONFLICT (account, peer, msg) DO UPDATE SET "
		"raw = excluded.raw, date = excluded.date "
		"WHERE messages.deleted_at IS NULL AND :replace = 1"_q);
	for (const auto &row : pending) {
		BindKey(query, row.key);
		query.bindValue(u":channel"_q, row.channel ? 1 : 0);
		query.bindValue(u":date"_q, qint64(row.date));
		query.bindValue(u":raw"_q, row.raw);
		query.bindValue(u":replace"_q, row.replaceExisting ? 1 : 0);
		if (!query.exec()) {
			LogFailure("storing a message", query);
			break;
		}
	}
	_db.commit();
}

std::optional<StoredMessage> MessageStore::stored(const MessageKey &key) {
	flushRaw();
	if (!ensureOpen()) {
		return std::nullopt;
	}
	auto query = QSqlQuery(_db);
	query.prepare(u"SELECT raw, media_path FROM messages "
		"WHERE account = :account AND peer = :peer AND msg = :msg"_q);
	BindKey(query, key);
	if (!query.exec() || !query.next()) {
		return std::nullopt;
	}
	return StoredMessage{
		.msg = key.msg,
		.raw = query.value(0).toByteArray(),
		.mediaPath = query.value(1).toString(),
	};
}

void MessageStore::setMediaPath(const MessageKey &key, const QString &path) {
	flushRaw();
	if (!ensureOpen()) {
		return;
	}
	auto query = QSqlQuery(_db);
	query.prepare(u"UPDATE messages SET media_path = :path "
		"WHERE account = :account AND peer = :peer AND msg = :msg"_q);
	BindKey(query, key);
	query.bindValue(u":path"_q, path);
	if (!query.exec()) {
		LogFailure("saving a media path", query);
	}
}

QString MessageStore::mediaPath(const MessageKey &key) {
	if (!ensureOpen()) {
		return QString();
	}
	auto query = QSqlQuery(_db);
	query.prepare(u"SELECT media_path FROM messages "
		"WHERE account = :account AND peer = :peer AND msg = :msg"_q);
	BindKey(query, key);
	return (query.exec() && query.next())
		? query.value(0).toString()
		: QString();
}

std::vector<StoredMessage> MessageStore::deletedBetween(
		uint64 account,
		uint64 peer,
		int64 above,
		int64 below) {
	auto result = std::vector<StoredMessage>();
	flushRaw();
	if (!ensureOpen()) {
		return result;
	}
	auto query = QSqlQuery(_db);
	query.prepare(u"SELECT msg, raw, media_path FROM messages "
		"WHERE account = :account AND peer = :peer "
		"AND deleted_at IS NOT NULL "
		"AND msg > :above AND msg < :below "
		"ORDER BY msg DESC"_q);
	query.bindValue(u":account"_q, ToSql(account));
	query.bindValue(u":peer"_q, ToSql(peer));
	query.bindValue(u":above"_q, qint64(above));
	query.bindValue(u":below"_q, qint64(below));
	if (!query.exec()) {
		LogFailure("reading deleted messages", query);
		return result;
	}
	while (query.next()) {
		result.push_back({
			.msg = query.value(0).toLongLong(),
			.raw = query.value(1).toByteArray(),
			.mediaPath = query.value(2).toString(),
		});
	}
	return result;
}

void MessageStore::recordDeleted(
		const MessageKey &key,
		uint64 sender,
		int64 date,
		const QString &text) {
	flushRaw();
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
		LogFailure("recording a deleted message", query);
	}
	markKeyDeleted(key, qint64(base::unixtime::now()));
}

void MessageStore::markKeyDeleted(const MessageKey &key, qint64 now) {
	auto query = QSqlQuery(_db);
	query.prepare(u"UPDATE messages SET deleted_at = :deleted_at "
		"WHERE account = :account AND peer = :peer AND msg = :msg "
		"AND deleted_at IS NULL"_q);
	BindKey(query, key);
	query.bindValue(u":deleted_at"_q, now);
	if (!query.exec()) {
		LogFailure("marking a message deleted", query);
	} else if (query.numRowsAffected() > 0) {
		_deleted.insert(key);
	}
}

void MessageStore::markDeleted(
		uint64 account,
		uint64 peer,
		const std::vector<int64> &ids) {
	flushRaw();
	if (ids.empty() || !ensureOpen()) {
		return;
	}
	const auto now = qint64(base::unixtime::now());
	_db.transaction();
	for (const auto id : ids) {
		markKeyDeleted({ .account = account, .peer = peer, .msg = id }, now);
	}
	_db.commit();
}

void MessageStore::markDeletedNonChannel(
		uint64 account,
		const std::vector<int64> &ids) {
	flushRaw();
	if (ids.empty() || !ensureOpen()) {
		return;
	}
	auto found = std::vector<MessageKey>();
	auto query = QSqlQuery(_db);
	query.prepare(u"SELECT peer FROM messages "
		"WHERE account = :account AND msg = :msg "
		"AND channel = 0 AND deleted_at IS NULL"_q);
	for (const auto id : ids) {
		query.bindValue(u":account"_q, ToSql(account));
		query.bindValue(u":msg"_q, qint64(id));
		if (!query.exec()) {
			LogFailure("finding deleted messages", query);
			return;
		}
		while (query.next()) {
			found.push_back({
				.account = account,
				.peer = FromSql(query.value(0).toLongLong()),
				.msg = id,
			});
		}
	}
	const auto now = qint64(base::unixtime::now());
	_db.transaction();
	for (const auto &key : found) {
		markKeyDeleted(key, now);
	}
	_db.commit();
}

void MessageStore::forget(const MessageKey &key) {
	flushRaw();
	if (!ensureOpen()) {
		return;
	}
	if (const auto path = mediaPath(key); !path.isEmpty()) {
		QFile::remove(path);
	}
	auto query = QSqlQuery(_db);
	for (const auto &table : { u"messages"_q, u"deleted_messages"_q }) {
		query.prepare(u"DELETE FROM "_q
			+ table
			+ u" WHERE account = :account AND peer = :peer AND msg = :msg"_q);
		BindKey(query, key);
		if (!query.exec()) {
			LogFailure("forgetting a message", query);
		}
	}
	_deleted.erase(key);
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
		LogFailure("recording an edit", query);
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

QString MessageStore::mediaFolder(uint64 account, uint64 peer) const {
	return cWorkingDir()
		+ u"tdata/mzgram/media/%1/%2/"_q.arg(account).arg(peer);
}

QString MessageStore::databasePath() const {
	return cWorkingDir() + u"tdata/mzgram_messages.db"_q;
}

void MessageStore::closeForFileOp() {
	if (_opened) {
		_db.close();
		_opened = false;
	}
}

void MessageStore::reopenAfterFileReplace() {
	closeForFileOp();
	_deleted.clear();
	if (ensureOpen()) {
		loadCaches();
	}
}

void MessageStore::wipeAll() {
	if (!ensureOpen()) {
		return;
	}
	auto query = QSqlQuery(_db);
	for (const auto table : {
			u"deleted_messages"_q,
			u"edit_history"_q,
			u"messages"_q }) {
		if (!query.exec(u"DELETE FROM "_q + table)) {
			LogFailure("wipeAll", query);
		}
	}
	_deleted.clear();
	QDir(cWorkingDir() + u"tdata/mzgram/media/"_q).removeRecursively();
}

} // namespace MZGram
