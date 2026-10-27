/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_url_scheme.h"

#include "base/basic_types.h"

#include <QtCore/QStringList>

#include <kshell.h>

#include <gio/gio.h>

namespace MZGram {
namespace {

[[nodiscard]] QByteArray HandlerType(const QString &protocol) {
	return (u"x-scheme-handler/"_q + protocol).toUtf8();
}

} // namespace

QString DefaultUrlSchemeCommand(const QString &protocol) {
	const auto type = HandlerType(protocol);
	const auto info = g_app_info_get_default_for_type(
		type.constData(),
		TRUE);
	if (!info) {
		return QString();
	}
	const auto line = g_app_info_get_commandline(info);
	const auto result = line ? QString::fromUtf8(line) : QString();
	g_object_unref(info);
	return result;
}

void OfferForUrlScheme(const UrlSchemeOffer &offer) {
	const auto type = HandlerType(offer.protocol);
	// The command line as lib_base writes it (base_url_scheme_linux.cpp),
	// GLib adds %u.
	const auto commandline = KShell::joinArgs(QStringList{ offer.executable }
		+ KShell::splitArgs(offer.arguments)
		+ QStringList{ u"--"_q }).toUtf8();
	auto offered = false;
	const auto all = g_app_info_get_all_for_type(type.constData());
	for (auto i = all; i; i = i->next) {
		const auto line = QByteArray(
			g_app_info_get_commandline(G_APP_INFO(i->data)));
		if (line == commandline + " %u" || line == commandline + " %U") {
			offered = true;
		}
	}
	g_list_free_full(all, g_object_unref);
	if (offered) {
		return;
	}
	const auto info = g_app_info_create_from_commandline(
		commandline.constData(),
		offer.name.toUtf8().constData(),
		GAppInfoCreateFlags(G_APP_INFO_CREATE_SUPPORTS_URIS
			| G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION),
		nullptr);
	if (info) {
		g_app_info_add_supports_type(info, type.constData(), nullptr);
		g_object_unref(info);
	}
}

} // namespace MZGram
