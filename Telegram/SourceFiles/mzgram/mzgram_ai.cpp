/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_ai.h"

#include "base/call_delayed.h"
#include "base/debug_log.h"
#include "base/options.h"
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "core/mime_type.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_groups.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "mzgram/mzgram_lang.h"
#include "settings.h"
#include "settings/settings_common.h"
#include "ui/boxes/confirm_box.h"
#include "ui/image/image.h"
#include "ui/layers/generic_box.h"
#include "ui/text/format_values.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/separate_panel.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "webview/webview_data_stream_memory.h"
#include "webview/webview_embed.h"
#include "webview/webview_interface.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"
#include "styles/style_window.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtGui/QPainterPath>
#include <QtCore/QStandardPaths>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>

namespace MZGram::Ai {
namespace {

// The message as the box shows it; all of it goes to the site.
constexpr auto kPreviewLength = 600;
// A site that never showed a field, as when it asks to sign in first.
constexpr auto kInsertTimeout = crl::time(35000);
// Attachments that did not download by then are left out.
constexpr auto kLoadTimeout = crl::time(120000);

base::options::toggle OptionAiAction({
	.id = kOptionAiAction,
	.name = "Show \"Ask AI\" in the message menu",
	.description = "Open a message in the chat of an AI site, in MZGram's "
		"own window, to send it there yourself",
	.defaultValue = true,
});

base::options::toggle OptionAiWarned({
	.id = kOptionAiWarned,
	.name = "MZGram: the Ask AI warning was accepted",
	.description = "The text and attachments of \"Ask AI\" leave Telegram "
		"for the AI site",
	.defaultValue = false,
});

[[nodiscard]] QString Name(Provider provider) {
	return QString::fromLatin1(Info(provider).name);
}

[[nodiscard]] QString ProfilesFolder() {
	return cWorkingDir() + u"tdata/"_q + QString::fromLatin1(kStorageFolder);
}

// Where attachments not yet on this computer are downloaded to.
[[nodiscard]] QString FilesFolder() {
	return cWorkingDir() + u"tdata/mzgram-ai-files"_q;
}

[[nodiscard]] QByteArray StorageToken() {
	const auto option = &base::options::lookup<QString>(kOptionAiStorageToken);
	auto token = option->value().toLatin1();
	if (token.isEmpty()) {
		token = QByteArray::fromStdString(
			Webview::GenerateStorageToken()).toHex();
		option->set(QString::fromLatin1(token));
	}
	return token;
}

// The AI sites' own profile, apart from Telegram's (Mini Apps, payments,
// TON sites). On Windows and Linux Webview keeps a profile in its folder,
// and a token clears nothing there: each sign-out starts a new folder.
[[nodiscard]] Webview::StorageId StorageId() {
	const auto token = StorageToken();
	return {
		.path = ProfilesFolder() + u"/"_q + QString::fromLatin1(token),
		.token = token,
	};
}

// The folders of profiles signed out of; one the browser still holds
// open goes the next time.
void RemoveOldProfiles() {
	const auto current = QString::fromLatin1(StorageToken());
	auto folder = QDir(ProfilesFolder());
	for (const auto &name : folder.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (name != current) {
			QDir(folder.filePath(name)).removeRecursively();
		}
	}
}

// A photo or a file of the chosen messages.
struct Attachment {
	QString name;
	QString mime;
	FullMsgId origin;
	PhotoData *photo = nullptr;
	DocumentData *document = nullptr;
	QByteArray bytes; // Once downloaded; the test's files from the start.
};

// What goes to the site: the text, the attachments and what was left out.
struct Collected {
	QString text;
	std::vector<Attachment> attachments;
	AttachmentChoice choice;
	base::weak_ptr<Main::Session> session;
};

[[nodiscard]] bool IsMedia(not_null<DocumentData*> document) {
	return document->sticker()
		|| document->isAnimation()
		|| document->isVideoFile()
		|| document->isVoiceMessage()
		|| document->isVideoMessage()
		|| document->isSong()
		|| document->isAudioFile();
}

[[nodiscard]] Collected Collect(
		const std::vector<not_null<HistoryItem*>> &items,
		QString text) {
	auto result = Collected{ .text = text.trimmed() };
	auto candidates = std::vector<AttachmentCandidate>();
	auto all = std::vector<Attachment>();
	auto photos = 0;
	auto files = 0;
	for (const auto &item : items) {
		result.session = &item->history()->session();
		const auto media = item->media();
		if (!media) {
			continue;
		}
		if (const auto photo = media->photo()) {
			if (photo->isNull()) {
				continue;
			}
			candidates.push_back({ .kind = AttachmentKind::Photo });
			all.push_back({
				.name = u"photo-%1.jpg"_q.arg(++photos),
				.mime = u"image/jpeg"_q,
				.origin = item->fullId(),
				.photo = photo,
			});
		} else if (const auto document = media->document()) {
			const auto name = document->filename();
			const auto mime = document->mimeString();
			candidates.push_back({
				.kind = (IsMedia(document)
					? AttachmentKind::Media
					: ClassifyFile(name, mime)),
				.size = document->size,
			});
			all.push_back({
				.name = name.isEmpty() ? u"file-%1"_q.arg(++files) : name,
				.mime = mime.isEmpty() ? u"application/octet-stream"_q : mime,
				.origin = item->fullId(),
				.document = document,
			});
		}
	}
	result.choice = ChooseAttachments(candidates);
	for (const auto index : result.choice.chosen) {
		result.attachments.push_back(all[index]);
	}
	return result;
}

// What the box says was left out, empty when nothing was.
[[nodiscard]] QString LeftOutText(const AttachmentChoice &choice) {
	auto parts = QStringList();
	const auto add = [&](const char *key, int count) {
		if (count) {
			parts.push_back(TrNow(key).arg(count));
		}
	};
	add("ai_left_over_limit", choice.overLimit);
	add("ai_left_too_large", choice.tooLarge);
	add("ai_left_archives", choice.archives);
	add("ai_left_media", choice.media);
	add("ai_left_other", choice.unsupported);
	return parts.isEmpty()
		? QString()
		: TrNow("ai_left_out").arg(parts.join(u"; "_q));
}

// The large photo as the site gets it: its own bytes, else a JPEG.
[[nodiscard]] QByteArray PhotoBytes(
		const std::shared_ptr<Data::PhotoMedia> &view) {
	auto result = view->imageBytes(Data::PhotoSize::Large);
	if (!result.isEmpty()) {
		return result;
	}
	if (const auto image = view->image(Data::PhotoSize::Large)) {
		auto buffer = QBuffer(&result);
		buffer.open(QIODevice::WriteOnly);
		image->original().save(&buffer, "JPG", 90);
	}
	return result;
}

[[nodiscard]] QString SafeFileName(QString name) {
	static const auto bad = QRegularExpression(u"[\\\\/:*?\"<>|]"_q);
	return name.replace(bad, u"_"_q);
}

// The site in a window of its own: a bar with what happened and the
// Insert text, Copy text and Save files buttons above the page.
class AiWindow final {
public:
	AiWindow(Provider provider, Collected request, QByteArray localPage);
	~AiWindow();

	[[nodiscard]] bool created() const;
	void show(Provider provider, Collected request);
	void eval(const QByteArray &script);
	// Every message the page posts, after the window handled it.
	void setObserver(Fn<void(QByteArray)> observer);

	// The tests: the text is in and the files are ready, the status, the
	// chooser's title, and its Add pressed.
	[[nodiscard]] bool filesReadyForTest() const;
	[[nodiscard]] QString statusForTest() const;
	[[nodiscard]] QString filesTitleForTest() const;
	bool addFilesForTest();
	// The layout, for the test: the bar's height, where the page starts,
	// whether the status stays left of the buttons, and the chooser's
	// "another file" as the last row of its list.
	[[nodiscard]] int barHeightForTest() const;
	[[nodiscard]] int pageTopForTest() const;
	[[nodiscard]] bool statusFitsForTest() const;
	[[nodiscard]] bool otherRowLastForTest() const;

	[[nodiscard]] rpl::producer<> closeRequests() const;
	[[nodiscard]] rpl::lifetime &lifetime() {
		return _lifetime;
	}

private:
	void setupBar();
	void setupWebview();
	void relayout();
	void navigate();
	void insert();
	void setStatus(const QString &text);
	void handleMessage(const QByteArray &message);
	bool allowNavigation(const QString &url, bool newWindow);
	void startLoading();
	void checkLoading();
	void filesStatus();
	[[nodiscard]] int readyCount() const;
	void chooseFiles(Pick pick);
	void filesBox(
		not_null<Ui::GenericBox*> box,
		std::vector<int> rows,
		Pick pick);
	[[nodiscard]] QImage thumbOf(int index) const;
	void chooseOther(Pick pick);
	void giveFiles(const std::vector<Attachment> &files);
	void saveFiles();
	[[nodiscard]] bool loadingDone() const;

	Provider _provider;
	Collected _request;
	// The CI test's page in place of the site, see StartTestHook.
	const QByteArray _localPage;
	std::unique_ptr<Ui::SeparatePanel> _panel;
	Ui::RpWidget *_body = nullptr;
	Ui::FlatLabel *_status = nullptr;
	Ui::RoundButton *_insert = nullptr;
	Ui::RoundButton *_copy = nullptr;
	Ui::RoundButton *_save = nullptr;
	Ui::RpWidget *_container = nullptr;
	std::unique_ptr<Webview::Window> _webview;
	rpl::variable<QString> _statusText;
	base::Timer _waiting;
	base::Timer _loading;
	crl::time _loadingStarted = 0;
	std::vector<std::shared_ptr<Data::PhotoMedia>> _photoViews;
	std::vector<std::shared_ptr<Data::DocumentMedia>> _documentViews;
	std::vector<bool> _failed;
	bool _insertOnLoad = false;
	bool _textInserted = false;
	// The files not given to the site yet, indices into the attachments.
	std::vector<int> _remaining;
	int _given = 0;
	QPointer<Ui::BoxContent> _filesBox;
	QPointer<Ui::RpWidget> _otherRow;
	QString _filesTitle;
	Fn<void()> _addFiles;
	Fn<void(QByteArray)> _observer;
	rpl::event_stream<> _closeRequests;
	rpl::lifetime _lifetime;

};

AiWindow *Opened = nullptr;

AiWindow::AiWindow(Provider provider, Collected request, QByteArray localPage)
: _provider(provider)
, _request(std::move(request))
, _localPage(std::move(localPage))
, _panel(std::make_unique<Ui::SeparatePanel>())
, _waiting([=] {
	setStatus(TrNow("ai_status_no_field"));
})
, _loading([=] {
	checkLoading();
}) {
	_panel->setWindowFlag(Qt::WindowStaysOnTopHint, false);
	_panel->setInnerSize(
		QSize(style::ConvertScale(960), style::ConvertScale(720)),
		true);
	_panel->showInner(base::make_unique_q<Ui::RpWidget>(_panel.get()));
	_body = _panel->inner();
	_panel->closeRequests() | rpl::on_next([=] {
		_closeRequests.fire({});
	}, _panel->lifetime());

	setupBar();
	setupWebview();
	_panel->show();
	show(_provider, _request);
}

AiWindow::~AiWindow() {
	// The webview goes before the panel: destroying the widget tree first
	// destroys the window WebView2 is still attached to.
	_webview = nullptr;
	_panel = nullptr;
}

void AiWindow::setupBar() {
	// st::boxLabel is at least 256px wide: in a narrow bar it went under
	// the buttons, so the status has no minimum width and wraps instead
	const auto statusSt = _body->lifetime().make_state<style::FlatLabel>(
		st::boxLabel);
	statusSt->minWidth = 0;
	_status = Ui::CreateChild<Ui::FlatLabel>(
		_body,
		_statusText.value(),
		*statusSt);
	_insert = Ui::CreateChild<Ui::RoundButton>(
		_body,
		Tr("ai_insert"),
		st::defaultBoxButton);
	_copy = Ui::CreateChild<Ui::RoundButton>(
		_body,
		Tr("ai_copy"),
		st::defaultBoxButton);
	_save = Ui::CreateChild<Ui::RoundButton>(
		_body,
		Tr("ai_save_files"),
		st::defaultBoxButton);
	_container = Ui::CreateChild<Ui::RpWidget>(_body);
	_insert->setClickedCallback([=] {
		insert();
	});
	_copy->setClickedCallback([=] {
		QGuiApplication::clipboard()->setText(_request.text);
		Ui::Toast::Show(TrNow("ai_copied"));
	});
	_save->setClickedCallback([=] {
		saveFiles();
	});
	_body->paintRequest() | rpl::on_next([=](QRect clip) {
		QPainter(_body).fillRect(clip, st::windowBg);
	}, _body->lifetime());
	rpl::combine(
		_body->sizeValue(),
		_insert->widthValue(),
		_copy->widthValue(),
		_save->widthValue(),
		_statusText.value()
	) | rpl::to_empty | rpl::on_next([=] {
		relayout();
	}, _body->lifetime());
}

void AiWindow::relayout() {
	const auto width = _body->width();
	const auto skip = st::boxRowPadding.left() / 2;
	auto right = skip;
	auto height = 0;
	auto anyButton = false;
	for (const auto button : { _save, _copy, _insert }) {
		if (button->isHidden()) {
			continue;
		}
		anyButton = true;
		button->moveToRight(right, skip, width);
		right += button->width() + skip;
		height = std::max(height, button->height());
	}
	const auto hasStatus = !_statusText.current().isEmpty();
	_status->setVisible(hasStatus);
	_status->resizeToWidth(StatusWidth(width, right, skip));
	// The site alone: no bar, the page fills the window
	const auto bar = BarHeight(
		anyButton,
		hasStatus,
		std::max(height, hasStatus ? _status->height() : 0) + skip * 2,
		st::defaultBoxButton.height + skip * 2);
	_status->moveToLeft(skip * 2, (bar - _status->height()) / 2, width);
	_container->setGeometry(
		0,
		bar,
		width,
		std::max(_body->height() - bar, 0));
}

void AiWindow::setupWebview() {
	_webview = std::make_unique<Webview::Window>(
		_container,
		Webview::WindowConfig{
			.opaqueBg = st::windowBg->c,
			.storageId = StorageId(),
		});
	const auto raw = _webview.get();
	if (!raw->widget()) {
		_webview = nullptr;
		_insert->hide();
		setStatus(TrNow("ai_no_webview"));
		return;
	}
	_container->sizeValue() | rpl::on_next([=](QSize size) {
		if (const auto widget = raw->widget()) {
			widget->setGeometry(QRect(QPoint(), size));
		}
	}, _container->lifetime());
	raw->widget()->show();
	// The site's button for adding files opens MZGram's chooser
	raw->init(PickerHookScript());

	raw->setNavigationPolicyHandler([=](const QString &url, bool newWindow) {
		return allowNavigation(url, newWindow);
	});
	raw->setNavigationDoneHandler([=](bool success) {
		if (base::take(_insertOnLoad)) {
			insert();
		}
	});
	raw->setMessageHandler([=](std::string message) {
		crl::on_main(_panel.get(), [=] {
			handleMessage(QByteArray::fromStdString(message));
		});
	});
	if (!_localPage.isEmpty()) {
		raw->setDataRequestHandler([=](Webview::DataRequest request) {
			if (!request.id.starts_with("mzgram-ai/")) {
				return Webview::DataResult::Failed;
			}
			request.done({
				.stream = std::make_unique<Webview::DataStreamFromMemory>(
					_localPage,
					"text/html; charset=utf-8"),
			});
			return Webview::DataResult::Done;
		});
	}
}

bool AiWindow::allowNavigation(const QString &url, bool newWindow) {
	if (!_localPage.isEmpty()) {
		return !newWindow;
	} else if (!newWindow) {
		return AllowedNavigation(url);
	}
	switch (NewWindowTarget(_provider, url)) {
	case NewWindow::Here:
		// Sign-in pages open here, not in the browser
		crl::on_main(_panel.get(), [=] {
			if (_webview) {
				_webview->navigate(url);
			}
		});
		return false;
	case NewWindow::Browser:
		// Webview opens it in the browser
		return true;
	case NewWindow::Refuse:
		return false;
	}
	return false;
}

bool AiWindow::created() const {
	return (_webview != nullptr);
}

void AiWindow::show(Provider provider, Collected request) {
	_provider = provider;
	_request = std::move(request);
	_textInserted = false;
	_given = 0;
	_remaining.clear();
	for (auto i = 0; i != int(_request.attachments.size()); ++i) {
		_remaining.push_back(i);
	}
	if (_filesBox) {
		_filesBox->closeBox();
	}
	_waiting.cancel();
	_panel->setTitle(rpl::single(Name(_provider)));
	const auto hasText = !_request.text.isEmpty();
	const auto hasFiles = !_request.attachments.empty();
	_copy->setVisible(hasText);
	_save->setVisible(hasFiles);
	if (!_webview) {
		relayout();
		return;
	}
	// The site alone (the main menu's buttons): nothing to put in
	_insert->setVisible(hasText || hasFiles);
	relayout();
	setStatus((hasText || hasFiles)
		? TrNow("ai_status_opening").arg(Name(_provider))
		: QString());
	startLoading();
	navigate();
	_panel->showAndActivate();
}

void AiWindow::navigate() {
	_insertOnLoad = !_request.text.isEmpty() || !_request.attachments.empty();
	if (!_localPage.isEmpty()) {
		_webview->navigateToData(
			u"mzgram-ai/"_q + QString::fromLatin1(Info(_provider).key) + u".html"_q);
	} else {
		_webview->navigate(QString::fromLatin1(Info(_provider).startUrl));
	}
}

void AiWindow::insert() {
	if (!_webview) {
		return;
	}
	_webview->eval(InsertScript(_provider, _request.text));
	_waiting.callOnce(kInsertTimeout);
}

void AiWindow::eval(const QByteArray &script) {
	if (_webview) {
		_webview->eval(script);
	}
}

void AiWindow::startLoading() {
	const auto count = int(_request.attachments.size());
	_photoViews.assign(count, nullptr);
	_documentViews.assign(count, nullptr);
	_failed.assign(count, false);
	_loading.cancel();
	if (!count) {
		return;
	}
	QDir().mkpath(FilesFolder());
	for (auto i = 0; i != count; ++i) {
		auto &attachment = _request.attachments[i];
		if (!attachment.bytes.isEmpty() || !_request.session) {
			continue;
		}
		if (const auto photo = attachment.photo) {
			const auto view = photo->createMediaView();
			view->wanted(Data::PhotoSize::Large, attachment.origin);
			_photoViews[i] = view;
		} else if (const auto document = attachment.document) {
			const auto view = document->createMediaView();
			_documentViews[i] = view;
			if (document->filepath(true).isEmpty()
				&& !view->loaded()
				&& !document->loading()) {
				document->save(
					attachment.origin,
					FilesFolder()
						+ u"/%1-"_q.arg(i)
						+ SafeFileName(attachment.name));
			}
		}
	}
	_loadingStarted = crl::now();
	checkLoading();
	if (!loadingDone()) {
		_loading.callEach(500);
	}
}

bool AiWindow::loadingDone() const {
	for (auto i = 0; i != int(_request.attachments.size()); ++i) {
		if (_request.attachments[i].bytes.isEmpty() && !_failed[i]) {
			return false;
		}
	}
	return true;
}

void AiWindow::checkLoading() {
	const auto count = int(_request.attachments.size());
	const auto late = (crl::now() - _loadingStarted > kLoadTimeout);
	auto done = 0;
	for (auto i = 0; i != count; ++i) {
		auto &attachment = _request.attachments[i];
		if (!attachment.bytes.isEmpty() || _failed[i]) {
			++done;
			continue;
		} else if (!_request.session) {
			_failed[i] = true;
			++done;
			continue;
		}
		if (const auto &view = _photoViews[i]) {
			if (view->loaded()) {
				attachment.bytes = PhotoBytes(view);
			}
		} else if (const auto document = attachment.document) {
			const auto path = document->filepath(true);
			if (!path.isEmpty() && !document->loading()) {
				auto file = QFile(path);
				if (file.size() <= kMaxAttachmentSize
					&& file.open(QIODevice::ReadOnly)) {
					attachment.bytes = file.readAll();
				}
			} else if (const auto &view = _documentViews[i]) {
				if (view->loaded()) {
					attachment.bytes = view->bytes();
				}
			}
		}
		if (attachment.bytes.isEmpty() && late) {
			_failed[i] = true;
		}
		if (!attachment.bytes.isEmpty() || _failed[i]) {
			++done;
		}
	}
	if (done < count) {
		setStatus(TrNow("ai_status_downloading").arg(done).arg(count));
		return;
	}
	_loading.cancel();
	filesStatus();
}

int AiWindow::readyCount() const {
	auto result = 0;
	for (const auto i : _remaining) {
		if (!_request.attachments[i].bytes.isEmpty()) {
			++result;
		}
	}
	return result;
}

// Once the text is in and the files are downloaded: how to add them.
void AiWindow::filesStatus() {
	if (!_textInserted || !loadingDone()) {
		return;
	} else if (_given > 0) {
		setStatus(_remaining.empty()
			? TrNow("ai_status_attached").arg(_given)
			: TrNow("ai_status_one_by_one").arg(readyCount()));
	} else if (const auto ready = readyCount()) {
		setStatus(TrNow("ai_status_press_add").arg(ready));
	} else {
		setStatus(TrNow("ai_status_inserted"));
	}
}

// The site's button for adding files was pressed: MZGram's chooser with
// the files of the messages, or the computer's dialog when there are none.
void AiWindow::chooseFiles(Pick pick) {
	if (_filesBox) {
		return;
	}
	auto rows = std::vector<int>();
	for (const auto i : _remaining) {
		if (!_failed[i]) {
			rows.push_back(i);
		}
	}
	if (rows.empty()) {
		chooseOther(pick);
		return;
	}
	auto box = Box([=](not_null<Ui::GenericBox*> box) {
		filesBox(box, rows, pick);
	});
	_filesBox = box.data();
	// The page is a window of its own above the panel: away while the
	// chooser is shown
	const auto widget = _webview ? _webview->widget() : nullptr;
	if (widget) {
		widget->hide();
	}
	_panel->showBox(
		std::move(box),
		Ui::LayerOption::KeepOther,
		anim::type::normal);
}

void AiWindow::filesBox(
		not_null<Ui::GenericBox*> box,
		std::vector<int> rows,
		Pick pick) {
	struct State {
		std::vector<Ui::Checkbox*> checks;
		std::vector<bool> allowed;
		std::vector<bool> wanted;
		std::vector<bool> ready;
		std::vector<QImage> thumbs;
		rpl::variable<int> count;
		base::Timer timer;
		bool answered = false;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto count = int(rows.size());
	_filesTitle = TrNow("ai_files_title").arg(Name(_provider));
	box->setTitle(rpl::single(_filesTitle));
	box->setWidth(st::boxWideWidth);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		Tr("ai_files_hint"),
		st::boxDividerLabel));
	Ui::AddSkip(box->verticalLayout());

	const auto ready = [=](int k) {
		return !_request.attachments[rows[k]].bytes.isEmpty();
	};
	const auto label = [=](int k) {
		const auto &file = _request.attachments[rows[k]];
		return file.name + u" \u00B7 "_q + (ready(k)
			? Ui::FormatSizeText(file.bytes.size())
			: TrNow("ai_files_downloading"));
	};
	const auto recount = [=] {
		auto result = 0;
		for (auto k = 0; k != count; ++k) {
			const auto on = state->allowed[k]
				&& state->ready[k]
				&& state->wanted[k];
			if (state->checks[k]->checked() != on) {
				state->checks[k]->setChecked(
					on,
					Ui::Checkbox::NotifyAboutChange::DontNotify);
			}
			result += on ? 1 : 0;
		}
		state->count = result;
	};
	const auto size = style::ConvertScale(44);
	const auto height = style::ConvertScale(52);
	const auto left = size + style::ConvertScale(14);
	auto notAllowed = false;
	for (auto k = 0; k != count; ++k) {
		const auto &file = _request.attachments[rows[k]];
		const auto allowed = AcceptsFile(pick.accept, file.name, file.mime);
		notAllowed |= !allowed;
		state->allowed.push_back(allowed);
		state->wanted.push_back(allowed);
		state->ready.push_back(ready(k));
		state->thumbs.push_back(QImage());
		if (const auto &view = _documentViews[rows[k]]) {
			view->thumbnailWanted(file.origin);
		}
		const auto row = box->addRow(object_ptr<Ui::RpWidget>(box));
		row->resize(row->width(), height);
		const auto check = Ui::CreateChild<Ui::Checkbox>(
			row,
			label(k),
			false,
			st::defaultBoxCheckbox);
		check->setDisabled(!allowed || !state->ready[k]);
		state->checks.push_back(check);
		check->checkedChanges() | rpl::on_next([=](bool checked) {
			if (state->allowed[k] && state->ready[k]) {
				state->wanted[k] = checked;
			}
			recount();
		}, check->lifetime());
		row->widthValue() | rpl::on_next([=](int width) {
			check->resizeToNaturalWidth(std::max(width - left, 1));
			check->moveToLeft(left, (height - check->height()) / 2, width);
		}, row->lifetime());
		row->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(row);
			p.setRenderHint(QPainter::Antialiasing);
			p.setOpacity(allowed ? 1. : 0.5);
			const auto rect = QRect(0, (height - size) / 2, size, size);
			const auto radius = style::ConvertScale(6);
			auto path = QPainterPath();
			path.addRoundedRect(rect, radius, radius);
			auto &thumb = state->thumbs[k];
			if (thumb.isNull()) {
				thumb = thumbOf(rows[k]);
			}
			if (!thumb.isNull()) {
				const auto ratio = row->devicePixelRatioF();
				auto scaled = thumb.scaled(
					rect.size() * ratio,
					Qt::KeepAspectRatioByExpanding,
					Qt::SmoothTransformation);
				const auto from = QRect(
					(scaled.width() - int(size * ratio)) / 2,
					(scaled.height() - int(size * ratio)) / 2,
					int(size * ratio),
					int(size * ratio));
				p.setClipPath(path);
				p.drawImage(rect, scaled, from);
				p.setClipping(false);
			} else {
				const auto &name = _request.attachments[rows[k]].name;
				const auto dot = name.lastIndexOf('.');
				const auto ext = (dot >= 0 && name.size() - dot <= 5)
					? name.mid(dot + 1).toUpper()
					: QString();
				p.fillPath(path, st::windowBgActive);
				p.setPen(st::windowFgActive);
				p.setFont(st::semiboldFont);
				p.drawText(rect, Qt::AlignCenter, ext);
			}
		}, row->lifetime());
	}
	recount();

	// The last row of the list: another file on this computer. Below the
	// list only Cancel and Add, so Add is never pushed off the box.
	const auto other = box->addRow(
		object_ptr<Ui::SettingsButton>(
			box,
			Tr("ai_files_other"),
			st::settingsButtonLightNoIcon),
		style::margins());
	_otherRow = other;
	other->setClickedCallback([=] {
		state->answered = true;
		box->closeBox();
		crl::on_main(_body, [=] {
			chooseOther(pick);
		});
	});

	if (notAllowed) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			Tr("ai_files_photos_only"),
			st::boxDividerLabel));
	}
	if (const auto left = LeftOutText(_request.choice); !left.isEmpty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(left),
			st::boxDividerLabel));
	}

	// The downloads still going
	state->timer.setCallback([=] {
		auto waiting = false;
		for (auto k = 0; k != count; ++k) {
			if (!state->ready[k] && ready(k)) {
				state->ready[k] = true;
				state->checks[k]->setText(label(k));
				state->checks[k]->setDisabled(!state->allowed[k]);
			}
			waiting |= !state->ready[k] && state->allowed[k];
			if (state->thumbs[k].isNull()) {
				box->update();
			}
		}
		recount();
		if (!waiting) {
			state->timer.cancel();
		}
	});
	state->timer.callEach(500);

	const auto add = [=] {
		auto checked = std::vector<int>();
		for (auto k = 0; k != count; ++k) {
			if (state->checks[k]->checked()) {
				checked.push_back(rows[k]);
			}
		}
		if (checked.empty() || state->answered) {
			return;
		}
		state->answered = true;
		const auto give = GiveFiles(checked, pick.multiple);
		_remaining = give.later;
		_given += int(give.now.size());
		auto files = std::vector<Attachment>();
		for (const auto i : give.now) {
			files.push_back(_request.attachments[i]);
		}
		box->closeBox();
		giveFiles(files);
	};
	_addFiles = add;
	box->addButton(rpl::combine(
		Tr("ai_files_add"),
		state->count.value()
	) | rpl::map([](QString text, int count) {
		return text.arg(count);
	}), add);
	box->addButton(Tr("button_cancel"), [=] {
		box->closeBox();
	});
	box->boxClosing() | rpl::on_next([=] {
		_addFiles = nullptr;
		if (!state->answered && _webview) {
			_webview->eval(CancelPickScript());
		}
		if (const auto widget = _webview ? _webview->widget() : nullptr) {
			widget->show();
		}
	}, box->lifetime());
}

// The small picture of a row: the thumbnail Telegram already has, or a
// picture from the file itself when it is small; nothing big is decoded.
QImage AiWindow::thumbOf(int index) const {
	const auto &file = _request.attachments[index];
	if (const auto &view = _photoViews[index]) {
		for (const auto size : {
				Data::PhotoSize::Thumbnail,
				Data::PhotoSize::Small,
				Data::PhotoSize::Large }) {
			if (const auto image = view->image(size)) {
				return image->original();
			}
		}
	} else if (const auto &view = _documentViews[index]) {
		if (const auto image = view->thumbnail()) {
			return image->original();
		}
	}
	constexpr auto kSmallPicture = 4 * 1024 * 1024;
	if (file.mime.startsWith(u"image/"_q)
		&& !file.bytes.isEmpty()
		&& file.bytes.size() <= kSmallPicture) {
		return QImage::fromData(file.bytes);
	}
	return QImage();
}

// Another file of the computer, by its own dialog.
void AiWindow::chooseOther(Pick pick) {
	const auto done = crl::guard(_body, [=](
			const FileDialog::OpenResult &result) {
		auto files = std::vector<Attachment>();
		for (const auto &path : result.paths) {
			auto file = QFile(path);
			if (file.size() > kMaxAttachmentSize
				|| !file.open(QIODevice::ReadOnly)) {
				continue;
			}
			const auto info = QFileInfo(path);
			files.push_back({
				.name = info.fileName(),
				.mime = Core::MimeTypeForFile(info).name(),
				.bytes = file.readAll(),
			});
			if (!pick.multiple) {
				break;
			}
		}
		if (files.empty()) {
			eval(CancelPickScript());
		} else {
			giveFiles(files);
		}
	});
	const auto failed = crl::guard(_body, [=] {
		eval(CancelPickScript());
	});
	const auto open = pick.multiple
		? FileDialog::GetOpenPaths
		: FileDialog::GetOpenPath;
	open(
		_panel.get(),
		TrNow("ai_files_other"),
		FileDialog::AllFilesFilter(),
		done,
		failed);
}

// The files to the input the site opened, as its dialog would give them.
void AiWindow::giveFiles(const std::vector<Attachment> &files) {
	if (!_webview) {
		return;
	}
	auto index = 0;
	for (const auto &file : files) {
		if (file.bytes.isEmpty()) {
			continue;
		}
		_webview->eval(FileStartScript(index, file.name, file.mime));
		for (auto from = 0; from < file.bytes.size(); from += kChunkSize) {
			_webview->eval(FileChunkScript(
				index,
				file.bytes.mid(from, kChunkSize)));
		}
		++index;
	}
	_webview->eval(GiveFilesScript());
}

bool AiWindow::filesReadyForTest() const {
	return _textInserted && loadingDone();
}

QString AiWindow::statusForTest() const {
	return _statusText.current();
}

int AiWindow::barHeightForTest() const {
	return _container->y();
}

int AiWindow::pageTopForTest() const {
	const auto widget = _webview ? _webview->widget() : nullptr;
	return widget
		? widget->mapTo(_body, QPoint()).y()
		: _container->y();
}

bool AiWindow::statusFitsForTest() const {
	if (_status->isHidden()) {
		return true;
	}
	auto left = _body->width();
	for (const auto button : { _save, _copy, _insert }) {
		if (!button->isHidden()) {
			left = std::min(left, button->x());
		}
	}
	const auto status = _status->geometry();
	return (status.x() + status.width() <= left)
		&& (status.y() >= 0)
		&& (status.y() + status.height() <= _container->y());
}

bool AiWindow::otherRowLastForTest() const {
	if (!_otherRow) {
		return false;
	}
	const auto parent = _otherRow->parentWidget();
	auto rows = 0;
	for (const auto child : parent->findChildren<QWidget*>(
			QString(),
			Qt::FindDirectChildrenOnly)) {
		auto checkbox = false;
		for (const auto widget : child->findChildren<QWidget*>()) {
			if (dynamic_cast<Ui::Checkbox*>(widget)) {
				checkbox = true;
				break;
			}
		}
		if (checkbox) {
			++rows;
			if (child->y() >= _otherRow->y()) {
				return false;
			}
		}
	}
	return (rows > 0);
}

QString AiWindow::filesTitleForTest() const {
	return _filesTitle;
}

bool AiWindow::addFilesForTest() {
	if (const auto add = _addFiles) {
		add();
		return true;
	}
	return false;
}

// The downloaded attachments into the Downloads folder, to add them by hand.
void AiWindow::saveFiles() {
	const auto downloads = QStandardPaths::writableLocation(
		QStandardPaths::DownloadLocation);
	const auto folder = (downloads.isEmpty() ? cWorkingDir() : downloads)
		+ u"/MZGram AI"_q;
	QDir().mkpath(folder);
	auto first = QString();
	for (const auto &attachment : _request.attachments) {
		if (attachment.bytes.isEmpty()) {
			continue;
		}
		const auto path = folder + u"/"_q + SafeFileName(attachment.name);
		auto file = QFile(path);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(attachment.bytes);
			if (first.isEmpty()) {
				first = path;
			}
		}
	}
	if (first.isEmpty()) {
		Ui::Toast::Show(TrNow("ai_status_downloading").arg(0).arg(
			int(_request.attachments.size())));
		return;
	}
	Ui::Toast::Show(TrNow("ai_files_saved").arg(QDir::toNativeSeparators(folder)));
	File::ShowInFolder(first);
}

void AiWindow::setStatus(const QString &text) {
	_statusText = text;
}

void AiWindow::handleMessage(const QByteArray &message) {
	if (const auto pick = ParsePick(message)) {
		chooseFiles(*pick);
	} else if (const auto given = ParseGiven(message)) {
		if (!*given) {
			setStatus(TrNow("ai_status_no_attach"));
		} else if (_given > 0) {
			filesStatus();
		} else {
			setStatus(TrNow("ai_status_attached").arg(*given));
		}
	} else {
		switch (ParseStatus(message)) {
		case Status::Inserted:
			_waiting.cancel();
			_textInserted = true;
			if (_request.attachments.empty()) {
				setStatus(TrNow("ai_status_inserted"));
			} else {
				filesStatus();
			}
			break;
		case Status::NoField:
			_waiting.cancel();
			setStatus(TrNow("ai_status_no_field"));
			break;
		case Status::None:
			break;
		}
	}
	if (_observer) {
		_observer(message);
	}
}

void AiWindow::setObserver(Fn<void(QByteArray)> observer) {
	_observer = std::move(observer);
}

rpl::producer<> AiWindow::closeRequests() const {
	return _closeRequests.events();
}

void CloseWindow() {
	delete base::take(Opened);
}

not_null<AiWindow*> OpenWindow(
		Provider provider,
		Collected request,
		QByteArray localPage = QByteArray()) {
	if (Opened && localPage.isEmpty()) {
		Opened->show(provider, std::move(request));
		return Opened;
	}
	CloseWindow();
	RemoveOldProfiles();
	const auto result = new AiWindow(
		provider,
		std::move(request),
		std::move(localPage));
	Opened = result;
	result->closeRequests() | rpl::on_next([=] {
		crl::on_main([=] {
			if (Opened == result) {
				CloseWindow();
			}
		});
	}, result->lifetime());
	return result;
}

// The text and attachments leave Telegram: said once, before the first
// time, and not again.
void Open(
		not_null<Window::SessionController*> controller,
		Provider provider,
		Collected request) {
	if (OptionAiWarned.value()) {
		OpenWindow(provider, std::move(request));
		return;
	}
	controller->show(Ui::MakeConfirmBox({
		.text = TrNow("ai_privacy_text").arg(Name(provider))
			+ u"\n\n"_q
			+ TrNow("ai_warning_files"),
		.confirmed = [=](Fn<void()> close) {
			OptionAiWarned.set(true);
			close();
			OpenWindow(provider, request);
		},
		.confirmText = Tr("ai_continue"),
		.cancelText = Tr("button_cancel"),
		.title = Tr("ai_privacy_title"),
	}));
}

void AskBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		Collected request) {
	box->setTitle(Tr("ai_ask"));
	box->setWidth(st::boxWideWidth);
	const auto container = box->verticalLayout();

	const auto message = request.text;
	if (!message.isEmpty()) {
		Ui::AddSubsectionTitle(container, Tr("ai_message"));
		const auto preview = (message.size() > kPreviewLength)
			? (message.left(kPreviewLength) + QChar(0x2026))
			: message;
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(preview),
			st::boxLabel));
		Ui::AddSkip(container);
	}
	if (!request.attachments.empty()) {
		auto names = QStringList();
		for (const auto &attachment : request.attachments) {
			names.push_back(attachment.name);
		}
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(TrNow("ai_attachments")
				.arg(int(request.attachments.size()))
				.arg(names.join(u", "_q))),
			st::boxLabel));
	}
	// What stays behind and why
	if (const auto left = LeftOutText(request.choice); !left.isEmpty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(left),
			st::boxDividerLabel));
	}
	Ui::AddSkip(container);
	const auto prompt = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		Ui::InputField::Mode::MultiLine,
		Tr("ai_prompt"),
		QString()));

	Ui::AddSkip(container);
	Ui::AddSubsectionTitle(container, Tr("ai_provider"));
	const auto chosen = box->lifetime().make_state<rpl::variable<Provider>>(
		DefaultProvider());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		int(chosen->current()));
	for (const auto &info : Providers()) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box,
			group,
			int(info.id),
			QString::fromLatin1(info.name),
			st::defaultBoxCheckbox));
	}
	group->setChangedCallback([=](int value) {
		*chosen = Provider(value);
	});

	// No site has a model parameter MZGram could rely on: said in one line
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		rpl::combine(
			Tr("ai_model_on_site"),
			chosen->value()
		) | rpl::map([](QString text, Provider provider) {
			return text.arg(Name(provider));
		}),
		st::boxDividerLabel));

	box->setFocusCallback([=] {
		prompt->setFocusFast();
	});
	box->addButton(Tr("ai_open"), [=] {
		auto copy = request;
		copy.text = ComposeText(prompt->getLastText(), message);
		const auto provider = chosen->current();
		box->closeBox();
		Open(controller, provider, std::move(copy));
	});
	box->addButton(Tr("button_cancel"), [=] {
		box->closeBox();
	});
}

// The message and, for an album, the other messages of it.
[[nodiscard]] std::vector<not_null<HistoryItem*>> AlbumOf(
		not_null<HistoryItem*> item) {
	if (const auto group = item->history()->owner().groups().find(item)) {
		return { begin(group->items), end(group->items) };
	}
	return { item };
}

void AddAskAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		Collected request) {
	if (request.text.isEmpty() && request.attachments.empty()) {
		return;
	}
	menu->addAction(TrNow("ai_ask"), crl::guard(controller, [=] {
		controller->show(Box(AskBox, controller, request));
	}), &st::menuIconBot);
}

} // namespace

const char kOptionAiAction[] = "mzgram-ai-action";
const char kOptionAiWarned[] = "mzgram-ai-warned";

bool AskAiAction() {
	return OptionAiAction.value();
}

void AddAskAiAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller,
		bool copyRestricted) {
	if (!AskAiAction() || copyRestricted || item->isService()) {
		return;
	}
	const auto items = AlbumOf(item);
	auto texts = QStringList();
	for (const auto &part : items) {
		const auto text = part->originalText().text.trimmed();
		if (!text.isEmpty()) {
			texts.push_back(text);
		}
	}
	AddAskAction(menu, controller, Collect(items, texts.join(u"\n\n"_q)));
}

// The selected messages, in the order of the chat as the text is.
[[nodiscard]] Collected CollectSelected(
		not_null<Window::SessionController*> controller,
		const MessageIdsList &ids,
		QString text) {
	auto items = std::vector<not_null<HistoryItem*>>();
	auto &data = controller->session().data();
	for (const auto &id : ids) {
		if (const auto item = data.message(id)) {
			if (!item->isService()) {
				items.push_back(item);
			}
		}
	}
	ranges::sort(items, [](not_null<HistoryItem*> a, not_null<HistoryItem*> b) {
		return (a->date() != b->date())
			? (a->date() < b->date())
			: (a->id < b->id);
	});
	return Collect(items, std::move(text));
}

void AddAskAiSelectedAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		MessageIdsList ids,
		QString text,
		bool copyRestricted,
		bool outsideSelection) {
	if (!AskAiAction() || copyRestricted || ids.empty()) {
		return;
	}
	auto request = CollectSelected(controller, ids, std::move(text));
	if (!outsideSelection) {
		AddAskAction(menu, controller, std::move(request));
		return;
	} else if (request.text.isEmpty() && request.attachments.empty()) {
		return;
	}
	// From a message outside the selection: it says it is about the
	// selected ones
	menu->addAction(
		TrNow("ask_selected").arg(int(ids.size())),
		crl::guard(controller, [=] {
			controller->show(Box(AskBox, controller, request));
		}),
		&st::menuIconBot);
}

void AskAboutSelected(
		not_null<Window::SessionController*> controller,
		MessageIdsList ids,
		QString text,
		bool copyRestricted) {
	if (ids.empty()) {
		return;
	} else if (copyRestricted) {
		controller->showToast(TrNow("ai_restricted"));
		return;
	}
	auto request = CollectSelected(controller, ids, std::move(text));
	if (request.text.isEmpty() && request.attachments.empty()) {
		return;
	}
	controller->show(Box(AskBox, controller, std::move(request)));
}

void OpenSite(Provider provider) {
	OpenWindow(provider, Collected());
}

void AddMainMenuButtons(
		not_null<Ui::VerticalLayout*> menu,
		not_null<Window::SessionController*> controller) {
	for (const auto &info : Providers()) {
		const auto provider = info.id;
		const auto option = &base::options::lookup<bool>(
			MenuButtonOption(provider).toLatin1().constData());
		const auto wrap = menu->add(
			object_ptr<Ui::SlideWrap<Ui::SettingsButton>>(
				menu,
				Settings::CreateButtonWithIcon(
					menu,
					rpl::single(QString::fromLatin1(info.name)),
					st::mainMenuButton,
					{ &st::menuIconBot })));
		wrap->toggleOn(rpl::single(
			rpl::empty
		) | rpl::then(
			option->changes()
		) | rpl::map([=] {
			return option->value();
		}));
		wrap->finishAnimating();
		wrap->entity()->setClickedCallback([=] {
			controller->window().hideSettingsAndLayer();
			OpenSite(provider);
		});
	}
}

void ClearSignIns() {
	CloseWindow();
	base::options::lookup<QString>(kOptionAiStorageToken).set(QString());
	RemoveOldProfiles();
}

void StartTestHook() {
#ifdef MZGRAM_TEST_HOOKS
	// The AI window test of the Windows CI (mzgram_ai_window_test.ps1):
	// the window of every site in turn, on a local page with an input field
	// like the site's, its button for adding files (a file input in the
	// page, Claude's with a label for it; Gemini's makes one when pressed)
	// and a Send button that marks a send. The text goes in by itself, the
	// files do not: the site's button opens MZGram's chooser, whose Add
	// gives both files at once. Then a field that takes one file at a time.
	// Only a build configured with MZGRAM_TEST_HOOKS has it.
	if (qEnvironmentVariableIsEmpty("MZGRAM_TEST_AI")) {
		return;
	}
	const auto field = [](const ProviderInfo &info) -> QByteArray {
		switch (info.id) {
		case Provider::Gemini:
			return "<rich-textarea><div class=\"ql-editor\" "
				"contenteditable=\"true\"></div></rich-textarea>";
		case Provider::Claude:
			return "<div class=\"ProseMirror\" contenteditable=\"true\"></div>";
		case Provider::ChatGpt:
			return "<div id=\"prompt-textarea\" class=\"ProseMirror\" "
				"contenteditable=\"true\"></div>";
		case Provider::DeepSeek:
			return "<textarea id=\"chat-input\"></textarea>";
		case Provider::Grok:
			return "<textarea aria-label=\"Ask Grok anything\"></textarea>";
		}
		return QByteArray();
	};
	const auto page = [=](const ProviderInfo &info, bool multiple) {
		const auto input = (info.id == Provider::Gemini)
			? QByteArray()
			: (QByteArray("<input type=\"file\" id=\"files\" ")
				+ (multiple ? "multiple " : "")
				+ "style=\"display:none\">");
		const auto label = (info.id == Provider::Claude)
			? QByteArray("<label for=\"files\" id=\"label\">Attach</label>")
			: QByteArray();
		return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>"
			"textarea,[contenteditable]{display:block;width:600px;"
			"min-height:80px;border:1px solid #888}</style></head><body>"
			"<form id=\"form\">" + field(info) + input + label
			+ "<button id=\"plus\" type=\"button\">+</button>"
			"<button id=\"send\" type=\"submit\">Send</button></form><script>"
			"window.mzgramSent=false;window.mzgramFiles=[];"
			"window.mzgramChanges=0;"
			"const record=(list)=>{window.mzgramChanges++;"
			"window.mzgramFiles=window.mzgramFiles.concat(Array.from(list)"
			".map(f=>f.name+':'+f.size));};"
			"const files=document.getElementById('files');"
			"if(files){files.addEventListener('change',()=>record(files.files));}"
			"document.getElementById('plus').addEventListener('click',()=>{"
			"let input=files;if(!input){input=document.createElement('input');"
			"input.type='file';input.multiple=true;"
			"input.addEventListener('change',()=>record(input.files));}"
			"input.click();});"
			"document.addEventListener('paste',e=>{if(e.clipboardData"
			"&&e.clipboardData.files.length){record(e.clipboardData.files);"
			"e.preventDefault();}},true);"
			"document.getElementById('form').addEventListener('submit',"
			"e=>{e.preventDefault();window.mzgramSent=true;});"
			"document.getElementById('send').addEventListener('click',"
			"()=>{window.mzgramSent=true;});"
			"document.addEventListener('keydown',e=>{if(e.key==='Enter')"
			"{window.mzgramSent=true;}},true);"
			"</script></body></html>";
	};
	// The site's button for adding files, pressed: Claude's label, else
	// the button
	const auto press = [](const ProviderInfo &info) -> QByteArray {
		return (info.id == Provider::Claude)
			? "document.getElementById('label').click();"
			: "document.getElementById('plus').click();";
	};
	const auto before = QByteArray(
		"window.external.invoke(JSON.stringify({mzgram_ai_before:"
		"window.mzgramFiles.length}));");
	const auto check = [](const ProviderInfo &info) -> QByteArray {
		return "(function(){const e=document.querySelector("
			+ ScriptString(QString::fromLatin1(info.inputSelectors.front()))
			+ ");window.external.invoke(JSON.stringify({mzgram_ai_test:{"
			"value:e?((e.tagName==='TEXTAREA')?e.value:e.innerText):null,"
			"files:window.mzgramFiles,"
			"changes:window.mzgramChanges,"
			"sent:window.mzgramSent===true}}));})();";
	};
	const auto text = ComposeText(
		u"Test prompt: \"quotes\" & <tags>"_q,
		QString::fromUtf8("Line one\n\xd0\x94\xd1\x80\xd1\x83\xd0\xb3\xd0\xb8\xd0\xb9 "
			"\xf0\x9f\x91\x8b"));
	// A text file and a 1x1 PNG
	const auto notes = QByteArray("MZGram test file\nsecond line\n");
	const auto pixel = QByteArray::fromBase64(
		"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==");
	const auto request = [=] {
		auto result = Collected{ .text = text };
		result.attachments.push_back({
			.name = u"notes.txt"_q,
			.mime = u"text/plain"_q,
			.bytes = notes,
		});
		result.attachments.push_back({
			.name = u"pixel.png"_q,
			.mime = u"image/png"_q,
			.bytes = pixel,
		});
		return result;
	};
	const auto expectedFiles = QStringList{
		u"notes.txt:%1"_q.arg(notes.size()),
		u"pixel.png:%1"_q.arg(pixel.size()),
	};
	const auto run = std::make_shared<Fn<void(int)>>();
	*run = [=](int index) {
		const auto &providers = Providers();
		// The five sites, then DeepSeek's field taking one file at a time,
		// then a site alone as the main menu's button opens it
		const auto oneByOne = (index == int(providers.size()));
		const auto alone = (index == int(providers.size()) + 1);
		if (index > int(providers.size()) + 1) {
			CloseWindow();
			LOG(("MZGram AI test: done"));
			base::call_delayed(1000, [] {
				Core::Quit();
			});
			return;
		} else if (alone) {
			const auto window = OpenWindow(
				Provider::Gemini,
				Collected(),
				page(Info(Provider::Gemini), true));
			base::call_delayed(3000, [=] {
				if (!window->created()) {
					LOG(("MZGram AI test: FAIL site alone: no webview"));
				} else if (window->barHeightForTest() != 0
					|| window->pageTopForTest() != 0) {
					LOG(("MZGram AI test: FAIL site alone: a bar of %1px, the page at %2px"
						).arg(window->barHeightForTest()
						).arg(window->pageTopForTest()));
				} else {
					LOG(("MZGram AI test: PASS site alone: no bar, the page fills the window"));
				}
				CloseWindow();
				base::call_delayed(1000, [=] {
					(*run)(index + 1);
				});
			});
			return;
		}
		const auto &info = oneByOne
			? Info(Provider::DeepSeek)
			: providers[index];
		const auto key = QString::fromLatin1(info.key);
		const auto finished = std::make_shared<bool>(false);
		const auto started = std::make_shared<bool>(false);
		const auto gives = std::make_shared<int>(0);
		const auto finish = [=](const QString &result) {
			if (*finished) {
				return;
			}
			*finished = true;
			LOG(("MZGram AI test: %1").arg(result));
			// Not from inside the window's own handler
			crl::on_main([=] {
				CloseWindow();
				base::call_delayed(1000, [=] {
					(*run)(index + 1);
				});
			});
		};
		const auto window = OpenWindow(
			info.id,
			request(),
			page(info, !oneByOne));
		if (!window->created()) {
			const auto available = Webview::Availability();
			finish(u"FAIL %1: no webview (%2, %3)"_q
				.arg(key)
				.arg(int(available.error))
				.arg(QString::fromStdString(available.details)));
			return;
		}
		LOG(("MZGram AI test: %1 window created, start %2, script %3 bytes"
			).arg(key
			).arg(QString::fromLatin1(info.startUrl)
			).arg(InsertScript(info.id, text).size()));
		// Once the text is in and the files are ready: nothing is in the
		// page yet, the window says to press the site's button
		const auto waitReady = std::make_shared<Fn<void()>>();
		*waitReady = [=] {
			if (*finished) {
				return;
			} else if (!window->filesReadyForTest()) {
				base::call_delayed(300, *waitReady);
				return;
			}
			const auto status = TrNow("ai_status_press_add").arg(2);
			if (window->statusForTest() != status) {
				finish(u"FAIL %1: the status is \"%2\""_q
					.arg(key)
					.arg(window->statusForTest()));
				return;
			} else if (!window->statusFitsForTest()
				|| window->barHeightForTest() <= 0
				|| window->pageTopForTest() != window->barHeightForTest()) {
				finish(u"FAIL %1: the status goes under the buttons or the page"_q
					.arg(key));
				return;
			}
			LOG(("MZGram AI test: PASS %1 layout: the status left of the buttons, the page below the bar"
				).arg(key));
			window->eval(before);
		};
		window->setObserver([=](QByteArray message) {
			if (ParsePick(message)) {
				crl::on_main([=] {
					if (*finished) {
						return;
					}
					const auto title = TrNow("ai_files_title").arg(Name(info.id));
					if (window->filesTitleForTest() != title) {
						finish(u"FAIL %1: the chooser is \"%2\""_q
							.arg(key)
							.arg(window->filesTitleForTest()));
					} else if (!window->otherRowLastForTest()) {
						finish(u"FAIL %1: no other file as the last row"_q.arg(key));
					} else if (!window->addFilesForTest()) {
						finish(u"FAIL %1: no chooser"_q.arg(key));
					}
				});
				return;
			} else if (const auto given = ParseGiven(message)) {
				LOG(("MZGram AI test: %1 files given by chooser: %2"
					).arg(key
					).arg(*given));
				if (oneByOne && ++*gives == 1) {
					const auto status = TrNow("ai_status_one_by_one").arg(1);
					if (*given != 1 || window->statusForTest() != status) {
						finish(u"FAIL one file at a time: %1, \"%2\""_q
							.arg(*given)
							.arg(window->statusForTest()));
					} else {
						window->eval(press(info));
					}
					return;
				}
				window->eval(check(info));
				return;
			} else if (ParseStatus(message) == Status::NoField) {
				finish(u"FAIL %1: no input field"_q.arg(key));
				return;
			} else if (ParseStatus(message) == Status::Inserted) {
				if (!*started) {
					*started = true;
					(*waitReady)();
				}
				return;
			}
			const auto object = QJsonDocument::fromJson(message).object();
			if (object.contains(u"mzgram_ai_before"_q)) {
				if (object.value(u"mzgram_ai_before"_q).toInt() != 0) {
					finish(u"FAIL %1: files in the page by themselves"_q.arg(key));
				} else {
					window->eval(press(info));
				}
				return;
			}
			const auto result = object.value(u"mzgram_ai_test"_q).toObject();
			if (result.isEmpty()) {
				return;
			}
			const auto value = result.value(u"value"_q).toString();
			const auto sent = result.value(u"sent"_q).toBool();
			const auto changes = result.value(u"changes"_q).toInt();
			auto files = QStringList();
			for (const auto &file : result.value(u"files"_q).toArray()) {
				files.push_back(file.toString());
			}
			if (value.simplified() == text.simplified()
				&& files == expectedFiles
				&& changes == (oneByOne ? 2 : 1)
				&& !sent) {
				finish(oneByOne
					? u"PASS one file at a time"_q
					: u"PASS %1: the field has the text, 2 files attached, nothing sent"_q.arg(key));
			} else {
				finish(u"FAIL %1: the field has \"%2\", files %3 in %4, sent %5"_q
					.arg(key)
					.arg(value)
					.arg(files.join(u", "_q))
					.arg(changes)
					.arg(sent ? u"yes"_q : u"no"_q));
			}
		});
		base::call_delayed(45000, [=] {
			finish(u"FAIL %1: no answer"_q.arg(key));
		});
	};
	base::call_delayed(3000, [=] {
		(*run)(0);
	});
#endif // MZGRAM_TEST_HOOKS
}

} // namespace MZGram::Ai
