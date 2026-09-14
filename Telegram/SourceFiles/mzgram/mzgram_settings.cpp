/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_settings.h"

#include "base/options.h"
#include "boxes/peer_list_box.h"
#include "boxes/peer_list_controllers.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mzgram/mzgram_archive.h"
#include "mzgram/mzgram_message_store.h"
#include "mzgram/mzgram_options.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/number_input.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

namespace Settings {
namespace {

using namespace Builder;

constexpr auto kMegabyte = int64(1024) * 1024;

// Strings stay in this file rather than in lang.strings: that file generates
// a header nearly every source includes, so each new key would rebuild the
// whole client. The cost is that this section is not in settings search.

[[nodiscard]] rpl::producer<QString> Text(const char *text) {
	return rpl::single(QString::fromUtf8(text));
}

[[nodiscard]] QString LimitText(int64 bytes) {
	return (bytes > 0)
		? (QString::number(bytes / kMegabyte) + u" MB"_q)
		: u"No limit"_q;
}

[[nodiscard]] rpl::producer<bool> OptionValue(const char id[]) {
	const auto option = &base::options::lookup<bool>(id);
	return rpl::single(
		rpl::empty
	) | rpl::then(
		option->changes()
	) | rpl::map([=] {
		return option->value();
	});
}

// The switches store their state in base::options, the same storage the
// Experimental section used, so settings made there earlier carry over.
void AddOptionToggle(
		SectionBuilder &builder,
		const QString &id,
		const char *title,
		const char optionId[],
		QStringList keywords,
		rpl::producer<bool> shown = nullptr) {
	const auto button = builder.addButton({
		.id = id,
		.title = Text(title),
		.st = &st::settingsButtonNoIcon,
		.toggled = OptionValue(optionId),
		.keywords = std::move(keywords),
		.shown = std::move(shown),
	});
	if (!button) {
		return;
	}
	const auto option = &base::options::lookup<bool>(optionId);
	button->toggledChanges(
	) | rpl::filter([=](bool toggled) {
		return (toggled != option->value());
	}) | rpl::on_next([=](bool toggled) {
		option->set(toggled);
	}, button->lifetime());
}

void MediaLimitBox(not_null<Ui::GenericBox*> box) {
	const auto megabytes = MZGram::MessageStore::Instance().mediaSizeLimit()
		/ kMegabyte;
	box->setTitle(Text("Media size limit"));
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		Text("Videos and files larger than this are not saved. Photos, "
			"voice messages, round videos and view-once media are always "
			"saved. Enter 0 for no limit."),
		st::boxLabel));
	// NumberInput is not an RpWidget, so addRow cannot take it directly.
	const auto wrap = box->addRow(object_ptr<Ui::FixedHeightWidget>(
		box,
		st::defaultInputField.heightMin));
	const auto input = Ui::CreateChild<Ui::NumberInput>(
		wrap,
		st::defaultInputField,
		Text("Megabytes"),
		QString::number(megabytes),
		1024 * 1024);
	wrap->widthValue() | rpl::on_next([=](int width) {
		input->move(0, 0);
		input->resize(width, input->height());
		wrap->resize(width, input->height());
	}, wrap->lifetime());
	const auto save = [=] {
		const auto value = input->getLastText().toLongLong();
		MZGram::MessageStore::Instance().setMediaSizeLimit(
			value * kMegabyte);
		box->closeBox();
	};
	QObject::connect(input, &Ui::MaskedInputField::submitted, [=] {
		save();
	});
	box->setFocusCallback([=] {
		input->setFocusFast();
	});
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
}

void ShowAddChatBox(
		not_null<Window::SessionController*> controller,
		not_null<Main::Session*> session) {
	const auto weak = std::make_shared<base::weak_qptr<Ui::BoxContent>>();
	auto chosen = [=](not_null<Data::Thread*> thread) {
		MZGram::SetTracked(thread->peer(), true);
		if (const auto strong = weak->get()) {
			strong->closeBox();
		}
	};
	auto initBox = [](not_null<PeerListBox*> box) {
		box->addButton(tr::lng_cancel(), [=] {
			box->closeBox();
		});
	};
	*weak = controller->show(Box<PeerListBox>(
		std::make_unique<ChooseRecipientBoxController>(ChooseRecipientArgs{
			.session = session,
			.callback = std::move(chosen),
		}),
		std::move(initBox)));
}

void FillTrackedChats(
		not_null<Ui::VerticalLayout*> list,
		not_null<Window::SessionController*> controller,
		not_null<Main::Session*> session) {
	while (list->count()) {
		delete list->widgetAt(0);
	}
	const auto account = session->uniqueId();
	const auto peers = MZGram::MessageStore::Instance().trackedPeers(account);
	if (peers.empty()) {
		list->add(
			object_ptr<Ui::FlatLabel>(
				list,
				Text("No chats yet. Add one here or from a chat menu."),
				st::defaultFlatLabel),
			st::settingsButtonNoIcon.padding);
	}
	for (const auto peerValue : peers) {
		const auto peer = session->data().peerLoaded(
			PeerId(PeerIdHelper(peerValue)));
		const auto name = peer
			? peer->name()
			: (u"Chat "_q + QString::number(peerValue));
		const auto button = list->add(object_ptr<Ui::SettingsButton>(
			list,
			rpl::single(name),
			st::settingsButtonNoIcon));
		button->setClickedCallback([=] {
			controller->show(Ui::MakeConfirmBox({
				.text = u"Stop saving deleted messages in %1? "
					"Messages saved so far stay."_q.arg(name),
				.confirmed = [=](Fn<void()> close) {
					MZGram::MessageStore::Instance().setTracked(
						account,
						peerValue,
						false);
					close();
				},
				.confirmText = tr::lng_box_remove(),
			}));
		});
	}
}

void BuildMZGramSection(SectionBuilder &builder) {
	using namespace MZGram;

	const auto session = builder.session();

	builder.addSkip();
	builder.addSubsectionTitle(Text("Appearance"));
	AddOptionToggle(
		builder,
		u"mzgram/message-seconds"_q,
		"Show seconds in message time",
		kOptionMessageSeconds,
		{ u"seconds"_q, u"time"_q, u"clock"_q });
	AddOptionToggle(
		builder,
		u"mzgram/disable-stories"_q,
		"Hide Stories",
		kOptionDisableStories,
		{ u"stories"_q, u"hide"_q });
	AddOptionToggle(
		builder,
		u"mzgram/disable-greeting-sticker"_q,
		"Disable greeting sticker",
		kOptionDisableGreetingSticker,
		{ u"greeting"_q, u"sticker"_q, u"hello"_q });
	AddOptionToggle(
		builder,
		u"mzgram/hide-channel-bottom-button"_q,
		"Hide channel bottom button",
		kOptionHideChannelBottomButton,
		{ u"channel"_q, u"mute"_q, u"unmute"_q, u"button"_q });
	builder.addSkip();
	builder.addDividerText(Text("Ported from AyuGram Desktop."));

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/open-archive-on-pull"_q,
		"Open Archive on pull down",
		kOptionOpenArchiveOnPull,
		{ u"archive"_q, u"pull"_q, u"overscroll"_q });
	builder.addSkip();
	builder.addDividerText(Text("Pulling the chat list down past the Archive "
		"row opens it, mirroring MZGram Android. MZGram's own code: "
		"AyuGram Desktop has no equivalent."));

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/media-preview-on-chat-preview"_q,
		"Media preview instead of Chat Preview",
		kOptionMediaPreviewOnChatPreview,
		{ u"media"_q, u"preview"_q, u"chat preview"_q, u"photo"_q, u"video"_q });
	builder.addSkip();
	builder.addDividerText(Text("Press-and-hold a chat's avatar normally "
		"opens the Chat Preview peek. When the last message is a photo or "
		"video, this shows it directly instead. MZGram's own code, "
		"mirroring the Android version."));

	builder.addSkip();
	builder.addSubsectionTitle(Text("Chat"));
	AddOptionToggle(
		builder,
		u"mzgram/auto-pause-video"_q,
		"Auto pause video",
		kOptionAutoPauseVideo,
		{ u"video"_q, u"pause"_q, u"focus"_q, u"minimize"_q });
	AddOptionToggle(
		builder,
		u"mzgram/voice-confirmation"_q,
		"Confirm before sending voice messages",
		kOptionVoiceConfirmation,
		{ u"voice"_q, u"confirm"_q });
	AddOptionToggle(
		builder,
		u"mzgram/round-confirmation"_q,
		"Confirm before sending round videos",
		kOptionRoundConfirmation,
		{ u"round"_q, u"video"_q, u"confirm"_q });
	builder.addSkip();
	builder.addDividerText(Text("Pauses the video viewer when the window is "
		"minimized or loses focus (MZGram's own code, mirroring the "
		"Android version). Voice and round confirmation are ported from "
		"AyuGram Desktop."));

	builder.addSkip();
	builder.addSubsectionTitle(Text("Context menu"));
	AddOptionToggle(
		builder,
		u"mzgram/repeat-message"_q,
		"Show \"Repeat\" in the message menu",
		kOptionRepeatMessage,
		{ u"repeat"_q, u"resend"_q });
	AddOptionToggle(
		builder,
		u"mzgram/message-details"_q,
		"Show \"Message details\" in the message menu",
		kOptionMessageDetails,
		{ u"details"_q, u"id"_q, u"views"_q });
	AddOptionToggle(
		builder,
		u"mzgram/scan-qr-code"_q,
		"Show \"Scan for QR code\" on photos",
		kOptionScanQrCode,
		{ u"qr"_q, u"scan"_q, u"code"_q });
	AddOptionToggle(
		builder,
		u"mzgram/save-message"_q,
		"Show \"Save message\" in the message menu",
		kOptionSaveMessage,
		{ u"save"_q, u"saved messages"_q, u"forward"_q });
	AddOptionToggle(
		builder,
		u"mzgram/set-reminder"_q,
		"Show \"Set a reminder\" in the message menu",
		kOptionSetReminder,
		{ u"reminder"_q, u"schedule"_q, u"saved messages"_q });
	AddOptionToggle(
		builder,
		u"mzgram/open-in"_q,
		"Show \"Open in...\" for downloaded files",
		kOptionOpenIn,
		{ u"open"_q, u"file"_q, u"video"_q });
	builder.addSkip();
	builder.addDividerText(Text("Repeat and Message details are ported from "
		"AyuGram Desktop. Scan for QR code, Save message, Set a reminder and "
		"Open in... are MZGram's own code, mirroring the Android version: QR "
		"scanning decodes an already-downloaded photo entirely offline, Save "
		"message forwards to Saved Messages in one click, Set a reminder "
		"forwards there scheduled for a chosen time, and Open in... shows "
		"the OS \"Open with\" dialog for a downloaded file."));

	builder.addSkip();
	builder.addSubsectionTitle(Text("Ghost mode"));
	AddOptionToggle(
		builder,
		u"mzgram/ghost-mode"_q,
		"Ghost mode",
		kOptionGhostMode,
		{ u"ghost"_q, u"invisible"_q, u"stealth"_q });

	// The exceptions only mean something while ghost mode is on.
	AddOptionToggle(
		builder,
		u"mzgram/ghost-read"_q,
		"Send read receipts",
		kOptionSendReadReceipts,
		{ u"ghost"_q, u"read"_q, u"receipts"_q },
		OptionValue(kOptionGhostMode));
	AddOptionToggle(
		builder,
		u"mzgram/ghost-typing"_q,
		"Send typing status",
		kOptionSendTyping,
		{ u"ghost"_q, u"typing"_q, u"recording"_q },
		OptionValue(kOptionGhostMode));
	AddOptionToggle(
		builder,
		u"mzgram/ghost-online"_q,
		"Send online status",
		kOptionSendOnline,
		{ u"ghost"_q, u"online"_q, u"last seen"_q },
		OptionValue(kOptionGhostMode));
	builder.addSkip();
	builder.addDividerText(Text("Ghost mode hides your read receipts, "
		"typing and online status. The switches above let some of them "
		"through."));

	builder.addSkip();
	builder.addSubsectionTitle(Text("Message history"));
	AddOptionToggle(
		builder,
		u"mzgram/anti-recall"_q,
		"Keep deleted messages",
		kOptionAntiRecall,
		{ u"anti-recall"_q, u"deleted"_q, u"messages"_q });
	AddOptionToggle(
		builder,
		u"mzgram/edit-history"_q,
		"Keep edit history",
		kOptionEditHistory,
		{ u"edit"_q, u"history"_q, u"edited"_q });
	AddOptionToggle(
		builder,
		u"mzgram/keep-self-destructing"_q,
		"Keep view-once media",
		kOptionKeepSelfDestructing,
		{ u"view once"_q, u"self-destruct"_q, u"timer"_q, u"media"_q });
	AddOptionToggle(
		builder,
		u"mzgram/mark-messages"_q,
		"Mark deleted and edited messages",
		kOptionMarkMessages,
		{ u"dim"_q, u"pencil"_q, u"deleted"_q, u"edited"_q });
	builder.addSkip();
	builder.addDividerText(Text("Works only in the chats below. Messages "
		"and media are saved on this device, in an unencrypted database, "
		"and stay after a restart."));

	builder.addSkip();
	builder.addSubsectionTitle(Text("Chats"));
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"mzgram/add-chat"_q,
		.title = Text("Add chat"),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			ShowAddChatBox(controller, session);
		},
		.keywords = { u"chats"_q, u"groups"_q, u"channels"_q, u"add"_q },
	});
	builder.add([=](const WidgetContext &ctx) {
		const auto list = ctx.container->add(
			object_ptr<Ui::VerticalLayout>(ctx.container));
		const auto controller = ctx.controller;
		const auto fill = [=] {
			FillTrackedChats(list, controller, session);
			list->resizeToWidth(ctx.container->width());
		};
		fill();
		MessageStore::Instance().trackedChanges(
		) | rpl::on_next(fill, list->lifetime());
		return SectionBuilder::WidgetToAdd{};
	});
	builder.addButton({
		.id = u"mzgram/media-limit"_q,
		.title = Text("Media size limit"),
		.st = &st::settingsButtonNoIcon,
		.label = MessageStore::Instance().mediaSizeLimitValue(
		) | rpl::map(LimitText),
		.onClick = [=] {
			controller->show(Box(MediaLimitBox));
		},
		.keywords = { u"size"_q, u"limit"_q, u"media"_q, u"video"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("In these chats photos, voice messages, "
		"round videos and view-once media are saved as they arrive. Videos "
		"and files are saved up to the size limit."));
}

class MZGramSection : public Section<MZGramSection> {
public:
	MZGramSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

MZGramSection::MZGramSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> MZGramSection::title() {
	return Text("MZGram");
}

void MZGramSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	const SectionBuildMethod buildMethod = [](
			not_null<Ui::VerticalLayout*> container,
			not_null<Window::SessionController*> controller,
			Fn<void(Type)> showOther,
			rpl::producer<> showFinished) {
		auto builder = SectionBuilder(WidgetContext{
			.container = container,
			.controller = controller,
			.showOther = std::move(showOther),
		});
		BuildMZGramSection(builder);
	};

	build(content, buildMethod);
	Ui::ResizeFitChild(this, content);
}

} // namespace

Type MZGramId() {
	return MZGramSection::Id();
}

} // namespace Settings
