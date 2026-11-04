/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_archive.h"
#include "mzgram/mzgram_archive_rules.h"

#include "core/file_location.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
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
	MediaFetch when = MediaFetch::Ahead;
	uint64 fileId = 0;
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

// The files whose server link could not be refreshed: the file of a
// deleted message the server no longer gives (NoteFileReferenceExpired).
[[nodiscard]] base::flat_set<uint64> &ExpiredFiles() {
	static const auto result = new base::flat_set<uint64>();
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

// The message without the parts whose switches are off (formatting,
// reactions), field by field as the server sent it otherwise.
[[nodiscard]] MTPMessage WithoutSwitchedOffParts(const MTPMessage &message) {
	if ((SaveFormatting() && SaveReactions())
		|| (message.type() != mtpc_message)) {
		return message;
	}
	const auto &data = message.c_message();
	using Flag = MTPDmessage::Flag;
	auto flags = data.vflags().v;
	if (!SaveFormatting()) {
		flags &= ~Flag::f_entities;
	}
	if (!SaveReactions()) {
		flags &= ~Flag::f_reactions;
	}
	return MTP_message(
		MTP_flags(flags),
		data.vid(),
		data.vfrom_id() ? *data.vfrom_id() : MTPPeer(),
		MTP_int(data.vfrom_boosts_applied().value_or_empty()),
		MTP_bytes(data.vfrom_rank().value_or_empty()),
		data.vpeer_id(),
		data.vsaved_peer_id() ? *data.vsaved_peer_id() : MTPPeer(),
		data.vfwd_from() ? *data.vfwd_from() : MTPMessageFwdHeader(),
		MTP_long(data.vvia_bot_id().value_or_empty()),
		MTP_long(data.vvia_business_bot_id().value_or_empty()),
		(data.vguestchat_via_from()
			? *data.vguestchat_via_from()
			: MTPPeer()),
		data.vreply_to() ? *data.vreply_to() : MTPMessageReplyHeader(),
		data.vdate(),
		data.vmessage(),
		data.vmedia() ? *data.vmedia() : MTPMessageMedia(),
		data.vreply_markup() ? *data.vreply_markup() : MTPReplyMarkup(),
		((SaveFormatting() && data.ventities())
			? *data.ventities()
			: MTPVector<MTPMessageEntity>()),
		MTP_int(data.vviews().value_or_empty()),
		MTP_int(data.vforwards().value_or_empty()),
		data.vreplies() ? *data.vreplies() : MTPMessageReplies(),
		MTP_int(data.vedit_date().value_or_empty()),
		MTP_bytes(data.vpost_author().value_or_empty()),
		MTP_long(data.vgrouped_id().value_or_empty()),
		((SaveReactions() && data.vreactions())
			? *data.vreactions()
			: MTPMessageReactions()),
		(data.vrestriction_reason()
			? *data.vrestriction_reason()
			: MTPVector<MTPRestrictionReason>()),
		MTP_int(data.vttl_period().value_or_empty()),
		MTP_int(data.vquick_reply_shortcut_id().value_or_empty()),
		MTP_long(data.veffect().value_or_empty()),
		data.vfactcheck() ? *data.vfactcheck() : MTPFactCheck(),
		MTP_int(data.vreport_delivery_until_date().value_or_empty()),
		MTP_long(data.vpaid_message_stars().value_or_empty()),
		(data.vsuggested_post()
			? *data.vsuggested_post()
			: MTPSuggestedPost()),
		MTP_int(data.vschedule_repeat_period().value_or_empty()),
		MTP_string(qs(data.vsummary_from_language().value_or_empty())),
		(data.vrich_message()
			? *data.vrich_message()
			: MTPRichMessage()));
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

// The entry is done: counted as saved, or as not downloaded (the link
// expired, or any other reason).
void Counted(const PendingSave &save, bool saved) {
	if (saved) {
		AddToCounter((save.when == MediaFetch::OnDeletion)
			? ArchiveCounter::SavedOnDeletion
			: ArchiveCounter::SavedAhead);
	} else if (ExpiredFiles().remove(save.fileId)) {
		AddToCounter(ArchiveCounter::Expired);
	} else {
		AddToCounter(ArchiveCounter::Failed);
	}
}

// True once the entry is done: saved, or given up after a failed download.
[[nodiscard]] bool TryFinish(const PendingSave &save) {
	auto &store = MessageStore::Instance();
	const auto finish = [&](bool saved) {
		if (saved) {
			store.setMediaPath(save.key, save.path);
		}
		Counted(save, saved);
		return true;
	};
	if (save.photo) {
		if (save.photoMedia->loaded()) {
			return finish(save.photoMedia->saveToFile(save.path));
		}
		return save.photo->loading() ? false : finish(false);
	}
	// The loader writes straight into the target, so a file that exists
	// while loading is still partial.
	if (save.document->loading()) {
		return false;
	} else if (QFileInfo::exists(save.path)) {
		return finish(true);
	} else if (!save.documentMedia->loaded()) {
		return finish(false);
	}
	const auto bytes = save.documentMedia->bytes();
	if (!bytes.isEmpty()) {
		QFile file(save.path);
		return finish(file.open(QIODevice::WriteOnly)
			&& file.write(bytes) == bytes.size());
	} else if (const auto from = save.document->filepath(true)
		; !from.isEmpty()) {
		return finish(QFile::copy(from, save.path));
	}
	return finish(false);
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

// The size a file is checked against: a document's own, the largest
// size of a photo (0 when not known, counted as small).
[[nodiscard]] int64 FileSize(PhotoData *photo, DocumentData *document) {
	if (document) {
		return document->size;
	} else if (photo) {
		auto result = int64(0);
		for (const auto size : {
				Data::PhotoSize::Large,
				Data::PhotoSize::Small,
				Data::PhotoSize::Thumbnail }) {
			result = std::max(result, int64(photo->imageByteSize(size)));
		}
		return result;
	}
	return 0;
}

[[nodiscard]] bool OnDevice(PhotoData *photo, DocumentData *document) {
	if (document) {
		if (!document->filepath(true).isEmpty()) {
			return true;
		}
		const auto view = document->activeMediaView();
		return view && view->loaded();
	} else if (photo) {
		const auto view = photo->activeMediaView();
		return view && view->loaded();
	}
	return false;
}

// Level 2 (in advance) and level 3 (when deleted) of the archive: the
// file, when it is not larger than the limit of its source at that
// moment. A file already in Telegram's cache is only copied.
void StartSave(
		not_null<Main::Session*> session,
		const MessageKey &key,
		FullMsgId itemId,
		PhotoData *photo,
		DocumentData *document,
		ArchiveSource source,
		MediaFetch when) {
	if (!photo && !document) {
		return;
	} else if (!SaveArchiveMedia()) {
		return;
	}
	auto &store = MessageStore::Instance();
	if (const auto saved = store.mediaPath(key)
		; !saved.isEmpty() && QFileInfo::exists(saved)) {
		return;
	}
	if (document && document->sticker()) {
		return;
	}
	// A file already on this device is only copied, whatever its size;
	// the limit is for downloads
	if (!OnDevice(photo, document)
		&& !Fetches(FileSize(photo, document), LimitMb(source), when)) {
		return;
	}

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
	const auto origin = Data::FileOrigin(Data::FileOriginMessage(itemId));
	const auto target = [&](const QString &extension) {
		return folder
			+ QString::number(key.peer)
			+ '_'
			+ QString::number(key.msg)
			+ (extension.isEmpty() ? QString() : ('.' + extension));
	};
	auto save = PendingSave{ .key = key, .when = when };
	if (photo) {
		save.photo = photo;
		save.fileId = photo->id;
		save.photoMedia = photo->createMediaView();
		save.path = target(u"jpg"_q);
		photo->load(origin, LoadFromCloudOrLocal, true);
	} else {
		save.document = document;
		save.fileId = document->id;
		save.documentMedia = document->createMediaView();
		save.path = target(QFileInfo(document->filename()).suffix());
		document->save(origin, save.path, LoadFromCloudOrLocal, true);
	}
	if (!TryFinish(save)) {
		entry.pending.push_back(std::move(save));
	}
}

void QueueMediaSave(
		not_null<HistoryItem*> item,
		const MessageKey &key,
		MediaFetch when) {
	const auto media = item->media();
	StartSave(
		&item->history()->session(),
		key,
		item->fullId(),
		media ? media->photo() : nullptr,
		media ? media->document() : nullptr,
		SourceOfItem(item),
		when);
}

// Level 3 for a kept message just marked deleted: the message in memory,
// else its file from the message as the server sent it.
void FetchOnDeletion(
		not_null<Main::Session*> session,
		const MessageKey &key) {
	if (!SaveArchiveMedia()) {
		return;
	}
	const auto itemId = FullMsgId(PeerId(key.peer), MsgId(key.msg));
	if (const auto item = session->data().message(itemId)) {
		QueueMediaSave(item, key, MediaFetch::OnDeletion);
		return;
	}
	const auto kept = MessageStore::Instance().stored(key);
	const auto parsed = kept ? Deserialize(kept->raw) : std::nullopt;
	if (!parsed || parsed->type() != mtpc_message) {
		return;
	}
	const auto &data = parsed->c_message();
	const auto media = data.vmedia();
	if (!media) {
		return;
	}
	auto photo = (PhotoData*)nullptr;
	auto document = (DocumentData*)nullptr;
	media->match([&](const MTPDmessageMediaPhoto &photoMedia) {
		if (const auto value = photoMedia.vphoto()) {
			const auto result = session->data().processPhoto(*value);
			if (!result->isNull()) {
				photo = result.get();
			}
		}
	}, [&](const MTPDmessageMediaDocument &documentMedia) {
		if (const auto value = documentMedia.vdocument()) {
			document = session->data().processDocument(*value).get();
		}
	}, [](const auto &) {
	});
	const auto peerId = PeerId(key.peer);
	const auto peer = session->data().peerLoaded(peerId);
	const auto from = data.vfrom_id()
		? session->data().peerLoaded(peerFromMTP(*data.vfrom_id()))
		: nullptr;
	const auto bot = from && from->isUser() && from->asUser()->isBot();
	const auto chat = peer
		? ChatKindOf(peer)
		: peerIsChannel(peerId)
		? ChatKind::Channel
		: peerIsChat(peerId)
		? ChatKind::Group
		: ChatKind::Private;
	StartSave(
		session,
		key,
		itemId,
		photo,
		document,
		SourceOf(bot, chat),
		MediaFetch::OnDeletion);
}

} // namespace

bool SavesChat(not_null<PeerData*> peer) {
	const auto user = peer->asUser();
	return SaveDeletedMessages()
		&& MaySaveChat(user && user->isBot(), ChatKindOf(peer));
}

ChatKind ChatKindOf(not_null<PeerData*> peer) {
	return peer->isBroadcast()
		? ChatKind::Channel
		: (peer->isChat() || peer->isMegagroup())
		? ChatKind::Group
		: ChatKind::Private;
}

// The author decides for a bot's message; a channel's post, a channel
// writing in a group and an anonymous admin have a chat as the author.
ArchiveSource SourceOfItem(not_null<const HistoryItem*> item) {
	const auto user = item->from()->asUser();
	return SourceOf(user && user->isBot(), ChatKindOf(item->history()->peer));
}

void CaptureMessage(not_null<HistoryItem*> item, const MTPMessage &message) {
	if (!Active()
		|| (message.type() != mtpc_message)
		|| !item->isRegular()) {
		return;
	}
	const auto history = item->history();
	// The owner's own messages are never kept.
	if (!KeepsDeleted(
			false,
			message.c_message().is_out(),
			SourceOfItem(item))) {
		return;
	}
	const auto key = KeyFor(item);
	MessageStore::Instance().storeRaw(
		key,
		history->peer->isChannel(),
		item->date(),
		Serialize(WithoutSwitchedOffParts(message)),
		!WouldShowExpired(message));
	QueueMediaSave(item, key, MediaFetch::Ahead);
}

const MTPMessage &PreferKeptCopy(
		not_null<History*> history,
		MsgId id,
		const MTPMessage &message,
		std::optional<MTPMessage> &storage) {
	if (!KeepSelfDestructing()
		|| !WouldShowExpired(message)
		|| !SavesChat(history->peer)) {
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
		|| !SavesChat(item->history()->peer)) {
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
		&& SavesChat(item->history()->peer)
		&& !SavedMediaPath(item).isEmpty();
}

void RecordRemoteDeletion(
		not_null<Main::Session*> session,
		PeerId peerId,
		const QVector<MTPint> &ids) {
	if (!AntiRecall()) {
		return;
	}
	const auto keys = MessageStore::Instance().markDeleted(
		session->uniqueId(),
		peerId.value,
		Ids(ids));
	for (const auto &key : keys) {
		FetchOnDeletion(session, key);
	}
}

void RecordRemoteDeletion(
		not_null<Main::Session*> session,
		const QVector<MTPint> &ids) {
	if (!AntiRecall()) {
		return;
	}
	// Only kept messages have stored rows, so no other check is needed.
	const auto keys = MessageStore::Instance().markDeletedNonChannel(
		session->uniqueId(),
		Ids(ids));
	for (const auto &key : keys) {
		FetchOnDeletion(session, key);
	}
}

void NoteFileReferenceExpired(uint64 fileId) {
	if (fileId) {
		ExpiredFiles().emplace(fileId);
	}
}

void ForgetDeletedByUser(not_null<HistoryItem*> item) {
	if (item->isRegular()) {
		MessageStore::Instance().forget(KeyFor(item));
	}
}

const QVector<MTPMessage> &MergePreserved(
		not_null<History*> history,
		const QVector<MTPMessage> &slice,
		bool older,
		QVector<MTPMessage> &storage) {
	if (!AntiRecall() || !SavesChat(history->peer)) {
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
