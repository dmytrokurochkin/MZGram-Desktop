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

[[nodiscard]] QString OpenCommand(const UrlSchemeRegistration &r) {
	return '"' + r.executable + u"\" "_q + r.arguments + u" -- \"%1\""_q;
}

[[nodiscard]] QString Icon(const QString &executable) {
	return '"' + executable + u",1\""_q;
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
	return u"tdesktop"_q;
}

QString UrlSchemeAppKey() {
	return u"TelegramDesktop"_q;
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
	return true;
}

#ifdef _WIN32

void RegisterUrlSchemes(const UrlSchemeRegistration &r) {
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

		const auto legacy = r.root + u"Classes\\"_q + scheme.protocol;
		WriteValue(legacy, u"URL Protocol"_q, QString());
		WriteValue(legacy, QString(), u"URL:"_q + scheme.protocolName);
		WriteValue(legacy + u"\\DefaultIcon"_q, QString(), Icon(r.executable));
		WriteValue(legacy + u"\\shell\\open\\command"_q, QString(), command);
	}
}

#endif // _WIN32

} // namespace MZGram
