/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_presence_rules.h"

namespace MZGram {
namespace {

constexpr auto kDay = TimeId(24 * 60 * 60);

} // namespace

bool ApproximateFits(HiddenLastSeen hidden, TimeId seen, TimeId now) {
	const auto maxAge = [&] {
		switch (hidden) {
		case HiddenLastSeen::Recently: return 3 * kDay;
		case HiddenLastSeen::WithinWeek: return 7 * kDay;
		case HiddenLastSeen::WithinMonth: return 30 * kDay;
		case HiddenLastSeen::None: return TimeId(0);
		}
		return TimeId(0);
	}();
	return (maxAge > 0)
		&& (seen > 0)
		&& (seen <= now + 60)
		&& (now - seen <= maxAge);
}

TimeId FirstReadAt(const std::vector<OutboxRead> &reads, int64 messageId) {
	auto result = TimeId(0);
	for (const auto &read : reads) {
		if (read.maxId >= messageId
			&& read.readAt > 0
			&& (!result || read.readAt < result)) {
			result = read.readAt;
		}
	}
	return result;
}

} // namespace MZGram
