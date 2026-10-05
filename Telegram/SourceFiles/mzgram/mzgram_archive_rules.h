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

// Settings > MZGram > Archive. Other people's deleted messages (with their
// view-once and timed media) and earlier versions of their edited messages
// are kept in every private chat, group, channel and secret chat; each part
// has its own switch, all on by default. There is no list of chats to pick.
//
// Kept free of session/history dependencies so the rules can be checked by
// the headless unit test.

// Keeps the id of the one switch of the previous version, so its value
// carries over.
extern const char kOptionSaveDeletedMessages[];
extern const char kOptionSaveEditHistory[];
// Files of kept messages, copied to Downloads/MZGram/Saved Attachments.
extern const char kOptionSaveArchiveMedia[];
// Bold, italic, links and the like of kept messages.
extern const char kOptionSaveFormatting[];
extern const char kOptionSaveReactions[];
// Chats with bots.
extern const char kOptionSaveForBots[];
// Kept deleted messages drawn at kDeletedOpacity.
extern const char kOptionSemiTransparentDeleted[];
// The marks before the time of a deleted or edited message; any text,
// empty for none.
extern const char kOptionDeletedMark[];
extern const char kOptionEditedMark[];

inline constexpr auto kDeletedOpacity = 0.75;

[[nodiscard]] bool SaveDeletedMessages();
[[nodiscard]] bool SaveEditHistory();
[[nodiscard]] bool SaveArchiveMedia();
[[nodiscard]] bool SaveFormatting();
[[nodiscard]] bool SaveReactions();
[[nodiscard]] bool SaveForBots();
[[nodiscard]] bool SemiTransparentDeleted();
[[nodiscard]] QString DeletedMark();
[[nodiscard]] QString EditedMark();
[[nodiscard]] QString DefaultDeletedMark();
[[nodiscard]] QString DefaultEditedMark();

// Whether another person's message is kept once deleted: never the
// account owner's own messages, never service messages, in a chat with a
// bot only while that switch is on, and nothing while the switch is off.
[[nodiscard]] bool KeepsDeleted(bool service, bool own, bool bot);
// The same for the earlier version of an edited message.
[[nodiscard]] bool KeepsEdit(bool service, bool own, bool bot);

} // namespace MZGram
