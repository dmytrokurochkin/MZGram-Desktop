/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_text_filters.h"

#include "base/options.h"

#include <QtCore/QChar>
#include <QtCore/QString>

namespace MZGram {
namespace {

// MZGram's own code (AyuGram4A has no equivalent on either platform).
base::options::toggle OptionStripZalgoText({
	.id = kOptionStripZalgoText,
	.name = "Zalgo filter",
	.description = "Removes stacked Unicode combining marks (Zalgo-style "
		"corrupted text) from names and chat titles shown to you.",
});

} // namespace

const char kOptionStripZalgoText[] = "mzgram-strip-zalgo-text";

bool StripZalgoText() {
	return OptionStripZalgoText.value();
}

QString StripZalgo(const QString &text) {
	if (!StripZalgoText() || text.isEmpty()) {
		return text;
	}
	auto result = QString();
	result.reserve(text.size());
	for (const auto &ch : text) {
		const auto category = ch.category();
		if (category != QChar::Mark_NonSpacing
			&& category != QChar::Mark_SpacingCombining
			&& category != QChar::Mark_Enclosing) {
			result.append(ch);
		}
	}
	return result;
}

} // namespace MZGram
