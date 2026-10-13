/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_local_pins_rules.h"

#include "base/basic_types.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace MZGram::LocalPins {

bool PinsLocally(int serverPinned, int localPinned, int serverLimit) {
	return (serverPinned >= serverLimit)
		&& (serverPinned + localPinned < kMaxTotal);
}

quint64 SortPosition(int index) {
	return 0xFFFFFFFF00000080ULL - quint64(index);
}

QByteArray Serialize(const Stored &stored) {
	auto object = QJsonObject();
	for (const auto &[key, ids] : stored) {
		if (ids.empty()) {
			continue;
		}
		auto list = QJsonArray();
		for (const auto id : ids) {
			// As text: a JSON number would lose the upper bits of a peer id
			list.append(QString::number(id));
		}
		object.insert(key, list);
	}
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

Stored Parse(const QByteArray &json) {
	auto result = Stored();
	const auto object = QJsonDocument::fromJson(json).object();
	for (auto i = object.begin(); i != object.end(); ++i) {
		auto ids = std::vector<quint64>();
		for (const auto &value : i.value().toArray()) {
			auto ok = false;
			const auto id = value.toString().toULongLong(&ok);
			if (ok && id) {
				ids.push_back(id);
			}
		}
		if (!ids.empty()) {
			result.emplace(i.key(), std::move(ids));
		}
	}
	return result;
}

} // namespace MZGram::LocalPins
