/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>

#include <vector>

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
// Keep messages: from users, from bots, in groups, in channels; all on
// by default. "From bots" keeps the id of the earlier "Save in chats with
// bots", so its value carries over.
extern const char kOptionSaveFromUsers[];
extern const char kOptionSaveForBots[];
extern const char kOptionSaveInGroups[];
extern const char kOptionSaveInChannels[];
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
[[nodiscard]] bool SaveFromUsers();
[[nodiscard]] bool SaveForBots();
[[nodiscard]] bool SaveInGroups();
[[nodiscard]] bool SaveInChannels();
[[nodiscard]] bool SemiTransparentDeleted();
[[nodiscard]] QString DeletedMark();
[[nodiscard]] QString EditedMark();
[[nodiscard]] QString DefaultDeletedMark();
[[nodiscard]] QString DefaultEditedMark();

// Which of the four switches decides for a message. The author first: a
// bot's message goes by "From bots" in any chat. Else the chat: a private
// chat by "From users", a group by "In groups" (a channel posting in the
// group and an anonymous admin too, as the group is where they write), a
// channel by "In channels". Turning one off keeps what was saved.
enum class ChatKind {
	Private,
	Group,
	Channel,
};
enum class ArchiveSource {
	User,
	Bot,
	Group,
	Channel,
};
[[nodiscard]] ArchiveSource SourceOf(bool authorBot, ChatKind chat);
[[nodiscard]] bool SavesFrom(ArchiveSource source);
// Whether anything of a chat may be kept, before its messages are known:
// a group may have bots' messages to keep while "In groups" is off.
[[nodiscard]] bool MaySaveChat(bool botChat, ChatKind chat);

// Whether another person's message is kept once deleted: never the
// account owner's own messages, never service messages, only while the
// switch of its source is on, and nothing while the switch is off.
[[nodiscard]] bool KeepsDeleted(bool service, bool own, ArchiveSource source);
// The same for the earlier version of an edited message.
[[nodiscard]] bool KeepsEdit(bool service, bool own, ArchiveSource source);

// A message of a chat, as the chat list sees it.
struct ChatListCandidate {
	int64 id = 0;
	bool keptDeleted = false;
	bool service = false;
};

// A chat whose own messages are all gone (the only one or the whole
// history deleted or cleared by the other side) stays in the chat list
// with its newest kept deleted message: its index in items, -1 for none
// (the chat goes as usual).
[[nodiscard]] int KeptChatListMessage(
	const std::vector<ChatListCandidate> &items);

} // namespace MZGram
