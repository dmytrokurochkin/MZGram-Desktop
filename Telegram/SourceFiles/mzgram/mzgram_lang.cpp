/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_lang.h"

#include "base/options.h"
#include "lang/lang_instance.h"

#include <QtCore/QLocale>

namespace MZGram {

// The choice in Settings, else the system's language (System).
QString CurrentLanguage() {
	const auto chosen = LanguageChoice();
	if (!chosen.isEmpty()) {
		return chosen;
	}
	const auto languages = QLocale::system().uiLanguages();
	return SupportedLanguage(languages.isEmpty()
		? QLocale::system().name()
		: languages.front());
}

QString TrNow(const char *key) {
	return TranslateTo(key, CurrentLanguage());
}

// A new value each time the MZGram language or the app's language changes.
rpl::producer<QString> Tr(const char *key) {
	return rpl::single(
		rpl::empty
	) | rpl::then(rpl::merge(
		Lang::GetInstance().updated(),
		base::options::lookup<QString>(kOptionLanguage).changes()
	)) | rpl::map([=] {
		return TrNow(key);
	});
}

} // namespace MZGram
