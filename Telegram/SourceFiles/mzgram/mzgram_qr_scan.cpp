/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_qr_scan.h"

#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "mzgram/mzgram_options.h"
#include "mzgram/quirc/quirc.h"
#include "ui/image/image.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>

#include <cstring>

namespace MZGram {
namespace {

// Runs the bundled quirc decoder over a already-in-memory image. Returns
// the first successfully decoded payload, or an empty string if no QR
// code was found. Everything here is local and offline.
[[nodiscard]] QString DecodeQrCode(const QImage &source) {
	if (source.isNull()) {
		return QString();
	}
	const auto gray = source.convertToFormat(QImage::Format_Grayscale8);
	if (gray.isNull() || gray.width() < 1 || gray.height() < 1) {
		return QString();
	}
	const auto q = quirc_new();
	if (!q) {
		return QString();
	}
	if (quirc_resize(q, gray.width(), gray.height()) < 0) {
		quirc_destroy(q);
		return QString();
	}
	auto w = 0;
	auto h = 0;
	const auto buffer = quirc_begin(q, &w, &h);
	for (auto y = 0; y != h; ++y) {
		memcpy(buffer + y * w, gray.constScanLine(y), w);
	}
	quirc_end(q);

	auto result = QString();
	const auto count = quirc_count(q);
	for (auto i = 0; i != count && result.isEmpty(); ++i) {
		quirc_code code;
		quirc_extract(q, i, &code);
		quirc_data data;
		if (quirc_decode(&code, &data) == QUIRC_SUCCESS) {
			result = QString::fromUtf8(
				reinterpret_cast<const char*>(data.payload),
				data.payload_len);
		}
	}
	quirc_destroy(q);
	return result;
}

void ShowQrResultBox(
		not_null<Window::SessionController*> controller,
		const QString &text) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"QR code"_q));
		box->setWidth(st::boxWideWidth);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(text),
			st::boxLabel));
		box->addButton(rpl::single(u"Copy"_q), [=] {
			QGuiApplication::clipboard()->setText(text);
			box->closeBox();
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

void ShowNoQrCodeFoundBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"QR code"_q));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"No QR code was found in this photo."_q),
			st::boxLabel));
		box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
	}));
}

} // namespace

void AddScanQrCodeAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<PhotoData*> photo,
		HistoryItem *item,
		not_null<Window::SessionController*> controller) {
	if (!ScanQrCodeAction()) {
		return;
	}
	if (item && item->isService()) {
		return;
	}
	const auto media = photo->activeMediaView();
	if (photo->isNull() || !media || !media->loaded()) {
		return;
	}
	menu->addAction(u"Scan for QR code"_q, [=] {
		const auto media = photo->activeMediaView();
		const auto image = media
			? media->image(Data::PhotoSize::Large)
			: nullptr;
		if (!image) {
			ShowNoQrCodeFoundBox(controller);
			return;
		}
		const auto text = DecodeQrCode(image->original());
		if (text.isEmpty()) {
			ShowNoQrCodeFoundBox(controller);
		} else {
			ShowQrResultBox(controller, text);
		}
	}, &st::menuIconQrCode);
}

} // namespace MZGram
