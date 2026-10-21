/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

#include <functional>
#include <vector>

namespace MZGram {

// The links MZGram opens: tg://, tonsite:// and ton://.
struct UrlScheme {
	QString protocol;
	QString protocolName;
};
[[nodiscard]] std::vector<UrlScheme> UrlSchemes();

// Names of the Windows registration: the classes <prefix>.tg and so on and
// the Default apps entry Software\<key>\Capabilities.
[[nodiscard]] QString UrlSchemeClassPrefix();
[[nodiscard]] QString UrlSchemeAppKey();

// The executable of a shell command: "C:\app\MZGram.exe" -- "%1".
[[nodiscard]] QString CommandExecutable(const QString &command);

[[nodiscard]] bool IsMZGramExecutable(const QString &path);

// Whether MZGram may point Software\Classes\<protocol>, the handler Windows
// uses when nothing is chosen in Default apps, to itself.
[[nodiscard]] bool MayTakeUrlScheme(
	const QString &currentCommand,
	const std::function<bool(const QString &path)> &exists);

#ifdef _WIN32

struct UrlSchemeRegistration {
	// Under HKEY_CURRENT_USER; tests use a key of their own.
	QString root;
	QString executable;
	QString arguments;
	QString appName;
	// The "registertg" code: take the links also from another app.
	bool force = false;
};
void RegisterUrlSchemes(const UrlSchemeRegistration &registration);

#endif // _WIN32

} // namespace MZGram
