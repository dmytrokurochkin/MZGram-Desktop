/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_lang.h"

#include "lang/lang_instance.h"

namespace MZGram {

// The app's language: its own id, or the one a custom language pack is
// based on.
bool UkrainianInterface() {
	const auto &instance = Lang::GetInstance();
	const auto isUkrainian = [](const QString &id) {
		return (id == u"uk"_q) || id.startsWith(u"uk-"_q);
	};
	return isUkrainian(instance.id()) || isUkrainian(instance.baseId());
}

QString TrNow(const char *key) {
	return Translate(key, UkrainianInterface());
}

// Follows the app's language: a new value each time the language changes.
rpl::producer<QString> Tr(const char *key) {
	return rpl::single(
		rpl::empty
	) | rpl::then(
		Lang::GetInstance().updated()
	) | rpl::map([=] {
		return TrNow(key);
	});
}

} // namespace MZGram
