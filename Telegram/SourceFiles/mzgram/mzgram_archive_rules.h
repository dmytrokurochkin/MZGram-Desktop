/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

namespace MZGram {

// Settings > MZGram > Archive > "Save deleted and edited messages", on by
// default: other people's deleted messages, earlier versions of their edited
// messages and their view-once and timed media are kept, in every private
// chat, group, channel and secret chat. There is no list of chats to pick.
//
// Kept free of session/history dependencies so the rules can be checked by
// the headless unit test.
extern const char kOptionSaveDeletedAndEdited[];
[[nodiscard]] bool SaveDeletedAndEdited();

// Whether a message is kept once deleted or edited: never the account
// owner's own messages, never service messages, and nothing while the
// switch is off.
[[nodiscard]] bool KeepsMessage(bool service, bool own);

} // namespace MZGram
