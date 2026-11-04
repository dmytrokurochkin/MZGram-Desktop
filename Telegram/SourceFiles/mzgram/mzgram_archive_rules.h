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

// Settings > MZGram > Archive > Download in advance. The archive keeps
// three levels for every kept message:
// 1. always its text, its data and, for each file, what is needed to
//    download it later (the message as the server sent it);
// 2. the file itself, downloaded as soon as the message comes, when it is
//    not larger than the limit of its source (MB; kNoLimit for any size);
// 3. when the message is deleted: a copy of the file from Telegram's
//    cache, else a download by the kept data, up to the limit plus
//    kDeletionExtraMb.
// Files saved earlier stay whatever the limits are.
extern const char kOptionLimitUsers[];
extern const char kOptionLimitBots[];
extern const char kOptionLimitGroups[];
extern const char kOptionLimitChannels[];
inline constexpr auto kNoLimit = -1;
inline constexpr auto kDeletionExtraMb = 100;

[[nodiscard]] int DefaultLimitMb(ArchiveSource source);
[[nodiscard]] int LimitMb(ArchiveSource source);
void SetLimitMb(ArchiveSource source, int mb);
[[nodiscard]] const char *LimitOption(ArchiveSource source);
// The values offered in Settings, kNoLimit last.
[[nodiscard]] std::vector<int> LimitChoices();

enum class MediaFetch {
	Ahead,
	OnDeletion,
};
// Whether a file of this many bytes is downloaded at that moment; a size
// of 0 (not known) counts as small.
[[nodiscard]] bool Fetches(int64 size, int limitMb, MediaFetch when);

// What happened to the files the archive downloaded, to choose the
// limits on real data: saved in advance, saved when deleted, not
// downloaded, and the server's link to the file expired
// (FILE_REFERENCE_EXPIRED: the file of a deleted message is gone).
enum class ArchiveCounter {
	SavedAhead,
	SavedOnDeletion,
	Failed,
	Expired,
};
inline constexpr auto kArchiveCounterCount = 4;
[[nodiscard]] int CounterValue(ArchiveCounter counter);
void AddToCounter(ArchiveCounter counter);
void ResetCounters();
[[nodiscard]] const char *CounterOption(ArchiveCounter counter);

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
