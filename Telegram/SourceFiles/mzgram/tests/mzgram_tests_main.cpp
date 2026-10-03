/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "base/basic_types.h"
#include "mzgram/mzgram_lang.h"
#include "mzgram/mzgram_text_filters.h"

#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QString>

#include "base/options.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

int failures = 0;
int total = 0;

void Check(bool condition, const char *name, const char *detail) {
	++total;
	if (condition) {
		std::printf("PASS: %s\n", name);
	} else {
		++failures;
		std::printf("FAIL: %s (%s)\n", name, detail);
	}
}

void SetZalgoOption(bool enabled) {
	base::options::lookup<bool>(MZGram::kOptionStripZalgoText).set(enabled);
}

void TestStripDoesNothingWhenOff() {
	SetZalgoOption(false);
	const auto zalgo = QString::fromUtf8(
		"Z\xcc\xb6\xcc\xa1\xcd\x81""a\xcc\x80\xcc\x81l\xcc\xb6g\xcc\xb5o\xcc\x80");
	const auto result = MZGram::StripZalgo(zalgo);
	Check(
		result == zalgo,
		"StripZalgo leaves text unchanged when the option is off",
		result.toStdString().c_str());
}

void TestStripRemovesCombiningMarksWhenOn() {
	SetZalgoOption(true);
	const auto zalgo = QString::fromUtf8(
		"Z\xcc\xb6\xcc\xa1\xcd\x81""a\xcc\x80\xcc\x81l\xcc\xb6g\xcc\xb5o\xcc\x80");
	const auto result = MZGram::StripZalgo(zalgo);
	Check(
		result == QString("Zalgo"),
		"StripZalgo removes combining marks when the option is on",
		result.toStdString().c_str());
}

void TestStripLeavesPlainTextUnchanged() {
	SetZalgoOption(true);
	const auto plain = QString("Just a normal message, punctuation included!");
	const auto result = MZGram::StripZalgo(plain);
	Check(
		result == plain,
		"StripZalgo leaves plain text unchanged",
		result.toStdString().c_str());
}

void TestStripHandlesEmpty() {
	SetZalgoOption(true);
	const auto result = MZGram::StripZalgo(QString());
	Check(
		result.isEmpty(),
		"StripZalgo handles an empty string",
		result.toStdString().c_str());
}

// Every MZGram text is in English and Ukrainian, with the same %1, %2.
void TestEveryPhraseInBothLanguages() {
	// Read the same in both languages.
	const auto same = QSet<QString>{ u"settings_title"_q, u"details_id"_q };
	const auto placeholders = [](const QString &text) {
		auto result = QStringList();
		auto i = QRegularExpression(u"%[0-9]"_q).globalMatch(text);
		while (i.hasNext()) {
			result.push_back(i.next().captured());
		}
		result.sort();
		return result;
	};
	auto keys = QSet<QString>();
	auto problems = QStringList();
	for (const auto &phrase : MZGram::Phrases()) {
		const auto key = QString::fromUtf8(phrase.key);
		const auto english = QString::fromUtf8(phrase.english);
		const auto ukrainian = QString::fromUtf8(phrase.ukrainian);
		if (keys.contains(key)) {
			problems.push_back(key + u": twice"_q);
		}
		keys.insert(key);
		if (english.trimmed().isEmpty() || ukrainian.trimmed().isEmpty()) {
			problems.push_back(key + u": empty"_q);
		} else if (english == ukrainian && !same.contains(key)) {
			problems.push_back(key + u": not translated"_q);
		}
		if (placeholders(english) != placeholders(ukrainian)) {
			problems.push_back(key + u": placeholders differ"_q);
		}
	}
	std::printf("string table: %d phrases\n", int(keys.size()));
	Check(
		problems.isEmpty() && keys.size() > 100,
		"every MZGram phrase is in English and Ukrainian",
		problems.join(u", "_q).toStdString().c_str());
}

// The app's language picks the text.
void TestTranslatePicksTheLanguage() {
	Check(
		MZGram::Translate("section_archive", false) == u"Archive"_q
			&& MZGram::Translate("section_archive", true)
				== QString::fromUtf8("Архів"),
		"Translate returns English or Ukrainian",
		MZGram::Translate("section_archive", true).toStdString().c_str());
}

// Every key the sources ask for is in the table: MZGram::Tr("..."),
// MZGram::TrNow("..."), and the settings screen's Text("...") and switch
// titles.
void TestEveryUsedKeyIsInTheTable() {
	auto keys = QSet<QString>();
	for (const auto &phrase : MZGram::Phrases()) {
		keys.insert(QString::fromUtf8(phrase.key));
	}
	const auto patterns = {
		QRegularExpression(u"\\bTr(?:Now)?\\(\"([a-z0-9_]+)\"\\)"_q),
		QRegularExpression(u"\\bText\\(\"([a-z0-9_]+)\"\\)"_q),
		QRegularExpression(u"_q,\\s*\"([a-z0-9_]+)\",\\s*kOption"_q),
	};
	auto used = 0;
	auto missing = QStringList();
	auto files = QDirIterator(
		QString::fromUtf8(MZGRAM_SOURCE_DIR),
		{ u"*.cpp"_q },
		QDir::Files,
		QDirIterator::Subdirectories);
	while (files.hasNext()) {
		auto file = QFile(files.next());
		if (!file.open(QIODevice::ReadOnly)) {
			continue;
		}
		const auto text = QString::fromUtf8(file.readAll());
		if (!text.contains(u"mzgram/mzgram_lang.h"_q)) {
			continue;
		}
		for (const auto &pattern : patterns) {
			auto i = pattern.globalMatch(text);
			while (i.hasNext()) {
				const auto key = i.next().captured(1);
				++used;
				if (!keys.contains(key)) {
					missing.push_back(key);
				}
			}
		}
	}
	std::printf("keys used in the sources: %d\n", used);
	Check(
		missing.isEmpty() && used > 100,
		"every key the sources use is in the string table",
		missing.join(u", "_q).toStdString().c_str());
}

} // namespace

int main() {
	const auto tests = std::vector<std::function<void()>>{
		TestStripDoesNothingWhenOff,
		TestStripRemovesCombiningMarksWhenOn,
		TestStripLeavesPlainTextUnchanged,
		TestStripHandlesEmpty,
		TestEveryPhraseInBothLanguages,
		TestTranslatePicksTheLanguage,
		TestEveryUsedKeyIsInTheTable,
	};
	for (const auto &test : tests) {
		test();
	}
	std::printf("\n%d/%d passed\n", total - failures, total);
	return failures ? 1 : 0;
}
