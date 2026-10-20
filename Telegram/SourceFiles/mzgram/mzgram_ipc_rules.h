/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

namespace MZGram {

// Another launch of MZGram passes what it was started with to the running
// one over the local socket (core/sandbox.cpp), as records ending with ';':
// "OPEN:<url>;", "CMD:<command>;" and so on.

// The OPEN record of a link the other launch was started with.
[[nodiscard]] QString IpcOpenRecord(const QUrl &url);

// The value of an OPEN record back as the link.
[[nodiscard]] QUrl IpcOpenUrl(const QString &value);

// The records received so far; what follows the last ';' stays in the
// buffer for the next read.
[[nodiscard]] QStringList IpcRecords(QByteArray &buffer);

struct IpcStartUrls {
	QList<QUrl> open;
	QList<QUrl> dropped;
};

// What the running app opens of the links of one connection.
// externalUrlReceived is kept per connection between reads.
[[nodiscard]] IpcStartUrls IpcFilterStartUrls(
	const QList<QUrl> &urls,
	bool &externalUrlReceived);

} // namespace MZGram
