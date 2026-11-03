/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <optional>
#include <vector>

namespace MZGram::Ai {

// The rules of "Ask AI" (mzgram_ai.cpp): which sites, what goes into their
// chat and where the window may go. No app dependencies, so mzgram_tests
// checks them.
//
// No paid API: the site of the AI opens in MZGram's own window (Webview,
// as Mini Apps use), signed in with the user's own account there, in a
// profile of its own (kStorageFolder) apart from Telegram's. The text is
// put into the site's input field and is sent only when the user presses
// Send on the site.

enum class Provider {
	Gemini,
	Claude,
	ChatGpt,
	DeepSeek,
	Grok,
};

// How the text gets into the site's chat.
enum class Prefill {
	// A script puts it into the input field (InsertScript). ChatGPT and Grok
	// read ?q= but send it at once, Claude's ?q= is undocumented, Gemini and
	// DeepSeek have none: no site has a parameter that only fills the field.
	Script,
};

// How the model is chosen.
enum class ModelChoice {
	// In the site's own picker, after it opens: no site documents a URL
	// parameter or a field for it that MZGram could rely on.
	OnSite,
};

struct ProviderInfo {
	Provider id = Provider::Gemini;
	const char *key = nullptr; // In the settings.
	const char *name = nullptr; // Shown as is, in every language.
	const char *startUrl = nullptr;
	// The site and its subdomains.
	std::vector<const char*> hosts;
	// CSS selectors of the input field, the best first.
	std::vector<const char*> inputSelectors;
	Prefill prefill = Prefill::Script;
	ModelChoice model = ModelChoice::OnSite;
};

// In the order the settings and the box list them.
[[nodiscard]] const std::vector<ProviderInfo> &Providers();
[[nodiscard]] const ProviderInfo &Info(Provider provider);
[[nodiscard]] std::optional<Provider> ProviderByKey(const QString &key);

// Settings > MZGram > AI: the AI the box starts with.
extern const char kOptionAiProvider[];
[[nodiscard]] Provider DefaultProvider();
void SetDefaultProvider(Provider provider);

// The token of the AI sites' profile (Webview::StorageId), made once.
extern const char kOptionAiStorageToken[];
inline constexpr auto kStorageFolder = "webview-mzgram-ai";

// What goes into the site's input field: the user's prompt, an empty line
// and the message; either alone when the other is empty.
[[nodiscard]] QString ComposeText(
	const QString &prompt,
	const QString &message);

// A JavaScript string literal of the text, in ASCII only: quotes,
// backslashes, line ends, < > & and every non-ASCII character are \u
// escapes, so no text can end the literal or the script. Valid JSON too.
[[nodiscard]] QByteArray ScriptString(const QString &text);

// The script that puts the text into the site's input field. It never
// presses Enter or Send: it posts {"mzgram_ai":"inserted"} when the field
// has the text, {"mzgram_ai":"no_field"} when no field showed up.
[[nodiscard]] QByteArray InsertScript(Provider provider, const QString &text);

enum class Status {
	None, // Not a message of InsertScript.
	Inserted,
	NoField,
};
[[nodiscard]] Status ParseStatus(const QByteArray &message);

// Attachments: photos and documents of the chosen messages go with the
// text, at most kMaxAttachments (what the sites take in one message), each
// at most kMaxAttachmentSize (the smallest limit of the five sites).
inline constexpr auto kMaxAttachments = 10;
inline constexpr auto kMaxAttachmentSize = qint64(30) * 1024 * 1024;

enum class AttachmentKind {
	Photo, // A photo, or a jpg, png or webp sent as a file.
	File, // A document an AI reads: text, code, pdf, office files.
	Archive, // zip, rar, 7z, tar, gz, apk and the like: never sent.
	Media, // Video, GIF, sticker, voice, round video, music: never sent.
	Unsupported, // Any other file.
};

// A file by its name and MIME type (a photo is Photo without a name).
[[nodiscard]] AttachmentKind ClassifyFile(
	const QString &name,
	const QString &mime);

struct AttachmentCandidate {
	AttachmentKind kind = AttachmentKind::Unsupported;
	qint64 size = 0;
};
struct AttachmentChoice {
	std::vector<int> chosen; // Indices into the candidates, in order.
	int overLimit = 0; // Past kMaxAttachments.
	int tooLarge = 0; // Over kMaxAttachmentSize.
	int archives = 0;
	int media = 0;
	int unsupported = 0;
};
// The first kMaxAttachments photos and files that are not too large, and
// what is left out and why.
[[nodiscard]] AttachmentChoice ChooseAttachments(
	const std::vector<AttachmentCandidate> &candidates);

// The site's own button for adding files. PickerHookScript, added to every
// page before the page's scripts run, keeps the browser's file dialog from
// opening for a file input (a click on it or on its label, its click() and
// showPicker()) and posts {"mzgram_ai":"pick","multiple":bool,"accept":"..."}
// in its place: MZGram shows its own chooser. The chosen files go into the
// page in pieces of kChunkSize bytes (base64 in a script each), then
// GiveFilesScript gives them to that input as the dialog would and posts
// {"mzgram_ai":"given","count":N}. Nothing is sent.
inline constexpr auto kChunkSize = 512 * 1024;
[[nodiscard]] QByteArray PickerHookScript();
[[nodiscard]] QByteArray FileStartScript(
	int index,
	const QString &name,
	const QString &mime);
[[nodiscard]] QByteArray FileChunkScript(int index, const QByteArray &bytes);
[[nodiscard]] QByteArray GiveFilesScript();
// The chooser closed with nothing chosen: the input hears "cancel", as
// from its dialog.
[[nodiscard]] QByteArray CancelPickScript();

struct Pick {
	bool multiple = false;
	QString accept; // The input's accept list, as the page has it.
};
[[nodiscard]] std::optional<Pick> ParsePick(const QByteArray &message);
// How many files the input got, from GiveFilesScript.
[[nodiscard]] std::optional<int> ParseGiven(const QByteArray &message);

// The checked files: all of them now to an input that takes several, else
// the first now and the rest at the next press of the site's button.
struct Give {
	std::vector<int> now;
	std::vector<int> later;
};
[[nodiscard]] Give GiveFiles(const std::vector<int> &checked, bool multiple);

// Whether the site's file input takes the file, by its accept list
// ("image/*", "application/pdf", ".pdf"); an empty list or "*/*" takes
// everything.
[[nodiscard]] bool AcceptsFile(
	const QString &accept,
	const QString &name,
	const QString &mime);

// Settings > MZGram > AI: a button for each site in the main menu, which
// opens the site with no text; all off by default.
[[nodiscard]] QString MenuButtonOption(Provider provider);
[[nodiscard]] bool MenuButton(Provider provider);
void SetMenuButton(Provider provider, bool shown);

// The window goes only to https addresses.
[[nodiscard]] bool AllowedNavigation(const QString &url);

// Where a link the site opens in a new window goes: the site and the
// sign-in pages stay in the window, other https links open in the
// browser, anything else nowhere.
enum class NewWindow {
	Here,
	Browser,
	Refuse,
};
[[nodiscard]] NewWindow NewWindowTarget(Provider provider, const QString &url);

} // namespace MZGram::Ai
