/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "base/basic_types.h"
#include "mzgram/mzgram_ai_rules.h"
#include "mzgram/mzgram_archive_rules.h"
#include "mzgram/mzgram_branding_rules.h"
#include "mzgram/mzgram_ipc_rules.h"
#include "mzgram/mzgram_lang.h"
#include "mzgram/mzgram_local_pins_rules.h"
#include "mzgram/mzgram_presence_rules.h"
#include "mzgram/mzgram_protected_content.h"
#include "mzgram/mzgram_self_update_rules.h"
#include "mzgram/mzgram_text_filters.h"
#include "mzgram/mzgram_update_install.h"
#include "mzgram/mzgram_update_rules.h"
#include "mzgram/mzgram_url_scheme.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QTemporaryDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QUrl>

#include "base/options.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

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
	const auto cyrillic = QRegularExpression(u"[\\x{0400}-\\x{04FF}]"_q);
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
		// Never part English, part Ukrainian
		if (english.contains(cyrillic)) {
			problems.push_back(key + u": Cyrillic in English"_q);
		}
		if (!same.contains(key) && !ukrainian.contains(cyrillic)) {
			problems.push_back(key + u": not Ukrainian"_q);
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


// A hidden last seen shows the time this device last saw the person online
// only while it fits what the server says; a message's read time is the
// earliest read event covering it.
void TestPresenceRules() {
	using namespace MZGram;
	const auto now = TimeId(1'800'000'000);
	const auto day = TimeId(24 * 60 * 60);
	auto problems = QStringList();
	if (!ApproximateFits(HiddenLastSeen::Recently, now - 3600, now)) {
		problems.push_back(u"an hour ago does not fit recently"_q);
	}
	if (ApproximateFits(HiddenLastSeen::Recently, now - 4 * day, now)) {
		problems.push_back(u"four days ago fits recently"_q);
	}
	if (!ApproximateFits(HiddenLastSeen::WithinWeek, now - 6 * day, now)
		|| !ApproximateFits(HiddenLastSeen::WithinMonth, now - 20 * day, now)) {
		problems.push_back(u"week or month bounds"_q);
	}
	if (ApproximateFits(HiddenLastSeen::None, now - 60, now)
		|| ApproximateFits(HiddenLastSeen::Recently, 0, now)) {
		problems.push_back(u"shown with nothing hidden or nothing seen"_q);
	}
	const auto reads = std::vector<OutboxRead>{
		{ .maxId = 100, .readAt = now - 300 },
		{ .maxId = 200, .readAt = now - 100 },
	};
	if (FirstReadAt(reads, 90) != now - 300
		|| FirstReadAt(reads, 150) != now - 100
		|| FirstReadAt(reads, 250) != 0) {
		problems.push_back(u"read time of a message"_q);
	}
	Check(
		problems.isEmpty(),
		"approximate last seen and read times follow the rules",
		problems.join(u", "_q).toStdString().c_str());
}

// Where the signs and read events come from, and where they are shown.
void TestPresenceIsWired() {
	const auto read = [](const char *path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll())
			: QString();
	};
	const auto wired = std::vector<std::pair<const char*, QString>>{
		{ "data/data_user.cpp", u"MZGram::RecordSeen(this, when);"_q },
		{ "data/data_peer_values.cpp", u"MZGram::ApproximateOnlineText(user, now)"_q },
		{ "api/api_updates.cpp", u"MZGram::RecordOutboxRead(history, d.vmax_id().v);"_q },
		{ "api/api_updates.cpp", u"MZGram::RecordSeen(user, base::unixtime::now());"_q },
		{ "mzgram/mzgram_context_actions.cpp", u"MTPmessages_GetOutboxReadDate("_q },
		{ "mzgram/mzgram_context_actions.cpp", u"MZGram::KeptReadTime(item)"_q },
	};
	auto missing = QStringList();
	for (const auto &[path, text] : wired) {
		if (!read(path).contains(text)) {
			missing.push_back(QString::fromUtf8(path) + u": "_q + text);
		}
	}
	if (read("api/api_updates.cpp").count(u"MZGram::RecordOutboxRead("_q) < 2) {
		missing.push_back(u"channel read events"_q);
	}
	Check(
		missing.isEmpty(),
		"read times and last seen signs are recorded and shown",
		missing.join(u", "_q).toStdString().c_str());
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
		u"Ai"_q,
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
		problems.isEmpty() && toggles == 44,
		"settings are a list of topic pages, every switch on one",
		problems.join(u", "_q).toStdString().c_str());
}

// The MZGram update check (mzgram_update_rules.h).
QByteArray ReleaseJson(
		const char *tag,
		bool draft = false,
		bool prerelease = false,
		const char *url = nullptr) {
	const auto page = url
		? QByteArray(url)
		: ("https://github.com/dmytrokurochkin/MZGram-Desktop/releases/tag/"
			+ QByteArray(tag));
	return "{\"tag_name\":\"" + QByteArray(tag)
		+ "\",\"draft\":" + (draft ? "true" : "false")
		+ ",\"prerelease\":" + (prerelease ? "true" : "false")
		+ ",\"html_url\":\"" + page + "\"}";
}

void TestUpdateVersions() {
	using namespace MZGram::Update;
	Check(StableCode(u"v1.0.0"_q) == 1000099, "update: v1.0.0 code", "");
	Check(StableCode(u"v1.2.3"_q) == 1020399, "update: v1.2.3 code", "");
	Check(
		StableCode(u"v1.10.0"_q) > StableCode(u"v1.9.99"_q),
		"update: 1.10.0 is newer than 1.9.99",
		"");
	Check(StableCode(u"v1.1.0-beta.1"_q) == -1, "update: a beta is not offered", "");
	for (const auto tag : { "1.0.0", "v1.0", "v1.100.0", "v01.0.0", "", "v7.2.8x" }) {
		Check(StableCode(QString::fromLatin1(tag)) == -1, "update: not a release tag", tag);
	}
}

void TestUpdateParse() {
	using namespace MZGram::Update;
	const auto stable = ParseLatest(ReleaseJson("v1.0.1"));
	Check(stable && stable->tag == u"v1.0.1"_q, "update: stable release parsed", "");
	Check(
		stable && stable->url == u"https://github.com/dmytrokurochkin/MZGram-Desktop/releases/tag/v1.0.1"_q,
		"update: release page link",
		"");
	Check(!ParseLatest(ReleaseJson("v2.0.0", true)), "update: a draft is ignored", "");
	Check(!ParseLatest(ReleaseJson("v2.0.0", false, true)), "update: a prerelease is ignored", "");
	Check(!ParseLatest(ReleaseJson("v2.0.0-beta.1")), "update: a beta tag is ignored", "");
	Check(!ParseLatest("not json"), "update: a broken answer is ignored", "");
	const auto other = ParseLatest(ReleaseJson("v2.0.0", false, false, "https://example.com/x"));
	Check(
		other && other->url.startsWith(u"https://github.com/dmytrokurochkin/MZGram-Desktop/releases/"_q),
		"update: only the MZGram releases page is opened",
		"");
}

void TestUpdateOffer() {
	using namespace MZGram::Update;
	const auto installed = StableCode(u"v1.0.0"_q);
	auto state = State();
	Apply(state, 200, ReleaseJson("v1.0.1"), u"\"e1\""_q);
	Check(state.etag == u"\"e1\""_q, "update: the ETag is kept", "");
	const auto offered = Offer(state, installed, true, false);
	Check(offered && offered->version() == u"1.0.1"_q, "update: a newer release is offered", "");
	Check(!Offer(state, installed, false, false), "update: nothing with the switch off", "");
	Check(!Offer(state, 0, true, false), "update: nothing in a build without a version", "");
	Check(!Offer(state, StableCode(u"v1.0.1"_q), true, false), "update: not the installed version", "");
	Check(!Offer(state, StableCode(u"v1.1.0"_q), true, false), "update: not an older version", "");

	state.dismissedTag = u"v1.0.1"_q;
	Check(!Offer(state, installed, true, false), "update: a closed version stays closed", "");
	Check(Offer(state, installed, true, true).has_value(), "update: the user's own check shows it", "");
	Apply(state, 200, ReleaseJson("v1.0.2"), u"\"e2\""_q);
	Check(Offer(state, installed, true, false).has_value(), "update: a newer one after closing is offered", "");

	Apply(state, 304, QByteArray(), u"\"e2\""_q);
	Check(state.latestTag == u"v1.0.2"_q, "update: 304 keeps the saved release", "");
	Apply(state, 500, QByteArray(), QString());
	Check(state.latestTag == u"v1.0.2"_q, "update: an error keeps the saved release", "");
	Apply(state, 200, ReleaseJson("v1.0.3", false, true), u"\"e3\""_q);
	Check(!Offer(state, installed, true, false), "update: a prerelease answer offers nothing", "");
	Apply(state, 404, QByteArray(), QString());
	Check(state.latestTag.isEmpty() && state.etag.isEmpty(), "update: no release yet clears it", "");
	Check(!Offer(state, installed, true, false), "update: nothing without a release", "");
}

// Pinned chats in MZGram only (mzgram_local_pins_rules.h).
void TestLocalPinsRule() {
	using namespace MZGram::LocalPins;
	Check(!PinsLocally(4, 0, 5), "local pins: below the server limit the server pins", "");
	Check(PinsLocally(5, 0, 5), "local pins: at the server limit MZGram pins", "");
	Check(PinsLocally(5, 4, 5), "local pins: up to ten in all", "");
	Check(!PinsLocally(5, 5, 5), "local pins: not past ten in all", "");
	Check(!PinsLocally(10, 0, 10), "local pins: not past ten with a higher limit", "");
}

void TestLocalPinsSortPosition() {
	using namespace MZGram::LocalPins;
	// Dialogs::PinnedDialogPos(index) = 0xFFFFFFFF000000FF - index, server
	// lists hold at most 100 pins; a date key is (date << 32) | counter.
	const auto lowestServerPin = 0xFFFFFFFF000000FFULL - 100;
	const auto newestDate = (quint64(0x7FFFFFFF) << 32) | 0x7FFFFFFFULL;
	const auto unreadOnTop = (quint64(2000000000) << 32 | 0xFFFFFFULL) | 0x8000000000000000ULL;
	Check(SortPosition(0) < lowestServerPin, "local pins: after every server pin", "");
	Check(SortPosition(kMaxTotal) > newestDate, "local pins: before every chat by date", "");
	Check(SortPosition(kMaxTotal) > unreadOnTop, "local pins: before unread on top", "");
	Check(SortPosition(0) > SortPosition(1), "local pins: in pin order", "");
}

void TestLocalPinsStorage() {
	using namespace MZGram::LocalPins;
	auto stored = Stored();
	stored[u"123_0"_q] = { 9000000001ULL, 0x0001000000000002ULL };
	stored[u"123_1"_q] = { 77ULL };
	stored[u"456_0"_q] = {};
	const auto parsed = Parse(Serialize(stored));
	Check(parsed.size() == 2, "local pins: empty lists are not stored", "");
	Check(
		parsed.count(u"123_0"_q)
			&& parsed.at(u"123_0"_q) == std::vector<quint64>{ 9000000001ULL, 0x0001000000000002ULL },
		"local pins: ids and their order kept (64-bit peer ids)",
		"");
	Check(parsed.count(u"123_1"_q) && parsed.at(u"123_1"_q).size() == 1, "local pins: the archive kept apart", "");
	Check(Parse("not json").empty(), "local pins: a broken file reads as none", "");
}

// The chat list sorts, pins and paints the local pins, with no request.
void TestLocalPinsAreWired() {
	const auto read = [](const QString &path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
			: QString();
	};
	auto problems = QStringList();
	if (!read(u"dialogs/dialogs_entry.cpp"_q).contains(u"MZGram::LocalPins::SortPosition(local)"_q)) {
		problems.push_back(u"not sorted"_q);
	}
	const auto menu = read(u"window/window_peer_menu.cpp"_q);
	const auto toggle = menu.indexOf(u"MZGram::LocalPins::WouldPinLocally(history)"_q);
	const auto request = menu.indexOf(u"MTPmessages_ToggleDialogPin("_q);
	if (toggle < 0 || request < 0 || toggle > request || !menu.contains(u"MZGram::LocalPins::Pin(history);"_q)) {
		problems.push_back(u"not pinned before the server request"_q);
	}
	if (!menu.contains(u"\"pin_locally\""_q) || !menu.contains(u"\"unpin_locally\""_q)) {
		problems.push_back(u"no menu labels"_q);
	}
	if (!read(u"dialogs/ui/dialogs_layout.cpp"_q).contains(u"PaintingLocalPin"_q)) {
		problems.push_back(u"no mark in the chat list"_q);
	}
	Check(
		problems.isEmpty(),
		"local pins: sorted, pinned without a request, marked",
		problems.join(u", "_q).toStdString().c_str());
}

// MZGram's own name (mzgram_branding_rules.h).
void TestBrandingRules() {
	using namespace MZGram::Branding;
	Check(VersionText(1000099) == u"1.0.0"_q, "branding: a stable version", VersionText(1000099).toStdString().c_str());
	Check(VersionText(1010002) == u"1.1.0 beta 2"_q, "branding: a beta version", VersionText(1010002).toStdString().c_str());
	Check(VersionText(0).isEmpty(), "branding: no version in a local build", "");
	Check(Rename(u"Quit Telegram"_q) == u"Quit MZGram"_q, "branding: the app's name", "");
	Check(Rename(u"Please don't quit Telegram Desktop"_q) == u"Please don't quit MZGram Desktop"_q, "branding: the desktop app's name", "");
	Check(Rename(QString::fromUtf8("Вийти з Telegram")) == QString::fromUtf8("Вийти з MZGram"), "branding: the name in a cloud pack", "");
	Check(Rename(u"install it from https://desktop.telegram.org"_q).contains(u"desktop.telegram.org"_q), "branding: links stay", "");
	auto missing = QStringList();
	for (const auto &replaced : ReplacedKeys()) {
		if (MZGram::Translate(replaced.phrase, false) == QString::fromUtf8(replaced.phrase)) {
			missing.push_back(QString::fromUtf8(replaced.phrase));
		}
	}
	Check(missing.isEmpty() && RenamedKeys().size() > 10, "branding: replaced texts are in the string table", missing.join(u", "_q).toStdString().c_str());
}

// About and the first screen call MZGram unofficial and never official.
void TestBrandingPhrases() {
	const auto officialEnglish = QRegularExpression(u"\\bofficial\\b"_q, QRegularExpression::CaseInsensitiveOption);
	const auto officialUkrainian = QRegularExpression(QString::fromUtf8("(?<![а-яіїєґА-ЯІЇЄҐ])[оО]фіційн"));
	auto problems = QStringList();
	for (const auto key : { "brand_about_text", "brand_intro_about" }) {
		for (const auto ukrainian : { false, true }) {
			const auto text = MZGram::Translate(key, ukrainian);
			if (text.contains(officialEnglish) || text.contains(officialUkrainian)) {
				problems.push_back(QString::fromUtf8(key) + (ukrainian ? u" uk"_q : u" en"_q) + u": official"_q);
			}
		}
	}
	if (!MZGram::Translate("brand_about_text", false).contains(u"Unofficial"_q)
		|| !MZGram::Translate("brand_about_text", true).contains(QString::fromUtf8("Неофіційний"))
		|| !MZGram::Translate("brand_about_text", false).contains(u"Telegram Desktop {version}"_q)
		|| !MZGram::Translate("brand_about_text", true).contains(u"Telegram Desktop {version}"_q)) {
		problems.push_back(u"About does not say unofficial, based on Telegram Desktop"_q);
	}
	if (!MZGram::Translate("brand_intro_about", false).contains(u"MZGram"_q)
		|| !MZGram::Translate("brand_intro_about", true).contains(u"MZGram"_q)) {
		problems.push_back(u"the first screen does not name MZGram"_q);
	}
	Check(problems.isEmpty(), "branding: About is unofficial, in English and Ukrainian", problems.join(u", "_q).toStdString().c_str());
}

// The name in the windows, About, menus, notifications and the exe.
void TestBrandingIsWired() {
	const auto read = [](const QString &path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
			: QString();
	};
	auto problems = QStringList();
	if (!read(u"lang/lang_instance.h"_q).contains(u"return MZGramBrandedValue(*this, key, _values[key]);"_q)) {
		problems.push_back(u"lang texts not renamed"_q);
	}
	const auto about = read(u"boxes/about_box.cpp"_q);
	if (!about.contains(u"MZGram::Tr(\"brand_about_text\")"_q)
		|| !about.contains(u"MZGram::Branding::kRepositoryUrl"_q)
		|| !about.contains(u"MZGram::Branding::kLicenseUrl"_q)
		|| !about.contains(u"MZGram::Branding::CurrentVersionText()"_q)
		|| about.contains(u"core.telegram.org/api"_q)
		|| about.contains(u"\"https://github.com/telegramdesktop/tdesktop\""_q)) {
		problems.push_back(u"About"_q);
	}
	for (const auto &path : { u"window/main_window.cpp"_q, u"window/window_saved_windows.cpp"_q, u"window/window_restore_shell.cpp"_q }) {
		if (read(path).contains(u"u\"Telegram\"_q"_q)) {
			problems.push_back(path + u": Telegram title"_q);
		}
	}
	for (const auto &path : { u"window/notifications_manager_default.cpp"_q, u"settings/sections/settings_notifications.cpp"_q, u"intro/intro_start.cpp"_q, u"window/window_main_menu.cpp"_q }) {
		if (read(path).contains(u"u\"Telegram Desktop\"_q"_q)) {
			problems.push_back(path + u": Telegram Desktop"_q);
		}
	}
	const auto appId = read(u"platform/win/windows_app_user_model_id.cpp"_q);
	if (!appId.contains(u"L\"MZGram.MZGramDesktop\""_q) || appId.contains(u"L\"Telegram.TelegramDesktop\""_q)) {
		problems.push_back(u"Windows app id"_q);
	}
	const auto rc = read(u"../Resources/winrc/Telegram.rc"_q);
	if (!rc.contains(u"VALUE \"FileDescription\", \"MZGram Desktop\""_q)
		|| !rc.contains(u"VALUE \"ProductName\", \"MZGram Desktop\""_q)
		|| !rc.contains(u"VALUE \"CompanyName\", \"MZGram\""_q)) {
		problems.push_back(u"exe properties"_q);
	}
	if (!read(u"core/version.h"_q).contains(u"AppName = \"MZGram Desktop\"_cs"_q)) {
		problems.push_back(u"AppName"_q);
	}
	Check(problems.isEmpty(), "branding: MZGram in the windows, About, menus, notifications and the exe", problems.join(u", "_q).toStdString().c_str());
}

// Linux ARM64 (aarch64) next to x64: built natively by CI and the release,
// with the same recipe, published and named in the notes.
void TestLinuxArm64IsBuilt() {
	const auto read = [](const QString &path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
			: QString();
	};
	auto problems = QStringList();
	const auto arm = u"matrix.arch == 'arm64' && 'ubuntu-24.04-arm'"_q;
	const auto push = read(u"../../.github/workflows/mzgram-linux.yml"_q);
	if (!push.contains(arm) || !push.contains(u"arch: [x64, arm64]"_q)) {
		problems.push_back(u"no arm64 in Linux CI"_q);
	}
	const auto release = read(u"../../.github/workflows/mzgram-desktop-release.yml"_q);
	if (!release.contains(arm)
		|| !release.contains(u"linux-${{ matrix.arch }}.tar.xz"_q)
		|| !release.contains(u"linux-arm64}'"_q)
		|| !release.contains(u"MZGram-Desktop-$VERSION-linux-arm64.tar.xz"_q)) {
		problems.push_back(u"no arm64 in the release"_q);
	}
	const auto docker = read(u"../build/docker/centos_env/Dockerfile"_q);
	if (docker.isEmpty() || docker.contains(u"x86_64-unknown-linux-gnu"_q)) {
		problems.push_back(u"the image recipe is x86_64 only"_q);
	}
	Check(problems.isEmpty(), "Linux arm64 is built and released", problems.join(u", "_q).toStdString().c_str());
}

// FEATURES.md, the list of what MZGram adds, is kept in both repositories
// with every feature change: it is there, has both platforms and the
// comparison, names no other clients and no commit hashes, and the README
// links to it.
void TestFeaturesFile() {
	const auto read = [](const QString &path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
			: QString();
	};
	const auto features = read(u"../../FEATURES.md"_q);
	auto problems = QStringList();
	if (features.isEmpty()) {
		problems.push_back(u"FEATURES.md is missing"_q);
	}
	for (const auto &heading : { u"\n## Android\n"_q, u"\n## Desktop\n"_q, u"\n## Android and Desktop compared\n"_q }) {
		if (!features.contains(heading)) {
			problems.push_back(heading.trimmed());
		}
	}
	// Kept reversed, as in the release notes script
	for (const auto &reversed : { u"uya"_q, u"oken"_q, u"yrucrem"_q }) {
		auto name = reversed;
		std::reverse(name.begin(), name.end());
		if (features.contains(name, Qt::CaseInsensitive)) {
			problems.push_back(u"names another client"_q);
		}
	}
	if (features.contains(QRegularExpression(u"\\b[0-9a-f]{9,40}\\b"_q))) {
		problems.push_back(u"a commit hash"_q);
	}
	if (!features.contains(u"MZGram interface is available in 12 languages"_q)) {
		problems.push_back(u"the languages are not named"_q);
	}
	if (!read(u"../../README.md"_q).contains(u"(FEATURES.md)"_q)) {
		problems.push_back(u"not linked from the README"_q);
	}
	Check(problems.isEmpty(), "FEATURES.md has Android, Desktop and the comparison", problems.join(u", "_q).toStdString().c_str());
}

// Self-update: the archive for this system and architecture, only from
// this repository's releases on GitHub over https.
void TestSelfUpdateArchiveChoice() {
	using namespace MZGram::Update;
	const auto asset = [](const QString &name) {
		return Asset{ .name = name, .url = QString::fromLatin1(kDownloadPrefix) + u"v1.2.0/"_q + name };
	};
	const auto assets = std::vector<Asset>{
		asset(u"MZGram-Desktop-1.2.0-windows-x64.zip"_q),
		asset(u"MZGram-Desktop-1.2.0-windows-arm64.zip"_q),
		asset(u"MZGram-Desktop-1.2.0-linux-x64.tar.xz"_q),
		asset(u"MZGram-Desktop-1.2.0-linux-arm64.tar.xz"_q),
		asset(u"MZGram-Desktop-1.2.0-source.tar.xz"_q),
		asset(u"SHA256SUMS"_q),
	};
	const auto name = [&](System system, Arch arch) {
		const auto chosen = ChooseArchive(assets, u"1.2.0"_q, system, arch);
		return chosen ? chosen->name : QString();
	};
	auto problems = QStringList();
	if (name(System::Windows, Arch::X64) != u"MZGram-Desktop-1.2.0-windows-x64.zip"_q
		|| name(System::Windows, Arch::Arm64) != u"MZGram-Desktop-1.2.0-windows-arm64.zip"_q
		|| name(System::Linux, Arch::X64) != u"MZGram-Desktop-1.2.0-linux-x64.tar.xz"_q
		|| name(System::Linux, Arch::Arm64) != u"MZGram-Desktop-1.2.0-linux-arm64.tar.xz"_q) {
		problems.push_back(u"wrong archive"_q);
	}
	if (!name(System::Other, Arch::X64).isEmpty()
		|| !name(System::Windows, Arch::Other).isEmpty()
		|| ChooseArchive(assets, u"1.3.0"_q, System::Windows, Arch::X64)) {
		problems.push_back(u"an archive for another build"_q);
	}
	if (!FindAsset(assets, u"SHA256SUMS"_q)) {
		problems.push_back(u"no SHA256SUMS"_q);
	}
	const auto json = QByteArray(R"({"tag_name":"v1.2.0","assets":[{"name":"SHA256SUMS","browser_download_url":"https://github.com/dmytrokurochkin/MZGram-Desktop/releases/download/v1.2.0/SHA256SUMS","size":200,"digest":"sha256:ab"}]})");
	const auto parsed = ParseAssets(json);
	if (parsed.size() != 1 || parsed[0].size != 200 || parsed[0].digest != u"sha256:ab"_q) {
		problems.push_back(u"assets not parsed"_q);
	}
	Check(problems.isEmpty(), "self-update: the archive for each system and architecture", problems.join(u", "_q).toStdString().c_str());
}

void TestSelfUpdateOnlyThisRepository() {
	using namespace MZGram::Update;
	auto problems = QStringList();
	for (const auto &ok : {
		u"https://github.com/dmytrokurochkin/MZGram-Desktop/releases/download/v1.2.0/a.zip"_q,
		u"https://objects.githubusercontent.com/github-production-release-asset/1"_q,
		u"https://release-assets.githubusercontent.com/github-production-release-asset/1"_q,
		u"https://api.github.com/repos/dmytrokurochkin/MZGram-Desktop/releases/tags/v1.2.0"_q,
	}) {
		if (!AllowedUrl(ok)) {
			problems.push_back(u"refused "_q + ok);
		}
	}
	for (const auto &bad : {
		u"http://github.com/dmytrokurochkin/MZGram-Desktop/releases/download/v1.2.0/a.zip"_q,
		u"https://example.com/a.zip"_q,
		u"https://github.com.example.com/a.zip"_q,
		u"https://user@github.com/a.zip"_q,
		u"https://github.com:8443/a.zip"_q,
		u"https://updates.tdesktop.com/tsetup/a.zip"_q,
		u"not an address"_q,
	}) {
		if (AllowedUrl(bad)) {
			problems.push_back(u"allowed "_q + bad);
		}
	}
	const auto prefix = QString::fromLatin1(kDownloadPrefix);
	if (!ReleaseFile(prefix + u"v1.2.0/a.zip"_q, u"v1.2.0"_q)
		|| ReleaseFile(prefix + u"v1.1.0/a.zip"_q, u"v1.2.0"_q)
		|| ReleaseFile(u"https://github.com/someone/MZGram-Desktop/releases/download/v1.2.0/a.zip"_q, u"v1.2.0"_q)
		|| ReleaseFile(u"https://github.com/telegramdesktop/tdesktop/releases/download/v1.2.0/a.zip"_q, u"v1.2.0"_q)) {
		problems.push_back(u"a file of another release or repository"_q);
	}
	Check(problems.isEmpty(), "self-update: only this repository on GitHub, over https", problems.join(u", "_q).toStdString().c_str());
}

// The redirect of a release download as GitHub sends it (v1.0.7 on
// 2026-10-30): to release-assets, with a signed query that keeps an encoded
// space, "attachment%3B%20filename%3D<name>". The signature is made up.
QByteArray GitHubLocation(const QString &name) {
	const auto file = name.toUtf8();
	return "https://release-assets.githubusercontent.com/"
		"github-production-release-asset/1367255579/"
		"2f409d5f-9b90-41ae-b934-6eb7b9737677"
		"?sp=r&sv=2018-11-09&sr=b&spr=https&se=2026-10-30T11%3A33%3A17Z"
		"&rscd=attachment%3B+filename%3D" + file
		+ "&rsct=application%2Foctet-stream&skv=2018-11-09"
		"&sig=Ab12%2BCd%2FEf%3D&jwt=eyJhbGciOiJIUzI1NiJ9.e30.c2ln"
		"&response-content-disposition=attachment%3B%20filename%3D" + file
		+ "&response-content-type=application%2Foctet-stream";
}

// The whole way of SHA256SUMS and of each archive (Windows and Linux): the
// address of the release file, GitHub's 302 to release-assets, the request
// there and its 200. MZGram up to 1.0.7 turned the next address into text,
// which decoded %20 into a space, and refused it (update_failed_network).
void TestSelfUpdateFollowsGitHubRedirect() {
	using namespace MZGram::Update;
	auto problems = QStringList();
	const auto prefix = QString::fromLatin1(kDownloadPrefix) + u"v1.2.0/"_q;
	for (const auto &name : {
		QString::fromLatin1(kSumsName),
		ArchiveName(u"1.2.0"_q, System::Windows, Arch::X64),
		ArchiveName(u"1.2.0"_q, System::Windows, Arch::Arm64),
		ArchiveName(u"1.2.0"_q, System::Linux, Arch::X64),
		ArchiveName(u"1.2.0"_q, System::Linux, Arch::Arm64),
	}) {
		const auto location = GitHubLocation(name);
		if (!AllowedUrl(QString::fromUtf8(location))
			|| !AllowedUrl(QUrl::fromEncoded(location, QUrl::StrictMode))) {
			problems.push_back(u"the address of "_q + name + u" refused"_q);
		}
		const auto start = QUrl(prefix + name, QUrl::StrictMode);
		if (const auto failure = RequestFailure(start, 0)) {
			problems.push_back(name + u": "_q + failure->reason);
			continue;
		}
		const auto outcome = AnswerOutcome(start, Answer{
			.status = 302,
			.location = location,
		});
		if (!outcome.redirect || outcome.failure) {
			problems.push_back(name + u": the 302 is not followed"_q);
			continue;
		}
		const auto next = *outcome.redirect;
		if (const auto failure = RequestFailure(next, 1)) {
			problems.push_back(name + u": the redirect refused, "_q + failure->reason);
		}
		if (next.toEncoded() != location) {
			problems.push_back(name + u": the redirect address changed"_q);
		}
		const auto done = AnswerOutcome(next, Answer{ .status = 200 });
		if (done.redirect || done.failure) {
			problems.push_back(name + u": its 200 is not the end"_q);
		}
	}
	// A Location relative to the address that answered
	const auto relative = AnswerOutcome(
		QUrl(prefix + u"SHA256SUMS"_q, QUrl::StrictMode),
		Answer{ .status = 302, .location = "/a/b?name=x%20y" });
	if (!relative.redirect
		|| relative.redirect->toEncoded() != "https://github.com/a/b?name=x%20y"
		|| RequestFailure(*relative.redirect, 1)) {
		problems.push_back(u"a relative redirect"_q);
	}
	Check(problems.isEmpty(), "self-update: GitHub's redirect of SHA256SUMS and of each archive is followed", problems.join(u", "_q).toStdString().c_str());
}

// Each way a download stops has its own text in the bar and its reason in
// log.txt, without the signed query of the address.
void TestSelfUpdateFailureReasons() {
	using namespace MZGram::Update;
	auto problems = QStringList();
	const auto expect = [&](
			const std::optional<Failure> &failure,
			const char *key,
			const QString &reason,
			const char *what) {
		if (!failure) {
			problems.push_back(QString::fromLatin1(what) + u": no failure"_q);
		} else if (qstrcmp(failure->key, key)) {
			problems.push_back(QString::fromLatin1(what)
				+ u": "_q
				+ QString::fromLatin1(failure->key));
		} else if (!failure->reason.contains(reason)
			|| failure->reason.contains(u"secret"_q)
			|| failure->reason.contains(u"sig="_q)) {
			problems.push_back(QString::fromLatin1(what)
				+ u": the reason "_q
				+ failure->reason);
		}
	};
	const auto good = QUrl(
		QString::fromLatin1(kDownloadPrefix) + u"v1.2.0/SHA256SUMS"_q,
		QUrl::StrictMode);
	const auto evil = QUrl::fromEncoded(
		"https://example.com/a.zip?sig=secret",
		QUrl::StrictMode);
	expect(RequestFailure(evil, 0), "update_failed_address", u"example.com"_q, "another host");
	expect(
		RequestFailure(QUrl(u"http://github.com/a.zip"_q), 0),
		"update_failed_address",
		u"http://github.com"_q,
		"no https");
	expect(RequestFailure(good, kMaxRedirects + 1), "update_failed_address", u"redirects"_q, "too many redirects");
	if (RequestFailure(good, kMaxRedirects)) {
		problems.push_back(u"the last allowed redirect refused"_q);
	}
	const auto away = AnswerOutcome(good, Answer{
		.status = 302,
		.location = "https://example.com/a.zip?sig=secret",
	});
	if (!away.redirect) {
		problems.push_back(u"a redirect to another host not read"_q);
	} else {
		expect(RequestFailure(*away.redirect, 1), "update_failed_address", u"example.com"_q, "a redirect to another host");
	}
	expect(AnswerOutcome(good, Answer{ .status = 403, .error = 201 }).failure, "update_failed_server", u"HTTP 403"_q, "403");
	expect(AnswerOutcome(good, Answer{ .status = 404, .error = 203 }).failure, "update_failed_server", u"HTTP 404"_q, "404");
	expect(AnswerOutcome(good, Answer{ .status = 503 }).failure, "update_failed_server", u"HTTP 503"_q, "503");
	const auto notModified = AnswerOutcome(good, Answer{ .status = 304 });
	if (notModified.redirect) {
		problems.push_back(u"304 taken for a redirect"_q);
	}
	expect(notModified.failure, "update_failed_server", u"HTTP 304"_q, "304");
	const auto nowhere = AnswerOutcome(good, Answer{ .status = 302 });
	if (nowhere.redirect) {
		problems.push_back(u"302 without Location taken for a redirect"_q);
	}
	expect(nowhere.failure, "update_failed_server", u"HTTP 302"_q, "302 without Location");
	expect(
		AnswerOutcome(good, Answer{
			.error = 4,
			.errorText = u"Socket operation timed out on https://x/a?sig=secret"_q,
		}).failure,
		"update_failed_network",
		u"network error 4: Socket operation timed out"_q,
		"a timeout");
	expect(AnswerOutcome(good, Answer{ .status = 200, .error = 2 }).failure, "update_failed_network", u"network error 2"_q, "a closed connection");
	expect(AnswerOutcome(good, Answer{}).failure, "update_failed_network", u"no answer"_q, "no answer");
	expect(BodyFailure(kMaxBodySize + 1), "update_failed_server", u"larger"_q, "a too large answer");
	if (BodyFailure(kMaxBodySize)) {
		problems.push_back(u"an answer of the largest size refused"_q);
	}
	const auto release = ReleaseJson("v1.2.0");
	if (ReleaseFailure(release, u"v1.2.0"_q, StableCode(u"v1.1.0"_q))) {
		problems.push_back(u"the offered release refused"_q);
	}
	expect(ReleaseFailure(release, u"v1.3.0"_q, StableCode(u"v1.1.0"_q)), "update_failed_release", u"v1.2.0, not v1.3.0"_q, "another release");
	expect(ReleaseFailure(release, u"v1.2.0"_q, StableCode(u"v1.2.0"_q)), "update_failed_release", u"not newer"_q, "not newer");
	expect(ReleaseFailure("not json", u"v1.2.0"_q, 0), "update_failed_release", u"not a stable release"_q, "a broken answer");
	if (LogAddress(QUrl::fromEncoded(GitHubLocation(u"a.zip"_q))).contains('?')) {
		problems.push_back(u"the signed query in log.txt"_q);
	}
	Check(problems.isEmpty(), "self-update: each failure has its own text and its reason", problems.join(u", "_q).toStdString().c_str());
}

// Every text a failure shows is in the string table (TrNow of a variable
// is not found by TestEveryUsedKeyIsInTheTable), and the installer writes
// its own one when a file cannot be written.
void TestSelfUpdateFailureTexts() {
	auto keys = QSet<QString>();
	for (const auto &phrase : MZGram::Phrases()) {
		keys.insert(QString::fromUtf8(phrase.key));
	}
	auto problems = QStringList();
	for (const auto key : {
		"update_failed_network",
		"update_failed_server",
		"update_failed_address",
		"update_failed_release",
		"update_failed_disk",
		"update_failed_checksum",
		"update_failed_unpack",
	}) {
		if (!keys.contains(QString::fromLatin1(key))) {
			problems.push_back(QString::fromLatin1(key));
		}
	}
	auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR)
		+ u"/mzgram/mzgram_self_update.cpp"_q);
	const auto installer = file.open(QIODevice::ReadOnly)
		? QString::fromUtf8(file.readAll())
		: QString();
	if (installer.count(u"kFailedDisk"_q) < 3) {
		problems.push_back(u"a file that cannot be written is not update_failed_disk"_q);
	}
	if (installer.contains(u"toString()"_q)
		|| QRegularExpression(u"\\bfail\\(\"[a-z_]+\"\\)"_q).match(installer).hasMatch()) {
		problems.push_back(u"an address as text or a failure without a reason"_q);
	}
	Check(problems.isEmpty(), "self-update: every failure text is in the table", problems.join(u", "_q).toStdString().c_str());
}

// The hook of the Update button test in CI is only in a build that asks for
// it (MZGRAM_TEST_HOOKS), never in a release.
void TestUpdateButtonHookStaysInTests() {
	const auto read = [](const QString &path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
			: QString();
	};
	auto problems = QStringList();
	const auto update = read(u"mzgram/mzgram_update.cpp"_q);
	const auto guard = update.indexOf(u"#ifdef MZGRAM_TEST_HOOKS"_q);
	const auto hook = update.indexOf(u"MZGRAM_TEST_PRESS_UPDATE"_q);
	if (guard < 0 || hook < guard || update.indexOf(u"#endif"_q, guard) < hook) {
		problems.push_back(u"the hook is not behind MZGRAM_TEST_HOOKS"_q);
	}
	if (read(u"../../.github/workflows/mzgram-desktop-release.yml"_q).contains(u"MZGRAM_TEST_HOOKS"_q)) {
		problems.push_back(u"a release build has the hook"_q);
	}
	if (!read(u"../../.github/workflows/mzgram-windows.yml"_q).contains(u"mzgram_update_button_test.ps1"_q)) {
		problems.push_back(u"the Windows CI does not press Update"_q);
	}
	Check(problems.isEmpty(), "self-update: the Update button test hook is never in a release", problems.join(u", "_q).toStdString().c_str());
}

// The archive must match SHA256SUMS and the GitHub digest.
void TestSelfUpdateChecksum() {
	using namespace MZGram::Update;
	const auto good = QString(64, 'a');
	const auto other = QString(64, 'b');
	const auto sums = ParseSums((good + u"  MZGram-Desktop-1.2.0-windows-x64.zip\n"_q
		+ other.toUpper() + u" *MZGram-Desktop-1.2.0-linux-x64.tar.xz\nbroken\n"_q).toUtf8());
	auto problems = QStringList();
	if (sums.size() != 2
		|| sums.at(u"MZGram-Desktop-1.2.0-windows-x64.zip"_q) != good
		|| sums.at(u"MZGram-Desktop-1.2.0-linux-x64.tar.xz"_q) != other) {
		problems.push_back(u"SHA256SUMS not read"_q);
	}
	if (!HashMatches(good, good, QString())
		|| !HashMatches(good, good.toUpper(), u"sha256:"_q + good)) {
		problems.push_back(u"a good file refused"_q);
	}
	if (HashMatches(other, good, QString())
		|| HashMatches(good, good, u"sha256:"_q + other)
		|| HashMatches(good, QString(), QString())
		|| HashMatches(QString(), good, QString())) {
		problems.push_back(u"a corrupt file accepted"_q);
	}
	Check(problems.isEmpty(), "self-update: SHA-256 against SHA256SUMS and the digest", problems.join(u", "_q).toStdString().c_str());
}

// Archive entries stay inside the app folder and never reach tdata.
void TestSelfUpdateEntries() {
	using namespace MZGram::Update;
	auto problems = QStringList();
	const auto expect = [&](const QString &entry, const QString &path) {
		if (EntryPath(entry) != path) {
			problems.push_back(entry + u" -> "_q + EntryPath(entry));
		}
	};
	expect(u"MZGram/"_q, u"/"_q);
	expect(u"MZGram/MZGram.exe"_q, u"MZGram.exe"_q);
	expect(u"MZGram\\Updater.exe"_q, u"Updater.exe"_q);
	expect(u"MZGram/modules/x64/d3d/d3dcompiler_47.dll"_q, u"modules/x64/d3d/d3dcompiler_47.dll"_q);
	expect(u"MZGram/modules/"_q, u"modules/"_q);
	for (const auto &bad : {
		u"MZGram.exe"_q,
		u"Other/MZGram.exe"_q,
		u"/MZGram/MZGram.exe"_q,
		u"MZGram/../evil.exe"_q,
		u"MZGram/a/../../evil.exe"_q,
		u"MZGram/C:/evil.exe"_q,
		u"MZGram//evil.exe"_q,
		u"MZGram/tdata/settings"_q,
		u"MZGram/TData/key_datas"_q,
		u"MZGram/tdata/"_q,
	}) {
		expect(bad, QString());
	}
	Check(problems.isEmpty(), "self-update: archive entries stay in the app folder, never tdata", problems.join(u", "_q).toStdString().c_str());
}

// Linux archives hold regular files and folders only: a link could lead a
// later entry out of the app folder, whatever its name.
void TestSelfUpdateEntryTypes() {
	using namespace MZGram::Update;
	auto problems = QStringList();
	for (const auto &plain : {
		u"-rwxr-xr-x runner/runner 99233284 2026-10-21 15:52 MZGram/MZGram"_q,
		u"drwxr-xr-x runner/runner 0 2026-10-21 15:52 MZGram/"_q,
	}) {
		if (!PlainArchiveEntry(plain)) {
			problems.push_back(u"refused: "_q + plain);
		}
	}
	for (const auto &link : {
		u"lrwxrwxrwx runner/runner 0 2026-10-21 15:52 MZGram/x -> /"_q,
		u"hrwxr-xr-x runner/runner 0 2026-10-21 15:52 MZGram/y link to MZGram/MZGram"_q,
		u"crw-r--r-- root/root 1,3 2026-10-21 15:52 MZGram/null"_q,
		QString(),
	}) {
		if (PlainArchiveEntry(link)) {
			problems.push_back(u"taken: "_q + link);
		}
	}
	auto file = QFile(
		QString::fromUtf8(MZGRAM_SOURCE_DIR) + u"/mzgram/mzgram_self_update.cpp"_q);
	const auto text = file.open(QIODevice::ReadOnly)
		? QString::fromUtf8(file.readAll())
		: QString();
	if (!text.contains(u"u\"-tvJf\"_q"_q) || !text.contains(u"PlainArchiveEntry(line)"_q)) {
		problems.push_back(u"the Linux unpack does not check the entry types"_q);
	}
	Check(problems.isEmpty(), "self-update: Linux archives with links are refused", problems.join(u", "_q).toStdString().c_str());
}

// The Windows Updater keeps the replaced files where the rules say and no
// longer has Telegram Desktop's installer entry to update.
void TestUpdaterBackupFolderIsShared() {
	auto file = QFile(
		QString::fromUtf8(MZGRAM_SOURCE_DIR) + u"/_other/updater_win.cpp"_q);
	const auto text = file.open(QIODevice::ReadOnly)
		? QString::fromUtf8(file.readAll())
		: QString();
	auto problems = QStringList();
	if (text.isEmpty() || text.contains(u"L\"tupdates\\\\mzgram_backup\""_q)) {
		problems.push_back(u"a backup folder of its own"_q);
	}
	if (text.contains(u"updateRegistry"_q)) {
		problems.push_back(u"updateRegistry"_q);
	}
	Check(problems.isEmpty(), "self-update: the Windows Updater uses the shared backup folder", problems.join(u", "_q).toStdString().c_str());
}

// The MZGram code of a prepared update, in its own tdata (never copied).
void TestSelfUpdatePreparedCode() {
	using namespace MZGram::Update;
	auto folder = QTemporaryDir();
	auto problems = QStringList();
	if (PreparedCode(folder.path()) != -1) {
		problems.push_back(u"a code without the file"_q);
	}
	QDir().mkpath(folder.path() + u"/tdata"_q);
	auto file = QFile(folder.path() + '/' + QString::fromLatin1(kPreparedCodeFile));
	if (file.open(QIODevice::WriteOnly)) {
		file.write("1020099");
		file.close();
	}
	if (PreparedCode(folder.path()) != 1020099) {
		problems.push_back(u"the code not read"_q);
	}
	if (QString::fromLatin1(kPreparedCodeFile).left(6) != u"tdata/"_q) {
		problems.push_back(u"the code would be copied into the app folder"_q);
	}
	Check(problems.isEmpty(), "self-update: the prepared update carries the MZGram code", problems.join(u", "_q).toStdString().c_str());
}

// The Updater backs up every file it replaces and puts everything back
// when a copy fails (mzgram_update_install.h).
void TestUpdateInstallRollsBack() {
	using namespace MZGram::UpdateInstall;
	auto problems = QStringList();
	const auto read = [](const std::string &path) {
		auto file = QFile(QString::fromStdString(path));
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<none>");
	};
	const auto write = [](const std::string &path, const QByteArray &content) {
		auto file = QFile(QString::fromStdString(path));
		return file.open(QIODevice::WriteOnly) && (file.write(content) == content.size());
	};
	const auto run = [&](int failAt, bool halfWrite) {
		auto folder = QTemporaryDir();
		const auto root = folder.path().toStdString();
		QDir().mkpath(QString::fromStdString(root + "/app"));
		QDir().mkpath(QString::fromStdString(root + "/update"));
		QDir().mkpath(QString::fromStdString(root + "/backup"));
		write(root + "/app/MZGram.exe", "old exe");
		write(root + "/app/lib.dll", "old lib");
		write(root + "/app/tdata_marker", "user data");
		write(root + "/update/MZGram.exe", "new exe");
		write(root + "/update/lib.dll", "new lib");
		write(root + "/update/added.dll", "new file");
		auto steps = std::vector<Step<std::string>>{
			{ root + "/update/MZGram.exe", root + "/app/MZGram.exe" },
			{ root + "/update/lib.dll", root + "/app/lib.dll" },
			{ root + "/update/added.dll", root + "/app/added.dll" },
		};
		auto copies = 0;
		auto files = Files<std::string>();
		files.exists = [](const std::string &path) {
			return QFile::exists(QString::fromStdString(path));
		};
		files.copy = [&](const std::string &from, const std::string &to) {
			const auto isUpdate = from.find("/update/") != std::string::npos;
			if (isUpdate && copies++ == failAt) {
				if (halfWrite) {
					write(to, "half");
				}
				return false;
			}
			QFile::remove(QString::fromStdString(to));
			return QFile::copy(QString::fromStdString(from), QString::fromStdString(to));
		};
		files.remove = [](const std::string &path) {
			return QFile::remove(QString::fromStdString(path));
		};
		files.backup = [&](std::size_t index) {
			return root + "/backup/" + std::to_string(index);
		};
		files.log = [](const char *, const std::string &) {};
		const auto installed = Install(steps, files);
		return std::make_tuple(
			installed,
			read(root + "/app/MZGram.exe"),
			read(root + "/app/lib.dll"),
			read(root + "/app/added.dll"),
			read(root + "/app/tdata_marker"));
	};
	{
		const auto [installed, exe, lib, added, data] = run(-1, false);
		if (!installed || exe != "new exe" || lib != "new lib" || added != "new file") {
			problems.push_back(u"the update is not in place"_q);
		}
		if (data != "user data") {
			problems.push_back(u"other files touched"_q);
		}
	}
	for (const auto failAt : { 0, 1, 2 }) {
		for (const auto halfWrite : { false, true }) {
			const auto [installed, exe, lib, added, data] = run(failAt, halfWrite);
			if (installed || exe != "old exe" || lib != "old lib" || added != "<none>" || data != "user data") {
				problems.push_back(u"not rolled back at %1%2: %3 %4 %5"_q
					.arg(failAt)
					.arg(halfWrite ? u" (half written)"_q : QString())
					.arg(QString::fromUtf8(exe), QString::fromUtf8(lib), QString::fromUtf8(added)));
			}
		}
	}
	Check(problems.isEmpty(), "self-update: the Updater backs up and rolls back", problems.join(u", "_q).toStdString().c_str());
}

// The Updater is built and shipped in every release archive, and wired to
// roll back; Telegram's own update servers stay off.
void TestReleasesCarryTheUpdater() {
	const auto read = [](const QString &path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
			: QString();
	};
	auto problems = QStringList();
	for (const auto &workflow : { u"mzgram-windows.yml"_q, u"mzgram-linux.yml"_q, u"mzgram-desktop-release.yml"_q }) {
		const auto text = read(u"../../.github/workflows/"_q + workflow);
		if (text.isEmpty() || text.contains(u"DESKTOP_APP_DISABLE_AUTOUPDATE=ON"_q)) {
			problems.push_back(workflow + u": the Updater is not built"_q);
		}
	}
	const auto release = read(u"../../.github/workflows/mzgram-desktop-release.yml"_q);
	if (!release.contains(u"cp \"$OUT/Updater.exe\" \"$RUNNER_TEMP/pkg/MZGram/\""_q)
		|| !release.contains(u"cp out/Release/Updater \"$RUNNER_TEMP/pkg/MZGram/\""_q)
		|| !release.contains(u"MZGram/Updater.exe"_q)
		|| !release.contains(u"MZGram/Updater$"_q)) {
		problems.push_back(u"the Updater is not in the release archives"_q);
	}
	for (const auto &updater : { u"_other/updater_win.cpp"_q, u"_other/updater_linux.cpp"_q }) {
		const auto text = read(updater);
		if (!text.contains(u"Install(steps, files)"_q) || !text.contains(u"kFailedMarker"_q)) {
			problems.push_back(updater + u": no rollback"_q);
		}
	}
	if (read(u"_other/updater_win.cpp"_q).contains(u"update()) {\n\t\t\t\t\tupdateRegistry();"_q)) {
		problems.push_back(u"the Updater writes Telegram's uninstall entry"_q);
	}
	const auto checker = read(u"core/update_checker.cpp"_q);
	if (!checker.contains(u"MZGram::Update::PreparedCode(readyPath)"_q)) {
		problems.push_back(u"the prepared MZGram version is not checked"_q);
	}
	if (!read(u"core/sandbox.cpp"_q).contains(u"MZGram::Update::PreparedReady()"_q)
		|| !read(u"core/launcher.cpp"_q).contains(u"MZGram::Update::PreparedReady()"_q)) {
		problems.push_back(u"a prepared update is not installed"_q);
	}
	if (read(u"mzgram/mzgram_self_update.cpp"_q).contains(u"tdesktop.com"_q)
		|| read(u"mzgram/mzgram_self_update_rules.cpp"_q).contains(u"telegram.org"_q)) {
		problems.push_back(u"Telegram's update servers"_q);
	}
	Check(problems.isEmpty(), "self-update: the Updater is in every release and rolls back", problems.join(u", "_q).toStdString().c_str());
}

// Telegram Desktop's updater is off in the code itself, and the MZGram
// check starts with the app and shows its bar in the chat list.
void TestUpdateIsWired() {
	const auto read = [](const QString &path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
			: QString();
	};
	auto problems = QStringList();
	const auto updater = read(u"core/update_checker.cpp"_q);
	if (!updater.contains(u"\nbool UpdaterIsDisabled = true;\n"_q)
		|| updater.contains(u"UpdaterIsDisabled = false"_q)) {
		problems.push_back(u"Telegram updater not off"_q);
	}
	if (!read(u"core/application.cpp"_q).contains(u"MZGram::Update::Start();"_q)) {
		problems.push_back(u"check not started"_q);
	}
	const auto dialogs = read(u"dialogs/dialogs_widget.cpp"_q);
	if (!dialogs.contains(u"MZGram::Update::Available("_q)
		|| !dialogs.contains(u"putBottomButton(_mzgramUpdate);"_q)) {
		problems.push_back(u"no bar in the chat list"_q);
	}
	Check(
		problems.isEmpty(),
		"update: Telegram's updater is off, the MZGram bar is wired",
		problems.join(u", "_q).toStdString().c_str());
}

void TestUpdateDue() {
	using namespace MZGram::Update;
	auto state = State();
	state.lastCheck = 1000;
	Check(!Due(state, 1000 + kCheckIntervalMs - 1, true, false), "update: not again within the interval", "");
	Check(Due(state, 1000 + kCheckIntervalMs, true, false), "update: due after the interval", "");
	Check(!Due(state, 1000 + kCheckIntervalMs, false, false), "update: never with the switch off", "");
	Check(Due(state, 1001, false, true), "update: the user's own check is always due", "");
}

[[nodiscard]] QString ReadSource(const char *path) {
	auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + '/' + path);
	return file.open(QIODevice::ReadOnly)
		? QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q)
		: QString();
}

[[nodiscard]] QStringList PlaceholdersOf(const QString &text) {
	auto result = QStringList();
	auto i = QRegularExpression(u"%[0-9]|\\{[a-z_]+\\}"_q).globalMatch(text);
	while (i.hasNext()) {
		result.push_back(i.next().captured());
	}
	result.sort();
	return result;
}

// Every MZGram phrase is in each of the twelve languages, in that language:
// every key, nothing more, the same %1 and {name} placeholders, and the
// language's own script and letters.
void TestEveryPhraseInEveryLanguage() {
	const auto cyrillic = QRegularExpression(u"[\\x{0400}-\\x{04FF}]"_q);
	const auto arabic = QRegularExpression(u"[\\x{0600}-\\x{06FF}]"_q);
	// Letters of the Cyrillic ones that only that language uses
	const auto russianOnly = QRegularExpression(QString::fromUtf8("[ыэъЫЭЪ]"));
	const auto ukrainianOnly = QRegularExpression(QString::fromUtf8("[іїєґІЇЄҐ]"));
	const auto notBelarusian = QRegularExpression(QString::fromUtf8("[ъщиЪЩИїєґЇЄҐ]"));
	const auto belarusianOnly = QRegularExpression(QString::fromUtf8("[ўЎ]"));
	const auto kazakhOnly = QRegularExpression(QString::fromUtf8("[әғқңөұүһӘҒҚҢӨҰҮҺ]"));
	// Texts that read the same in every language: names, ids
	const auto same = QSet<QString>{ u"settings_title"_q, u"details_id"_q };
	auto problems = QStringList();
	auto english = std::map<QString, QString>();
	const auto longDash = QChar(0x2014);
	for (const auto &phrase : MZGram::Phrases()) {
		english.emplace(QString::fromUtf8(phrase.key), QString::fromUtf8(phrase.english));
		if (QString::fromUtf8(phrase.english).contains(longDash)
			|| QString::fromUtf8(phrase.ukrainian).contains(longDash)) {
			problems.push_back(QString::fromUtf8(phrase.key) + u": long dash"_q);
		}
	}
	for (const auto code : MZGram::kLanguages) {
		const auto language = QString::fromLatin1(code);
		if (language == u"en"_q || language == u"uk"_q) {
			if (!MZGram::Translations(language).empty()) {
				problems.push_back(language + u": in the other tables"_q);
			}
			continue;
		}
		const auto cyrillicLanguage = (language == u"ru"_q)
			|| (language == u"be"_q)
			|| (language == u"kk"_q);
		auto seen = QSet<QString>();
		auto letters = QString();
		auto identical = 0;
		for (const auto &translation : MZGram::Translations(language)) {
			const auto key = QString::fromUtf8(translation.key);
			const auto text = QString::fromUtf8(translation.text);
			if (seen.contains(key)) {
				problems.push_back(language + ' ' + key + u": twice"_q);
			}
			seen.insert(key);
			const auto i = english.find(key);
			if (i == end(english)) {
				problems.push_back(language + ' ' + key + u": not an MZGram key"_q);
				continue;
			} else if (text.trimmed().isEmpty()) {
				problems.push_back(language + ' ' + key + u": empty"_q);
			} else if (PlaceholdersOf(text) != PlaceholdersOf(i->second)) {
				problems.push_back(language + ' ' + key + u": placeholders differ"_q);
			}
			if (text == i->second && !same.contains(key)) {
				++identical;
			}
			letters += text;
			if (text.contains(longDash)) {
				problems.push_back(language + ' ' + key + u": long dash"_q);
			}
			if (!cyrillicLanguage && text.contains(cyrillic)) {
				problems.push_back(language + ' ' + key + u": Cyrillic"_q);
			}
			if (language == u"ru"_q && text.contains(ukrainianOnly)) {
				problems.push_back(language + ' ' + key + u": Ukrainian letters"_q);
			} else if (language == u"be"_q && text.contains(notBelarusian)) {
				problems.push_back(language + ' ' + key + u": not Belarusian letters"_q);
			}
		}
		for (const auto &[key, text] : english) {
			if (!seen.contains(key)) {
				problems.push_back(language + ' ' + key + u": missing"_q);
			}
		}
		// The language as a whole, not word by word
		const auto check = [&](bool good, const char *what) {
			if (!good) {
				problems.push_back(language + u": "_q + QString::fromUtf8(what));
			}
		};
		const auto has = [&](const char *pattern) {
			return letters.contains(QRegularExpression(QString::fromUtf8(pattern)));
		};
		if (language == u"ru"_q) {
			check(letters.contains(russianOnly), "not Russian");
		} else if (language == u"be"_q) {
			check(letters.contains(belarusianOnly) && has("і"), "not Belarusian");
		} else if (language == u"kk"_q) {
			check(letters.contains(kazakhOnly), "not Kazakh");
		} else if (language == u"ar"_q) {
			check(letters.contains(arabic), "not Arabic");
		} else if (language == u"pl"_q) {
			check(has("[ąęłśżźćń]"), "not Polish");
		} else if (language == u"de"_q) {
			check(has("[äöüß]"), "not German");
		} else if (language == u"fr"_q) {
			check(has("[éèàçê]"), "not French");
		} else if (language == u"es"_q) {
			check(has("[ñáéíóú]"), "not Spanish");
		} else if (language == u"pt-BR"_q) {
			check(has("[ãõçáéê]"), "not Portuguese");
		} else if (language == u"tr"_q) {
			check(has("[ığşçöü]"), "not Turkish");
		} else {
			check(false, "an unknown language");
		}
		// A few names (MZGram, GitHub) read the same; whole English texts
		// are not translations.
		check(identical * 10 < int(english.size()), "many texts left in English");
	}
	Check(
		problems.isEmpty(),
		"every MZGram phrase is in all twelve languages",
		problems.join(u", "_q).toStdString().c_str());
}

// The MZGram language: the choice in Settings, else the system's language
// when MZGram has it, else English; a missing text is English too.
void TestLanguageChoice() {
	auto problems = QStringList();
	const auto expect = [&](const QString &locale, const QString &language) {
		if (MZGram::SupportedLanguage(locale) != language) {
			problems.push_back(locale + u" -> "_q + MZGram::SupportedLanguage(locale));
		}
	};
	expect(u"de_AT"_q, u"de"_q);
	expect(u"pt-PT"_q, u"pt-BR"_q);
	expect(u"pt_BR"_q, u"pt-BR"_q);
	expect(u"kk"_q, u"kk"_q);
	expect(u"be-BY"_q, u"be"_q);
	expect(u"uk-UA"_q, u"uk"_q);
	expect(u"ar-EG"_q, u"ar"_q);
	expect(u"zh_CN"_q, u"en"_q);
	expect(u"it"_q, u"en"_q);
	expect(QString(), u"en"_q);
	auto &option = base::options::lookup<QString>(MZGram::kOptionLanguage);
	if (!option.value().isEmpty() || !MZGram::LanguageChoice().isEmpty()) {
		problems.push_back(u"not System by default"_q);
	}
	option.set(u"pl"_q);
	if (MZGram::LanguageChoice() != u"pl"_q) {
		problems.push_back(u"the choice is not kept"_q);
	}
	option.set(u"xx"_q);
	if (!MZGram::LanguageChoice().isEmpty()) {
		problems.push_back(u"an unknown choice is not System"_q);
	}
	option.set(QString());
	if (MZGram::TranslateTo("section_archive", u"de"_q) == MZGram::Translate("section_archive", false)) {
		problems.push_back(u"German is English"_q);
	}
	if (MZGram::TranslateTo("section_archive", u"it"_q) != u"Archive"_q
		|| MZGram::TranslateTo("section_archive", u"uk"_q) != MZGram::Translate("section_archive", true)) {
		problems.push_back(u"an unknown language is not English"_q);
	}
	Check(
		problems.isEmpty(),
		"the MZGram language: the choice, else the system's, else English",
		problems.join(u", "_q).toStdString().c_str());
}

// The picker in Settings > MZGram > Interface, and the texts follow it.
void TestLanguagePickerIsWired() {
	auto missing = QStringList();
	const auto wired = std::vector<std::pair<const char*, QString>>{
		{ "mzgram/mzgram_settings.cpp", u"Box(LanguageBox)"_q },
		{ "mzgram/mzgram_settings.cpp", u"Text(\"mzgram_language\")"_q },
		{ "mzgram/mzgram_lang.cpp", u"lookup<QString>(kOptionLanguage).changes()"_q },
		{ "mzgram/mzgram_lang.cpp", u"TranslateTo(key, CurrentLanguage())"_q },
		{ "../CMakeLists.txt", u"mzgram/mzgram_lang_translations.cpp"_q },
		{ "../cmake/mzgram_tests.cmake", u"mzgram/mzgram_lang_translations.cpp"_q },
	};
	for (const auto &[path, text] : wired) {
		if (!ReadSource(path).contains(text)) {
			missing.push_back(QString::fromUtf8(path) + u": "_q + text);
		}
	}
	Check(
		missing.isEmpty(),
		"the MZGram language picker is in Settings and the texts follow it",
		missing.join(u", "_q).toStdString().c_str());
}

// A hidden last seen is shown as any last seen time: no "(approx.)".
void TestLastSeenIsPlain() {
	auto problems = QStringList();
	for (const auto &phrase : MZGram::Phrases()) {
		if (QString::fromUtf8(phrase.key).contains(u"approx"_q)) {
			problems.push_back(QString::fromUtf8(phrase.key));
		}
	}
	const auto presence = ReadSource("mzgram/mzgram_presence.cpp");
	if (presence.contains(u"approx\""_q) || !presence.contains(u"return text;"_q)) {
		problems.push_back(u"mzgram_presence.cpp marks the time"_q);
	}
	Check(
		problems.isEmpty(),
		"a hidden last seen is shown as a plain time",
		problems.join(u", "_q).toStdString().c_str());
}

// A chat whose own messages are all gone stays in the chat list with its
// newest kept deleted message.
void TestDeletedChatStays() {
	using MZGram::ChatListCandidate;
	auto problems = QStringList();
	const auto kept = [](int64 id) {
		return ChatListCandidate{ .id = id, .keptDeleted = true };
	};
	if (MZGram::KeptChatListMessage({ kept(5) }) != 0) {
		problems.push_back(u"the only message, deleted"_q);
	}
	if (MZGram::KeptChatListMessage({ kept(5), kept(9), kept(7) }) != 1) {
		problems.push_back(u"the newest of the whole history"_q);
	}
	if (MZGram::KeptChatListMessage({ ChatListCandidate{ .id = 9, .keptDeleted = true, .service = true }, kept(3) }) != 1) {
		problems.push_back(u"a service message"_q);
	}
	if (MZGram::KeptChatListMessage({ ChatListCandidate{ .id = 9 } }) != -1
		|| MZGram::KeptChatListMessage({}) != -1) {
		problems.push_back(u"nothing kept"_q);
	}
	auto &deleted = base::options::lookup<bool>(MZGram::kOptionSaveDeletedMessages);
	deleted.set(false);
	if (MZGram::KeptChatListMessage({ kept(5) }) != -1) {
		problems.push_back(u"kept with the switch off"_q);
	}
	deleted.set(true);
	const auto history = ReadSource("history/history.cpp");
	const auto hook = history.indexOf(u"item = MZGram::NewestPreservedItem(this);"_q);
	const auto setter = history.indexOf(u"void History::setLastServerMessage(HistoryItem *item) {"_q);
	if (hook < 0 || setter < 0 || hook < setter || hook > setter + 400) {
		problems.push_back(u"History::setLastServerMessage does not keep it"_q);
	}
	if (!ReadSource("mzgram/mzgram_anti_recall.cpp").contains(u"KeptChatListMessage(candidates)"_q)) {
		problems.push_back(u"NewestPreservedItem does not use the rule"_q);
	}
	Check(
		problems.isEmpty(),
		"a chat with only kept deleted messages stays in the chat list",
		problems.join(u", "_q).toStdString().c_str());
}

// "Check for updates": says latest, available or failed; an error is not
// "the latest version".
void TestManualUpdateCheck() {
	using namespace MZGram::Update;
	auto problems = QStringList();
	const auto installed = StableCode(u"v1.0.1"_q);
	const auto release = [](const QByteArray &tag) {
		return QByteArray("{\"tag_name\":\"") + tag
			+ "\",\"draft\":false,\"prerelease\":false,\"html_url\":\"https://github.com/dmytrokurochkin/MZGram-Desktop/releases/tag/"
			+ tag + "\"}";
	};
	auto state = State();
	auto outcome = ApplyManual(state, true, 200, release("v1.0.2"), u"\"e1\""_q, installed);
	if (outcome.result != ManualResult::Available || !outcome.release || outcome.release->tag != u"v1.0.2"_q) {
		problems.push_back(u"a newer version"_q);
	}
	state.dismissedTag = u"v1.0.2"_q;
	outcome = ApplyManual(state, true, 304, QByteArray(), QString(), installed);
	if (outcome.result != ManualResult::Available) {
		problems.push_back(u"a closed newer version, unchanged answer"_q);
	}
	state = State();
	if (ApplyManual(state, true, 200, release("v1.0.1"), QString(), installed).result != ManualResult::Latest) {
		problems.push_back(u"the installed version"_q);
	}
	if (ApplyManual(state, true, 404, QByteArray(), QString(), installed).result != ManualResult::Latest) {
		problems.push_back(u"no release yet"_q);
	}
	state.latestTag = u"v1.0.5"_q;
	state.latestUrl = u"https://github.com/dmytrokurochkin/MZGram-Desktop/releases/tag/v1.0.5"_q;
	const auto errors = std::vector<std::tuple<bool, int, QByteArray>>{
		{ false, 0, QByteArray() },
		{ false, 403, QByteArray() },
		{ false, 500, QByteArray() },
		{ true, 200, QByteArray("<html>") },
	};
	for (const auto &[ok, status, body] : errors) {
		if (ApplyManual(state, ok, status, body, QString(), installed).result != ManualResult::Failed) {
			problems.push_back(u"an error at "_q + QString::number(status));
		}
	}
	if (state.latestTag != u"v1.0.5"_q) {
		problems.push_back(u"an error changed the saved release"_q);
	}
	const auto settings = ReadSource("mzgram/mzgram_settings.cpp");
	if (!settings.contains(u"Text(\"check_updates_now\")"_q)
		|| !settings.contains(u"MZGram::Update::CheckNow("_q)
		|| !settings.contains(u"update_check_failed"_q)) {
		problems.push_back(u"no button in Settings"_q);
	}
	if (!ReadSource("mzgram/mzgram_update.cpp").contains(u"ApplyManual("_q)) {
		problems.push_back(u"the check does not use the rule"_q);
	}
	Check(
		problems.isEmpty(),
		"Check for updates says latest, available or failed",
		problems.join(u", "_q).toStdString().c_str());
}

// What MZGram shows comes from MZGram's phrases, in the MZGram language:
// Telegram's phrases (tr::lng_*) follow Telegram's language, so a Cancel or
// OK of Telegram's on an MZGram box was in another language than the box.
// MZGram's sources use no tr::lng_ phrase, every confirmation box of MZGram
// names its own Cancel (the box's default is Telegram's), dates on MZGram's
// boxes use the MZGram language, and the update bar's texts follow a change
// of the language. A place that shows Telegram's text on purpose says why:
// "Telegram's own text: <why>" on that line or up to ten lines above it.
void TestMZGramTextsAreInTheMZGramLanguage() {
	auto problems = QStringList();
	const auto allowed = [](const QStringList &lines, int index) {
		for (auto i = std::max(0, index - 10); i <= index; ++i) {
			if (lines[i].contains(u"Telegram's own text: "_q)) {
				return true;
			}
		}
		return false;
	};
	const auto confirmBoxes = [&](const QString &path, const QString &text, bool onlyMZGram) {
		auto from = 0;
		while ((from = text.indexOf(u"MakeConfirmBox({"_q, from)) >= 0) {
			const auto till = text.indexOf(u"}));"_q, from);
			const auto box = text.mid(from, (till < 0 ? text.size() : till) - from);
			from += 16;
			if (onlyMZGram && !box.contains(u"MZGram::Tr"_q)) {
				continue;
			}
			if (!box.contains(u".cancelText = "_q)) {
				problems.push_back(path + u": a confirmation box with Telegram's Cancel"_q);
			}
			if (box.contains(u"tr::lng_"_q)) {
				problems.push_back(path + u": Telegram's phrase in an MZGram box"_q);
			}
		}
	};
	auto files = QDirIterator(
		QString::fromUtf8(MZGRAM_SOURCE_DIR) + u"/mzgram"_q,
		{ u"*.cpp"_q },
		QDir::Files);
	auto checked = 0;
	while (files.hasNext()) {
		const auto path = files.next();
		const auto name = QFileInfo(path).fileName();
		if (name == u"mzgram_lang_table.cpp"_q
			|| name == u"mzgram_lang_translations.cpp"_q) {
			continue;
		}
		auto file = QFile(path);
		if (!file.open(QIODevice::ReadOnly)) {
			continue;
		}
		++checked;
		const auto text = QString::fromUtf8(file.readAll()).replace(u"\r\n"_q, u"\n"_q);
		const auto lines = text.split('\n');
		for (auto i = 0; i != lines.size(); ++i) {
			const auto &line = lines[i];
			if (line.trimmed().startsWith(u"//"_q) || allowed(lines, i)) {
				continue;
			}
			if (line.contains(u"tr::lng_"_q)) {
				problems.push_back(name + ':' + QString::number(i + 1) + u": Telegram's phrase"_q);
			}
			if (line.contains(u"QLocale()"_q)) {
				problems.push_back(name + ':' + QString::number(i + 1) + u": a date in the system's language"_q);
			}
		}
		confirmBoxes(name, text, false);
	}
	// MZGram's boxes in Telegram's sources
	const auto voice = "history/view/controls/history_view_voice_record_bar.cpp";
	const auto voiceText = ReadSource(voice);
	if (!voiceText.contains(u"send_voice_message_question"_q)) {
		problems.push_back(u"the voice message confirmation is not found"_q);
	}
	confirmBoxes(QString::fromUtf8(voice), voiceText, true);
	confirmBoxes(
		u"media_view_overlay_widget.cpp"_q,
		ReadSource("media/view/media_view_overlay_widget.cpp"),
		true);
	// The update bar follows the language
	const auto bar = ReadSource("mzgram/mzgram_update_bar.cpp");
	for (const auto key : { "update_page", "update_open", "update_retry", "update_close" }) {
		if (!bar.contains(u"follow("_q) || !bar.contains(u"\""_q + QString::fromLatin1(key) + u"\");"_q)) {
			problems.push_back(u"update bar: "_q + QString::fromLatin1(key) + u" does not follow the language"_q);
		}
	}
	if (!bar.contains(u"Tr(\"update_available\")"_q)) {
		problems.push_back(u"update bar: the text does not follow the language"_q);
	}
	// Every button phrase is in every language
	for (const auto code : MZGram::kLanguages) {
		const auto language = QString::fromLatin1(code);
		for (const auto key : { "button_cancel", "button_ok", "button_save", "button_delete", "button_close", "button_send" }) {
			const auto text = MZGram::TranslateTo(key, language);
			if (text.isEmpty() || text == QString::fromLatin1(key)) {
				problems.push_back(language + ' ' + QString::fromLatin1(key) + u": missing"_q);
			} else if (language != u"en"_q
				&& QString::fromLatin1(key) != u"button_ok"_q
				&& text == MZGram::TranslateTo(key, u"en"_q)) {
				problems.push_back(language + ' ' + QString::fromLatin1(key) + u": English"_q);
			}
		}
	}
	Check(
		problems.isEmpty() && checked > 10,
		"MZGram's boxes, buttons, dates and update bar are in the MZGram language",
		problems.join(u", "_q).toStdString().c_str());
}

// Another launch of MZGram passes its links to the running one as records
// ending with ';' (mzgram_ipc_rules.h). A link with ';' or '%' in it stays
// one OPEN record, in 7 bits, and comes back as the same link.
void TestIpcLinkStaysOneRecord() {
	using namespace MZGram;
	const auto links = std::vector<QString>{
		u"tg://join?invite=AbC_dEf"_q,
		u"tg://resolve?domain=mzgram&text=a;b"_q,
		u"tg://resolve?domain=mzgram;OPEN:file:///C:/notes.txt"_q,
		u"tg://msg_url?url=https://example.com/a%3Bb&text=100%25;"_q,
		u"tg://resolve?domain=mzgram&text=%D0%BF%D1%80%D0%B8%D0%B2%D1%96%D1%82"_q,
	};
	auto problems = QStringList();
	for (const auto &link : links) {
		const auto url = QUrl(link);
		const auto record = IpcOpenRecord(url);
		auto buffer = record.toLatin1();
		const auto records = IpcRecords(buffer);
		const auto sevenBit = std::all_of(
			record.begin(),
			record.end(),
			[](QChar ch) { return ch.unicode() >= 32 && ch.unicode() < 128; });
		if (records.size() != 1 || !buffer.isEmpty()) {
			problems.push_back(link + u": "_q
				+ QString::number(records.size()) + u" records"_q);
		} else if (!records[0].startsWith(u"OPEN:"_q)
			|| IpcOpenUrl(records[0].mid(5)) != url) {
			problems.push_back(link + u": comes back as "_q
				+ IpcOpenUrl(records[0].mid(5)).toString());
		} else if (!sevenBit) {
			problems.push_back(link + u": not 7 bits"_q);
		}
	}
	// What follows the last ';' waits for the next read.
	auto buffer = (IpcOpenRecord(QUrl(links[0])) + u"OPEN:tg:"_q).toLatin1();
	if (IpcRecords(buffer).size() != 1 || buffer != "OPEN:tg:") {
		problems.push_back(u"the rest of a record is not kept"_q);
	}
	Check(
		problems.isEmpty(),
		"a link with ';' stays one record between two launches",
		problems.join(u", "_q).toStdString().c_str());
}

// A launch with a link passes that link, a launch to send files passes only
// local files. Local files coming in the same connection as a link are left
// out, in that read and in the next reads of that connection.
void TestIpcLocalFilesWithLinkAreDropped() {
	using namespace MZGram;
	const auto link = QUrl(u"tg://resolve?domain=mzgram"_q);
	const auto file = QUrl::fromLocalFile(u"C:/notes.txt"_q);
	const auto other = QUrl::fromLocalFile(u"C:/photo.jpg"_q);
	auto problems = QStringList();

	auto withLink = false;
	const auto first = IpcFilterStartUrls({ file, link }, withLink);
	if (first.open != QList<QUrl>{ link }
		|| first.dropped != QList<QUrl>{ file }
		|| !withLink) {
		problems.push_back(u"a local file with a link is opened"_q);
	}
	const auto next = IpcFilterStartUrls({ other }, withLink);
	if (!next.open.isEmpty() || next.dropped != QList<QUrl>{ other }) {
		problems.push_back(u"a local file after a link is opened"_q);
	}

	auto onlyFiles = false;
	const auto files = IpcFilterStartUrls({ file, other }, onlyFiles);
	if (files.open != QList<QUrl>{ file, other }
		|| !files.dropped.isEmpty()
		|| onlyFiles) {
		problems.push_back(u"local files alone are not opened"_q);
	}
	auto onlyLink = false;
	const auto links = IpcFilterStartUrls({ link }, onlyLink);
	if (links.open != QList<QUrl>{ link } || !links.dropped.isEmpty()) {
		problems.push_back(u"a link alone is not opened"_q);
	}
	Check(
		problems.isEmpty(),
		"local files coming with a link from another launch are left out",
		problems.join(u", "_q).toStdString().c_str());
}

// The running app reads and writes those records with these rules, and keeps
// "a link came" for the whole connection.
void TestIpcRulesAreWired() {
	auto file = QFile(
		QString::fromUtf8(MZGRAM_SOURCE_DIR) + u"/core/sandbox.cpp"_q);
	const auto text = file.open(QIODevice::ReadOnly)
		? QString::fromUtf8(file.readAll())
		: QString();
	auto missing = QStringList();
	for (const auto &call : {
			u"commands += MZGram::IpcOpenRecord(url);"_q,
			u"MZGram::IpcRecords(client->buffer)"_q,
			u"MZGram::IpcOpenUrl(cmd.mid(5))"_q,
			u"alive->externalUrlReceived = externalUrlReceived;"_q,
			u"for (const auto &url : filtered.open) {"_q,
		}) {
		if (!text.contains(call)) {
			missing.push_back(call);
		}
	}
	if (text.contains(u"url.toString(QUrl::FullyEncoded) + ';'"_q)) {
		missing.push_back(u"a link written as it is"_q);
	}
	Check(
		missing.isEmpty(),
		"the running app uses the rules for links from another launch",
		missing.join(u", "_q).toStdString().c_str());
}

// tg://, tonsite:// and ton:// on Windows: the executable of a handler's
// command, and when MZGram may take a protocol over.
void TestUrlSchemeRules() {
	using namespace MZGram;
	auto problems = QStringList();
	const auto quoted = u"\"C:\\Program Files\\MZGram\\MZGram.exe\" -workdir \"C:/data/\" -- \"%1\""_q;
	if (CommandExecutable(quoted) != u"C:\\Program Files\\MZGram\\MZGram.exe"_q) {
		problems.push_back(u"quoted: "_q + CommandExecutable(quoted));
	}
	if (CommandExecutable(u"C:\\Telegram\\Telegram.exe -- \"%1\""_q) != u"C:\\Telegram\\Telegram.exe"_q
		|| !CommandExecutable(QString()).isEmpty()) {
		problems.push_back(u"not quoted or empty"_q);
	}
	if (!IsMZGramExecutable(u"C:\\Temp\\x.zip.610\\MZGram\\mzgram.EXE"_q)
		|| IsMZGramExecutable(u"C:\\Telegram\\Telegram.exe"_q)
		|| IsMZGramExecutable(u"C:\\MZGram\\NotMZGram.exe"_q)) {
		problems.push_back(u"MZGram.exe told apart wrong"_q);
	}
	const auto all = [](const QString &) { return true; };
	const auto none = [](const QString &) { return false; };
	const auto telegram = u"\"C:\\Telegram\\Telegram.exe\" -- \"%1\""_q;
	if (MayTakeUrlScheme(telegram, all)) {
		problems.push_back(u"takes Telegram Desktop's links"_q);
	}
	if (!MayTakeUrlScheme(telegram, none)) {
		problems.push_back(u"leaves a handler that is not there any more"_q);
	}
	if (!MayTakeUrlScheme(QString(), all)
		|| !MayTakeUrlScheme(u"\"D:\\old\\MZGram.exe\" -- \"%1\""_q, all)) {
		problems.push_back(u"leaves no handler or an older MZGram"_q);
	}

	// Windows reads an unquoted path with spaces up to the first .exe
	const auto spaces = u"C:\\Program Files\\Other App\\Other.exe -- \"%1\""_q;
	if (CommandExecutable(spaces) != u"C:\\Program Files\\Other App\\Other.exe"_q) {
		problems.push_back(u"unquoted with spaces: "_q + CommandExecutable(spaces));
	}
	const auto onlyOther = [](const QString &path) {
		return (path == u"C:\\Program Files\\Other App\\Other.exe"_q);
	};
	if (MayTakeUrlScheme(spaces, onlyOther)) {
		problems.push_back(u"takes an unquoted handler with spaces"_q);
	}

	// Linux: GLib quotes with ', the binary is MZGram
	const auto linuxCommand = u"'/opt/My Apps/MZGram/MZGram' -- %u"_q;
	if (CommandExecutable(linuxCommand) != u"/opt/My Apps/MZGram/MZGram"_q
		|| !IsMZGramExecutable(CommandExecutable(linuxCommand))
		|| IsMZGramExecutable(u"/usr/bin/telegram-desktop"_q)
		|| IsMZGramExecutable(u"/opt/MZGram/MZGram.sh"_q)) {
		problems.push_back(u"Linux command: "_q + CommandExecutable(linuxCommand));
	}
	const auto flatpak = u"/usr/bin/flatpak run --command=Telegram org.telegram.desktop -- @@u %u @@"_q;
	if (CommandExecutable(flatpak) != u"/usr/bin/flatpak"_q) {
		problems.push_back(u"flatpak: "_q + CommandExecutable(flatpak));
	}
	if (!HandlerExecutableExists(u"telegram-desktop"_q)
		|| HandlerExecutableExists(u"/mzgram/tests/not/there/Telegram"_q)
		|| MayTakeUrlScheme(u"telegram-desktop -- %u"_q, HandlerExecutableExists)) {
		problems.push_back(u"a Linux handler found in PATH is taken"_q);
	}
	Check(
		problems.isEmpty(),
		"MZGram takes tg:// only from nobody or from MZGram",
		problems.join(u", "_q).toStdString().c_str());
}

#ifdef _WIN32

// The registration goes into HKEY_CURRENT_USER under a key of the test's
// own instead of Software, next to the keys Telegram Desktop 7.3.1 writes
// (lib_base, base_url_scheme_win.cpp).
const auto kUrlSchemeTestRoot = u"Software\\MZGramTests\\UrlScheme\\"_q;

QString TestRegRead(const QString &key, const QString &name = QString()) {
	const auto wkey = (kUrlSchemeTestRoot + key).toStdWString();
	const auto wname = name.toStdWString();
	auto buffer = std::wstring(4096, L'\0');
	auto size = DWORD(buffer.size() * sizeof(WCHAR));
	if (RegGetValue(HKEY_CURRENT_USER, wkey.c_str(), wname.c_str(), RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, buffer.data(), &size) != ERROR_SUCCESS) {
		return u"(none)"_q;
	}
	return QString::fromWCharArray(buffer.c_str());
}

void TestRegWrite(const QString &key, const QString &name, const QString &value, DWORD type = REG_SZ) {
	const auto wkey = (kUrlSchemeTestRoot + key).toStdWString();
	const auto wname = name.toStdWString();
	const auto wvalue = value.toStdWString();
	auto handle = HKEY();
	if (RegCreateKeyEx(HKEY_CURRENT_USER, wkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr) != ERROR_SUCCESS) {
		return;
	}
	RegSetValueEx(handle, wname.empty() ? nullptr : wname.c_str(), 0, type, reinterpret_cast<const BYTE*>(wvalue.c_str()), DWORD((wvalue.size() + 1) * sizeof(WCHAR)));
	RegCloseKey(handle);
}

bool TestRegExists(const QString &key) {
	const auto wkey = (kUrlSchemeTestRoot + key).toStdWString();
	auto handle = HKEY();
	if (RegOpenKeyEx(HKEY_CURRENT_USER, wkey.c_str(), 0, KEY_READ, &handle) != ERROR_SUCCESS) {
		return false;
	}
	RegCloseKey(handle);
	return true;
}

void TestRegClear() {
	const auto wkey = kUrlSchemeTestRoot.toStdWString();
	RegDeleteTree(HKEY_CURRENT_USER, wkey.c_str());
}

QString TestOpenCommand(const QString &exe) {
	return '"' + QDir::toNativeSeparators(exe) + u"\"  -- \"%1\""_q;
}

void TestTouch(const QString &path) {
	QDir().mkpath(QFileInfo(path).path());
	auto file = QFile(path);
	if (file.open(QIODevice::WriteOnly)) {
		file.close();
	}
}

// Telegram Desktop as it registers itself, or MZGram up to 1.0.4 that did
// the same with its own name and path.
void TestRegisterAsTelegram(const QString &exe, const QString &name, bool legacy) {
	for (const auto &scheme : MZGram::UrlSchemes()) {
		const auto named = u"tdesktop."_q + scheme.protocol;
		TestRegWrite(u"Classes\\"_q + named + u"\\shell\\open\\command"_q, QString(), TestOpenCommand(exe));
		TestRegWrite(u"TelegramDesktop\\Capabilities\\UrlAssociations"_q, scheme.protocol, named);
		if (legacy) {
			TestRegWrite(u"Classes\\"_q + scheme.protocol + u"\\shell\\open\\command"_q, QString(), TestOpenCommand(exe));
		}
	}
	TestRegWrite(u"TelegramDesktop\\Capabilities"_q, u"ApplicationName"_q, name);
	TestRegWrite(u"TelegramDesktop\\Capabilities"_q, u"ApplicationDescription"_q, name);
	TestRegWrite(u"RegisteredApplications"_q, name, u"SOFTWARE\\TelegramDesktop\\Capabilities"_q);
}

void TestRegisterMZGram(const QString &exe, bool force = false, const QString &arguments = QString()) {
	MZGram::RegisterUrlSchemes({
		.root = kUrlSchemeTestRoot,
		.executable = QDir::toNativeSeparators(exe),
		.arguments = arguments,
		.appName = u"MZGram Desktop"_q,
		.force = force,
	});
}

// Telegram Desktop's installer (Telegram/build/setup.iss) leaves its folder
// in its uninstall entry.
void TestRegisterTelegramInstaller(const QString &exe) {
	TestRegWrite(
		u"Microsoft\\Windows\\CurrentVersion\\Uninstall\\{53F49750-6209-4FBF-9CA8-7A333C87D1ED}_is1"_q,
		u"InstallLocation"_q,
		QDir::toNativeSeparators(QFileInfo(exe).path()) + '\\');
}

// MZGram registers its own entry for Windows Settings > Default apps and
// leaves Telegram Desktop's registration and handler as they are.
void TestUrlSchemeKeepsTelegramDesktop() {
	auto dir = QTemporaryDir();
	const auto telegramExe = dir.path() + u"/Telegram/Telegram.exe"_q;
	const auto mzgramExe = dir.path() + u"/MZGram/MZGram.exe"_q;
	TestTouch(telegramExe);
	TestTouch(mzgramExe);
	const auto telegram = TestOpenCommand(telegramExe);
	const auto mzgram = TestOpenCommand(mzgramExe);
	auto problems = QStringList();

	TestRegClear();
	TestRegisterAsTelegram(telegramExe, u"Telegram Desktop"_q, true);
	TestRegisterMZGram(mzgramExe);
	for (const auto &scheme : MZGram::UrlSchemes()) {
		const auto p = scheme.protocol;
		if (TestRegRead(u"Classes\\tdesktop."_q + p + u"\\shell\\open\\command"_q) != telegram) {
			problems.push_back(u"tdesktop."_q + p + u" changed"_q);
		}
		if (TestRegRead(u"Classes\\"_q + p + u"\\shell\\open\\command"_q) != telegram) {
			problems.push_back(p + u":// taken from Telegram Desktop"_q);
		}
		if (TestRegRead(u"Classes\\mzgram."_q + p + u"\\shell\\open\\command"_q) != mzgram
			|| TestRegRead(u"MZGramDesktop\\Capabilities\\UrlAssociations"_q, p) != u"mzgram."_q + p) {
			problems.push_back(u"no own "_q + p);
		}
	}
	if (TestRegRead(u"TelegramDesktop\\Capabilities"_q, u"ApplicationName"_q) != u"Telegram Desktop"_q) {
		problems.push_back(u"Telegram Desktop's entry renamed"_q);
	}
	if (TestRegRead(u"RegisteredApplications"_q, u"MZGram Desktop"_q) != kUrlSchemeTestRoot + u"MZGramDesktop\\Capabilities"_q
		|| TestRegRead(u"MZGramDesktop\\Capabilities"_q, u"ApplicationName"_q) != u"MZGram Desktop"_q) {
		problems.push_back(u"no own Default apps entry"_q);
	}

	TestRegisterMZGram(mzgramExe, true);
	if (TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != mzgram
		|| TestRegRead(u"Classes\\tdesktop.tg\\shell\\open\\command"_q) != telegram) {
		problems.push_back(u"registertg does not take tg://"_q);
	}

	TestRegClear();
	TestRegWrite(u"Classes\\tg\\shell\\open\\command"_q, QString(), TestOpenCommand(dir.path() + u"/Gone/Telegram.exe"_q));
	TestRegisterMZGram(mzgramExe);
	if (TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != mzgram
		|| TestRegRead(u"Classes\\ton\\shell\\open\\command"_q) != mzgram) {
		problems.push_back(u"no handler or a gone one stays"_q);
	}
	// Handlers written as REG_EXPAND_SZ or unquoted with spaces
	const auto otherExe = dir.path() + u"/Other App/Other.exe"_q;
	TestTouch(otherExe);
	qputenv("MZGRAM_TEST_URL_SCHEME", QDir::toNativeSeparators(dir.path()).toUtf8());
	const auto expanded = u"\"%MZGRAM_TEST_URL_SCHEME%\\Other App\\Other.exe\" -- \"%1\""_q;
	const auto unquoted = QDir::toNativeSeparators(otherExe) + u" -- \"%1\""_q;
	TestRegClear();
	TestRegWrite(u"Classes\\tg\\shell\\open\\command"_q, QString(), expanded, REG_EXPAND_SZ);
	TestRegWrite(u"Classes\\ton\\shell\\open\\command"_q, QString(), unquoted);
	TestRegisterMZGram(mzgramExe);
	if (TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) == mzgram) {
		problems.push_back(u"a REG_EXPAND_SZ handler taken"_q);
	}
	if (TestRegRead(u"Classes\\ton\\shell\\open\\command"_q) != unquoted) {
		problems.push_back(u"an unquoted handler with spaces taken"_q);
	}

	// The working folder of a -workdir launch
	TestRegClear();
	TestRegisterMZGram(mzgramExe, false, u"-workdir \"C:/data/\""_q);
	const auto workdir = '"' + QDir::toNativeSeparators(mzgramExe) + u"\" -workdir \"C:/data/\" -- \"%1\""_q;
	if (TestRegRead(u"Classes\\mzgram.tg\\shell\\open\\command"_q) != workdir
		|| TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != workdir) {
		problems.push_back(u"-workdir lost"_q);
	}
	TestRegClear();
	Check(
		problems.isEmpty(),
		"MZGram registers its links apart from Telegram Desktop",
		problems.join(u", "_q).toStdString().c_str());
}

// What MZGram up to 1.0.4 wrote into Telegram Desktop's registration goes
// back to Telegram Desktop, or away when Telegram Desktop is not there.
void TestUrlSchemeCleansUpTelegramKeys() {
	auto dir = QTemporaryDir();
	const auto telegramExe = dir.path() + u"/Telegram/Telegram.exe"_q;
	const auto oldExe = dir.path() + u"/Old/MZGram/MZGram.exe"_q;
	const auto mzgramExe = dir.path() + u"/MZGram/MZGram.exe"_q;
	TestTouch(telegramExe);
	TestTouch(oldExe);
	TestTouch(mzgramExe);
	const auto telegram = TestOpenCommand(telegramExe);
	const auto old = TestOpenCommand(oldExe);
	const auto mzgram = TestOpenCommand(mzgramExe);
	auto problems = QStringList();

	// Telegram Desktop, then MZGram 1.0.4 over it, then Telegram Desktop's
	// handler back with its next version.
	TestRegClear();
	TestRegisterAsTelegram(telegramExe, u"Telegram Desktop"_q, true);
	TestRegisterAsTelegram(oldExe, u"MZGram Desktop"_q, false);
	TestRegisterMZGram(mzgramExe);
	if (TestRegRead(u"Classes\\tdesktop.tg\\shell\\open\\command"_q) != telegram
		|| TestRegRead(u"Classes\\tdesktop.ton\\shell\\open\\command"_q) != telegram) {
		problems.push_back(u"Telegram Desktop's classes still open MZGram"_q);
	}
	if (TestRegRead(u"TelegramDesktop\\Capabilities"_q, u"ApplicationName"_q) != u"Telegram Desktop"_q
		|| TestRegRead(u"TelegramDesktop\\Capabilities"_q, u"ApplicationDescription"_q) != u"Telegram Desktop"_q) {
		problems.push_back(u"Telegram Desktop's entry still MZGram"_q);
	}
	if (TestRegRead(u"RegisteredApplications"_q, u"MZGram Desktop"_q) != kUrlSchemeTestRoot + u"MZGramDesktop\\Capabilities"_q) {
		problems.push_back(u"MZGram Desktop still on Telegram Desktop's entry"_q);
	}

	// Telegram Desktop, then MZGram 1.0.4 over all of it: the path of
	// Telegram Desktop is not known, its classes wait for its next version.
	TestRegClear();
	TestRegisterAsTelegram(telegramExe, u"Telegram Desktop"_q, true);
	TestRegisterAsTelegram(oldExe, u"MZGram Desktop"_q, true);
	TestRegisterMZGram(mzgramExe);
	if (TestRegRead(u"Classes\\tdesktop.tg\\shell\\open\\command"_q) != old
		|| TestRegRead(u"TelegramDesktop\\Capabilities"_q, u"ApplicationName"_q) != u"Telegram Desktop"_q
		|| TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != mzgram) {
		problems.push_back(u"Telegram Desktop's keys over 1.0.4 handled wrong"_q);
	}

	// MZGram 1.0.4 alone.
	TestRegClear();
	TestRegisterAsTelegram(oldExe, u"MZGram Desktop"_q, true);
	TestRegisterMZGram(mzgramExe);
	if (TestRegExists(u"Classes\\tdesktop.tg"_q)
		|| TestRegExists(u"Classes\\tdesktop.tonsite"_q)
		|| TestRegExists(u"TelegramDesktop"_q)) {
		problems.push_back(u"the keys of 1.0.4 stay without Telegram Desktop"_q);
	}
	if (TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != mzgram
		|| TestRegRead(u"Classes\\mzgram.tg\\shell\\open\\command"_q) != mzgram) {
		problems.push_back(u"tg:// does not open this MZGram"_q);
	}

	// MZGram 1.0.4 without Telegram Desktop, another app took tg:// since:
	// Telegram Desktop's class goes, it never opens the other app.
	const auto otherExe = dir.path() + u"/Other/Other.exe"_q;
	TestTouch(otherExe);
	const auto other = TestOpenCommand(otherExe);
	TestRegClear();
	TestRegisterAsTelegram(oldExe, u"MZGram Desktop"_q, true);
	TestRegWrite(u"Classes\\tg\\shell\\open\\command"_q, QString(), other);
	TestRegisterMZGram(mzgramExe);
	if (TestRegExists(u"Classes\\tdesktop.tg"_q)
		|| TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != other) {
		problems.push_back(u"Telegram Desktop's class opens another app"_q);
	}

	// Telegram Desktop from its installer, MZGram 1.0.4 over all of it:
	// Telegram.exe is found in the installer's folder.
	TestRegClear();
	TestRegisterAsTelegram(telegramExe, u"Telegram Desktop"_q, true);
	TestRegisterTelegramInstaller(telegramExe);
	TestRegisterAsTelegram(oldExe, u"MZGram Desktop"_q, true);
	TestRegisterMZGram(mzgramExe);
	if (TestRegRead(u"Classes\\tdesktop.tg\\shell\\open\\command"_q) != telegram
		|| TestRegRead(u"Classes\\tdesktop.ton\\shell\\open\\command"_q) != telegram
		|| TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != telegram
		|| TestRegRead(u"Classes\\tonsite\\shell\\open\\command"_q) != telegram) {
		problems.push_back(u"Telegram Desktop from its installer not given back"_q);
	}

	// Telegram Desktop with its own class, tg:// taken by MZGram 1.0.0
	// from a zip in Temp: tg:// goes back to Telegram Desktop.
	TestRegClear();
	TestRegisterAsTelegram(telegramExe, u"Telegram Desktop"_q, false);
	TestRegWrite(u"Classes\\tg\\shell\\open\\command"_q, QString(), old);
	TestRegisterMZGram(mzgramExe);
	if (TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != telegram) {
		problems.push_back(u"tg:// of an old MZGram not given back"_q);
	}

	// registertg is kept: the next version does not give tg:// back.
	TestRegisterMZGram(mzgramExe, true);
	TestRegisterMZGram(mzgramExe);
	if (TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != mzgram) {
		problems.push_back(u"registertg undone by the next version"_q);
	}

	// Telegram Desktop not found at first, then its next version writes its
	// class again: tg:// goes back to it then.
	TestRegClear();
	TestRegisterAsTelegram(telegramExe, u"Telegram Desktop"_q, true);
	TestRegisterAsTelegram(oldExe, u"MZGram Desktop"_q, true);
	TestRegisterMZGram(mzgramExe);
	TestRegisterAsTelegram(telegramExe, u"Telegram Desktop"_q, false);
	TestRegisterMZGram(mzgramExe);
	if (TestRegRead(u"Classes\\tg\\shell\\open\\command"_q) != telegram) {
		problems.push_back(u"tg:// not given back once Telegram Desktop is found"_q);
	}
	TestRegClear();
	Check(
		problems.isEmpty(),
		"MZGram gives Telegram Desktop's registration back",
		problems.join(u", "_q).toStdString().c_str());
}

#endif // _WIN32

// The app registers its links with these rules on every platform.
void TestUrlSchemeIsWired() {
	const auto read = [](const QString &path) {
		auto file = QFile(QString::fromUtf8(MZGRAM_SOURCE_DIR) + path);
		return file.open(QIODevice::ReadOnly)
			? QString::fromUtf8(file.readAll())
			: QString();
	};
	const auto application = read(u"/core/application.cpp"_q);
	const auto codes = read(u"/settings/settings_codes.cpp"_q);
	auto missing = QStringList();
	for (const auto &call : {
			u"MZGram::RegisterUrlSchemes({"_q,
			u"if (!force && !MZGram::MayTakeUrlScheme("_q,
			u"MZGram::DefaultUrlSchemeCommand(scheme.protocol),"_q,
			u"MZGram::HandlerExecutableExists)) {"_q,
			u"MZGram::OfferForUrlScheme({"_q,
		}) {
		if (!application.contains(call)) {
			missing.push_back(call);
		}
	}
	if (application.contains(u".shortAppName = u\"tdesktop\"_q"_q)) {
		missing.push_back(u"Telegram Desktop's names"_q);
	}
	if (!codes.contains(u"Core::Application::RegisterUrlScheme(true);"_q)) {
		missing.push_back(u"registertg"_q);
	}
	Check(
		missing.isEmpty(),
		"the app registers its links with MZGram's names",
		missing.join(u", "_q).toStdString().c_str());
}

// "Ask AI" (mzgram_ai_rules.h): the five sites, what goes into their chat
// and where the window may go.
void TestAiProviders() {
	using namespace MZGram::Ai;
	auto problems = QStringList();
	const auto &providers = Providers();
	const auto expected = std::vector<std::tuple<Provider, const char*, const char*, const char*>>{
		{ Provider::Gemini, "gemini", "Gemini", "https://gemini.google.com/app" },
		{ Provider::Claude, "claude", "Claude", "https://claude.ai/new" },
		{ Provider::ChatGpt, "chatgpt", "ChatGPT", "https://chatgpt.com/" },
		{ Provider::DeepSeek, "deepseek", "DeepSeek", "https://chat.deepseek.com/" },
		{ Provider::Grok, "grok", "Grok", "https://grok.com/" },
	};
	if (providers.size() != expected.size()) {
		problems.push_back(u"not the five sites"_q);
	} else {
		for (auto i = 0; i != int(expected.size()); ++i) {
			const auto &[id, key, name, url] = expected[i];
			const auto &info = providers[i];
			const auto label = QString::fromLatin1(key);
			if (info.id != id
				|| QString::fromLatin1(info.key) != label
				|| QString::fromLatin1(info.name) != QString::fromLatin1(name)) {
				problems.push_back(label + u": id, key or name"_q);
			}
			if (QString::fromLatin1(info.startUrl) != QString::fromLatin1(url)) {
				problems.push_back(label + u": start address"_q);
			}
			if (&Info(id) != &info) {
				problems.push_back(label + u": Info"_q);
			}
			if (ProviderByKey(label) != id) {
				problems.push_back(label + u": by key"_q);
			}
			// No text in the address: no site only fills the field from it
			const auto start = QUrl(QString::fromLatin1(info.startUrl));
			if (start.hasQuery() || start.scheme() != u"https"_q) {
				problems.push_back(label + u": a query or not https"_q);
			}
			if (NewWindowTarget(id, QString::fromLatin1(info.startUrl)) != NewWindow::Here) {
				problems.push_back(label + u": its own site leaves the window"_q);
			}
			if (info.inputSelectors.empty()) {
				problems.push_back(label + u": no input field"_q);
			}
			if (info.prefill != Prefill::Script || info.model != ModelChoice::OnSite) {
				problems.push_back(label + u": a prefill or model nobody checked"_q);
			}
		}
	}
	if (ProviderByKey(u"bard"_q) || ProviderByKey(QString())) {
		problems.push_back(u"an unknown key is a site"_q);
	}
	Check(problems.isEmpty(), "ai: Gemini, Claude, ChatGPT, DeepSeek and Grok, each with its site and input field", problems.join(u", "_q).toStdString().c_str());
}

// Settings > MZGram > AI: the default site is kept.
void TestAiDefaultProvider() {
	using namespace MZGram::Ai;
	auto problems = QStringList();
	for (const auto provider : { Provider::Claude, Provider::Grok, Provider::ChatGpt, Provider::DeepSeek, Provider::Gemini }) {
		SetDefaultProvider(provider);
		if (DefaultProvider() != provider) {
			problems.push_back(QString::number(int(provider)));
		}
	}
	Check(problems.isEmpty(), "ai: the default site is kept", problems.join(u", "_q).toStdString().c_str());
}

// The prompt, an empty line, the message.
void TestAiComposeText() {
	using namespace MZGram::Ai;
	auto problems = QStringList();
	const auto expect = [&](const QString &prompt, const QString &message, const QString &result) {
		const auto composed = ComposeText(prompt, message);
		if (composed != result) {
			problems.push_back(u"\""_q + prompt + u"\" + \""_q + message + u"\" = \""_q + composed + '"');
		}
	};
	expect(QString(), u"Hello"_q, u"Hello"_q);
	expect(u"Translate to English"_q, QString::fromUtf8("Привіт"), QString::fromUtf8("Translate to English\n\nПривіт"));
	expect(u"  Summarize \r\n"_q, u" line1\r\nline2 "_q, u"Summarize\n\nline1\nline2"_q);
	expect(u"Explain"_q, u"  "_q, u"Explain"_q);
	expect(QString(), QString(), QString());
	Check(problems.isEmpty(), "ai: the prompt and the message", problems.join(u", "_q).toStdString().c_str());
}

// Any text stays one string literal of the script.
void TestAiScriptString() {
	using namespace MZGram::Ai;
	auto problems = QStringList();
	const auto samples = {
		u"He said \"hi\" \\ 'yes'"_q,
		u"</script><script>alert(1)</script>"_q,
		u"line1\nline2\r\n\tend"_q,
		QString(QString::fromUtf8("Привіт 👋 ") + QChar(0x2028) + QChar(0x2029) + QChar(0)),
		u"${x}`'&<>"_q,
		QString(),
	};
	for (const auto &sample : samples) {
		const auto literal = ScriptString(sample);
		auto ascii = !literal.isEmpty();
		for (const auto byte : literal) {
			if (uchar(byte) >= 0x80 || uchar(byte) < 0x20) {
				ascii = false;
			}
		}
		if (!ascii || !literal.startsWith('"') || !literal.endsWith('"')) {
			problems.push_back(sample + u": not one ASCII literal"_q);
			continue;
		}
		const auto inner = literal.mid(1, literal.size() - 2);
		for (const auto bad : { '"', '\'', '<', '>', '&', '\n', '\r' }) {
			if (inner.contains(bad)) {
				problems.push_back(sample + u": a raw "_q + QChar(bad));
			}
		}
		const auto parsed = QJsonDocument::fromJson("[" + literal + "]");
		if (!parsed.isArray() || parsed.array().at(0).toString() != sample) {
			problems.push_back(sample + u": another text"_q);
		}
	}
	Check(problems.isEmpty(), "ai: any text is one escaped string of the script", problems.join(u", "_q).toStdString().c_str());
}

// The script fills the field and never sends.
void TestAiInsertScript() {
	using namespace MZGram::Ai;
	auto problems = QStringList();
	const auto text = u"Line \"one\"\n</script>"_q;
	auto scripts = QSet<QByteArray>();
	for (const auto &info : Providers()) {
		const auto label = QString::fromLatin1(info.key);
		const auto script = InsertScript(info.id, text);
		scripts.insert(script);
		if (!script.contains(ScriptString(text))) {
			problems.push_back(label + u": not the escaped text"_q);
		}
		if (script.contains("</script>") || script.contains("Line \"one\"")) {
			problems.push_back(label + u": the raw text"_q);
		}
		for (const auto selector : info.inputSelectors) {
			if (!script.contains(ScriptString(QString::fromLatin1(selector)))) {
				problems.push_back(label + u": no selector "_q + QString::fromLatin1(selector));
			}
		}
		if (!script.contains("window.external.invoke")
			|| !script.contains("inserted")
			|| !script.contains("no_field")) {
			problems.push_back(label + u": does not say what happened"_q);
		}
		for (const auto sends : { "click(", "submit(", "requestSubmit", "KeyboardEvent", "keydown", "keypress", "keyup", "Enter" }) {
			if (script.contains(sends)) {
				problems.push_back(label + u": "_q + QString::fromLatin1(sends));
			}
		}
	}
	if (scripts.size() != 5) {
		problems.push_back(u"not a script per site"_q);
	}
	Check(problems.isEmpty(), "ai: the script fills the input field and never sends", problems.join(u", "_q).toStdString().c_str());
}

void TestAiStatus() {
	using namespace MZGram::Ai;
	Check(ParseStatus("{\"mzgram_ai\":\"inserted\"}") == Status::Inserted
		&& ParseStatus("{\"mzgram_ai\":\"no_field\"}") == Status::NoField
		&& ParseStatus("{\"mzgram_ai\":\"sent\"}") == Status::None
		&& ParseStatus("{}") == Status::None
		&& ParseStatus("inserted") == Status::None
		&& ParseStatus(QByteArray()) == Status::None,
		"ai: what the script says",
		"");
}

// Only https in the window; the site and the sign-in pages stay in it.
void TestAiNavigation() {
	using namespace MZGram::Ai;
	auto problems = QStringList();
	for (const auto &[url, allowed] : std::vector<std::pair<QString, bool>>{
		{ u"https://claude.ai/login"_q, true },
		{ u"https://accounts.google.com/v3/signin"_q, true },
		{ u"about:blank"_q, true },
		{ u"http://claude.ai/"_q, false },
		{ u"javascript:alert(1)"_q, false },
		{ u"file:///C:/Users"_q, false },
		{ u"data:text/html,x"_q, false },
		{ QString(), false },
	}) {
		if (AllowedNavigation(url) != allowed) {
			problems.push_back(url);
		}
	}
	using Target = NewWindow;
	for (const auto &[provider, url, target] : std::vector<std::tuple<Provider, QString, Target>>{
		{ Provider::Gemini, u"https://accounts.google.com/signin"_q, Target::Here },
		{ Provider::Claude, u"https://claude.ai/chat/1"_q, Target::Here },
		{ Provider::ChatGpt, u"https://auth.openai.com/log-in"_q, Target::Here },
		{ Provider::ChatGpt, u"https://login.microsoftonline.com/common"_q, Target::Here },
		{ Provider::Grok, u"https://accounts.x.ai/sign-in"_q, Target::Here },
		{ Provider::DeepSeek, u"https://chat.deepseek.com/sign_in"_q, Target::Here },
		{ Provider::Claude, u"https://appleid.apple.com/auth"_q, Target::Here },
		{ Provider::Claude, u"https://en.wikipedia.org/wiki/Kyiv"_q, Target::Browser },
		{ Provider::Gemini, u"https://claude.ai/"_q, Target::Browser },
		{ Provider::ChatGpt, u"https://chatgpt.com.example.org/"_q, Target::Browser },
		{ Provider::ChatGpt, u"https://evilchatgpt.com/"_q, Target::Browser },
		{ Provider::Claude, u"https://claude.ai@example.org/"_q, Target::Browser },
		{ Provider::Claude, u"http://claude.ai/"_q, Target::Refuse },
		{ Provider::Claude, u"javascript:alert(1)"_q, Target::Refuse },
		{ Provider::Grok, u"data:text/html,x"_q, Target::Refuse },
		{ Provider::Grok, u"file:///etc/passwd"_q, Target::Refuse },
	}) {
		if (NewWindowTarget(provider, url) != target) {
			problems.push_back(u"new window "_q + url);
		}
	}
	Check(problems.isEmpty(), "ai: the window goes only to https, the site and its sign-in stay in it", problems.join(u", "_q).toStdString().c_str());
}

// Every text of "Ask AI" is in the table (and so, by
// TestEveryPhraseInEveryLanguage, in the twelve languages).
void TestAiTexts() {
	auto keys = QSet<QString>();
	for (const auto &phrase : MZGram::Phrases()) {
		keys.insert(QString::fromUtf8(phrase.key));
	}
	auto missing = QStringList();
	for (const auto key : {
		"section_ai",
		"ai_show_action",
		"ai_default_provider",
		"ai_sign_out",
		"ai_sign_out_confirm",
		"ai_signed_out",
		"ai_info",
		"ai_ask",
		"ai_message",
		"ai_prompt",
		"ai_provider",
		"ai_model",
		"ai_model_on_site",
		"ai_open",
		"ai_privacy_title",
		"ai_privacy_text",
		"ai_privacy_note",
		"ai_continue",
		"ai_status_opening",
		"ai_status_inserted",
		"ai_status_no_field",
		"ai_insert",
		"ai_copy",
		"ai_copied",
		"ai_no_webview",
	}) {
		if (!keys.contains(QString::fromLatin1(key))) {
			missing.push_back(QString::fromLatin1(key));
		}
	}
	Check(missing.isEmpty(), "ai: every text is in the string table", missing.join(u", "_q).toStdString().c_str());
}

// The menu item, the window with its own profile, the warning, the test
// hook only in test builds and its run in the Windows CI.
void TestAiIsWired() {
	auto problems = QStringList();
	const auto ai = ReadSource("mzgram/mzgram_ai.cpp");
	if (ai.isEmpty()) {
		problems.push_back(u"no mzgram_ai.cpp"_q);
	}
	for (const auto needed : {
		"Webview::Window",
		"kStorageFolder",
		"kOptionAiStorageToken",
		"GenerateStorageToken",
		"removeRecursively",
		"InsertScript(",
		"ParseStatus(",
		"NewWindowTarget(",
		"AllowedNavigation(",
		"kOptionAiWarned",
		"\"ai_privacy_text\"",
		"\"ai_model_on_site\"",
	}) {
		if (!ai.contains(QString::fromLatin1(needed))) {
			problems.push_back(u"mzgram_ai.cpp: no "_q + QString::fromLatin1(needed));
		}
	}
	// Never one of Telegram's own web profiles
	for (const auto telegram : { "resolveStorageId", "TonSiteStorageId", "wvproxy" }) {
		if (ai.contains(QString::fromLatin1(telegram))) {
			problems.push_back(u"mzgram_ai.cpp: "_q + QString::fromLatin1(telegram));
		}
	}
	const auto guard = ai.indexOf(u"#ifdef MZGRAM_TEST_HOOKS"_q);
	const auto hook = ai.indexOf(u"MZGRAM_TEST_AI"_q);
	if (guard < 0 || hook < guard || ai.indexOf(u"#endif // MZGRAM_TEST_HOOKS"_q, guard) < ai.lastIndexOf(u"MZGRAM_TEST_AI"_q)) {
		problems.push_back(u"the test hook is not behind MZGRAM_TEST_HOOKS"_q);
	}
	// The menu item is where Copy text is, with its restriction
	for (const auto path : { "history/history_inner_widget.cpp", "history/view/history_view_context_menu.cpp" }) {
		const auto text = ReadSource(path);
		const auto at = text.indexOf(u"MZGram::Ai::AddAskAiAction("_q);
		if (at < 0) {
			problems.push_back(QString::fromLatin1(path) + u": no Ask AI"_q);
		} else if (!text.mid(at, 300).contains(u"hasCopyRestriction("_q)) {
			problems.push_back(QString::fromLatin1(path) + u": Ask AI without the copy restriction"_q);
		}
	}
	if (!ReadSource("core/application.cpp").contains(u"MZGram::Ai::StartTestHook()"_q)) {
		problems.push_back(u"the test hook is never started"_q);
	}
	const auto cmake = ReadSource("../CMakeLists.txt");
	const auto hooks = cmake.indexOf(u"if (MZGRAM_TEST_HOOKS)"_q);
	if (hooks < 0 || !cmake.mid(hooks, 400).contains(u"mzgram/mzgram_ai.cpp"_q)) {
		problems.push_back(u"CMake: mzgram_ai.cpp without MZGRAM_TEST_HOOKS"_q);
	}
	if (!ReadSource("../../.github/workflows/mzgram-windows.yml").contains(u"mzgram_ai_window_test.ps1"_q)) {
		problems.push_back(u"the Windows CI does not open the AI window"_q);
	}
	Check(problems.isEmpty(), "ai: menu item, own profile, warning and the window test are wired", problems.join(u", "_q).toStdString().c_str());
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
		TestPresenceRules,
		TestPresenceIsWired,
		TestSettingsAreTopicPages,
		TestUpdateVersions,
		TestUpdateParse,
		TestUpdateOffer,
		TestUpdateDue,
		TestUpdateIsWired,
		TestSelfUpdateArchiveChoice,
		TestSelfUpdateOnlyThisRepository,
		TestSelfUpdateFollowsGitHubRedirect,
		TestSelfUpdateFailureReasons,
		TestSelfUpdateFailureTexts,
		TestUpdateButtonHookStaysInTests,
		TestSelfUpdateChecksum,
		TestSelfUpdateEntries,
		TestSelfUpdateEntryTypes,
		TestUpdaterBackupFolderIsShared,
		TestSelfUpdatePreparedCode,
		TestUpdateInstallRollsBack,
		TestReleasesCarryTheUpdater,
		TestBrandingRules,
		TestBrandingPhrases,
		TestBrandingIsWired,
		TestLinuxArm64IsBuilt,
		TestFeaturesFile,
		TestLocalPinsRule,
		TestLocalPinsSortPosition,
		TestLocalPinsStorage,
		TestLocalPinsAreWired,
		TestEveryPhraseInEveryLanguage,
		TestLanguageChoice,
		TestLanguagePickerIsWired,
		TestLastSeenIsPlain,
		TestDeletedChatStays,
		TestManualUpdateCheck,
		TestMZGramTextsAreInTheMZGramLanguage,
		TestIpcLinkStaysOneRecord,
		TestIpcLocalFilesWithLinkAreDropped,
		TestIpcRulesAreWired,
		TestUrlSchemeRules,
#ifdef _WIN32
		TestUrlSchemeKeepsTelegramDesktop,
		TestUrlSchemeCleansUpTelegramKeys,
#endif // _WIN32
		TestUrlSchemeIsWired,
		TestAiProviders,
		TestAiDefaultProvider,
		TestAiComposeText,
		TestAiScriptString,
		TestAiInsertScript,
		TestAiStatus,
		TestAiNavigation,
		TestAiTexts,
		TestAiIsWired,
	};
	for (const auto &test : tests) {
		test();
	}
	std::printf("\n%d/%d passed\n", total - failures, total);
	return failures ? 1 : 0;
}
