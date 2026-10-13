/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mzgram/mzgram_local_pins_rules.h"

class History;

namespace Dialogs {
class Entry;
} // namespace Dialogs

namespace Data {
class Folder;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace MZGram::LocalPins {

// Telegram allows a few pinned chats in the main list and the archive (the
// server refuses more; Premium raises the limit, which MZGram does not
// touch). Past that limit MZGram pins chats locally, up to kMaxTotal pinned
// chats in all: right after the server's pinned chats, in pin order, kept in
// tdata/mzgram_local_pins.json, never sent to the server. Custom chat
// folders keep the server's own pins.

// The place of a chat among the local pins of its list, -1 when not pinned locally.
[[nodiscard]] int Index(not_null<const Dialogs::Entry*> entry);
[[nodiscard]] bool IsPinned(not_null<const Dialogs::Entry*> entry);

// Local pins of a list whose chats are known.
[[nodiscard]] int Count(not_null<Main::Session*> session, Data::Folder *folder);

// Whether pinning this chat now pins it in MZGram only.
[[nodiscard]] bool WouldPinLocally(not_null<History*> history);

void Pin(not_null<History*> history);
void Unpin(not_null<History*> history);

} // namespace MZGram::LocalPins
