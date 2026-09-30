/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_msg_id.h"
#include "ui/text/text_entity.h"

class QTime;
class QString;
class History;

namespace MZGram {

extern const char kOptionGhostMode[];
extern const char kOptionSendReadReceipts[];
extern const char kOptionSendTyping[];
extern const char kOptionSendOnline[];
extern const char kOptionAntiRecall[];
extern const char kOptionEditHistory[];
extern const char kOptionKeepSelfDestructing[];
extern const char kOptionMarkMessages[];

// Ghost mode is a master switch: each of the three below stays meaningful on
// its own, so a user can keep typing status while withholding read receipts.
[[nodiscard]] bool GhostMode();
[[nodiscard]] bool SendReadReceipts();
[[nodiscard]] bool SendTyping();
[[nodiscard]] bool SendOnline();

// MZGram's own code, Spy mode section. Independent of ghost mode; reuses
// the SendOnline() suppression point.
extern const char kOptionSpyHideOnlineStatus[];

// MZGram's own code. While ghost mode is suppressing read receipts,
// replying to or reacting to a specific message is an explicit
// interaction, so that one message is marked read on the server
// regardless -- independent of the batched/suppressed read-request
// queue in Data::Histories.
void MarkMessageReadDueToInteraction(
	not_null<History*> history,
	MsgId messageId);

// MZGram's own code. Holds an outgoing message back for a few seconds
// before it actually sends, so composing and sending right away does not
// create a burst of activity that can make you look online. The settings
// screen warns this is not recommended on an unreliable connection.
extern const char kOptionGhostAutoDelaySend[];
[[nodiscard]] bool GhostAutoDelaySend();

// MZGram's own code. Forces every outgoing message to be sent without a
// notification sound for the recipient while ghost mode is on, regardless
// of the per-message "send without sound" choice.
extern const char kOptionGhostSilentSend[];
[[nodiscard]] bool GhostSilentSend();

// MZGram's own code. Turns ghost mode on (used right before a story is
// shown, when the user picked "enable and view" on the prompt below).
void EnableGhostMode();

// MZGram's own code. Before opening the story viewer for the first time
// (not when swiping to the next already-open story), offers to turn ghost
// mode on so viewing does not mark the story as seen for the other side.
extern const char kOptionOfferGhostModeBeforeStories[];
[[nodiscard]] bool OfferGhostModeBeforeStories();

[[nodiscard]] bool AntiRecall();
[[nodiscard]] bool EditHistory();
[[nodiscard]] bool KeepSelfDestructing();
// Dims kept deleted messages and puts a pencil on edited ones.
[[nodiscard]] bool MarkKeptMessages();

extern const char kOptionMessageSeconds[];
[[nodiscard]] bool MessageSeconds();
// Formats a message bubble's time, adding seconds when the option is on.
[[nodiscard]] QString FormatMessageTime(const QTime &time);

extern const char kOptionDisableStories[];
[[nodiscard]] bool DisableStories();

extern const char kOptionDisableGreetingSticker[];
[[nodiscard]] bool DisableGreetingSticker();

extern const char kOptionHideChannelBottomButton[];
[[nodiscard]] bool HideChannelBottomButton();

extern const char kOptionAutoPauseVideo[];
[[nodiscard]] bool AutoPauseVideo();

extern const char kOptionVoiceConfirmation[];
extern const char kOptionRoundConfirmation[];
[[nodiscard]] bool VoiceConfirmation();
[[nodiscard]] bool RoundConfirmation();

extern const char kOptionRepeatMessage[];
extern const char kOptionMessageDetails[];
[[nodiscard]] bool RepeatMessageAction();
[[nodiscard]] bool MessageDetailsAction();

extern const char kOptionScanQrCode[];
[[nodiscard]] bool ScanQrCodeAction();

extern const char kOptionOpenArchiveOnPull[];
[[nodiscard]] bool OpenArchiveOnPull();

extern const char kOptionSaveMessage[];
[[nodiscard]] bool SaveMessageAction();

extern const char kOptionSetReminder[];
[[nodiscard]] bool SetReminderAction();

extern const char kOptionOpenIn[];
[[nodiscard]] bool OpenInAction();

// MZGram's own code, no Nekogram/AyuGram equivalent found. Long-pressing
// (Android) or press-and-holding the avatar (Desktop) on a chat list row
// normally opens the native Chat Preview peek. When the chat's last
// message is a photo or video, this shows that media directly instead.
extern const char kOptionMediaPreviewOnChatPreview[];
[[nodiscard]] bool MediaPreviewOnChatPreview();

// Ported concept from AyuGram4A (AyuConfig.disableAds).
extern const char kOptionDisableSponsoredMessages[];
[[nodiscard]] bool DisableSponsoredMessages();

// MZGram's own code. Strips Zalgo-style combining-mark text corruption
// from display names shown to the user.
extern const char kOptionStripZalgoText[];
[[nodiscard]] bool StripZalgoText();
[[nodiscard]] QString StripZalgo(const QString &text);
// Strips Zalgo only when there are no entities: entity offsets are fixed
// against the original server-sent text, so shortening the text first
// would misalign any formatting/mention spans applied on top of it.
[[nodiscard]] TextWithEntities StripZalgoMessageText(TextWithEntities text);

} // namespace MZGram
