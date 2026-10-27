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

// The executable of a shell command: "C:\app\MZGram.exe" -- "%1" on
// Windows (also unquoted with spaces, up to the first .exe, as Windows
// reads it), '/opt/My Apps/MZGram' -- %u on Linux (as GLib quotes it).
[[nodiscard]] QString CommandExecutable(const QString &command);

// MZGram.exe on Windows, MZGram on Linux.
[[nodiscard]] bool IsMZGramExecutable(const QString &path);

// Whether MZGram may become the handler of a protocol whose handler runs
// currentCommand now: Software\Classes\<protocol> on Windows (used when
// nothing is chosen in Default apps), the default handler on Linux. Only
// when there is none, it is MZGram or its executable is gone.
[[nodiscard]] bool MayTakeUrlScheme(
	const QString &currentCommand,
	const std::function<bool(const QString &path)> &exists);

// The executable of a Linux handler is there: a full path that exists, or
// a bare name, which GLib found in PATH before it offered the handler.
[[nodiscard]] bool HandlerExecutableExists(const QString &path);

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

#else // _WIN32

// The command line of the default handler of protocol, empty when there is
// none (mzgram_url_scheme_linux.cpp).
[[nodiscard]] QString DefaultUrlSchemeCommand(const QString &protocol);

// MZGram among the apps for a protocol, so that it can be chosen, while
// another app stays its default handler. MZGram has no launcher of its own
// (Platform::InstallLauncher skips it, Telegram's updater being off).
struct UrlSchemeOffer {
	QString executable;
	QString arguments;
	QString protocol;
	QString name;
};
void OfferForUrlScheme(const UrlSchemeOffer &offer);

#endif // _WIN32

} // namespace MZGram
