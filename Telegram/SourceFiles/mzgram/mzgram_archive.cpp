/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_archive.h"

#include "core/file_location.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "mzgram/mzgram_anti_recall.h"
#include "mzgram/mzgram_message_store.h"
#include "mzgram/mzgram_options.h"
#include "storage/cache/storage_cache_database.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace MZGram {
namespace {

struct PendingSave {
	MessageKey key;
	PhotoData *photo = nullptr;
	DocumentData *document = nullptr;
	std::shared_ptr<Data::PhotoMedia> photoMedia;
	std::shared_ptr<Data::DocumentMedia> documentMedia;
	QString path;
};

struct SessionSaves {
	std::vector<PendingSave> pending;
	bool subscribed = false;
};

// Leaked on purpose: entries are removed with their session, and a static
// destructor would run after the data the media views point to is gone.
[[nodiscard]] base::flat_map<not_null<Main::Session*>, SessionSaves> &Saves() {
	static const auto result
		= new base::flat_map<not_null<Main::Session*>, SessionSaves>();
	return *result;
}

[[nodiscard]] bool Active() {
	return AntiRecall() || KeepSelfDestructing();
}

[[nodiscard]] QByteArray Serialize(const MTPMessage &message) {
	auto counter = ::tl::details::LengthCounter();
	message.write(counter);
	auto buffer = mtpBuffer();
	buffer.reserve(counter.length);
	message.write(buffer);
	return QByteArray(
		reinterpret_cast<const char*>(buffer.constData()),
		buffer.size() * sizeof(mtpPrime));
}

[[nodiscard]] std::optional<MTPMessage> Deserialize(const QByteArray &raw) {
	if (raw.isEmpty() || (raw.size() % sizeof(mtpPrime))) {
		return std::nullopt;
	}
	const auto start = reinterpret_cast<const mtpPrime*>(raw.constData());
	const auto end = start + (raw.size() / sizeof(mtpPrime));
	auto from = start;
	auto result = MTPMessage();
	if (!result.read(from, end) || (from != end)) {
		return std::nullopt;
	}
	return result;
}

// True for a server copy that HistoryItem turns into an "expired" stub, see
// CheckMessageMedia and ShowTtlMediaAsExpired. That is view-once or timed
// media which lost its file, or which is incoming and already opened: the
// server may send either shape. Such a copy never replaces the kept one.
[[nodiscard]] bool WouldShowExpired(const MTPMessage &message) {
	if (message.type() != mtpc_message) {
		return false;
	}
	const auto &data = message.c_message();
	const auto media = data.vmedia();
	if (!media) {
		return false;
	}
	const auto opened = !data.is_out() && !data.is_media_unread();
	return media->match([&](const MTPDmessageMediaPhoto &photoMedia) {
		const auto photo = photoMedia.vphoto();
		return photoMedia.vttl_seconds()
			&& (opened || !photo || (photo->type() == mtpc_photoEmpty));
	}, [&](const MTPDmessageMediaDocument &documentMedia) {
		const auto document = documentMedia.vdocument();
		return documentMedia.vttl_seconds()
			&& (opened
				|| !document
				|| (document->type() == mtpc_documentEmpty));
	}, [](const auto &) {
		return false;
	});
}

[[nodiscard]] std::vector<int64> Ids(const QVector<MTPint> &ids) {
	auto result = std::vector<int64>();
	result.reserve(ids.size());
	for (const auto &id : ids) {
		result.push_back(id.v);
	}
	return result;
}

[[nodiscard]] QString SavedMediaPath(not_null<HistoryItem*> item) {
	const auto path = MessageStore::Instance().mediaPath(KeyFor(item));
	return (!path.isEmpty() && QFileInfo::exists(path)) ? path : QString();
}

// True once the entry is done: saved, or given up after a failed download.
[[nodiscard]] bool TryFinish(const PendingSave &save) {
	auto &store = MessageStore::Instance();
	if (save.photo) {
		if (save.photoMedia->loaded()) {
			if (save.photoMedia->saveToFile(save.path)) {
				store.setMediaPath(save.key, save.path);
			}
			return true;
		}
		return !save.photo->loading();
	}
	// The loader writes straight into the target, so a file that exists
	// while loading is still partial.
	if (save.document->loading()) {
		return false;
	} else if (QFileInfo::exists(save.path)) {
		store.setMediaPath(save.key, save.path);
		return true;
	} else if (!save.documentMedia->loaded()) {
		return true;
	}
	const auto bytes = save.documentMedia->bytes();
	if (!bytes.isEmpty()) {
		QFile file(save.path);
		if (file.open(QIODevice::WriteOnly)
			&& file.write(bytes) == bytes.size()) {
			store.setMediaPath(save.key, save.path);
		}
	} else if (const auto from = save.document->filepath(true)
		; !from.isEmpty() && QFile::copy(from, save.path)) {
		store.setMediaPath(save.key, save.path);
	}
	return true;
}

void CheckPending(not_null<Main::Session*> session) {
	auto &saves = Saves();
	const auto i = saves.find(session);
	if (i == end(saves)) {
		return;
	}
	auto &pending = i->second.pending;
	pending.erase(ranges::remove_if(pending, TryFinish), end(pending));
}

void QueueMediaSave(not_null<HistoryItem*> item, const MessageKey &key) {
	const auto media = item->media();
	const auto photo = media ? media->photo() : nullptr;
	const auto document = media ? media->document() : nullptr;
	if (!photo && !document) {
		return;
	}
	auto &store = MessageStore::Instance();
	if (const auto saved = store.mediaPath(key)
		; !saved.isEmpty() && QFileInfo::exists(saved)) {
		return;
	}
	if (document) {
		if (document->sticker()) {
			return;
		}
		// The user asked for these regardless of size: they are the kinds
		// people delete or burn on purpose.
		const auto always = document->isVoiceMessage()
			|| document->isVideoMessage()
			|| media->ttlSeconds();
		const auto limit = store.mediaSizeLimit();
		if (!always && limit > 0 && document->size > limit) {
			return;
		}
	}

	const auto session = &item->history()->session();
	auto &entry = Saves()[session];
	for (const auto &existing : entry.pending) {
		if (existing.key == key) {
			return;
		}
	}
	if (!entry.subscribed) {
		entry.subscribed = true;
		session->downloaderTaskFinished(
		) | rpl::on_next([=] {
			CheckPending(session);
		}, session->lifetime());
		session->lifetime().add([=] {
			Saves().remove(session);
		});
	}

	const auto folder = store.mediaFolder(key.account, key.peer);
	QDir().mkpath(folder);
	const auto origin = Data::FileOrigin(
		Data::FileOriginMessage(item->fullId()));
	const auto target = [&](const QString &extension) {
		return folder
			+ QString::number(key.msg)
			+ (extension.isEmpty() ? QString() : ('.' + extension));
	};
	auto save = PendingSave{ .key = key };
	if (photo) {
		save.photo = photo;
		save.photoMedia = photo->createMediaView();
		save.path = target(u"jpg"_q);
		photo->load(origin, LoadFromCloudOrLocal, true);
	} else {
		save.document = document;
		save.documentMedia = document->createMediaView();
		save.path = target(QFileInfo(document->filename()).suffix());
		document->save(origin, save.path, LoadFromCloudOrLocal, true);
	}
	if (!TryFinish(save)) {
		entry.pending.push_back(std::move(save));
	}
}

} // namespace

bool IsTracked(not_null<PeerData*> peer) {
	return MessageStore::Instance().isTracked(
		peer->session().uniqueId(),
		peer->id.value);
}

void SetTracked(not_null<PeerData*> peer, bool tracked) {
	MessageStore::Instance().setTracked(
		peer->session().uniqueId(),
		peer->id.value,
		tracked);
}

void CaptureMessage(not_null<HistoryItem*> item, const MTPMessage &message) {
	if (!Active()
		|| (message.type() != mtpc_message)
		|| !item->isRegular()) {
		return;
	}
	const auto history = item->history();
	if (!IsTracked(history->peer)) {
		return;
	}
	const auto key = KeyFor(item);
	MessageStore::Instance().storeRaw(
		key,
		history->peer->isChannel(),
		item->date(),
		Serialize(message),
		!WouldShowExpired(message));
	QueueMediaSave(item, key);
}

const MTPMessage &PreferKeptCopy(
		not_null<History*> history,
		MsgId id,
		const MTPMessage &message,
		std::optional<MTPMessage> &storage) {
	if (!KeepSelfDestructing()
		|| !WouldShowExpired(message)
		|| !IsTracked(history->peer)) {
		return message;
	}
	const auto kept = MessageStore::Instance().stored({
		.account = history->session().uniqueId(),
		.peer = history->peer->id.value,
		.msg = id.bare,
	});
	if (!kept
		|| kept->mediaPath.isEmpty()
		|| !QFileInfo::exists(kept->mediaPath)) {
		return message;
	}
	auto parsed = Deserialize(kept->raw);
	if (!parsed || WouldShowExpired(*parsed)) {
		return message;
	}
	storage = std::move(parsed);
	return *storage;
}

void RestoreSavedMedia(not_null<HistoryItem*> item) {
	const auto media = item->media();
	if (!media
		|| !Active()
		|| !(media->photo() || media->document())
		|| !(media->ttlSeconds() || IsPreservedDeleted(item))
		|| !IsTracked(item->history()->peer)) {
		return;
	}
	const auto path = SavedMediaPath(item);
	if (path.isEmpty()) {
		return;
	}
	if (const auto photo = media->photo()) {
		// The loader checks the cache before the server, and the server no
		// longer has the file. The put is queued ahead of any later get.
		const auto key = photo->location(
			Data::PhotoSize::Large).file().cacheKey();
		QFile file(path);
		if (key && file.open(QIODevice::ReadOnly)) {
			item->history()->owner().cache().put(
				key,
				Storage::Cache::details::TaggedValue(
					file.readAll(),
					Data::kImageCacheTag));
		}
	} else if (const auto document = media->document()) {
		if (document->filepath(true).isEmpty()) {
			document->setLocation(Core::FileLocation(path));
		}
	}
}

bool KeepsMediaAgainst(
		not_null<HistoryItem*> item,
		const MTPMessage &edition) {
	const auto media = item->media();
	return KeepSelfDestructing()
		&& media
		&& media->ttlSeconds()
		&& WouldShowExpired(edition)
		&& IsTracked(item->history()->peer)
		&& !SavedMediaPath(item).isEmpty();
}

void RecordRemoteDeletion(
		not_null<Main::Session*> session,
		PeerId peerId,
		const QVector<MTPint> &ids) {
	if (!AntiRecall()) {
		return;
	}
	auto &store = MessageStore::Instance();
	const auto account = session->uniqueId();
	if (store.isTracked(account, peerId.value)) {
		store.markDeleted(account, peerId.value, Ids(ids));
	}
}

void RecordRemoteDeletion(
		not_null<Main::Session*> session,
		const QVector<MTPint> &ids) {
	if (!AntiRecall()) {
		return;
	}
	// Only tracked chats have stored rows, so no tracked check is needed.
	MessageStore::Instance().markDeletedNonChannel(
		session->uniqueId(),
		Ids(ids));
}

void ForgetDeletedByUser(not_null<HistoryItem*> item) {
	if (item->isRegular() && IsTracked(item->history()->peer)) {
		MessageStore::Instance().forget(KeyFor(item));
	}
}

const QVector<MTPMessage> &MergePreserved(
		not_null<History*> history,
		const QVector<MTPMessage> &slice,
		bool older,
		QVector<MTPMessage> &storage) {
	if (!AntiRecall() || !IsTracked(history->peer)) {
		return slice;
	}
	constexpr auto kLowest = int64(0);
	constexpr auto kHighest = std::numeric_limits<int64>::max();

	auto sliceMin = kHighest;
	auto sliceMax = kLowest;
	for (const auto &message : slice) {
		if (const auto id = IdFromMessage(message).bare; id > 0) {
			sliceMin = std::min(sliceMin, id);
			sliceMax = std::max(sliceMax, id);
		}
	}
	const auto empty = (sliceMax == kLowest);

	// The range this slice fills: from its own edge up to the part of the
	// history already loaded, so nothing between two slices is skipped.
	auto above = kLowest;
	auto below = kHighest;
	if (older) {
		const auto current = int64(history->minMsgId().bare);
		above = empty ? kLowest : (sliceMin - 1);
		below = (current > 0)
			? current
			: (empty || history->loadedAtBottom())
			? kHighest
			: (sliceMax + 1);
	} else {
		const auto current = int64(history->maxMsgId().bare);
		above = (current > 0)
			? current
			: empty
			? kLowest
			: (sliceMin - 1);
		below = empty ? kHighest : (sliceMax + 1);
	}

	const auto deleted = MessageStore::Instance().deletedBetween(
		history->session().uniqueId(),
		history->peer->id.value,
		above,
		below);
	if (deleted.empty()) {
		return slice;
	}

	auto result = QVector<MTPMessage>();
	result.reserve(slice.size() + int(deleted.size()));
	auto changed = false;
	auto next = deleted.begin();
	const auto pushDeletedAbove = [&](int64 id) {
		for (; next != deleted.end() && next->msg > id; ++next) {
			if (auto parsed = Deserialize(next->raw)) {
				result.push_back(std::move(*parsed));
				changed = true;
			}
		}
		if (next != deleted.end() && next->msg == id) {
			++next; // The server still has it, so it was never deleted.
		}
	};
	for (const auto &message : slice) {
		if (const auto id = int64(IdFromMessage(message).bare); id > 0) {
			pushDeletedAbove(id);
		}
		result.push_back(message);
	}
	pushDeletedAbove(kLowest);

	if (!changed) {
		return slice;
	}
	storage = std::move(result);
	return storage;
}

} // namespace MZGram
