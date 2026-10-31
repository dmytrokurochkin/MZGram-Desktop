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
