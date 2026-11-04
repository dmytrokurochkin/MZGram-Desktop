/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>

namespace MZGram {

// Settings > MZGram > Interface > Show ID: the ID of a user, bot, group or
// channel as a row of its profile. Hidden by default; Telegram API shows
// the bare number, Bot API the number the Bot API uses: -100 and the
// number for a channel or supergroup, minus the number for a group, the
// bare number for a user or bot. A click on it copies it.
extern const char kOptionPeerId[];

enum class PeerIdMode {
	Hidden = 0,
	TelegramApi = 1,
	BotApi = 2,
};

enum class PeerIdKind {
	User,
	Group,
	Channel,
};

[[nodiscard]] PeerIdMode PeerIdShown();
void SetPeerIdShown(PeerIdMode mode);

// Empty when hidden.
[[nodiscard]] QString FormatPeerId(
	uint64 bareId,
	PeerIdKind kind,
	PeerIdMode mode);

} // namespace MZGram
