/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_branding_rules.h"

#include "base/basic_types.h"

namespace MZGram::Branding {

QString VersionText(qint64 code) {
	if (code <= 0) {
		return QString();
	}
	const auto major = code / 1000000;
	const auto minor = (code / 10000) % 100;
	const auto patch = (code / 100) % 100;
	const auto beta = code % 100;
	auto result = u"%1.%2.%3"_q.arg(major).arg(minor).arg(patch);
	if (beta != 99) {
		result += u" beta %1"_q.arg(beta);
	}
	return result;
}

const std::vector<const char*> &RenamedKeys() {
	static const auto result = std::vector<const char*>{
		"lng_open_from_tray",
		"lng_quit_from_tray",
		"lng_tray_icon_text",
		"lng_error_start_minimized_passcoded",
		"lng_proxy_unsupported",
		"lng_update_telegram",
		"lng_sure_save_language",
		"lng_settings_auto_start",
		"lng_settings_auto_start_disabled_uwp",
		"lng_settings_use_winhello_about",
		"lng_settings_use_systempwd_about",
		"lng_passcode_about",
		"lng_passcode_about2",
		"lng_passcode_about3",
		"lng_passcode_winhello_unlock",
		"lng_message_unsupported",
		"lng_unsupported_message_text",
		"lng_unsupported_block_text",
		"lng_bot_share_location_unavailable",
		"lng_gift_transfer_unlocks_update_about",
		"lng_passport_app_out_of_date",
		"lng_export_progress",
		"lng_outdated_soon",
		"lng_outdated_now",
	};
	return result;
}

const std::vector<Replaced> &ReplacedKeys() {
	static const auto result = std::vector<Replaced>{
		{ "lng_intro_about", "brand_intro_about" },
	};
	return result;
}

QString Rename(QString text) {
	text.replace(u"Telegram Desktop"_q, QString::fromLatin1(kDesktopName));
	text.replace(u"Telegram"_q, QString::fromLatin1(kAppName));
	return text;
}

} // namespace MZGram::Branding
