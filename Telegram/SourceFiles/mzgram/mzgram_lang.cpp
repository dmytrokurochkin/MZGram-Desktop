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

// The choice in Settings: Telegram's language (the default), the system's
// or one of MZGram's languages.
QString CurrentLanguage() {
	const auto languages = QLocale::system().uiLanguages();
	const auto system = languages.isEmpty()
		? QLocale::system().name()
		: languages.front();
	// A custom language pack is a copy of an official one
	const auto &instance = Lang::GetInstance();
	const auto telegram = instance.id().startsWith('#')
		? instance.baseId()
		: instance.id();
	return ResolveLanguage(LanguageChoice(), telegram, system);
}

QString TrNow(const char *key) {
	return TranslateTo(key, CurrentLanguage());
}

QLocale CurrentLocale() {
	return QLocale(QString(CurrentLanguage()).replace('-', '_'));
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
