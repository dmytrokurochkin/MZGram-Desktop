/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <map>
#include <vector>

namespace MZGram::LocalPins {

// Pinned chats kept by MZGram itself, on this device only (see
// mzgram_local_pins.h). These rules have no app dependencies, so
// mzgram_tests checks them.

// Server pins and local pins together, in one list.
inline constexpr auto kMaxTotal = 10;

// Whether pinning one more chat goes locally: the server's limit is
// reached, and server and local pins together stay within kMaxTotal.
[[nodiscard]] bool PinsLocally(int serverPinned, int localPinned, int serverLimit);

// The chat list sort key of the local pin at index (0 first): below every
// server pin (Dialogs::PinnedDialogPos, 0xFFFFFFFF000000FF - pinned index)
// and above every chat sorted by date.
[[nodiscard]] quint64 SortPosition(int index);

// The stored pins: "<user id>_<folder id>" -> peer ids in pin order.
using Stored = std::map<QString, std::vector<quint64>>;
[[nodiscard]] QByteArray Serialize(const Stored &stored);
[[nodiscard]] Stored Parse(const QByteArray &json);

} // namespace MZGram::LocalPins
