/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_edit_history_box.h"

#include "base/unixtime.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

namespace MZGram {
namespace {

[[nodiscard]] QString FormatWhen(int64 unixtime) {
	const auto when = base::unixtime::parse(TimeId(unixtime));
	return QLocale().toString(when, QLocale::ShortFormat);
}

} // namespace

void EditHistoryBox(
		not_null<Ui::GenericBox*> box,
		MessageKey key,
		QString currentText) {
	// Strings are hardcoded until MZGram has its own lang keys.
	box->setTitle(rpl::single(u"Edit history"_q));
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });

	const auto addLabel = [&](const QString &text) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(text),
			st::aboutLabel));
	};

	const auto versions = MessageStore::Instance().edits(key);
	if (versions.empty()) {
		addLabel(u"No earlier versions were recorded for this message."_q);
		return;
	}
	// Each row is stamped with the moment that version stopped being current,
	// which is the only time the client observes; the server never reports
	// when an intermediate version was first written.
	for (const auto &version : versions) {
		addLabel(u"Replaced "_q + FormatWhen(version.replacedAt));
		addLabel(version.text.isEmpty() ? u"(empty)"_q : version.text);
	}
	addLabel(u"Current"_q);
	addLabel(currentText.isEmpty() ? u"(empty)"_q : currentText);
}

} // namespace MZGram
