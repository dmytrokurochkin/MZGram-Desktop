/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_settings_rows.h"

#include "base/basic_types.h"
#include "base/options.h"

#include <cstring>

namespace MZGram {
namespace {

base::options::toggle OptionHidePremium({
	.id = "mzgram-hide-row-premium",
	.name = "MZGram: hide Telegram Premium in Settings",
});
base::options::toggle OptionHideStars({
	.id = "mzgram-hide-row-stars",
	.name = "MZGram: hide My Stars in Settings",
});
base::options::toggle OptionHideCurrency({
	.id = "mzgram-hide-row-currency",
	.name = "MZGram: hide Gram Earnings in Settings",
});
base::options::toggle OptionHideBusiness({
	.id = "mzgram-hide-row-business",
	.name = "MZGram: hide Telegram Business in Settings",
});
base::options::toggle OptionHideGift({
	.id = "mzgram-hide-row-send-gift",
	.name = "MZGram: hide Premium Gifting in Settings",
});
base::options::toggle OptionHideFaq({
	.id = "mzgram-hide-row-faq",
	.name = "MZGram: hide Telegram FAQ in Settings",
});
base::options::toggle OptionHideFeatures({
	.id = "mzgram-hide-row-features",
	.name = "MZGram: hide Telegram Features in Settings",
});
base::options::toggle OptionHideAskQuestion({
	.id = "mzgram-hide-row-ask-question",
	.name = "MZGram: hide Ask a Question in Settings",
});

} // namespace

// The tests come first: the rows are not written yet.
const std::vector<HideableRow> &HideableRows() {
	static const auto result = std::vector<HideableRow>();
	return result;
}

bool RowHidden(const char *key) {
	Q_UNUSED(key);
	return false;
}

void SetRowHidden(const char *key, bool hidden) {
	Q_UNUSED(key);
	Q_UNUSED(hidden);
}

bool HelpHidden() {
	return false;
}

} // namespace MZGram
