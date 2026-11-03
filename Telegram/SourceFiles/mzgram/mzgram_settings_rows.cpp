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

const std::vector<HideableRow> &HideableRows() {
	static const auto result = std::vector<HideableRow>{
		{ "premium", "mzgram-hide-row-premium", "hide_row_premium" },
		{ "stars", "mzgram-hide-row-stars", "hide_row_stars" },
		{ "currency", "mzgram-hide-row-currency", "hide_row_currency" },
		{ "business", "mzgram-hide-row-business", "hide_row_business" },
		{ "send-gift", "mzgram-hide-row-send-gift", "hide_row_send_gift" },
		{ "faq", "mzgram-hide-row-faq", "hide_row_faq", true },
		{ "features", "mzgram-hide-row-features", "hide_row_features", true },
		{
			"ask-question",
			"mzgram-hide-row-ask-question",
			"hide_row_ask_question",
			true,
		},
	};
	return result;
}

bool RowHidden(const char *key) {
	for (const auto &row : HideableRows()) {
		if (!std::strcmp(row.key, key)) {
			return base::options::lookup<bool>(row.option).value();
		}
	}
	return false;
}

void SetRowHidden(const char *key, bool hidden) {
	for (const auto &row : HideableRows()) {
		if (!std::strcmp(row.key, key)) {
			base::options::lookup<bool>(row.option).set(hidden);
		}
	}
}

bool HelpHidden() {
	for (const auto &row : HideableRows()) {
		if (row.help && !RowHidden(row.key)) {
			return false;
		}
	}
	return true;
}

} // namespace MZGram
