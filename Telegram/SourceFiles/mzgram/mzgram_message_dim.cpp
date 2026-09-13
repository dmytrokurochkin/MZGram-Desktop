/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_message_dim.h"

#include "history/history_item.h"
#include "history/view/history_view_element.h"
#include "mzgram/mzgram_anti_recall.h"
#include "mzgram/mzgram_options.h"
#include "ui/chat/chat_style.h"
#include "ui/painter.h"
#include "ui/style/style_core.h"

namespace MZGram {
namespace {

constexpr auto kDeletedOpacity = 0.5;

// Only deleted messages are dimmed. Edited ones still exist for everyone, so
// they get a pencil in their date instead, see history_view_bottom_info.cpp.
[[nodiscard]] float64 MessageOpacity(not_null<const HistoryItem*> item) {
	return (MarkKeptMessages() && IsPreservedDeleted(item))
		? kDeletedOpacity
		: 1.;
}

} // namespace

void DrawMessage(
		Painter &p,
		not_null<const HistoryView::Element*> view,
		const Ui::ChatPaintContext &context) {
	const auto opacity = MessageOpacity(view->data());
	if (opacity >= 1.) {
		view->draw(p, context);
		return;
	}
	const auto size = QSize(view->width(), view->height());
	if (size.isEmpty()) {
		return;
	}
	// Message painting sets the painter opacity back to 1 in many places, so
	// a plain setOpacity would dim only parts of the bubble. The view is drawn
	// into a layer instead and the layer is composed once. Painter and context
	// share the same origin, so patterns sampled from the viewport line up.
	const auto ratio = style::DevicePixelRatio();
	auto layer = QImage(size * ratio, QImage::Format_ARGB32_Premultiplied);
	layer.setDevicePixelRatio(ratio);
	layer.fill(Qt::transparent);
	{
		Painter q(&layer);
		view->draw(q, context);
	}
	const auto was = p.opacity();
	p.setOpacity(was * opacity);
	p.drawImage(0, 0, layer);
	p.setOpacity(was);
}

} // namespace MZGram
