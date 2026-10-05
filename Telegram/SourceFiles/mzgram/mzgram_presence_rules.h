/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <vector>

namespace MZGram {

// What the server says about a hidden last seen.
enum class HiddenLastSeen {
	None,
	Recently,
	WithinWeek,
	WithinMonth,
};

// Whether a time this device noted as the person being online fits what
// the server says, so it can be shown, marked as approximate, instead.
[[nodiscard]] bool ApproximateFits(
	HiddenLastSeen hidden,
	TimeId seen,
	TimeId now);

// A read event: every own message of a chat up to maxId was read by readAt.
struct OutboxRead {
	int64 maxId = 0;
	TimeId readAt = 0;
};

// When an own message was read, from the read events kept for its chat:
// the earliest event that covers it, or 0 when none does.
[[nodiscard]] TimeId FirstReadAt(
	const std::vector<OutboxRead> &reads,
	int64 messageId);

} // namespace MZGram
