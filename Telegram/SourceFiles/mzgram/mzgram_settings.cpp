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
#include "mzgram/mzgram_ai.h"
#include "mzgram/mzgram_archive.h"
#include "mzgram/mzgram_archive_rules.h"
#include "mzgram/mzgram_archive_wipe.h"
#include "mzgram/mzgram_lang.h"
#include "mzgram/mzgram_message_store.h"
#include "mzgram/mzgram_options.h"
#include "mzgram/mzgram_peer_id.h"
#include "mzgram/mzgram_protected_content.h"
#include "mzgram/mzgram_settings_rows.h"
#include "mzgram/mzgram_unblock.h"
#include "mzgram/mzgram_update.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "settings.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/text/format_values.h"
#include "ui/toast/toast.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

#include <QtCore/QFile>
#include <QtGui/QPainterPath>

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

// A switch titled with a name shown as is in every language.
void AddNamedOptionToggle(
		SectionBuilder &builder,
		const QString &id,
		const QString &title,
		const char optionId[],
		QStringList keywords) {
	const auto button = builder.addButton({
		.id = id,
		.title = rpl::single(title),
		.st = &st::settingsButtonNoIcon,
		.toggled = OptionValue(optionId),
		.keywords = std::move(keywords),
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

// The current text of a mark option, kept up to date.
[[nodiscard]] rpl::producer<QString> MarkValue(const char optionId[]) {
	const auto option = &base::options::lookup<QString>(optionId);
	return rpl::single(
		rpl::empty
	) | rpl::then(
		option->changes()
	) | rpl::map([=] {
		return option->value();
	});
}

// Edits the deleted or edited mark: any text, empty for none.
void MarkBox(
		not_null<Ui::GenericBox*> box,
		const char *titleKey,
		const char optionId[]) {
	const auto option = &base::options::lookup<QString>(optionId);
	box->setTitle(Text(titleKey));
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		Text("archive_mark_info"),
		st::boxLabel));
	const auto input = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		Text(titleKey),
		option->value()));
	const auto save = [=] {
		option->set(input->getLastText());
		box->closeBox();
	};
	input->submits() | rpl::on_next([=] {
		save();
	}, input->lifetime());
	box->setFocusCallback([=] {
		input->setFocusFast();
	});
	box->addButton(Text("button_save"), save);
	box->addButton(Text("button_cancel"), [=] {
		box->closeBox();
	});
}

// The MZGram language: System and the languages, each by its own name; a
// choice applies at once.
void LanguageBox(not_null<Ui::GenericBox*> box) {
	const auto option = &base::options::lookup<QString>(
		MZGram::kOptionLanguage);
	box->setTitle(Text("mzgram_language"));
	const auto system = QString::fromLatin1(MZGram::kLanguageSystem);
	auto selected = (MZGram::LanguageChoice() == system) ? 1 : 0;
	for (auto i = 0; i != MZGram::kLanguageCount; ++i) {
		if (MZGram::LanguageChoice() == QString::fromLatin1(MZGram::kLanguages[i])) {
			selected = i + 2;
		}
	}
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(selected);
	const auto add = [&](int value, const QString &label) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box,
			group,
			value,
			label,
			st::defaultBoxCheckbox));
	};
	add(0, MZGram::TrNow("mzgram_language_telegram"));
	add(1, MZGram::TrNow("mzgram_language_system"));
	for (auto i = 0; i != MZGram::kLanguageCount; ++i) {
		add(i + 2, QString::fromUtf8(MZGram::kLanguageNames[i]));
	}
	group->setChangedCallback([=](int value) {
		option->set((value > 1)
			? QString::fromLatin1(MZGram::kLanguages[value - 2])
			: (value == 1)
			? system
			: QString());
		box->closeBox();
	});
	box->addButton(Text("button_cancel"), [=] {
		box->closeBox();
	});
}

// The current choice, by its name.
[[nodiscard]] rpl::producer<QString> LanguageValue() {
	return rpl::combine(
		MZGram::Tr("mzgram_language_telegram"),
		MZGram::Tr("mzgram_language_system")
	) | rpl::map([](QString telegram, QString system) {
		const auto chosen = MZGram::LanguageChoice();
		for (auto i = 0; i != MZGram::kLanguageCount; ++i) {
			if (chosen == QString::fromLatin1(MZGram::kLanguages[i])) {
				return QString::fromUtf8(MZGram::kLanguageNames[i]);
			}
		}
		return (chosen == QString::fromLatin1(MZGram::kLanguageSystem))
			? system
			: telegram;
	});
}

// What Clear archive left: nothing, at once.
[[nodiscard]] rpl::event_stream<MZGram::ArchiveSizes> &ArchiveSizeResets() {
	static auto result = rpl::event_stream<MZGram::ArchiveSizes>();
	return result;
}

// The archive's size as Telegram's Storage Usage shows it: the total in
// large type, a bar split by kind in the colours of Telegram's palette and
// a legend with a dot and the size of each kind; "Counting..." until the
// count on a background thread ends (the media can take gigabytes).
[[nodiscard]] object_ptr<Ui::RpWidget> MakeArchiveSize(
		not_null<QWidget*> parent) {
	struct State {
		std::optional<MZGram::ArchiveSizes> sizes;
		int count = 0;
	};
	auto result = object_ptr<Ui::RpWidget>(parent);
	const auto raw = result.data();
	const auto state = raw->lifetime().make_state<State>();
	const auto big = style::font(
		style::ConvertScale(24),
		style::FontFlag::Bold,
		0);
	const auto skip = style::ConvertScale(8);
	const auto bar = style::ConvertScale(8);
	const auto row = style::ConvertScale(26);
	const auto dot = style::ConvertScale(10);
	raw->resize(
		raw->width(),
		skip + big->height + skip + bar + skip + 4 * row + skip);
	const auto colors = std::array<const style::color*, 4>{
		&st::historyPeer4UserpicBg,
		&st::historyPeer2UserpicBg,
		&st::historyPeer1UserpicBg,
		&st::historyPeer5UserpicBg,
	};
	const auto names = std::array<const char*, 4>{
		"archive_size_database",
		"archive_size_media",
		"archive_size_attachments",
		"archive_size_other",
	};
	raw->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(raw);
		p.setRenderHint(QPainter::Antialiasing);
		const auto left = st::boxRowPadding.left();
		const auto width = std::max(
			raw->width() - left - st::boxRowPadding.right(),
			1);
		const auto &sizes = state->sizes;
		const auto parts = sizes
			? std::array<qint64, 4>{
				sizes->database,
				sizes->media,
				sizes->attachments,
				sizes->other }
			: std::array<qint64, 4>{};
		auto y = skip;
		p.setFont(big);
		p.setPen(st::windowFg);
		p.drawText(left, y + big->ascent, sizes
			? Ui::FormatSizeText(sizes->total())
			: MZGram::TrNow("archive_size_counting"));
		y += big->height + skip;

		const auto line = QRect(left, y, width, bar);
		auto path = QPainterPath();
		path.addRoundedRect(line, bar / 2., bar / 2.);
		p.fillPath(path, st::windowBgOver);
		if (sizes && sizes->total() > 0) {
			p.setClipPath(path);
			auto x = double(left);
			for (auto i = 0; i != 4; ++i) {
				if (!parts[i]) {
					continue;
				}
				const auto part = std::max(
					width * double(parts[i]) / sizes->total(),
					double(style::ConvertScale(3)));
				p.fillRect(QRectF(x, y, part, bar), (*colors[i])->c);
				x += part;
			}
			p.setClipping(false);
		}
		y += bar + skip;

		p.setFont(st::normalFont);
		const auto text = (row - st::normalFont->height) / 2
			+ st::normalFont->ascent;
		for (auto i = 0; i != 4; ++i) {
			p.setPen(Qt::NoPen);
			p.setBrush((*colors[i])->c);
			p.drawEllipse(left, y + (row - dot) / 2, dot, dot);
			p.setPen(st::windowFg);
			p.drawText(left + dot + skip, y + text, MZGram::TrNow(names[i]));
			if (sizes) {
				const auto size = Ui::FormatSizeText(parts[i]);
				p.setPen(st::windowSubTextFg);
				p.drawText(
					left + width - st::normalFont->width(size),
					y + text,
					size);
			}
			y += row;
		}
	}, raw->lifetime());

	const auto apply = crl::guard(raw, [=](
			int number,
			MZGram::ArchiveSizes sizes) {
		if (state->count == number) {
			state->sizes = sizes;
			raw->update();
		}
	});
	const auto &store = MZGram::MessageStore::Instance();
	const auto database = store.databasePath();
	const auto folder = cWorkingDir() + u"tdata/mzgram"_q;
	const auto media = folder + u"/media"_q;
	const auto attachments = store.attachmentsFolder();
	const auto number = ++state->count;
	crl::async([=] {
		const auto sizes = MZGram::MeasureArchive(
			database,
			folder,
			media,
			attachments);
		crl::on_main([=]() mutable {
			apply(number, sizes);
		});
	});
	ArchiveSizeResets().events(
	) | rpl::on_next([=](MZGram::ArchiveSizes sizes) {
		++state->count;
		state->sizes = sizes;
		raw->update();
	}, raw->lifetime());
	return result;
}

// A limit of Download in advance, as it reads: the size, or No limit.
[[nodiscard]] QString LimitText(int mb) {
	return (mb < 0)
		? MZGram::TrNow("archive_limit_none")
		: Ui::FormatSizeText(int64(mb) * 1024 * 1024);
}

[[nodiscard]] rpl::producer<QString> LimitValue(MZGram::ArchiveSource source) {
	const auto option = &base::options::lookup<int>(
		MZGram::LimitOption(source));
	return rpl::combine(
		MZGram::Tr("archive_limit_none"),
		rpl::single(rpl::empty) | rpl::then(option->changes())
	) | rpl::map([=](const QString &, auto) {
		return LimitText(MZGram::LimitMb(source));
	});
}

// The largest file downloaded in advance for one kind of chat.
void LimitBox(
		not_null<Ui::GenericBox*> box,
		MZGram::ArchiveSource source,
		const char *titleKey) {
	box->setTitle(Text(titleKey));
	const auto choices = MZGram::LimitChoices();
	auto selected = 0;
	for (auto i = 0; i != int(choices.size()); ++i) {
		if (choices[i] == MZGram::LimitMb(source)) {
			selected = i;
		}
	}
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(selected);
	for (auto i = 0; i != int(choices.size()); ++i) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box,
			group,
			i,
			LimitText(choices[i]),
			st::defaultBoxCheckbox));
	}
	group->setChangedCallback([=](int value) {
		MZGram::SetLimitMb(source, choices[value]);
		box->closeBox();
	});
	box->addButton(Text("button_cancel"), [=] {
		box->closeBox();
	});
}

// The counters of the files the archive downloaded, as they change.
[[nodiscard]] object_ptr<Ui::RpWidget> MakeArchiveCounters(
		not_null<QWidget*> parent) {
	using namespace MZGram;
	auto changes = rpl::producer<>(rpl::single(rpl::empty));
	for (auto i = 0; i != kArchiveCounterCount; ++i) {
		changes = rpl::merge(
			std::move(changes),
			base::options::lookup<int>(
				CounterOption(ArchiveCounter(i))).changes());
	}
	auto text = rpl::combine(
		Tr("archive_counter_saved"),
		Tr("archive_counter_failed"),
		Tr("archive_counter_expired"),
		std::move(changes)
	) | rpl::map([](
			const QString &saved,
			const QString &failed,
			const QString &expired,
			auto) {
		return saved.arg(
			CounterValue(ArchiveCounter::SavedAhead)
		).arg(
			CounterValue(ArchiveCounter::SavedOnDeletion)
		) + '\n' + failed.arg(
			CounterValue(ArchiveCounter::Failed)
		) + '\n' + expired.arg(
			CounterValue(ArchiveCounter::Expired));
	});
	return object_ptr<Ui::FlatLabel>(
		parent,
		std::move(text),
		st::boxDividerLabel);
}

void BuildArchive(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSubsectionTitle(Text("archive_size"));
	builder.addControl({
		.factory = [](not_null<Ui::VerticalLayout*> parent) {
			return MakeArchiveSize(parent);
		},
		.id = u"mzgram/archive-size"_q,
		.title = Text("archive_size"),
		.keywords = { u"size"_q, u"storage"_q, u"space"_q },
	});
	builder.addSkip();
	builder.addDivider();
	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/save-deleted-messages"_q,
		"save_deleted_messages",
		kOptionSaveDeletedMessages,
		{ u"anti-recall"_q, u"deleted"_q, u"view once"_q });
	AddOptionToggle(
		builder,
		u"mzgram/save-edit-history"_q,
		"save_edit_history",
		kOptionSaveEditHistory,
		{ u"edited"_q, u"history"_q });
	builder.addSkip();
	builder.addDividerText(Text("archive_info"));

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/save-archive-media"_q,
		"save_archive_media",
		kOptionSaveArchiveMedia,
		{ u"media"_q, u"files"_q, u"attachments"_q });
	AddOptionToggle(
		builder,
		u"mzgram/save-formatting"_q,
		"save_formatting",
		kOptionSaveFormatting,
		{ u"formatting"_q });
	AddOptionToggle(
		builder,
		u"mzgram/save-reactions"_q,
		"save_reactions",
		kOptionSaveReactions,
		{ u"reactions"_q });
	builder.addSkip();
	builder.addDividerText(Text("saved_media_note"));

	// Download in advance: the largest file for each kind of chat; when a
	// message is deleted a file up to 100 MB larger is downloaded then
	builder.addSkip();
	builder.addSubsectionTitle(Text("archive_limits"));
	{
		const auto controller = builder.controller();
		const auto addLimit = [&](
				ArchiveSource source,
				const char *titleKey,
				const QString &id) {
			builder.addButton({
				.id = id,
				.title = Text(titleKey),
				.st = &st::settingsButtonNoIcon,
				.label = LimitValue(source),
				.onClick = [=] {
					controller->show(Box(LimitBox, source, titleKey));
				},
				.keywords = { u"download"_q, u"limit"_q, u"size"_q },
			});
		};
		addLimit(
			ArchiveSource::User,
			"archive_limit_users",
			u"mzgram/archive-limit-users"_q);
		addLimit(
			ArchiveSource::Bot,
			"archive_limit_bots",
			u"mzgram/archive-limit-bots"_q);
		addLimit(
			ArchiveSource::Group,
			"archive_limit_groups",
			u"mzgram/archive-limit-groups"_q);
		addLimit(
			ArchiveSource::Channel,
			"archive_limit_channels",
			u"mzgram/archive-limit-channels"_q);
	}
	builder.addSkip();
	builder.addDividerText(Text("archive_limits_info"));

	// What happened to the downloads, to choose the limits on real data
	builder.addSkip();
	builder.addSubsectionTitle(Text("archive_counters"));
	builder.addControl({
		.factory = [](not_null<Ui::VerticalLayout*> parent) {
			return MakeArchiveCounters(parent);
		},
		.id = u"mzgram/archive-counters"_q,
		.title = Text("archive_counters"),
		.margin = st::boxRowPadding,
		.keywords = { u"saved"_q, u"failed"_q, u"expired"_q },
	});
	builder.addButton({
		.id = u"mzgram/archive-counters-reset"_q,
		.title = Text("archive_counters_reset"),
		.st = &st::settingsButtonNoIcon,
		.onClick = [] {
			ResetCounters();
		},
		.keywords = { u"reset"_q, u"counters"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("archive_counters_info"));

	// Whose messages are kept; turning one off keeps what was saved
	builder.addSkip();
	builder.addSubsectionTitle(Text("keep_from_title"));
	AddOptionToggle(
		builder,
		u"mzgram/save-from-users"_q,
		"keep_from_users",
		kOptionSaveFromUsers,
		{ u"users"_q, u"people"_q, u"private"_q });
	AddOptionToggle(
		builder,
		u"mzgram/save-for-bots"_q,
		"keep_from_bots",
		kOptionSaveForBots,
		{ u"bots"_q });
	AddOptionToggle(
		builder,
		u"mzgram/save-in-groups"_q,
		"keep_from_groups",
		kOptionSaveInGroups,
		{ u"groups"_q });
	AddOptionToggle(
		builder,
		u"mzgram/save-in-channels"_q,
		"keep_from_channels",
		kOptionSaveInChannels,
		{ u"channels"_q });
	builder.addSkip();
	builder.addDividerText(Text("keep_from_info"));

	builder.addSkip();
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"mzgram/deleted-mark"_q,
		.title = Text("deleted_mark_text"),
		.st = &st::settingsButtonNoIcon,
		.label = MarkValue(kOptionDeletedMark),
		.onClick = [=] {
			controller->show(Box(MarkBox, "deleted_mark_text", kOptionDeletedMark));
		},
		.keywords = { u"deleted"_q, u"mark"_q },
	});
	builder.addButton({
		.id = u"mzgram/edited-mark"_q,
		.title = Text("edited_mark_text"),
		.st = &st::settingsButtonNoIcon,
		.label = MarkValue(kOptionEditedMark),
		.onClick = [=] {
			controller->show(Box(MarkBox, "edited_mark_text", kOptionEditedMark));
		},
		.keywords = { u"edited"_q, u"mark"_q, u"pencil"_q },
	});
	AddOptionToggle(
		builder,
		u"mzgram/semi-transparent-deleted"_q,
		"semi_transparent_deleted",
		kOptionSemiTransparentDeleted,
		{ u"dim"_q, u"deleted"_q, u"transparent"_q });
	builder.addSkip();
	builder.addDividerText(Text("archive_look_info"));

	builder.addSkip();
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
			// The box closes first, then the file dialog opens
			const auto importArchive = crl::guard(controller, [=] {
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
			});
			controller->show(Ui::MakeConfirmBox({
				.text = MZGram::TrNow("import_archive_confirm"),
				.confirmed = [=](Fn<void()> close) {
					close();
					importArchive();
				},
				.confirmText = MZGram::TrNow("import"),
				.cancelText = Text("button_cancel"),
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
				.confirmed = [=](Fn<void()> close) {
					close();
					// The rows go at once, the files on a background
					// thread: gigabytes take minutes to delete
					const auto aside = MessageStore::Instance().wipeAll();
					ArchiveSizeResets().fire({});
					Ui::Toast::Show(MZGram::TrNow("archive_cleared"));
					if (aside.isEmpty()) {
						return;
					}
					crl::async([=] {
						const auto removed = MZGram::RemoveWiped(aside);
						crl::on_main([=] {
							Ui::Toast::Show(removed.complete
								? MZGram::TrNow("archive_files_deleted").arg(
									Ui::FormatSizeText(removed.bytes))
								: MZGram::TrNow("archive_files_left"));
						});
					});
				},
				.confirmText = Text("button_delete"),
				.cancelText = Text("button_cancel"),
				.confirmStyle = &st::attentionBoxButton,
			}));
		},
		.keywords = { u"clear"_q, u"wipe"_q, u"delete"_q, u"archive"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("clear_archive_info"));

	// Telegram's local cache on this device. Telegram Desktop keeps no
	// messages or chat list on disk; they load from the server on every
	// start. So this clears the files cache (the same as "Clear all" in
	// Manage local storage) and restarts, so everything loads again. The
	// account's own files (session keys, settings, drafts) and the MZGram
	// archive are not touched.
	builder.addSkip();
	builder.addButton({
		.id = u"mzgram/erase-local-database"_q,
		.title = Text("erase_local_database"),
		.st = &st::settingsAttentionButton,
		.onClick = [=] {
			const auto session = &controller->session();
			controller->show(Ui::MakeConfirmBox({
				.text = MZGram::TrNow("erase_local_database_confirm"),
				.confirmed = [=](Fn<void()> close) {
					close();
					session->data().cache().clear();
					session->data().cacheBigFile().clear();
					Core::Restart();
				},
				.confirmText = MZGram::Tr("erase_local_database_button"),
				.cancelText = Text("button_cancel"),
				.confirmStyle = &st::attentionBoxButton,
			}));
		},
		.keywords = { u"cache"_q, u"local"_q, u"database"_q, u"clear"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("erase_local_database_info"));
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
		u"mzgram/copy-photo"_q,
		"show_copy_photo",
		kOptionCopyPhoto,
		{ u"copy"_q, u"photo"_q, u"image"_q, u"clipboard"_q });
	AddOptionToggle(
		builder,
		u"mzgram/delete-downloaded-file"_q,
		"show_delete_downloaded",
		kOptionDeleteDownloadedFile,
		{ u"delete"_q, u"file"_q, u"download"_q, u"disk"_q });
	AddOptionToggle(
		builder,
		u"mzgram/forward-no-quote"_q,
		"show_forward_no_quote",
		kOptionForwardNoQuote,
		{ u"forward"_q, u"sender"_q, u"quote"_q, u"name"_q });
	builder.addSkip();
	builder.addDividerText(Text("message_menu_more_info"));
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

// Show ID: hidden, as in Telegram API or as in Bot API; applies at once.
void PeerIdBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(Text("peer_id"));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		int(MZGram::PeerIdShown()));
	const auto add = [&](MZGram::PeerIdMode mode, const char *key) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box,
			group,
			int(mode),
			MZGram::TrNow(key),
			st::defaultBoxCheckbox));
	};
	add(MZGram::PeerIdMode::Hidden, "peer_id_hidden");
	add(MZGram::PeerIdMode::TelegramApi, "peer_id_telegram");
	add(MZGram::PeerIdMode::BotApi, "peer_id_bot");
	group->setChangedCallback([=](int value) {
		MZGram::SetPeerIdShown(MZGram::PeerIdMode(value));
		box->closeBox();
	});
	box->addButton(Text("button_cancel"), [=] {
		box->closeBox();
	});
}

// The current choice, by its name.
[[nodiscard]] rpl::producer<QString> PeerIdValue() {
	const auto option = &base::options::lookup<int>(MZGram::kOptionPeerId);
	return rpl::combine(
		MZGram::Tr("peer_id_hidden"),
		MZGram::Tr("peer_id_telegram"),
		MZGram::Tr("peer_id_bot"),
		rpl::single(rpl::empty) | rpl::then(option->changes())
	) | rpl::map([](QString hidden, QString telegram, QString bot, auto) {
		switch (MZGram::PeerIdShown()) {
		case MZGram::PeerIdMode::TelegramApi: return telegram;
		case MZGram::PeerIdMode::BotApi: return bot;
		case MZGram::PeerIdMode::Hidden: break;
		}
		return hidden;
	});
}

void BuildInterface(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSkip();
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"mzgram/language"_q,
		.title = Text("mzgram_language"),
		.st = &st::settingsButtonNoIcon,
		.label = LanguageValue(),
		.onClick = [=] {
			controller->show(Box(LanguageBox));
		},
		.keywords = { u"language"_q, u"translation"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("mzgram_language_info"));

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

	builder.addSkip();
	builder.addButton({
		.id = u"mzgram/peer-id"_q,
		.title = Text("peer_id"),
		.st = &st::settingsButtonNoIcon,
		.label = PeerIdValue(),
		.onClick = [=] {
			controller->show(Box(PeerIdBox));
		},
		.keywords = { u"id"_q, u"peer"_q, u"bot api"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("peer_id_info"));

	// Rows of Telegram's own Settings page, each hidden by its own switch
	builder.addSkip();
	builder.addSubsectionTitle(Text("hide_rows_title"));
	for (const auto &row : HideableRows()) {
		AddOptionToggle(
			builder,
			u"mzgram/hide-row-"_q + QString::fromLatin1(row.key),
			row.phrase,
			row.option,
			{ u"hide"_q, u"settings"_q, QString::fromLatin1(row.key) });
	}
	builder.addSkip();
	builder.addDividerText(Text("hide_rows_info"));
}

// The default site of "Ask AI": a choice applies at once.
void AiProviderBox(not_null<Ui::GenericBox*> box) {
	using namespace MZGram::Ai;
	box->setTitle(Text("ai_default_provider"));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		int(DefaultProvider()));
	for (const auto &info : Providers()) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box,
			group,
			int(info.id),
			QString::fromLatin1(info.name),
			st::defaultBoxCheckbox));
	}
	group->setChangedCallback([=](int value) {
		SetDefaultProvider(Provider(value));
		box->closeBox();
	});
	box->addButton(Text("button_cancel"), [=] {
		box->closeBox();
	});
}

// The default site, by its name.
[[nodiscard]] rpl::producer<QString> AiProviderValue() {
	using namespace MZGram::Ai;
	const auto option = &base::options::lookup<QString>(kOptionAiProvider);
	return rpl::single(
		rpl::empty
	) | rpl::then(
		option->changes()
	) | rpl::map([] {
		return QString::fromLatin1(
			MZGram::Ai::Info(MZGram::Ai::DefaultProvider()).name);
	});
}

void BuildAi(SectionBuilder &builder) {
	using namespace MZGram::Ai;

	builder.addSkip();
	const auto controller = builder.controller();
	AddOptionToggle(
		builder,
		u"mzgram/ai-action"_q,
		"ai_show_action",
		kOptionAiAction,
		{ u"ai"_q, u"gemini"_q, u"claude"_q, u"chatgpt"_q, u"deepseek"_q, u"grok"_q });
	builder.addButton({
		.id = u"mzgram/ai-provider"_q,
		.title = Text("ai_default_provider"),
		.st = &st::settingsButtonNoIcon,
		.label = AiProviderValue(),
		.onClick = [=] {
			controller->show(Box(AiProviderBox));
		},
		.keywords = { u"ai"_q, u"default"_q },
	});
	builder.addButton({
		.id = u"mzgram/ai-sign-out"_q,
		.title = Text("ai_sign_out"),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = MZGram::TrNow("ai_sign_out_confirm"),
				.confirmed = [](Fn<void()> close) {
					ClearSignIns();
					close();
					Ui::Toast::Show(MZGram::TrNow("ai_signed_out"));
				},
				.confirmText = Text("ai_sign_out"),
				.cancelText = Text("button_cancel"),
				.confirmStyle = &st::attentionBoxButton,
			}));
		},
		.keywords = { u"ai"_q, u"sign out"_q, u"log out"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("ai_info"));

	// A button for each site in the main menu, to open it with no message
	builder.addSkip();
	builder.addSubsectionTitle(Text("ai_menu_buttons"));
	for (const auto &info : Providers()) {
		const auto option = MenuButtonOption(info.id).toLatin1();
		AddNamedOptionToggle(
			builder,
			u"mzgram/ai-menu-"_q + QString::fromLatin1(info.key),
			QString::fromLatin1(info.name),
			option.constData(),
			{ u"ai"_q, u"menu"_q, QString::fromLatin1(info.key) });
	}
	builder.addSkip();
	builder.addDividerText(Text("ai_menu_buttons_info"));
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

void BuildOther(SectionBuilder &builder) {
	using namespace MZGram;

	builder.addSkip();
	AddOptionToggle(
		builder,
		u"mzgram/check-updates"_q,
		"check_updates",
		kOptionCheckUpdates,
		{ u"update"_q, u"version"_q, u"release"_q, u"github"_q });
	builder.addButton({
		.id = u"mzgram/check-updates-now"_q,
		.title = Text("check_updates_now"),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			MZGram::Update::CheckNow([](MZGram::Update::ManualOutcome outcome) {
				using Result = MZGram::Update::ManualResult;
				Ui::Toast::Show((outcome.result == Result::Available)
					? MZGram::TrNow("update_check_available").arg(
						outcome.release->version())
					: (outcome.result == Result::Latest)
					? MZGram::TrNow("update_check_latest")
					: MZGram::TrNow("update_check_failed"));
			});
		},
		.keywords = { u"update"_q, u"check"_q, u"version"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("check_updates_info"));
#ifdef Q_OS_WIN
	// Only when pressed: the mark Windows asks about goes from MZGram's own
	// files; Windows protection is never changed
	builder.addSkip();
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"mzgram/unblock-files"_q,
		.title = Text("unblock_files"),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = MZGram::TrNow("unblock_files_confirm"),
				.confirmed = [](Fn<void()> close) {
					const auto folder = cExeDir();
					const auto count = MZGram::UnblockFolder(folder);
					close();
					Ui::Toast::Show(MZGram::MarkedFiles(folder)
						? MZGram::TrNow("unblock_files_denied")
						: MZGram::TrNow("unblock_files_done").arg(count));
				},
				.confirmText = Text("unblock_files"),
				.cancelText = Text("button_cancel"),
				.title = Text("unblock_files"),
			}));
		},
		.keywords = { u"unblock"_q, u"smartscreen"_q, u"windows"_q },
	});
	builder.addSkip();
	builder.addDividerText(Text("unblock_files_info"));
#endif // Q_OS_WIN
}

struct Topic {
	const char *title = nullptr;
	const style::icon *icon = nullptr;
	void (*build)(SectionBuilder &builder) = nullptr;
};

// The topics, in the order the MZGram page lists them.
[[nodiscard]] const std::array<Topic, 8> &Topics() {
	static const auto result = std::array<Topic, 8>{ {
		{ "section_archive", &st::menuIconArchive, BuildArchive },
		{ "section_ghost_mode", &st::menuIconStealth, BuildGhostMode },
		{ "section_message_menu", &st::menuIconChatBubble, BuildMessageMenu },
		{ "section_media_calls", &st::menuIconPhone, BuildMediaCalls },
		{ "section_interface", &st::menuIconPalette, BuildInterface },
		{ "section_ads_filters", &st::menuIconBlock, BuildAdsFilters },
		{ "section_ai", &st::menuIconBot, BuildAi },
		{ "section_other", &st::menuIconInfo, BuildOther },
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
	case 7: return MZGramTopicSection<7>::Id();
	}
	Unexpected("Index in MZGram TopicId.");
}

} // namespace

Type MZGramId() {
	return MZGramSection::Id();
}

} // namespace Settings
