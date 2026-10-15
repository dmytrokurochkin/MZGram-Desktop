/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_update_bar.h"

#include "mzgram/mzgram_lang.h"
#include "mzgram/mzgram_self_update.h"
#include "mzgram/mzgram_update.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "styles/style_dialogs.h"
#include "styles/style_widgets.h"

namespace MZGram::Update {
namespace {

using Step = Status::Step;

[[nodiscard]] QString StatusText(const Release &release, const Status &status) {
	const auto version = release.version();
	switch (status.step) {
	case Step::Offer:
		return TrNow("update_available").arg(version);
	case Step::Downloading:
		return TrNow("update_downloading").arg(version).arg(status.percent);
	case Step::Verifying:
		return TrNow("update_verifying").arg(version);
	case Step::Installing:
		return TrNow("update_installing").arg(version);
	case Step::Failed:
		return TrNow(status.failure ? status.failure : "update_failed_network").arg(version);
	case Step::NotWritable:
		return TrNow("update_not_writable").arg(version);
	case Step::RolledBack:
		return TrNow("update_rolled_back").arg(version);
	}
	return QString();
}

} // namespace

object_ptr<Ui::RpWidget> CreateBar(
		not_null<QWidget*> parent,
		const Release &release) {
	auto result = object_ptr<Ui::RpWidget>(parent);
	const auto raw = result.data();
	const auto minHeight = st::dialogsUpdateButton.height;
	raw->resize(parent->width(), minHeight);

	const auto label = Ui::CreateChild<Ui::FlatLabel>(
		raw,
		QString(),
		st::defaultFlatLabel);
	const auto page = Ui::CreateChild<Ui::LinkButton>(
		raw,
		TrNow("update_page"),
		st::defaultLinkButton);
	const auto update = Ui::CreateChild<Ui::LinkButton>(
		raw,
		TrNow("update_open"),
		st::defaultLinkButton);
	const auto retry = Ui::CreateChild<Ui::LinkButton>(
		raw,
		TrNow("update_retry"),
		st::defaultLinkButton);
	const auto close = Ui::CreateChild<Ui::LinkButton>(
		raw,
		TrNow("update_close"),
		st::defaultLinkButton);
	page->setClickedCallback([=] { OpenReleasePage(release); });
	update->setClickedCallback([=] { Install(release); });
	retry->setClickedCallback([=] { Install(release); });
	close->setClickedCallback([=] { Dismiss(release); });

	// The buttons from the right, then the text in what is left; the bar
	// grows when the text takes more lines (the chat list lays it out by
	// its height).
	const auto layout = [=] {
		const auto width = raw->width();
		const auto skip = style::ConvertScale(12);
		const auto buttons = { close, retry, update, page };
		auto buttonsWidth = 0;
		for (const auto button : buttons) {
			if (!button->isHidden()) {
				buttonsWidth += button->width() + skip;
			}
		}
		label->resizeToWidth(std::max(width - skip * 2 - buttonsWidth, 0));
		const auto height = std::max(minHeight, label->height() + skip);
		if (raw->height() != height) {
			raw->resize(width, height);
		}
		auto right = skip;
		for (const auto button : buttons) {
			if (!button->isHidden()) {
				button->moveToRight(right, (height - button->height()) / 2, width);
				right += button->width() + skip;
			}
		}
		label->moveToLeft(skip, (height - label->height()) / 2, width);
	};

	StatusValue(
	) | rpl::on_next([=](const Status &status) {
		const auto step = status.step;
		const auto failed = (step == Step::Failed);
		const auto stopped = failed
			|| (step == Step::NotWritable)
			|| (step == Step::RolledBack);
		page->setVisible(stopped);
		update->setVisible(step == Step::Offer);
		retry->setVisible(failed);
		close->setVisible(stopped || (step == Step::Offer));
		label->setText(StatusText(release, status));
		layout();
		raw->update();
	}, raw->lifetime());

	raw->widthValue() | rpl::on_next([=] {
		layout();
	}, raw->lifetime());

	raw->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(raw);
		p.fillRect(raw->rect(), st::windowBg);
		p.fillRect(0, 0, raw->width(), st::lineWidth, st::shadowFg);
	}, raw->lifetime());

	return result;
}

} // namespace MZGram::Update
