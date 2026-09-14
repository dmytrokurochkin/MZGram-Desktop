/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class QTime;
class QString;

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

} // namespace MZGram
