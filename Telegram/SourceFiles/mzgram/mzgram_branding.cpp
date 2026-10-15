/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_branding.h"

#include "core/version.h"
#include "lang/lang_instance.h"
#include "lang_auto_counts.h" // kKeysCount.
#include "mzgram/mzgram_lang.h"
#include "mzgram/mzgram_update.h"

namespace MZGram::Branding {

QString CurrentVersionText() {
	const auto text = VersionText(MZGram::Update::InstalledCode());
	return text.isEmpty()
		? (u"Telegram Desktop "_q + BaseVersionText())
		: text;
}

QString BaseVersionText() {
	return QString::fromLatin1(AppVersionStr);
}

} // namespace MZGram::Branding

namespace Lang {

QString MZGramBrandedValue(
		const Instance &instance,
		ushort key,
		const QString &value) {
	// -1: as is, -2: the app's name renamed, 0..: replaced by that phrase
	static const auto marks = [] {
		auto result = std::vector<int>(kKeysCount, -1);
		for (const auto name : MZGram::Branding::RenamedKeys()) {
			const auto index = GetKeyIndex(QLatin1String(name));
			if (index < kKeysCount) {
				result[index] = -2;
			}
		}
		const auto &replaced = MZGram::Branding::ReplacedKeys();
		for (auto i = 0; i != int(replaced.size()); ++i) {
			const auto index = GetKeyIndex(QLatin1String(replaced[i].langKey));
			if (index < kKeysCount) {
				result[index] = i;
			}
		}
		return result;
	}();
	const auto mark = (key < marks.size()) ? marks[key] : -1;
	if (mark == -1) {
		return value;
	} else if (mark == -2) {
		return MZGram::Branding::Rename(value);
	}
	const auto isUkrainian = [](const QString &id) {
		return (id == u"uk"_q) || id.startsWith(u"uk-"_q);
	};
	const auto ukrainian = isUkrainian(instance.id())
		|| isUkrainian(instance.baseId());
	return MZGram::Translate(
		MZGram::Branding::ReplacedKeys()[mark].phrase,
		ukrainian);
}

} // namespace Lang
