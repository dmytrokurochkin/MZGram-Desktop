/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_ipc_rules.h"

#include "base/basic_types.h"

namespace MZGram {
namespace {

// Telegram Desktop 7.2.9 (core/sandbox.cpp), moved here to be tested: a
// value is written in 7 bits, with ';' and '%' as %xxxx, so a link with
// ';' in it stays one record.
[[nodiscard]] QChar HexDigit(ushort value) {
	value &= 0x000F;
	return QChar::fromLatin1((value >= 10) ? ('a' + (value - 10)) : ('0' + value));
}

[[nodiscard]] ushort HexDigitValue(QChar ch) {
	const auto code = ch.unicode();
	return ((code >= uchar('a'))
		? (code - uchar('a') + 10)
		: (code - uchar('0'))) & 0x000F;
}

[[nodiscard]] QString EscapeTo7bit(const QString &value) {
	auto result = QString();
	result.reserve(value.size() * 2);
	for (const auto ch : value) {
		const auto code = ch.unicode();
		if (code < 32
			|| code > 127
			|| ch == QChar('%')
			|| ch == QChar(';')) {
			result.append('%');
			result.append(HexDigit(code >> 12));
			result.append(HexDigit(code >> 8));
			result.append(HexDigit(code >> 4));
			result.append(HexDigit(code));
		} else {
			result.append(ch);
		}
	}
	return result;
}

[[nodiscard]] QString EscapeFrom7bit(const QString &value) {
	auto result = QString();
	result.reserve(value.size());
	for (auto i = 0; i != value.size(); ++i) {
		const auto ch = value.at(i);
		if (ch == QChar('%') && (i + 4 < value.size())) {
			result.append(QChar(ushort(
				(HexDigitValue(value.at(i + 1)) << 12)
				| (HexDigitValue(value.at(i + 2)) << 8)
				| (HexDigitValue(value.at(i + 3)) << 4)
				| HexDigitValue(value.at(i + 4)))));
			i += 4;
		} else {
			result.append(ch);
		}
	}
	return result;
}

} // namespace

QString IpcOpenRecord(const QUrl &url) {
	return u"OPEN:"_q
		+ EscapeTo7bit(url.toString(QUrl::FullyEncoded))
		+ ';';
}

QUrl IpcOpenUrl(const QString &value) {
	return QUrl(EscapeFrom7bit(value).mid(0, 8192));
}

QStringList IpcRecords(QByteArray &buffer) {
	const auto commands = QString::fromLatin1(buffer);
	auto result = QStringList();
	auto from = 0;
	for (auto to = commands.indexOf(QChar(';'), from)
		; to >= from
		; to = commands.indexOf(QChar(';'), from)) {
		result.push_back(commands.mid(from, to - from));
		from = to + 1;
	}
	if (from > 0) {
		buffer = buffer.mid(from);
	}
	return result;
}

// A link launch carries a single non-file url and a send-files launch
// carries only local paths, so a connection mixing both means the sender
// did not escape the record separator. Once such a connection shows a
// non-file url its local paths are dropped for good.
IpcStartUrls IpcFilterStartUrls(
		const QList<QUrl> &urls,
		bool &externalUrlReceived) {
	for (const auto &url : urls) {
		if (!url.isLocalFile()) {
			externalUrlReceived = true;
		}
	}
	auto result = IpcStartUrls();
	for (const auto &url : urls) {
		if (externalUrlReceived && url.isLocalFile()) {
			result.dropped.push_back(url);
		} else {
			result.open.push_back(url);
		}
	}
	return result;
}

} // namespace MZGram
