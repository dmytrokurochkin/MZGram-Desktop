/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_peer_id.h"

#include "base/options.h"

namespace MZGram {
namespace {

// The Bot API's channels: -(10^12 + id)
constexpr auto kBotApiChannelShift = 1000000000000ULL;

base::options::option<int> OptionPeerId({
	.id = kOptionPeerId,
	.name = "Show ID",
	.description = "Hidden, as in Telegram API or as in Bot API.",
	.defaultValue = int(PeerIdMode::Hidden),
});

} // namespace

const char kOptionPeerId[] = "mzgram-peer-id";

PeerIdMode PeerIdShown() {
	const auto value = OptionPeerId.value();
	return (value == int(PeerIdMode::TelegramApi))
		? PeerIdMode::TelegramApi
		: (value == int(PeerIdMode::BotApi))
		? PeerIdMode::BotApi
		: PeerIdMode::Hidden;
}

void SetPeerIdShown(PeerIdMode mode) {
	OptionPeerId.set(int(mode));
}

QString FormatPeerId(uint64 bareId, PeerIdKind kind, PeerIdMode mode) {
	if (mode == PeerIdMode::Hidden || !bareId) {
		return QString();
	} else if (mode == PeerIdMode::TelegramApi || kind == PeerIdKind::User) {
		return QString::number(bareId);
	} else if (kind == PeerIdKind::Group) {
		return QChar('-') + QString::number(bareId);
	}
	return QChar('-') + QString::number(kBotApiChannelShift + bareId);
}

} // namespace MZGram
