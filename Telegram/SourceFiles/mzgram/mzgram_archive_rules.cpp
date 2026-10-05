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

base::options::toggle OptionSaveForBots({
	.id = kOptionSaveForBots,
	.name = "Save in chats with bots",
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
const char kOptionSaveForBots[] = "mzgram-save-for-bots";
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

bool SaveForBots() {
	return OptionSaveForBots.value();
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

bool KeepsDeleted(bool service, bool own, bool bot) {
	return SaveDeletedMessages()
		&& !service
		&& !own
		&& (!bot || SaveForBots());
}

bool KeepsEdit(bool service, bool own, bool bot) {
	return SaveEditHistory()
		&& !service
		&& !own
		&& (!bot || SaveForBots());
}

} // namespace MZGram
