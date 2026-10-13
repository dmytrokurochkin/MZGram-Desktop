/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_update_bar.h"

#include "mzgram/mzgram_lang.h"
#include "mzgram/mzgram_update.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "styles/style_dialogs.h"
#include "styles/style_widgets.h"

namespace MZGram::Update {

object_ptr<Ui::RpWidget> CreateBar(
		not_null<QWidget*> parent,
		const Release &release) {
	auto result = object_ptr<Ui::RpWidget>(parent);
	const auto raw = result.data();
	const auto height = st::dialogsUpdateButton.height;
	raw->resize(parent->width(), height);

	const auto label = Ui::CreateChild<Ui::FlatLabel>(
		raw,
		TrNow("update_available").arg(release.version()),
		st::defaultFlatLabel);
	const auto update = Ui::CreateChild<Ui::LinkButton>(
		raw,
		TrNow("update_open"),
		st::defaultLinkButton);
	const auto close = Ui::CreateChild<Ui::LinkButton>(
		raw,
		TrNow("update_close"),
		st::defaultLinkButton);
	update->setClickedCallback([=] { OpenReleasePage(release); });
	close->setClickedCallback([=] { Dismiss(release); });

	raw->widthValue() | rpl::on_next([=](int width) {
		const auto skip = style::ConvertScale(12);
		close->moveToRight(skip, (height - close->height()) / 2, width);
		update->moveToRight(
			skip + close->width() + skip,
			(height - update->height()) / 2,
			width);
		const auto available = width
			- skip * 4
			- close->width()
			- update->width();
		label->resizeToWidth(std::max(available, 0));
		label->moveToLeft(skip, (height - label->height()) / 2, width);
	}, raw->lifetime());

	raw->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(raw);
		p.fillRect(raw->rect(), st::windowBg);
		p.fillRect(0, 0, raw->width(), st::lineWidth, st::shadowFg);
	}, raw->lifetime());

	return result;
}

} // namespace MZGram::Update
