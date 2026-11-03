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

} // namespace

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
