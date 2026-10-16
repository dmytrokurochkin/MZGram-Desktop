/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>
#include <rpl/producer.h>

#include <array>
#include <vector>

namespace MZGram {

// MZGram's own strings, in its twelve languages. They are kept out of
// lang.strings: that file generates a header nearly every source includes,
// so each new key would rebuild the whole client, and Telegram's cloud
// language packs would not have MZGram's keys anyway.
//
// Their language is chosen in Settings > MZGram > Interface, apart from
// Telegram's language: System (the system's language when MZGram has it,
// English otherwise) or one of kLanguages.

struct Phrase {
	const char *key = nullptr;
	const char *english = nullptr;
	const char *ukrainian = nullptr;
};

// A key's text in one of the other languages.
struct Translation {
	const char *key = nullptr;
	const char *text = nullptr;
};

inline constexpr auto kLanguageCount = 12;
// Codes, in the order the picker lists them.
inline constexpr std::array<const char*, kLanguageCount> kLanguages = {
	"en", "uk", "ru", "pl", "es", "de", "fr", "pt-BR", "tr", "ar", "be", "kk",
};
// Each language's own name; not translated.
inline constexpr std::array<const char*, kLanguageCount> kLanguageNames = {
	"English",
	"Українська",
	"Русский",
	"Polski",
	"Español",
	"Deutsch",
	"Français",
	"Português (Brasil)",
	"Türkçe",
	"العربية",
	"Беларуская",
	"Қазақша",
};

// mzgram_lang_table.cpp: English and Ukrainian, no app dependencies.
[[nodiscard]] const std::vector<Phrase> &Phrases();
[[nodiscard]] QString Translate(const char *key, bool ukrainian);

// mzgram_lang_translations.cpp: the other languages, by code; empty for
// English, Ukrainian and an unknown code.
[[nodiscard]] const std::vector<Translation> &Translations(const QString &language);

// A key's text in a language; English when the language or the key's
// text in it is missing.
[[nodiscard]] QString TranslateTo(const char *key, const QString &language);

// The MZGram language for a system locale name ("de_AT", "pt-PT", "kk"):
// one MZGram has, else "en". Portuguese is Brazilian.
[[nodiscard]] QString SupportedLanguage(const QString &localeName);

// The choice in Settings: empty for System, else a code from kLanguages.
extern const char kOptionLanguage[];
[[nodiscard]] QString LanguageChoice();

// mzgram_lang.cpp: in the MZGram language now.
[[nodiscard]] QString CurrentLanguage();
[[nodiscard]] QString TrNow(const char *key);
[[nodiscard]] rpl::producer<QString> Tr(const char *key);

} // namespace MZGram
