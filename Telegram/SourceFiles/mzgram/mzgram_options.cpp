/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_options.h"

#include "mzgram/mzgram_archive_rules.h"

#include "apiwrap.h"
#include "base/options.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "main/main_session.h"

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

base::options::toggle OptionGhostAutoDelaySend({
	.id = kOptionGhostAutoDelaySend,
	.name = "Ghost mode: delay sending messages",
	.description = "Hold an outgoing message a few seconds before actually "
		"sending it. Not recommended on an unreliable connection.",
});

base::options::toggle OptionGhostSilentSend({
	.id = kOptionGhostSilentSend,
	.name = "Ghost mode: send without sound",
	.description = "Send every outgoing message without a notification "
		"sound for the recipient.",
});

base::options::toggle OptionDisableSponsoredMessages({
	.id = kOptionDisableSponsoredMessages,
	.name = "Disable sponsored messages",
	.description = "Stops sponsored (ad) messages in channels from being "
		"requested or shown.",
});

// Not gated on ghost mode being on already -- this is what offers turning it on.
base::options::toggle OptionOfferGhostModeBeforeStories({
	.id = kOptionOfferGhostModeBeforeStories,
	.name = "Offer ghost mode before Stories",
	.description = "Before opening a story for the first time, ask "
		"whether to turn ghost mode on first.",
});

base::options::toggle OptionMessageSeconds({
	.id = kOptionMessageSeconds,
	.name = "Message time with seconds",
	.description = "Show seconds in message bubble timestamps",
});

base::options::toggle OptionDisableStories({
	.id = kOptionDisableStories,
	.name = "Hide Stories",
	.description = "Hide the stories strip in the chat list and profiles",
});

base::options::toggle OptionDisableGreetingSticker({
	.id = kOptionDisableGreetingSticker,
	.name = "Disable greeting sticker",
	.description = "Hide the hello sticker in new empty private chats",
});

base::options::toggle OptionHideChannelBottomButton({
	.id = kOptionHideChannelBottomButton,
	.name = "Hide channel bottom button",
	.description = "Hide the mute/unmute bar in channels you cannot post to",
});

// Mirrors the autoPauseVideo option of MZGram Android.
base::options::toggle OptionAutoPauseVideo({
	.id = kOptionAutoPauseVideo,
	.name = "Auto pause video",
	.description = "Pause video in the viewer when the window is minimized "
		"or loses focus",
});

base::options::toggle OptionVoiceConfirmation({
	.id = kOptionVoiceConfirmation,
	.name = "Confirm before sending voice messages",
	.description = "Ask for confirmation before sending a voice message",
});

base::options::toggle OptionRoundConfirmation({
	.id = kOptionRoundConfirmation,
	.name = "Confirm before sending round videos",
	.description = "Ask for confirmation before sending a round video",
});

base::options::toggle OptionRepeatMessage({
	.id = kOptionRepeatMessage,
	.name = "Show \"Repeat\" in the message menu",
	.description = "Resend a message's content to the same chat",
});

base::options::toggle OptionMessageDetails({
	.id = kOptionMessageDetails,
	.name = "Show \"Message details\" in the message menu",
	.description = "Show the message id, dates and media info",
});

// Mirrors the QR scanning feature already present in Telegram for Android. Decodes a QR
// code from an already-downloaded photo entirely offline, using the
// bundled quirc library (see mzgram/quirc).
base::options::toggle OptionScanQrCode({
	.id = kOptionScanQrCode,
	.name = "Show \"Scan for QR code\" on photos",
	.description = "Decode a QR code from a downloaded photo, offline",
});

// As on MZGram Android (openArchiveOnPull).
base::options::toggle OptionOpenArchiveOnPull({
	.id = kOptionOpenArchiveOnPull,
	.name = "Open Archive on pull down",
	.description = "Pulling the chat list down past the Archive row opens it",
});

// As on MZGram Android (showAddToSavedMessages).
base::options::toggle OptionSaveMessage({
	.id = kOptionSaveMessage,
	.name = "Show \"Save message\" in the message menu",
	.description = "Forward a message to Saved Messages in one click",
});

// As on MZGram Android (showSetReminder).
base::options::toggle OptionSetReminder({
	.id = kOptionSetReminder,
	.name = "Show \"Set a reminder\" in the message menu",
	.description = "Forward a message to Saved Messages, scheduled for later",
});

// As on MZGram Android (showOpenIn).
base::options::toggle OptionOpenIn({
	.id = kOptionOpenIn,
	.name = "Show \"Open in...\" for downloaded files",
	.description = "Adds the OS \"Open with\" dialog to the file menu",
});

base::options::toggle OptionMediaPreviewOnChatPreview({
	.id = kOptionMediaPreviewOnChatPreview,
	.name = "Media preview instead of Chat Preview",
	.description = "When the last message is a photo or video, show it "
		"instead of the Chat Preview peek",
});

} // namespace

const char kOptionGhostMode[] = "mzgram-ghost-mode";
const char kOptionSendReadReceipts[] = "mzgram-ghost-send-read-receipts";
const char kOptionSendTyping[] = "mzgram-ghost-send-typing";
const char kOptionSendOnline[] = "mzgram-ghost-send-online";
const char kOptionGhostAutoDelaySend[] = "mzgram-ghost-auto-delay-send";
const char kOptionGhostSilentSend[] = "mzgram-ghost-silent-send";
const char kOptionOfferGhostModeBeforeStories[] = "mzgram-offer-ghost-mode-before-stories";
const char kOptionDisableSponsoredMessages[] = "mzgram-disable-sponsored-messages";
// The id keeps its first name so a saved choice survives the rename.
const char kOptionMessageSeconds[] = "mzgram-message-seconds";
const char kOptionDisableStories[] = "mzgram-disable-stories";
const char kOptionDisableGreetingSticker[] =
	"mzgram-disable-greeting-sticker";
const char kOptionHideChannelBottomButton[] =
	"mzgram-hide-channel-bottom-button";
const char kOptionAutoPauseVideo[] = "mzgram-auto-pause-video";
const char kOptionVoiceConfirmation[] = "mzgram-voice-confirmation";
const char kOptionRoundConfirmation[] = "mzgram-round-confirmation";
const char kOptionRepeatMessage[] = "mzgram-repeat-message";
const char kOptionMessageDetails[] = "mzgram-message-details";
const char kOptionScanQrCode[] = "mzgram-scan-qr-code";
const char kOptionOpenArchiveOnPull[] = "mzgram-open-archive-on-pull";
const char kOptionSaveMessage[] = "mzgram-save-message";
const char kOptionSetReminder[] = "mzgram-set-reminder";
const char kOptionOpenIn[] = "mzgram-open-in";
const char kOptionMediaPreviewOnChatPreview[] =
	"mzgram-media-preview-on-chat-preview";

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

bool GhostAutoDelaySend() {
	return GhostMode() && OptionGhostAutoDelaySend.value();
}

bool GhostSilentSend() {
	return GhostMode() && OptionGhostSilentSend.value();
}

void EnableGhostMode() {
	base::options::lookup<bool>(kOptionGhostMode).set(true);
}

bool OfferGhostModeBeforeStories() {
	return OptionOfferGhostModeBeforeStories.value();
}

bool DisableSponsoredMessages() {
	return OptionDisableSponsoredMessages.value();
}

TextWithEntities StripZalgoMessageText(TextWithEntities text) {
	if (!text.entities.isEmpty()) {
		return text;
	}
	text.text = StripZalgo(text.text);
	return text;
}

void MarkMessageReadDueToInteraction(
		not_null<History*> history,
		MsgId messageId) {
	if (SendReadReceipts() || !messageId) {
		return;
	}
	const auto peer = history->peer;
	const auto session = &history->session();
	if (const auto channel = peer->asChannel()) {
		session->api().request(MTPchannels_ReadHistory(
			channel->inputChannel(),
			MTP_int(messageId)
		)).send();
	} else {
		session->api().request(MTPmessages_ReadHistory(
			peer->input(),
			MTP_int(messageId)
		)).done([session, peer](const MTPmessages_AffectedMessages &result) {
			session->api().applyAffectedMessages(peer, result);
		}).send();
	}
}

// See mzgram_archive_rules.h.
bool AntiRecall() {
	return SaveDeletedMessages();
}

bool EditHistory() {
	return SaveEditHistory();
}

bool KeepSelfDestructing() {
	return SaveDeletedMessages();
}

bool MessageSeconds() {
	return OptionMessageSeconds.value();
}

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

bool AutoPauseVideo() {
	return OptionAutoPauseVideo.value();
}

bool VoiceConfirmation() {
	return OptionVoiceConfirmation.value();
}

bool RoundConfirmation() {
	return OptionRoundConfirmation.value();
}

bool RepeatMessageAction() {
	return OptionRepeatMessage.value();
}

bool MessageDetailsAction() {
	return OptionMessageDetails.value();
}

bool ScanQrCodeAction() {
	return OptionScanQrCode.value();
}

bool OpenArchiveOnPull() {
	return OptionOpenArchiveOnPull.value();
}

bool SaveMessageAction() {
	return OptionSaveMessage.value();
}

bool SetReminderAction() {
	return OptionSetReminder.value();
}

bool OpenInAction() {
	return OptionOpenIn.value();
}

bool MediaPreviewOnChatPreview() {
	return OptionMediaPreviewOnChatPreview.value();
}

} // namespace MZGram
