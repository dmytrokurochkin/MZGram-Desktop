/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "base/basic_types.h"
#include "mzgram/mzgram_archive_rules.h"
#include "mzgram/mzgram_lang.h"
#include "mzgram/mzgram_protected_content.h"
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
		problems.isEmpty() && keys.size() > 50,
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
		missing.isEmpty() && used > 50,
		"every key the sources use is in the string table",
		missing.join(u", "_q).toStdString().c_str());
}


// Settings > MZGram > Message menu > "Forward and save protected content".

void SetProtectedContentOption(bool enabled) {
	base::options::lookup<bool>(
		MZGram::kOptionSaveProtectedContent).set(enabled);
}

// The app's checks keep the restriction with the switch off and lift it
// with the switch on; chats without it are never restricted.
void TestProtectedContentFollowsTheSwitch() {
	SetProtectedContentOption(false);
	const auto off = !MZGram::AllowsForwarding(false)
		&& MZGram::ForbidsForward(true)
		&& !MZGram::SendAsCopy(true);
	SetProtectedContentOption(true);
	const auto on = MZGram::AllowsForwarding(false)
		&& !MZGram::ForbidsForward(true)
		&& MZGram::SendAsCopy(true);
	const auto open = MZGram::AllowsForwarding(true)
		&& !MZGram::ForbidsForward(false)
		&& !MZGram::SendAsCopy(false);
	SetProtectedContentOption(false);
	const auto openOff = MZGram::AllowsForwarding(true)
		&& !MZGram::ForbidsForward(false)
		&& !MZGram::SendAsCopy(false);
	Check(
		off && on && open && openOff,
		"protected content checks follow the switch",
		!off
			? "switch off does not restrict"
			: !on
			? "switch on still restricts"
			: "unprotected chat restricted");
}

// Forwarding a protected message sends a copy: its text, or its file
// uploaded again with the text as caption; a file not downloaded yet is
// downloaded first.
void TestProtectedCopyPlan() {
	using namespace MZGram;
	using Media = CopyMedia;
	using Kind = CopyKind;
	struct Case {
		const char *what;
		CopySource source;
		CopyPlan plan;
	};
	const auto cases = std::vector<Case>{
		{ "text",
			{ .media = Media::None, .hasText = true },
			{ .kind = Kind::Text } },
		{ "empty",
			{ .media = Media::None },
			{ .kind = Kind::Skip } },
		{ "photo with caption",
			{ .media = Media::Photo, .hasText = true, .fileOnDisk = true },
			{ .kind = Kind::Photo, .caption = true } },
		{ "photo not downloaded",
			{ .media = Media::Photo },
			{ .kind = Kind::Photo, .download = true } },
		{ "photo, captions dropped",
			{
				.media = Media::Photo,
				.hasText = true,
				.fileOnDisk = true,
				.dropCaption = true,
			},
			{ .kind = Kind::Photo } },
		{ "voice",
			{ .media = Media::Voice, .hasText = true, .fileOnDisk = true },
			{ .kind = Kind::Voice } },
		{ "round not downloaded",
			{ .media = Media::Round },
			{ .kind = Kind::Round, .download = true } },
		{ "file not downloaded",
			{ .media = Media::File, .hasText = true },
			{ .kind = Kind::File, .download = true, .caption = true } },
		{ "poll with text",
			{ .media = Media::Other, .hasText = true },
			{ .kind = Kind::Text } },
		{ "poll",
			{ .media = Media::Other },
			{ .kind = Kind::Skip } },
	};
	auto problems = QStringList();
	for (const auto &[what, source, plan] : cases) {
		const auto result = PlanCopy(source);
		if (result.kind != plan.kind
			|| result.download != plan.download
			|| result.caption != plan.caption) {
			problems.push_back(QString::fromUtf8(what));
		}
	}
	Check(
		problems.isEmpty(),
		"protected messages are copied with their text and file",
		problems.join(u", "_q).toStdString().c_str());
}

// The app's checks and forwarding go through the switch.
void TestProtectedContentIsWired() {
	const auto read = [](const char *path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll())
			: QString();
	};
	const auto peer = u"MZGram::AllowsForwarding(allowsForwardingOnServer())"_q;
	const auto wired = std::vector<std::pair<const char*, QString>>{
		{ "data/data_channel.cpp", peer },
		{ "data/data_chat.cpp", peer },
		{ "data/data_user.cpp", peer },
		{
			"history/history_item.cpp",
			u"MZGram::ForbidsForward(forbidsForwardOnServer())"_q,
		},
		{
			"data/data_peer_values.cpp",
			u"rpl::map(MZGram::AllowsForwarding)"_q,
		},
		{ "mzgram/mzgram_settings.cpp", u"kOptionSaveProtectedContent"_q },
	};
	auto missing = QStringList();
	for (const auto &[path, text] : wired) {
		if (!read(path).contains(text)) {
			missing.push_back(QString::fromUtf8(path));
		}
	}
	// Copies are sent before forwardMessages forwards the rest.
	const auto api = read("apiwrap.cpp");
	const auto start = api.indexOf(u"void ApiWrap::forwardMessages("_q);
	const auto copies = api.indexOf(
		u"MZGram::SendProtectedCopies(this, draft, action);"_q,
		start);
	const auto forward = api.indexOf(u"CollectForwardRanges("_q, start);
	if (start < 0 || copies < 0 || forward < 0 || copies > forward) {
		missing.push_back(u"apiwrap.cpp"_q);
	}
	Check(
		missing.isEmpty(),
		"protected content checks and forwarding use the switch",
		missing.join(u", "_q).toStdString().c_str());
}


// Settings > MZGram > Archive: a switch for each part, all on by default.

// Other people's messages are kept in every chat, the owner's own and
// service messages never are; deleted messages and edits each follow their
// own switch, and chats with bots follow theirs.
void TestArchiveRules() {
	using namespace MZGram;
	auto &deleted = base::options::lookup<bool>(kOptionSaveDeletedMessages);
	auto &edits = base::options::lookup<bool>(kOptionSaveEditHistory);
	auto &bots = base::options::lookup<bool>(kOptionSaveForBots);
	auto problems = QStringList();
	for (const auto id : {
			kOptionSaveDeletedMessages,
			kOptionSaveEditHistory,
			kOptionSaveArchiveMedia,
			kOptionSaveFormatting,
			kOptionSaveReactions,
			kOptionSaveForBots,
			kOptionSemiTransparentDeleted,
		}) {
		if (!base::options::lookup<bool>(id).value()) {
			problems.push_back(QString::fromUtf8(id) + u" off by default"_q);
		}
	}
	if (DeletedMark() != QString::fromUtf8("\xf0\x9f\xa7\xb9")
		|| EditedMark() != QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f")) {
		problems.push_back(u"marks are not the broom and the pencil"_q);
	}
	if (kDeletedOpacity != 0.75) {
		problems.push_back(u"deleted opacity is not 75%"_q);
	}
	if (!KeepsDeleted(false, false, false) || !KeepsEdit(false, false, false)) {
		problems.push_back(u"other people's message not kept"_q);
	}
	if (KeepsDeleted(false, true, false) || KeepsEdit(false, true, false)) {
		problems.push_back(u"own message kept"_q);
	}
	if (KeepsDeleted(true, false, false) || KeepsEdit(true, false, false)) {
		problems.push_back(u"service message kept"_q);
	}
	if (!KeepsDeleted(false, false, true)) {
		problems.push_back(u"bot chat not kept by default"_q);
	}
	bots.set(false);
	if (KeepsDeleted(false, false, true) || KeepsEdit(false, false, true)) {
		problems.push_back(u"bot chat kept with its switch off"_q);
	}
	if (!KeepsDeleted(false, false, false)) {
		problems.push_back(u"bot switch affects other chats"_q);
	}
	bots.set(true);
	edits.set(false);
	if (KeepsEdit(false, false, false) || !KeepsDeleted(false, false, false)) {
		problems.push_back(u"edit history switch not on its own"_q);
	}
	edits.set(true);
	deleted.set(false);
	if (KeepsDeleted(false, false, false) || !KeepsEdit(false, false, false)) {
		problems.push_back(u"deleted switch not on its own"_q);
	}
	deleted.set(true);
	Check(
		problems.isEmpty(),
		"archive keeps other people's messages, a switch for each part",
		problems.join(u", "_q).toStdString().c_str());
}

// No list of chats any more: nothing in the sources picks chats, and the
// archive asks the rules above.
void TestArchiveIsForEveryChat() {
	const auto read = [](const char *path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll())
			: QString();
	};
	auto problems = QStringList();
	const auto gone = {
		u"IsTracked("_q,
		u"SetTracked("_q,
		u"trackedPeers("_q,
		u"addMZGramKeepMessages"_q,
		u"\"tracked_chats\""_q,
	};
	for (const auto path : {
			"mzgram/mzgram_archive.cpp",
			"mzgram/mzgram_anti_recall.cpp",
			"mzgram/mzgram_settings.cpp",
			"mzgram/mzgram_lang_table.cpp",
			"window/window_peer_menu.cpp",
			"history/history_item.cpp",
		}) {
		const auto text = read(path);
		if (text.isEmpty()) {
			problems.push_back(QString::fromUtf8(path) + u": not read"_q);
		}
		for (const auto &word : gone) {
			if (text.contains(word)) {
				problems.push_back(QString::fromUtf8(path) + u": "_q + word);
			}
		}
	}
	const auto antiRecall = read("mzgram/mzgram_anti_recall.cpp");
	if (!antiRecall.contains(
			u"KeepsDeleted(item->isService(), item->out(), IsBotChat(item))"_q)
		|| !antiRecall.contains(
			u"KeepsEdit(item->isService(), item->out(), IsBotChat(item))"_q)) {
		problems.push_back(u"anti-recall does not ask the rules"_q);
	}
	const auto archive = read("mzgram/mzgram_archive.cpp");
	if (!archive.contains(u"message.c_message().is_out(),"_q)
		|| !archive.contains(u"KeepsDeleted("_q)
		|| !archive.contains(u"Serialize(WithoutSwitchedOffParts(message))"_q)
		|| !archive.contains(u"if (!SaveArchiveMedia()) {"_q)) {
		problems.push_back(u"capture does not ask the rules"_q);
	}
	const auto settings = read("mzgram/mzgram_settings.cpp");
	for (const auto option : {
			u"kOptionSaveDeletedMessages"_q,
			u"kOptionSaveEditHistory"_q,
			u"kOptionSaveArchiveMedia"_q,
			u"kOptionSaveFormatting"_q,
			u"kOptionSaveReactions"_q,
			u"kOptionSaveForBots"_q,
			u"kOptionSemiTransparentDeleted"_q,
			u"kOptionDeletedMark"_q,
			u"kOptionEditedMark"_q,
		}) {
		if (!settings.contains(option)) {
			problems.push_back(u"no switch in settings: "_q + option);
		}
	}
	const auto store = read("mzgram/mzgram_message_store.cpp");
	if (!store.contains(u"/MZGram/Saved Attachments/"_q)
		|| !store.contains(u".nomedia"_q)) {
		problems.push_back(u"no Saved Attachments folder"_q);
	}
	if (!read("history/view/history_view_bottom_info.cpp").contains(
			u"MZGram::DeletedMark()"_q)) {
		problems.push_back(u"deleted mark not drawn from the setting"_q);
	}
	Check(
		problems.isEmpty(),
		"archive works in every chat, with no chat list",
		problems.join(u", "_q).toStdString().c_str());
}


// Saved media has no size limit and no total quota, and nothing deletes
// saved files on its own: no limit in the capture, no quota or eviction
// in the store, no setting or text for either.
void TestArchiveMediaHasNoLimit() {
	const auto read = [](const char *path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll())
			: QString();
	};
	auto problems = QStringList();
	const auto gone = {
		u"mediaSizeLimit"_q,
		u"MediaSizeLimit"_q,
		u"totalMediaCap"_q,
		u"TotalMediaCap"_q,
		u"enforceMediaCap"_q,
		u"\"media_size_limit\""_q,
		u"\"total_media_cap\""_q,
		u"document->size >"_q,
	};
	for (const auto path : {
			"mzgram/mzgram_archive.cpp",
			"mzgram/mzgram_message_store.cpp",
			"mzgram/mzgram_message_store.h",
			"mzgram/mzgram_settings.cpp",
			"mzgram/mzgram_lang_table.cpp",
		}) {
		const auto text = read(path);
		if (text.isEmpty()) {
			problems.push_back(QString::fromUtf8(path) + u": not read"_q);
		}
		for (const auto &word : gone) {
			if (text.contains(word)) {
				problems.push_back(QString::fromUtf8(path) + u": "_q + word);
			}
		}
	}
	const auto store = read("mzgram/mzgram_message_store.cpp");
	if (!store.contains(
			u"WHERE key IN ('media_size_limit', 'total_media_cap')"_q)) {
		problems.push_back(u"old limits are not dropped"_q);
	}
	Check(
		problems.isEmpty(),
		"archive media has no size limit and no quota",
		problems.join(u", "_q).toStdString().c_str());
}


// Settings > MZGram > Archive > "Clear Telegram local database": clears
// Telegram's cache and restarts, never the MZGram archive.
void TestEraseLocalDatabase() {
	auto file = QFile(
		QString::fromUtf8(MZGRAM_SOURCE_DIR) + u"/mzgram/mzgram_settings.cpp"_q);
	const auto text = file.open(QIODevice::ReadOnly)
		? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
		: QString();
	const auto start = text.indexOf(u"u\"mzgram/erase-local-database\"_q"_q);
	const auto end = text.indexOf(u"});"_q, start);
	const auto body = (start < 0) ? QString() : text.mid(start, end - start);
	auto problems = QStringList();
	if (body.isEmpty()) {
		problems.push_back(u"no button"_q);
	}
	for (const auto &needed : {
			u"data().cache().clear()"_q,
			u"data().cacheBigFile().clear()"_q,
			u"Core::Restart()"_q,
			u"erase_local_database_confirm"_q,
		}) {
		if (!body.contains(needed)) {
			problems.push_back(u"missing "_q + needed);
		}
	}
	if (body.contains(u"wipeAll"_q)) {
		problems.push_back(u"clears the archive"_q);
	}
	Check(
		problems.isEmpty(),
		"clearing Telegram's local database keeps the archive",
		problems.join(u", "_q).toStdString().c_str());
}


// Hiding the own online status is left to Telegram's own privacy settings:
// no such switch, and no Privacy topic left for it.
void TestNoPrivacyDuplicates() {
	auto problems = QStringList();
	for (const auto path : {
			"mzgram/mzgram_options.cpp",
			"mzgram/mzgram_options.h",
			"mzgram/mzgram_settings.cpp",
			"mzgram/mzgram_lang_table.cpp",
		}) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		const auto text = file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll())
			: QString();
		if (text.isEmpty()) {
			problems.push_back(QString::fromUtf8(path) + u": not read"_q);
		}
		for (const auto &word : {
				u"SpyHideOnlineStatus"_q,
				u"\"hide_own_online"_q,
				u"BuildPrivacy"_q,
			}) {
			if (text.contains(word)) {
				problems.push_back(QString::fromUtf8(path) + u": "_q + word);
			}
		}
	}
	Check(
		problems.isEmpty(),
		"no switches that copy Telegram's privacy settings",
		problems.join(u", "_q).toStdString().c_str());
}


// Settings > MZGram lists the topics; each opens its own page with that
// topic's switches. No switch is on the list page itself, and each topic
// page builder is in the topic table.
void TestSettingsAreTopicPages() {
	auto file = QFile(
		QString::fromUtf8(MZGRAM_SOURCE_DIR) + u"/mzgram/mzgram_settings.cpp"_q);
	// A Windows checkout has CRLF line ends.
	const auto text = file.open(QIODevice::ReadOnly)
		? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
		: QString();
	auto problems = QStringList();
	const auto topics = {
		u"Archive"_q,
		u"GhostMode"_q,
		u"MessageMenu"_q,
		u"MediaCalls"_q,
		u"Interface"_q,
		u"AdsFilters"_q,
	};
	auto toggles = 0;
	for (const auto &topic : topics) {
		const auto name = u"void Build"_q + topic + u"(SectionBuilder &builder) {"_q;
		const auto start = text.indexOf(name);
		if (start < 0) {
			problems.push_back(topic + u": no page"_q);
			continue;
		}
		const auto end = text.indexOf(u"\n}\n"_q, start);
		const auto body = text.mid(start, end - start);
		const auto count = int(body.count(u"AddOptionToggle("_q))
			+ int(body.count(u"addButton({"_q));
		if (!count) {
			problems.push_back(topic + u": empty page"_q);
		}
		toggles += count;
		if (!text.contains(u", Build"_q + topic + u" },"_q)) {
			problems.push_back(topic + u": not in the topic table"_q);
		}
	}
	const auto mainStart = text.indexOf(
		u"void BuildMZGramSection(SectionBuilder &builder) {"_q);
	const auto mainBody = text.mid(
		mainStart,
		text.indexOf(u"\n}\n"_q, mainStart) - mainStart);
	if (mainStart < 0
		|| mainBody.contains(u"AddOptionToggle("_q)
		|| !mainBody.contains(u"addSectionButton("_q)) {
		problems.push_back(u"list page"_q);
	}
	std::printf("settings topic pages: %d switches and buttons\n", toggles);
	Check(
		problems.isEmpty() && toggles == 38,
		"settings are a list of topic pages, every switch on one",
		problems.join(u", "_q).toStdString().c_str());
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
		TestProtectedContentFollowsTheSwitch,
		TestProtectedCopyPlan,
		TestProtectedContentIsWired,
		TestArchiveRules,
		TestArchiveIsForEveryChat,
		TestArchiveMediaHasNoLimit,
		TestEraseLocalDatabase,
		TestNoPrivacyDuplicates,
		TestSettingsAreTopicPages,
	};
	for (const auto &test : tests) {
		test();
	}
	std::printf("\n%d/%d passed\n", total - failures, total);
	return failures ? 1 : 0;
}
