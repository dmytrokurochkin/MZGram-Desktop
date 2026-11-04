/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_archive_rules.h"

#include "base/options.h"

namespace MZGram {
namespace {

base::options::toggle OptionSaveDeletedMessages({
	.id = kOptionSaveDeletedMessages,
	.name = "Save deleted messages",
	.description = "Keep messages others delete and their view-once media, "
		"in every chat.",
	.defaultValue = true,
});

base::options::toggle OptionSaveEditHistory({
	.id = kOptionSaveEditHistory,
	.name = "Save edit history",
	.description = "Keep earlier versions of messages others edit.",
	.defaultValue = true,
});

base::options::toggle OptionSaveArchiveMedia({
	.id = kOptionSaveArchiveMedia,
	.name = "Save media",
	.description = "Copy files of kept messages to Saved Attachments.",
	.defaultValue = true,
});

base::options::toggle OptionSaveFormatting({
	.id = kOptionSaveFormatting,
	.name = "Save formatting",
	.defaultValue = true,
});

base::options::toggle OptionSaveReactions({
	.id = kOptionSaveReactions,
	.name = "Save reactions",
	.defaultValue = true,
});

base::options::toggle OptionSaveFromUsers({
	.id = kOptionSaveFromUsers,
	.name = "Keep messages from users",
	.defaultValue = true,
});

base::options::toggle OptionSaveForBots({
	.id = kOptionSaveForBots,
	.name = "Keep messages from bots",
	.defaultValue = true,
});

base::options::toggle OptionSaveInGroups({
	.id = kOptionSaveInGroups,
	.name = "Keep messages in groups",
	.defaultValue = true,
});

base::options::toggle OptionSaveInChannels({
	.id = kOptionSaveInChannels,
	.name = "Keep messages in channels",
	.defaultValue = true,
});

base::options::toggle OptionSemiTransparentDeleted({
	.id = kOptionSemiTransparentDeleted,
	.name = "Semi-transparent deleted messages",
	.defaultValue = true,
});

base::options::option<QString> OptionDeletedMark({
	.id = kOptionDeletedMark,
	.name = "Deleted mark",
	.defaultValue = DefaultDeletedMark(),
});

base::options::option<QString> OptionEditedMark({
	.id = kOptionEditedMark,
	.name = "Edited mark",
	.defaultValue = DefaultEditedMark(),
});

base::options::option<int> OptionLimitUsers({
	.id = kOptionLimitUsers,
	.name = "Download in advance: users",
	.defaultValue = 300,
});

base::options::option<int> OptionLimitBots({
	.id = kOptionLimitBots,
	.name = "Download in advance: bots",
	.defaultValue = 5,
});

base::options::option<int> OptionLimitGroups({
	.id = kOptionLimitGroups,
	.name = "Download in advance: groups",
	.defaultValue = 20,
});

base::options::option<int> OptionLimitChannels({
	.id = kOptionLimitChannels,
	.name = "Download in advance: channels",
	.defaultValue = 5,
});

base::options::option<int> OptionCounterSavedAhead({
	.id = "mzgram-archive-saved-ahead",
	.name = "Archive: saved in advance",
});

base::options::option<int> OptionCounterSavedOnDeletion({
	.id = "mzgram-archive-saved-on-deletion",
	.name = "Archive: saved when deleted",
});

base::options::option<int> OptionCounterFailed({
	.id = "mzgram-archive-failed",
	.name = "Archive: not downloaded",
});

base::options::option<int> OptionCounterExpired({
	.id = "mzgram-archive-expired",
	.name = "Archive: link expired",
});

[[nodiscard]] base::options::option<int> &LimitOf(ArchiveSource source) {
	switch (source) {
	case ArchiveSource::Bot: return OptionLimitBots;
	case ArchiveSource::Group: return OptionLimitGroups;
	case ArchiveSource::Channel: return OptionLimitChannels;
	case ArchiveSource::User: break;
	}
	return OptionLimitUsers;
}

[[nodiscard]] base::options::option<int> &CounterOf(ArchiveCounter counter) {
	switch (counter) {
	case ArchiveCounter::SavedOnDeletion: return OptionCounterSavedOnDeletion;
	case ArchiveCounter::Failed: return OptionCounterFailed;
	case ArchiveCounter::Expired: return OptionCounterExpired;
	case ArchiveCounter::SavedAhead: break;
	}
	return OptionCounterSavedAhead;
}

} // namespace

const char kOptionLimitUsers[] = "mzgram-archive-limit-users";
const char kOptionLimitBots[] = "mzgram-archive-limit-bots";
const char kOptionLimitGroups[] = "mzgram-archive-limit-groups";
const char kOptionLimitChannels[] = "mzgram-archive-limit-channels";

int DefaultLimitMb(ArchiveSource source) {
	switch (source) {
	case ArchiveSource::Bot: return 5;
	case ArchiveSource::Group: return 20;
	case ArchiveSource::Channel: return 5;
	case ArchiveSource::User: break;
	}
	return 300;
}

int LimitMb(ArchiveSource source) {
	const auto value = LimitOf(source).value();
	return (value < 0) ? kNoLimit : value;
}

void SetLimitMb(ArchiveSource source, int mb) {
	LimitOf(source).set((mb < 0) ? kNoLimit : mb);
}

const char *LimitOption(ArchiveSource source) {
	switch (source) {
	case ArchiveSource::Bot: return kOptionLimitBots;
	case ArchiveSource::Group: return kOptionLimitGroups;
	case ArchiveSource::Channel: return kOptionLimitChannels;
	case ArchiveSource::User: break;
	}
	return kOptionLimitUsers;
}

std::vector<int> LimitChoices() {
	return { 0, 5, 20, 50, 100, 300, 500, 1000, 2000, kNoLimit };
}

bool Fetches(int64 size, int limitMb, MediaFetch when) {
	if (true) {
		return true;
	}
	const auto extra = (when == MediaFetch::OnDeletion)
		? kDeletionExtraMb
		: 0;
	return size <= int64(limitMb + extra) * 1024 * 1024;
}

int CounterValue(ArchiveCounter counter) {
	return 0;
}

void AddToCounter(ArchiveCounter counter) {
	auto &option = CounterOf(counter);
	option.set(option.value() + 1);
}

void ResetCounters() {
	for (auto i = 0; i != kArchiveCounterCount; ++i) {
		CounterOf(ArchiveCounter(i)).set(0);
	}
}

const char *CounterOption(ArchiveCounter counter) {
	switch (counter) {
	case ArchiveCounter::SavedOnDeletion:
		return "mzgram-archive-saved-on-deletion";
	case ArchiveCounter::Failed: return "mzgram-archive-failed";
	case ArchiveCounter::Expired: return "mzgram-archive-expired";
	case ArchiveCounter::SavedAhead: break;
	}
	return "mzgram-archive-saved-ahead";
}

const char kOptionSaveDeletedMessages[] = "mzgram-save-deleted-and-edited";
const char kOptionSaveEditHistory[] = "mzgram-save-edit-history";
const char kOptionSaveArchiveMedia[] = "mzgram-save-archive-media";
const char kOptionSaveFormatting[] = "mzgram-save-formatting";
const char kOptionSaveReactions[] = "mzgram-save-reactions";
const char kOptionSaveFromUsers[] = "mzgram-save-from-users";
const char kOptionSaveForBots[] = "mzgram-save-for-bots";
const char kOptionSaveInGroups[] = "mzgram-save-in-groups";
const char kOptionSaveInChannels[] = "mzgram-save-in-channels";
const char kOptionSemiTransparentDeleted[] = "mzgram-semi-transparent-deleted";
const char kOptionDeletedMark[] = "mzgram-deleted-mark";
const char kOptionEditedMark[] = "mzgram-edited-mark";

bool SaveDeletedMessages() {
	return OptionSaveDeletedMessages.value();
}

bool SaveEditHistory() {
	return OptionSaveEditHistory.value();
}

bool SaveArchiveMedia() {
	return OptionSaveArchiveMedia.value();
}

bool SaveFormatting() {
	return OptionSaveFormatting.value();
}

bool SaveReactions() {
	return OptionSaveReactions.value();
}

bool SaveFromUsers() {
	return OptionSaveFromUsers.value();
}

bool SaveForBots() {
	return OptionSaveForBots.value();
}

bool SaveInGroups() {
	return OptionSaveInGroups.value();
}

bool SaveInChannels() {
	return OptionSaveInChannels.value();
}

ArchiveSource SourceOf(bool authorBot, ChatKind chat) {
	if (authorBot) {
		return ArchiveSource::Bot;
	}
	switch (chat) {
	case ChatKind::Group: return ArchiveSource::Group;
	case ChatKind::Channel: return ArchiveSource::Channel;
	case ChatKind::Private: break;
	}
	return ArchiveSource::User;
}

bool SavesFrom(ArchiveSource source) {
	switch (source) {
	case ArchiveSource::Bot: return SaveForBots();
	case ArchiveSource::Group: return SaveInGroups();
	case ArchiveSource::Channel: return SaveInChannels();
	case ArchiveSource::User: break;
	}
	return SaveFromUsers();
}

bool MaySaveChat(bool botChat, ChatKind chat) {
	switch (chat) {
	case ChatKind::Group: return SaveInGroups() || SaveForBots();
	case ChatKind::Channel: return SaveInChannels();
	case ChatKind::Private: break;
	}
	return botChat ? SaveForBots() : SaveFromUsers();
}

bool SemiTransparentDeleted() {
	return OptionSemiTransparentDeleted.value();
}

QString DeletedMark() {
	return OptionDeletedMark.value();
}

QString EditedMark() {
	return OptionEditedMark.value();
}

QString DefaultDeletedMark() {
	return QString::fromUtf8("\xf0\x9f\xa7\xb9");
}

QString DefaultEditedMark() {
	return QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f");
}

bool KeepsDeleted(bool service, bool own, ArchiveSource source) {
	return SaveDeletedMessages()
		&& !service
		&& !own
		&& SavesFrom(source);
}

bool KeepsEdit(bool service, bool own, ArchiveSource source) {
	return SaveEditHistory()
		&& !service
		&& !own
		&& SavesFrom(source);
}

int KeptChatListMessage(const std::vector<ChatListCandidate> &items) {
	if (!SaveDeletedMessages()) {
		return -1;
	}
	auto result = -1;
	for (auto i = 0; i != int(items.size()); ++i) {
		const auto &item = items[i];
		if (item.keptDeleted
			&& !item.service
			&& (result < 0 || item.id > items[result].id)) {
			result = i;
		}
	}
	return result;
}

} // namespace MZGram
