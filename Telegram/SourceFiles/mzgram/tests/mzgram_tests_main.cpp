/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_text_filters.h"

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

} // namespace

int main() {
	const auto tests = std::vector<std::function<void()>>{
		TestStripDoesNothingWhenOff,
		TestStripRemovesCombiningMarksWhenOn,
		TestStripLeavesPlainTextUnchanged,
		TestStripHandlesEmpty,
	};
	for (const auto &test : tests) {
		test();
	}
	std::printf("\n%d/%d passed\n", total - failures, total);
	return failures ? 1 : 0;
}
