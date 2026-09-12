/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_options.h"

#include "base/options.h"

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

} // namespace

const char kOptionGhostMode[] = "mzgram-ghost-mode";
const char kOptionSendReadReceipts[] = "mzgram-ghost-send-read-receipts";
const char kOptionSendTyping[] = "mzgram-ghost-send-typing";
const char kOptionSendOnline[] = "mzgram-ghost-send-online";

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

} // namespace MZGram
