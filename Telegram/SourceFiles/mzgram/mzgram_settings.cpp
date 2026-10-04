/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_settings.h"

#include "base/options.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mzgram/mzgram_archive.h"
#include "mzgram/mzgram_archive_rules.h"
#include "mzgram/mzgram_lang.h"
#include "mzgram/mzgram_message_store.h"
#include "mzgram/mzgram_options.h"
#include "mzgram/mzgram_protected_content.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/toast/toast.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

#include <QtCore/QFile>

#include <array>

namespace Settings {
namespace {

using namespace Builder;

// Texts come from MZGram's own table (mzgram_lang.h), in the app's
// language, not from lang.strings. The cost is that this section is not in
// settings search.

[[nodiscard]] rpl::producer<QString> Text(const char *key) {
	return MZGram::Tr(key);
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
		const char *titleKey,
		const char optionId[],
		QStringList keywords,
		rpl::producer<bool> shown = nullptr) {
	const auto button = builder.addButton({
		.id = id,
		.title = Text(titleKey),
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

void BuildArchive(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/save-deleted-and-edited"_q,
		"save_deleted_and_edited",
		kOptionSaveDeletedAndEdited,
		{
			u"anti-recall"_q,
			u"deleted"_q,
			u"edited"_q,
			u"history"_q,
			u"view once"_q,
		});
	AddOptionToggle(
		builder,
		u"mzgram/mark-messages"_q,
		"mark_messages",
		kOptionMarkMessages,
		{ u"dim"_q, u"pencil"_q, u"deleted"_q, u"edited"_q });
	builder.addSkip();
	builder.addDividerText(Text("archive_info"));

	builder.addSkip();
	builder.addDividerText(Text("saved_media_note"));

	builder.addSkip();
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"mzgram/export-archive"_q,
		.title = Text("export_archive"),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			MessageStore::Instance().closeForFileOp();
			FileDialog::GetWritePath(
				Core::App().getFileDialogParent(),
				MZGram::TrNow("export_archive_title"),
				MZGram::TrNow("sqlite_database_filter"),
				"mzgram_archive_export.db",
				[=](QString &&result) {
					if (result.isEmpty()) {
						return;
					}
					QFile::remove(result);
					if (QFile::copy(MessageStore::Instance().databasePath(), result)) {
						Ui::Toast::Show(MZGram::TrNow("archive_exported"));
					} else {
						Ui::Toast::Show(MZGram::TrNow("archive_export_failed"));
					}
				});
		},
		.keywords = { u"export"_q, u"archive"_q, u"backup"_q },
	});
	builder.addButton({
		.id = u"mzgram/import-archive"_q,
		.title = Text("import_archive"),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = MZGram::TrNow("import_archive_confirm"),
				.confirmed = crl::guard(controller, [=] {
					FileDialog::GetOpenPath(
						Core::App().getFileDialogParent(),
						MZGram::TrNow("import_archive_title"),
						MZGram::TrNow("sqlite_database_filter"),
						[=](const FileDialog::OpenResult &result) {
							if (result.paths.isEmpty()) {
								return;
							}
							MessageStore::Instance().closeForFileOp();
							const auto path = MessageStore::Instance()
								.databasePath();
							QFile::remove(path);
							if (QFile::copy(result.paths.front(), path)) {
								MessageStore::Instance()
									.reopenAfterFileReplace();
								Ui::Toast::Show(MZGram::TrNow("archive_imported"));
							} else {
								Ui::Toast::Show(
									MZGram::TrNow("archive_import_failed"));
							}
						});
				}),
				.confirmText = MZGram::TrNow("import"),
				.confirmStyle = &st::attentionBoxButton,
			}));
		},
		.keywords = { u"import"_q, u"archive"_q, u"restore"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("export_import_info"));

	builder.addSkip();
	builder.addButton({
		.id = u"mzgram/wipe-archive"_q,
		.title = Text("clear_archive"),
		.st = &st::settingsAttentionButton,
		.onClick = [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = MZGram::TrNow("clear_archive_confirm"),
				.confirmed = [=] {
					MessageStore::Instance().wipeAll();
				},
				.confirmText = tr::lng_box_delete(),
				.confirmStyle = &st::attentionBoxButton,
			}));
		},
		.keywords = { u"clear"_q, u"wipe"_q, u"delete"_q, u"archive"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("clear_archive_info"));
}

void BuildPrivacy(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/spy-hide-online-status"_q,
		"hide_own_online",
		kOptionSpyHideOnlineStatus,
		{ u"spy"_q, u"online"_q, u"last seen"_q });
	builder.addSkip();
	builder.addDividerText(Text("hide_own_online_info"));
}

void BuildGhostMode(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/ghost-mode"_q,
		"ghost_mode",
		kOptionGhostMode,
		{ u"ghost"_q, u"invisible"_q, u"stealth"_q });

	// The exceptions only mean something while ghost mode is on.
	AddOptionToggle(
		builder,
		u"mzgram/ghost-read"_q,
		"send_read_receipts",
		kOptionSendReadReceipts,
		{ u"ghost"_q, u"read"_q, u"receipts"_q },
		OptionValue(kOptionGhostMode));
	AddOptionToggle(
		builder,
		u"mzgram/ghost-typing"_q,
		"send_typing",
		kOptionSendTyping,
		{ u"ghost"_q, u"typing"_q, u"recording"_q },
		OptionValue(kOptionGhostMode));
	AddOptionToggle(
		builder,
		u"mzgram/ghost-online"_q,
		"send_online",
		kOptionSendOnline,
		{ u"ghost"_q, u"online"_q, u"last seen"_q },
		OptionValue(kOptionGhostMode));
	builder.addSkip();
	builder.addDividerText(Text("ghost_mode_info"));

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/ghost-auto-delay-send"_q,
		"delay_sending",
		kOptionGhostAutoDelaySend,
		{ u"delay"_q, u"send"_q, u"online"_q },
		OptionValue(kOptionGhostMode));
	builder.addSkip();
	builder.addDividerText(Text("delay_sending_info"));

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/ghost-silent-send"_q,
		"send_without_sound",
		kOptionGhostSilentSend,
		{ u"silent"_q, u"sound"_q, u"notification"_q },
		OptionValue(kOptionGhostMode));
	builder.addSkip();
	builder.addDividerText(Text("send_without_sound_info"));

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/offer-ghost-mode-before-stories"_q,
		"offer_ghost_stories",
		kOptionOfferGhostModeBeforeStories,
		{ u"ghost"_q, u"stories"_q, u"seen"_q });
	builder.addSkip();
	builder.addDividerText(Text("offer_ghost_stories_info"));
}

void BuildMessageMenu(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/repeat-message"_q,
		"show_repeat",
		kOptionRepeatMessage,
		{ u"repeat"_q, u"resend"_q });
	AddOptionToggle(
		builder,
		u"mzgram/message-details"_q,
		"show_details",
		kOptionMessageDetails,
		{ u"details"_q, u"id"_q, u"views"_q });
	AddOptionToggle(
		builder,
		u"mzgram/scan-qr-code"_q,
		"show_scan_qr",
		kOptionScanQrCode,
		{ u"qr"_q, u"scan"_q, u"code"_q });
	AddOptionToggle(
		builder,
		u"mzgram/save-message"_q,
		"show_save_message",
		kOptionSaveMessage,
		{ u"save"_q, u"saved messages"_q, u"forward"_q });
	AddOptionToggle(
		builder,
		u"mzgram/set-reminder"_q,
		"show_set_reminder",
		kOptionSetReminder,
		{ u"reminder"_q, u"schedule"_q, u"saved messages"_q });
	AddOptionToggle(
		builder,
		u"mzgram/open-in"_q,
		"show_open_in",
		kOptionOpenIn,
		{ u"open"_q, u"file"_q, u"video"_q });
	builder.addSkip();
	builder.addDividerText(Text("message_menu_info"));
	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/save-protected-content"_q,
		"save_protected_content",
		kOptionSaveProtectedContent,
		{ u"protected"_q, u"forward"_q, u"save"_q, u"copy"_q, u"screenshot"_q });
	builder.addSkip();
	builder.addDividerText(Text("save_protected_content_info"));
}

void BuildMediaCalls(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/auto-pause-video"_q,
		"auto_pause_video",
		kOptionAutoPauseVideo,
		{ u"video"_q, u"pause"_q, u"focus"_q, u"minimize"_q });
	AddOptionToggle(
		builder,
		u"mzgram/voice-confirmation"_q,
		"confirm_voice",
		kOptionVoiceConfirmation,
		{ u"voice"_q, u"confirm"_q });
	AddOptionToggle(
		builder,
		u"mzgram/round-confirmation"_q,
		"confirm_round",
		kOptionRoundConfirmation,
		{ u"round"_q, u"video"_q, u"confirm"_q });
	builder.addSkip();
	builder.addDividerText(Text("media_calls_info"));

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/media-preview-on-chat-preview"_q,
		"media_preview",
		kOptionMediaPreviewOnChatPreview,
		{ u"media"_q, u"preview"_q, u"chat preview"_q, u"photo"_q, u"video"_q });
	builder.addSkip();
	builder.addDividerText(Text("media_preview_info"));
}

void BuildInterface(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/message-seconds"_q,
		"message_seconds",
		kOptionMessageSeconds,
		{ u"seconds"_q, u"time"_q, u"clock"_q });
	AddOptionToggle(
		builder,
		u"mzgram/disable-stories"_q,
		"hide_stories",
		kOptionDisableStories,
		{ u"stories"_q, u"hide"_q });
	AddOptionToggle(
		builder,
		u"mzgram/disable-greeting-sticker"_q,
		"disable_greeting_sticker",
		kOptionDisableGreetingSticker,
		{ u"greeting"_q, u"sticker"_q, u"hello"_q });
	AddOptionToggle(
		builder,
		u"mzgram/hide-channel-bottom-button"_q,
		"hide_channel_button",
		kOptionHideChannelBottomButton,
		{ u"channel"_q, u"mute"_q, u"unmute"_q, u"button"_q });
	builder.addSkip();
	builder.addDividerText(Text("interface_info"));

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/open-archive-on-pull"_q,
		"open_archive_on_pull",
		kOptionOpenArchiveOnPull,
		{ u"archive"_q, u"pull"_q, u"overscroll"_q });
	builder.addSkip();
	builder.addDividerText(Text("open_archive_on_pull_info"));
}

void BuildAdsFilters(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/disable-sponsored-messages"_q,
		"disable_sponsored",
		kOptionDisableSponsoredMessages,
		{ u"sponsored"_q, u"ads"_q, u"advertisement"_q });
	builder.addSkip();
	builder.addDividerText(Text("disable_sponsored_info"));

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/strip-zalgo-text"_q,
		"zalgo_filter",
		kOptionStripZalgoText,
		{ u"zalgo"_q, u"corrupted"_q, u"text"_q });
	builder.addSkip();
	builder.addDividerText(Text("zalgo_filter_info"));
}

struct Topic {
	const char *title = nullptr;
	const style::icon *icon = nullptr;
	void (*build)(SectionBuilder &builder) = nullptr;
};

// The topics, in the order the MZGram page lists them.
[[nodiscard]] const std::array<Topic, 7> &Topics() {
	static const auto result = std::array<Topic, 7>{ {
		{ "section_archive", &st::menuIconArchive, BuildArchive },
		{ "section_privacy", &st::menuIconLock, BuildPrivacy },
		{ "section_ghost_mode", &st::menuIconStealth, BuildGhostMode },
		{ "section_message_menu", &st::menuIconChatBubble, BuildMessageMenu },
		{ "section_media_calls", &st::menuIconPhone, BuildMediaCalls },
		{ "section_interface", &st::menuIconPalette, BuildInterface },
		{ "section_ads_filters", &st::menuIconBlock, BuildAdsFilters },
	} };
	return result;
}

[[nodiscard]] Type TopicId(int index);

// Settings > MZGram: one row per topic; each opens the topic's own page.
void BuildMZGramSection(SectionBuilder &builder) {
	const auto &topics = Topics();
	builder.addSkip();
	for (auto i = 0; i != int(topics.size()); ++i) {
		builder.addSectionButton({
			.title = Text(topics[i].title),
			.targetSection = TopicId(i),
			.icon = { topics[i].icon },
		});
	}
	builder.addSkip();
	builder.addDividerText(Text("settings_topics_info"));
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
	return Text("settings_title");
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

// A topic's own page, opened from the MZGram page; Back returns there.
template <int Index>
class MZGramTopicSection : public Section<MZGramTopicSection<Index>> {
public:
	MZGramTopicSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
	: Section<MZGramTopicSection<Index>>(parent, controller) {
		setupContent();
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return Text(Topics()[Index].title);
	}

private:
	void setupContent() {
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
			Topics()[Index].build(builder);
		};
		this->build(content, buildMethod);
		Ui::ResizeFitChild(this, content);
	}

};

Type TopicId(int index) {
	switch (index) {
	case 0: return MZGramTopicSection<0>::Id();
	case 1: return MZGramTopicSection<1>::Id();
	case 2: return MZGramTopicSection<2>::Id();
	case 3: return MZGramTopicSection<3>::Id();
	case 4: return MZGramTopicSection<4>::Id();
	case 5: return MZGramTopicSection<5>::Id();
	case 6: return MZGramTopicSection<6>::Id();
	}
	Unexpected("Index in MZGram TopicId.");
}

} // namespace

Type MZGramId() {
	return MZGramSection::Id();
}

} // namespace Settings
