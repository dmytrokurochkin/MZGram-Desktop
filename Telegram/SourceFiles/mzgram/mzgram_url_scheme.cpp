/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_url_scheme.h"

#include "base/basic_types.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

namespace MZGram {
namespace {

#ifdef _WIN32

// What Telegram Desktop registers (lib_base, base_url_scheme_win.cpp).
// MZGram up to 1.0.4 wrote its own name and path there too.
const auto kTelegramClassPrefix = u"tdesktop"_q;
const auto kTelegramAppKey = u"TelegramDesktop"_q;
const auto kTelegramAppName = u"Telegram Desktop"_q;

// The uninstall entry of Telegram Desktop's installer (Telegram/build/
// setup.iss, AppId), with the folder Telegram.exe is in.
const auto kTelegramUninstallKey = u"Microsoft\\Windows\\CurrentVersion\\"
	"Uninstall\\{53F49750-6209-4FBF-9CA8-7A333C87D1ED}_is1"_q;

// Protocols whose Software\Classes\<protocol> this MZGram took on purpose:
// there was no other handler, or the "registertg" code. A protocol taken
// by MZGram but missing here was taken from Telegram Desktop by MZGram up
// to 1.0.4 and goes back to it.
const auto kTakenKey = u"\\TakenUrlSchemes"_q;

const auto kOpenCommand = u"\\shell\\open\\command"_q;
const auto kDefaultIcon = u"\\DefaultIcon"_q;

[[nodiscard]] QString ReadValue(const QString &key, const QString &name) {
	const auto wkey = key.toStdWString();
	const auto wname = name.toStdWString();
	// REG_EXPAND_SZ too, expanded: a handler that runs
	// %LOCALAPPDATA%\...\app.exe is a handler all the same.
	const auto flags = DWORD(RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ);
	auto size = DWORD();
	if (RegGetValue(
			HKEY_CURRENT_USER,
			wkey.c_str(),
			wname.c_str(),
			flags,
			nullptr,
			nullptr,
			&size) != ERROR_SUCCESS
		|| size < sizeof(WCHAR)) {
		return QString();
	}
	auto buffer = std::wstring(size / sizeof(WCHAR), L'\0');
	if (RegGetValue(
			HKEY_CURRENT_USER,
			wkey.c_str(),
			wname.c_str(),
			flags,
			nullptr,
			buffer.data(),
			&size) != ERROR_SUCCESS) {
		return QString();
	}
	return QString::fromWCharArray(buffer.c_str());
}

void WriteValue(
		const QString &key,
		const QString &name,
		const QString &value) {
	const auto wkey = key.toStdWString();
	auto handle = HKEY();
	if (RegCreateKeyEx(
			HKEY_CURRENT_USER,
			wkey.c_str(),
			0,
			nullptr,
			REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE,
			nullptr,
			&handle,
			nullptr) != ERROR_SUCCESS) {
		return;
	}
	const auto wname = name.toStdWString();
	const auto wvalue = value.toStdWString();
	RegSetValueEx(
		handle,
		wname.empty() ? nullptr : wname.c_str(),
		0,
		REG_SZ,
		reinterpret_cast<const BYTE*>(wvalue.c_str()),
		DWORD((wvalue.size() + 1) * sizeof(WCHAR)));
	RegCloseKey(handle);
}

void DeleteValue(const QString &key, const QString &name) {
	const auto wkey = key.toStdWString();
	const auto wname = name.toStdWString();
	RegDeleteKeyValue(HKEY_CURRENT_USER, wkey.c_str(), wname.c_str());
}

void DeleteKey(const QString &key) {
	const auto wkey = key.toStdWString();
	RegDeleteTree(HKEY_CURRENT_USER, wkey.c_str());
	RegDeleteKey(HKEY_CURRENT_USER, wkey.c_str());
}

[[nodiscard]] QString ClassKey(
		const UrlSchemeRegistration &r,
		const QString &name) {
	return r.root + u"Classes\\"_q + name;
}

// As lib_base writes it (base_url_scheme_win.cpp).
[[nodiscard]] QString OpenCommand(
		const QString &executable,
		const QString &arguments) {
	return '"' + executable + u"\" "_q + arguments + u" -- \"%1\""_q;
}

[[nodiscard]] QString Icon(const QString &executable) {
	return '"' + executable + u",1\""_q;
}

[[nodiscard]] bool Exists(const QString &path) {
	return QFileInfo::exists(path);
}

// A command that runs an app other than MZGram, and that app is there.
[[nodiscard]] bool OtherAppCommand(const QString &command) {
	const auto executable = CommandExecutable(command);
	return !executable.isEmpty()
		&& !IsMZGramExecutable(executable)
		&& Exists(executable);
}

[[nodiscard]] bool RunsMZGram(const QString &command) {
	return IsMZGramExecutable(CommandExecutable(command));
}

// The command Telegram Desktop opens protocol with: its own class, when
// MZGram did not write over it; else the handler Windows has for the
// protocol; else Telegram.exe in the folder of its installer. Empty when
// none of them leads to Telegram Desktop.
[[nodiscard]] QString TelegramCommand(
		const UrlSchemeRegistration &r,
		const QString &protocol) {
	const auto named = ReadValue(
		ClassKey(r, kTelegramClassPrefix + '.' + protocol) + kOpenCommand,
		QString());
	if (OtherAppCommand(named)) {
		return named;
	}
	const auto legacy = ReadValue(
		ClassKey(r, protocol) + kOpenCommand,
		QString());
	if (OtherAppCommand(legacy)) {
		return legacy;
	}
	const auto folder = ReadValue(
		r.root + kTelegramUninstallKey,
		u"InstallLocation"_q);
	if (folder.isEmpty()) {
		return QString();
	}
	const auto executable = QDir::toNativeSeparators(
		QDir(folder).filePath(u"Telegram.exe"_q));
	return Exists(executable)
		? OpenCommand(executable, QString())
		: QString();
}

// MZGram up to 1.0.4 registered itself under Telegram Desktop's names, so
// Default apps showed Telegram Desktop's entry as "MZGram Desktop", its
// classes opened MZGram, and Software\Classes\<protocol> was taken from it.
// All of that goes back to Telegram Desktop when it is registered and can
// be found (TelegramCommand), or away when it is not registered. When it is
// registered but cannot be found, its keys wait for its next version, which
// writes them again.
void CleanUpTelegramKeys(const UrlSchemeRegistration &r) {
	const auto telegram = !ReadValue(
		r.root + u"RegisteredApplications"_q,
		kTelegramAppName).isEmpty();
	const auto taken = r.root + UrlSchemeAppKey() + kTakenKey;
	for (const auto &scheme : UrlSchemes()) {
		const auto named = ClassKey(
			r,
			kTelegramClassPrefix + '.' + scheme.protocol);
		const auto legacy = ClassKey(r, scheme.protocol);
		const auto namedTaken = RunsMZGram(
			ReadValue(named + kOpenCommand, QString()));
		const auto legacyTaken = RunsMZGram(
			ReadValue(legacy + kOpenCommand, QString()))
			&& ReadValue(taken, scheme.protocol).isEmpty();
		if (!telegram) {
			if (namedTaken) {
				DeleteKey(named);
			}
			continue;
		} else if (!namedTaken && !legacyTaken) {
			continue;
		}
		const auto command = TelegramCommand(r, scheme.protocol);
		if (command.isEmpty()) {
			continue;
		}
		const auto icon = Icon(CommandExecutable(command));
		if (namedTaken) {
			WriteValue(named + kOpenCommand, QString(), command);
			WriteValue(named + kDefaultIcon, QString(), icon);
		}
		if (legacyTaken) {
			WriteValue(legacy + kOpenCommand, QString(), command);
			WriteValue(legacy + kDefaultIcon, QString(), icon);
		}
	}
	const auto app = r.root + kTelegramAppKey;
	const auto capabilities = app + u"\\Capabilities"_q;
	if (ReadValue(capabilities, u"ApplicationName"_q) != r.appName) {
		return;
	} else if (telegram) {
		WriteValue(capabilities, u"ApplicationName"_q, kTelegramAppName);
		WriteValue(
			capabilities,
			u"ApplicationDescription"_q,
			kTelegramAppName);
	} else {
		DeleteKey(capabilities);
		const auto wapp = app.toStdWString();
		RegDeleteKey(HKEY_CURRENT_USER, wapp.c_str());
	}
}

#endif // _WIN32

} // namespace

std::vector<UrlScheme> UrlSchemes() {
	return {
		{ u"tg"_q, u"Telegram Link"_q },
		{ u"tonsite"_q, u"TonSite Link"_q },
		{ u"ton"_q, u"Ton Link"_q },
	};
}

QString UrlSchemeClassPrefix() {
	return u"mzgram"_q;
}

QString UrlSchemeAppKey() {
	return u"MZGramDesktop"_q;
}

QString CommandExecutable(const QString &command) {
	const auto trimmed = command.trimmed();
	if (trimmed.startsWith('"') || trimmed.startsWith('\'')) {
		const auto end = trimmed.indexOf(trimmed.front(), 1);
		return (end > 1) ? trimmed.mid(1, end - 1) : QString();
	}
	const auto exe = u".exe"_q;
	for (auto space = trimmed.indexOf(' ')
		; space > 0
		; space = trimmed.indexOf(' ', space + 1)) {
		if (trimmed.left(space).endsWith(exe, Qt::CaseInsensitive)) {
			return trimmed.left(space);
		}
	}
	return trimmed.endsWith(exe, Qt::CaseInsensitive)
		? trimmed
		: trimmed.section(' ', 0, 0);
}

bool IsMZGramExecutable(const QString &path) {
	const auto slash = std::max(path.lastIndexOf('\\'), path.lastIndexOf('/'));
	const auto name = path.mid(slash + 1);
	return !name.compare(u"MZGram.exe"_q, Qt::CaseInsensitive)
		|| (name == u"MZGram"_q);
}

bool MayTakeUrlScheme(
		const QString &currentCommand,
		const std::function<bool(const QString &path)> &exists) {
	const auto executable = CommandExecutable(currentCommand);
	return executable.isEmpty()
		|| IsMZGramExecutable(executable)
		|| !exists(executable);
}

bool HandlerExecutableExists(const QString &path) {
	return !QDir::isAbsolutePath(path) || QFileInfo::exists(path);
}

#ifdef _WIN32

void RegisterUrlSchemes(const UrlSchemeRegistration &r) {
	CleanUpTelegramKeys(r);

	const auto app = r.root + UrlSchemeAppKey();
	const auto capabilities = app + u"\\Capabilities"_q;
	WriteValue(capabilities, u"ApplicationName"_q, r.appName);
	WriteValue(capabilities, u"ApplicationDescription"_q, r.appName);
	WriteValue(r.root + u"RegisteredApplications"_q, r.appName, capabilities);

	const auto command = OpenCommand(r.executable, r.arguments);
	const auto icon = Icon(r.executable);
	for (const auto &scheme : UrlSchemes()) {
		const auto named = UrlSchemeClassPrefix() + '.' + scheme.protocol;
		WriteValue(ClassKey(r, named) + kDefaultIcon, QString(), icon);
		WriteValue(ClassKey(r, named) + kOpenCommand, QString(), command);
		WriteValue(
			capabilities + u"\\UrlAssociations"_q,
			scheme.protocol,
			named);

		// The handler of another app, Telegram Desktop's for one, stays:
		// MZGram is chosen for these links in Windows Settings > Default
		// apps. What MZGram takes on purpose is marked (kTakenKey).
		const auto legacy = ClassKey(r, scheme.protocol);
		const auto current = ReadValue(legacy + kOpenCommand, QString());
		if (!r.force && !MayTakeUrlScheme(current, Exists)) {
			DeleteValue(app + kTakenKey, scheme.protocol);
			continue;
		} else if (r.force || !RunsMZGram(current)) {
			WriteValue(app + kTakenKey, scheme.protocol, u"1"_q);
		}
		WriteValue(legacy, u"URL Protocol"_q, QString());
		WriteValue(legacy, QString(), u"URL:"_q + scheme.protocolName);
		WriteValue(legacy + kDefaultIcon, QString(), icon);
		WriteValue(legacy + kOpenCommand, QString(), command);
	}
}

#endif // _WIN32

} // namespace MZGram
