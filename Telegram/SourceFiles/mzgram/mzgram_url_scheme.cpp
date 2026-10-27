/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_url_scheme.h"

#include "base/basic_types.h"

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

[[nodiscard]] QString ReadValue(const QString &key, const QString &name) {
	const auto wkey = key.toStdWString();
	const auto wname = name.toStdWString();
	auto size = DWORD();
	if (RegGetValue(HKEY_CURRENT_USER, wkey.c_str(), wname.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS
		|| size < sizeof(WCHAR)) {
		return QString();
	}
	auto buffer = std::wstring(size / sizeof(WCHAR), L'\0');
	if (RegGetValue(HKEY_CURRENT_USER, wkey.c_str(), wname.c_str(), RRF_RT_REG_SZ, nullptr, buffer.data(), &size) != ERROR_SUCCESS) {
		return QString();
	}
	return QString::fromWCharArray(buffer.c_str());
}

void WriteValue(const QString &key, const QString &name, const QString &value) {
	const auto wkey = key.toStdWString();
	auto handle = HKEY();
	if (RegCreateKeyEx(HKEY_CURRENT_USER, wkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &handle, nullptr) != ERROR_SUCCESS) {
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

void DeleteKey(const QString &key) {
	const auto wkey = key.toStdWString();
	RegDeleteTree(HKEY_CURRENT_USER, wkey.c_str());
	RegDeleteKey(HKEY_CURRENT_USER, wkey.c_str());
}

[[nodiscard]] QString OpenCommand(const UrlSchemeRegistration &r) {
	return '"' + r.executable + u"\" "_q + r.arguments + u" -- \"%1\""_q;
}

[[nodiscard]] QString Icon(const QString &executable) {
	return '"' + executable + u",1\""_q;
}

[[nodiscard]] bool Exists(const QString &path) {
	return QFileInfo::exists(path);
}

// MZGram up to 1.0.4 registered itself under Telegram Desktop's names, so
// Default apps showed Telegram Desktop's entry as "MZGram Desktop" and its
// links opened MZGram. What MZGram wrote there goes back to Telegram Desktop
// when it is registered (the path of Telegram Desktop is taken from its
// Software\Classes\<protocol>), or away when it is not.
void CleanUpTelegramKeys(const UrlSchemeRegistration &r) {
	const auto telegram = !ReadValue(
		r.root + u"RegisteredApplications"_q,
		kTelegramAppName).isEmpty();
	for (const auto &scheme : UrlSchemes()) {
		const auto named = r.root
			+ u"Classes\\"_q
			+ kTelegramClassPrefix
			+ '.'
			+ scheme.protocol;
		const auto command = ReadValue(
			named + u"\\shell\\open\\command"_q,
			QString());
		if (!IsMZGramExecutable(CommandExecutable(command))) {
			continue;
		}
		const auto handler = ReadValue(
			r.root + u"Classes\\"_q + scheme.protocol + u"\\shell\\open\\command"_q,
			QString());
		const auto executable = CommandExecutable(handler);
		if (!executable.isEmpty()
			&& !IsMZGramExecutable(executable)
			&& Exists(executable)) {
			WriteValue(named + u"\\shell\\open\\command"_q, QString(), handler);
			WriteValue(named + u"\\DefaultIcon"_q, QString(), Icon(executable));
		} else if (!telegram) {
			DeleteKey(named);
		}
	}
	const auto app = r.root + kTelegramAppKey;
	const auto capabilities = app + u"\\Capabilities"_q;
	if (ReadValue(capabilities, u"ApplicationName"_q) != r.appName) {
		return;
	} else if (telegram) {
		WriteValue(capabilities, u"ApplicationName"_q, kTelegramAppName);
		WriteValue(capabilities, u"ApplicationDescription"_q, kTelegramAppName);
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
	if (trimmed.startsWith('"')) {
		const auto end = trimmed.indexOf('"', 1);
		return (end > 1) ? trimmed.mid(1, end - 1) : QString();
	}
	return trimmed.section(' ', 0, 0);
}

bool IsMZGramExecutable(const QString &path) {
	const auto slash = std::max(path.lastIndexOf('\\'), path.lastIndexOf('/'));
	return !path.mid(slash + 1).compare(u"MZGram.exe"_q, Qt::CaseInsensitive);
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
	return QFileInfo::exists(path);
}

#ifdef _WIN32

void RegisterUrlSchemes(const UrlSchemeRegistration &r) {
	CleanUpTelegramKeys(r);

	const auto capabilities = r.root + UrlSchemeAppKey() + u"\\Capabilities"_q;
	WriteValue(capabilities, u"ApplicationName"_q, r.appName);
	WriteValue(capabilities, u"ApplicationDescription"_q, r.appName);
	WriteValue(r.root + u"RegisteredApplications"_q, r.appName, capabilities);

	const auto command = OpenCommand(r);
	for (const auto &scheme : UrlSchemes()) {
		const auto named = UrlSchemeClassPrefix() + '.' + scheme.protocol;
		const auto namedBase = r.root + u"Classes\\"_q + named;
		WriteValue(namedBase + u"\\DefaultIcon"_q, QString(), Icon(r.executable));
		WriteValue(namedBase + u"\\shell\\open\\command"_q, QString(), command);
		WriteValue(capabilities + u"\\UrlAssociations"_q, scheme.protocol, named);

		// The handler of another app, Telegram Desktop's for one, stays:
		// MZGram is chosen for these links in Windows Settings > Default apps.
		const auto legacy = r.root + u"Classes\\"_q + scheme.protocol;
		const auto current = ReadValue(
			legacy + u"\\shell\\open\\command"_q,
			QString());
		if (!r.force && !MayTakeUrlScheme(current, Exists)) {
			continue;
		}
		WriteValue(legacy, u"URL Protocol"_q, QString());
		WriteValue(legacy, QString(), u"URL:"_q + scheme.protocolName);
		WriteValue(legacy + u"\\DefaultIcon"_q, QString(), Icon(r.executable));
		WriteValue(legacy + u"\\shell\\open\\command"_q, QString(), command);
	}
}

#endif // _WIN32

} // namespace MZGram
