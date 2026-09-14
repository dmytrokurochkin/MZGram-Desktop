/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_options.h"

#include "base/options.h"

#include <QtCore/QLocale>
#include <QtCore/QString>
#include <QtCore/QTime>

namespace MZGram {
namespace {

base::options::toggle OptionGhostMode({
	.id = kOptionGhostMode,
	.name = "Ghost mode",
	.description = "Withhold read receipts, typing status and online status",
});

base::options::toggle OptionSendReadReceipts({
	.id = kOptionSendReadReceipts,
	.name = "Ghost mode: send read receipts",
	.description = "Let others see that you read their messages",
	.defaultValue = false,
});

base::options::toggle OptionSendTyping({
	.id = kOptionSendTyping,
	.name = "Ghost mode: send typing status",
	.description = "Let others see when you are typing or recording",
	.defaultValue = false,
});

base::options::toggle OptionSendOnline({
	.id = kOptionSendOnline,
	.name = "Ghost mode: send online status",
	.description = "Let others see when you are online",
	.defaultValue = false,
});

// Off by default: both keep other people's text on disk in plain SQLite.
base::options::toggle OptionAntiRecall({
	.id = kOptionAntiRecall,
	.name = "Anti-recall",
	.description = "Keep messages others delete, marked as deleted",
});

base::options::toggle OptionEditHistory({
	.id = kOptionEditHistory,
	.name = "Edit history",
	.description = "Keep earlier versions of edited messages",
});

base::options::toggle OptionKeepSelfDestructing({
	.id = kOptionKeepSelfDestructing,
	.name = "Keep self-destructing media",
	.description = "Keep view-once and timed media after it is opened",
});

// On by default: it only changes how messages are drawn.
base::options::toggle OptionMarkMessages({
	.id = kOptionMarkMessages,
	.name = "Mark deleted and edited messages",
	.description = "Dim kept deleted messages and add a pencil to edited ones",
	.defaultValue = true,
});

// Ported from AyuGram Desktop dev (ayu/ayu_settings.h, showMessageSeconds).
base::options::toggle OptionMessageSeconds({
	.id = kOptionMessageSeconds,
	.name = "Message time with seconds",
	.description = "Show seconds in message bubble timestamps",
});

// Ported from AyuGram Desktop dev (ayu/ayu_settings.h, disableStories).
base::options::toggle OptionDisableStories({
	.id = kOptionDisableStories,
	.name = "Hide Stories",
	.description = "Hide the stories strip in the chat list and profiles",
});

// Ported from AyuGram Desktop dev
// (ayu/ayu_settings.h, disableGreetingSticker).
base::options::toggle OptionDisableGreetingSticker({
	.id = kOptionDisableGreetingSticker,
	.name = "Disable greeting sticker",
	.description = "Hide the hello sticker in new empty private chats",
});

// Ported from AyuGram Desktop dev (ayu/ayu_settings.h, channelBottomButton,
// the Hidden variant; MuteUnmute/DiscussWithFallback are not ported).
base::options::toggle OptionHideChannelBottomButton({
	.id = kOptionHideChannelBottomButton,
	.name = "Hide channel bottom button",
	.description = "Hide the mute/unmute bar in channels you cannot post to",
});

} // namespace

const char kOptionGhostMode[] = "mzgram-ghost-mode";
const char kOptionSendReadReceipts[] = "mzgram-ghost-send-read-receipts";
const char kOptionSendTyping[] = "mzgram-ghost-send-typing";
const char kOptionSendOnline[] = "mzgram-ghost-send-online";
const char kOptionAntiRecall[] = "mzgram-anti-recall";
const char kOptionEditHistory[] = "mzgram-edit-history";
const char kOptionKeepSelfDestructing[] = "mzgram-keep-self-destructing";
// The id keeps its first name so a saved choice survives the rename.
const char kOptionMarkMessages[] = "mzgram-dim-marked";
const char kOptionMessageSeconds[] = "mzgram-message-seconds";
const char kOptionDisableStories[] = "mzgram-disable-stories";
const char kOptionDisableGreetingSticker[] =
	"mzgram-disable-greeting-sticker";
const char kOptionHideChannelBottomButton[] =
	"mzgram-hide-channel-bottom-button";

bool GhostMode() {
	return OptionGhostMode.value();
}

bool SendReadReceipts() {
	return !GhostMode() || OptionSendReadReceipts.value();
}

bool SendTyping() {
	return !GhostMode() || OptionSendTyping.value();
}

bool SendOnline() {
	return !GhostMode() || OptionSendOnline.value();
}

bool AntiRecall() {
	return OptionAntiRecall.value();
}

bool EditHistory() {
	return OptionEditHistory.value();
}

bool KeepSelfDestructing() {
	return OptionKeepSelfDestructing.value();
}

bool MarkKeptMessages() {
	return OptionMarkMessages.value();
}

bool MessageSeconds() {
	return OptionMessageSeconds.value();
}

// Ported from AyuGram Desktop dev
// (ayu/utils/telegram_helpers.cpp, formatMessageTime).
QString FormatMessageTime(const QTime &time) {
	if (!MessageSeconds()) {
		return QLocale().toString(time, QLocale::ShortFormat);
	}
	const auto format = QLocale().timeFormat(QLocale::ShortFormat).contains(
		u"AP"_q)
		? u"h:mm:ss AP"_q
		: u"HH:mm:ss"_q;
	return QLocale().toString(time, format);
}

bool DisableStories() {
	return OptionDisableStories.value();
}

bool DisableGreetingSticker() {
	return OptionDisableGreetingSticker.value();
}

bool HideChannelBottomButton() {
	return OptionHideChannelBottomButton.value();
}

} // namespace MZGram
