/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_context_actions.h"
#include "mzgram/mzgram_lang.h"

#include "api/api_common.h"
#include "apiwrap.h"
#include "base/unixtime.h"
#include "core/file_utilities.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_types.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "history/view/history_view_schedule_box.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "menu/menu_send_details.h"
#include "mzgram/mzgram_options.h"
#include "mzgram/mzgram_presence.h"
#include "ui/layers/generic_box.h"
#include "ui/text/format_values.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

namespace MZGram {
namespace {

[[nodiscard]] QString FormatMoment(TimeId date) {
	if (date <= 0) {
		return QString();
	}
	const auto when = base::unixtime::parse(date);
	return QLocale().toString(when, QLocale::ShortFormat);
}

[[nodiscard]] int64 MediaByteSize(not_null<Data::Media*> media) {
	if (const auto document = media->document()) {
		return document->size;
	} else if (const auto photo = media->photo()) {
		for (const auto size : {
				Data::PhotoSize::Large,
				Data::PhotoSize::Small,
				Data::PhotoSize::Thumbnail }) {
			if (const auto bytes = photo->videoByteSize(size)) {
				return bytes;
			}
		}
		for (const auto size : {
				Data::PhotoSize::Large,
				Data::PhotoSize::Small,
				Data::PhotoSize::Thumbnail }) {
			if (const auto bytes = photo->imageByteSize(size)) {
				return bytes;
			}
		}
	}
	return 0;
}

[[nodiscard]] QString MediaMime(not_null<Data::Media*> media) {
	if (const auto document = media->document()) {
		return document->mimeString();
	} else if (const auto photo = media->photo()) {
		return photo->hasVideo() ? u"video/mp4"_q : u"image/jpeg"_q;
	}
	return QString();
}

[[nodiscard]] QString MediaResolution(not_null<Data::Media*> media) {
	const auto format = [](QSize size) {
		return (size.isEmpty() || !size.isValid())
			? QString()
			: (QString::number(size.width())
				+ u"x"_q
				+ QString::number(size.height()));
	};
	if (const auto document = media->document()) {
		return format(document->dimensions);
	} else if (const auto photo = media->photo()) {
		for (const auto size : {
				Data::PhotoSize::Large,
				Data::PhotoSize::Small,
				Data::PhotoSize::Thumbnail }) {
			if (const auto found = photo->size(size)) {
				return format(*found);
			}
		}
	}
	return QString();
}

} // namespace

void AddRepeatMessageAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller) {
	if (!RepeatMessageAction()) {
		return;
	}
	if (!item->isHistoryEntry()
		|| item->isService()
		|| item->isLocal()
		|| !item->allowsForward()
		|| item->id <= 0) {
		return;
	}
	const auto history = item->history();
	const auto itemId = item->fullId();
	const auto session = &history->session();
	menu->addAction(MZGram::TrNow("repeat"), [=] {
		const auto current = session->data().message(itemId);
		if (!current) {
			return;
		}
		auto draft = Data::ForwardDraft{
			.ids = MessageIdsList{ 1, itemId },
			.options = Data::ForwardOptions::NoSenderNames,
		};
		auto resolved = history->resolveForwardDraft(draft);
		if (resolved.items.empty()) {
			return;
		}
		auto action = Api::SendAction(history);
		action.clearDraft = false;
		session->api().forwardMessages(
			std::move(resolved),
			action,
			[] {});
	}, &st::menuIconForward);
}

void AddMessageDetailsAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller) {
	if (!MessageDetailsAction()) {
		return;
	}
	if (item->isLocal()) {
		return;
	}
	const auto itemId = item->fullId();
	const auto owner = &item->history()->owner();
	menu->addAction(MZGram::TrNow("message_details"), [=] {
		const auto item = owner->message(itemId);
		if (!item) {
			return;
		}
		controller->show(Box([=](not_null<Ui::GenericBox*> box) {
			box->setTitle(MZGram::Tr("message_details"));
			box->setWidth(st::boxWideWidth);
			box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });

			const auto addRow = [&](const QString &label, const QString &value) {
				if (value.isEmpty()) {
					return;
				}
				box->addRow(object_ptr<Ui::FlatLabel>(
					box,
					rpl::single(label + u": "_q + value),
					st::aboutLabel));
			};

			const auto edited = item->Get<HistoryMessageEdited>();
			const auto forwarded = item->Get<HistoryMessageForwarded>();
			const auto isForwarded = forwarded
				&& !forwarded->story
				&& forwarded->psaType.isEmpty();

			addRow(MZGram::TrNow("details_id"), QString::number(item->id.bare));
			addRow(MZGram::TrNow("details_date"), FormatMoment(item->date()));
			if (edited) {
				addRow(MZGram::TrNow("details_edited"), FormatMoment(edited->date));
			}
			// An own message the other side read: when, from the server
			// where it tells, otherwise as this device learned of it.
			if (item->out()
				&& item->isRegular()
				&& !item->unread(item->history())) {
				const auto read = box->lifetime().make_state<
					rpl::variable<QString>>(QString::fromUtf8("\xe2\x80\xa6"));
				const auto show = [=](TimeId at, bool fromServer) {
					*read = !at
						? MZGram::TrNow("details_read_unknown")
						: fromServer
						? FormatMoment(at)
						: MZGram::TrNow("details_read_local").arg(
							FormatMoment(at));
				};
				const auto kept = MZGram::KeptReadTime(item);
				show(kept.first, kept.second);
				const auto user = item->history()->peer->asUser();
				if (!kept.second && user && !user->isBot() && !user->isSelf()) {
					const auto weak = QPointer<Ui::GenericBox>(box.get());
					item->history()->session().api().request(
						MTPmessages_GetOutboxReadDate(
							user->input(),
							MTP_int(item->id.bare))
					).done([=](const MTPOutboxReadDate &result) {
						const auto at = result.data().vdate().v;
						if (const auto item = owner->message(itemId)) {
							MZGram::RememberServerReadTime(item, at);
						}
						if (weak) {
							show(at, true);
						}
					}).send();
				}
				box->addRow(object_ptr<Ui::FlatLabel>(
					box,
					read->value() | rpl::map([](const QString &value) {
						return MZGram::TrNow("details_read") + u": "_q + value;
					}),
					st::aboutLabel));
			}
			if (isForwarded) {
				addRow(
					MZGram::TrNow("details_originally_sent"),
					FormatMoment(forwarded->originalDate));
			}
			if (item->hasViews() && item->viewsCount() > 0) {
				addRow(MZGram::TrNow("details_views"), QString::number(item->viewsCount()));
			}

			if (const auto media = item->media()) {
				const auto size = MediaByteSize(media);
				if (size > 0) {
					addRow(MZGram::TrNow("details_file_size"), Ui::FormatSizeText(size));
				}
				addRow(MZGram::TrNow("details_mime_type"), MediaMime(media));
				if (const auto document = media->document()) {
					addRow(MZGram::TrNow("details_file_name"), document->filename());
				}
				addRow(MZGram::TrNow("details_resolution"), MediaResolution(media));
			}
		}));
	}, &st::menuIconInfo);
}

void AddSaveMessageAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller) {
	if (!SaveMessageAction()) {
		return;
	}
	if (!item->isHistoryEntry()
		|| item->isService()
		|| item->isLocal()
		|| !item->allowsForward()
		|| item->id <= 0) {
		return;
	}
	const auto history = item->history();
	if (history->peer->isSelf()) {
		return; // Already in Saved Messages.
	}
	const auto itemId = item->fullId();
	const auto session = &history->session();
	menu->addAction(MZGram::TrNow("save_message"), [=] {
		const auto current = session->data().message(itemId);
		if (!current) {
			return;
		}
		auto draft = Data::ForwardDraft{ .ids = MessageIdsList{ 1, itemId } };
		auto resolved = history->resolveForwardDraft(draft);
		if (resolved.items.empty()) {
			return;
		}
		const auto to = session->data().history(session->user());
		auto action = Api::SendAction(to);
		action.clearDraft = false;
		session->api().forwardMessages(
			std::move(resolved),
			action,
			[] {});
	}, &st::menuIconSavedMessages);
}

void AddSetReminderAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller) {
	if (!SetReminderAction()) {
		return;
	}
	if (!item->isHistoryEntry()
		|| item->isService()
		|| item->isLocal()
		|| !item->allowsForward()
		|| item->id <= 0) {
		return;
	}
	const auto history = item->history();
	if (history->peer->isSelf()) {
		return; // Reminders forward into Saved Messages; already there.
	}
	const auto itemId = item->fullId();
	const auto session = &history->session();
	menu->addAction(MZGram::TrNow("set_reminder"), [=] {
		const auto submit = [=](Api::SendOptions options) {
			const auto current = session->data().message(itemId);
			if (!current) {
				return;
			}
			auto draft = Data::ForwardDraft{ .ids = MessageIdsList{ 1, itemId } };
			auto resolved = history->resolveForwardDraft(draft);
			if (resolved.items.empty()) {
				return;
			}
			const auto to = session->data().history(session->user());
			auto action = Api::SendAction(to, options);
			action.clearDraft = false;
			session->api().forwardMessages(
				std::move(resolved),
				action,
				[] {});
		};
		controller->show(HistoryView::PrepareScheduleBox(
			session,
			controller->uiShow(),
			SendMenu::Details{
				.type = SendMenu::Type::Reminder,
				.barePeerId = session->user()->id.value,
			},
			submit));
	}, &st::menuIconSchedule);
}

void AddOpenInAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<DocumentData*> document) {
	if (!OpenInAction()) {
		return;
	}
	const auto filepath = document->filepath(true);
	if (filepath.isEmpty()) {
		return;
	}
	menu->addAction(MZGram::TrNow("open_in"), [=] {
		File::OpenWith(filepath);
	}, &st::menuIconFile);
}

} // namespace MZGram
