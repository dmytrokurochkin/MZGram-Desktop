/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>
#include <rpl/producer.h>

#include <vector>

namespace MZGram {

// MZGram's own strings, in English and Ukrainian. They are kept out of
// lang.strings: that file generates a header nearly every source includes,
// so each new key would rebuild the whole client, and the Ukrainian cloud
// language pack would not have MZGram's keys anyway.

struct Phrase {
	const char *key = nullptr;
	const char *english = nullptr;
	const char *ukrainian = nullptr;
};

// mzgram_lang_table.cpp: the table itself, no app dependencies.
[[nodiscard]] const std::vector<Phrase> &Phrases();
[[nodiscard]] QString Translate(const char *key, bool ukrainian);

// mzgram_lang.cpp: in the app's current language.
[[nodiscard]] bool UkrainianInterface();
[[nodiscard]] QString TrNow(const char *key);
[[nodiscard]] rpl::producer<QString> Tr(const char *key);

} // namespace MZGram
