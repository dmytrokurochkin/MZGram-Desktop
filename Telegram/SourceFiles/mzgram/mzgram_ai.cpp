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
#include "styles/style_widgets.h"
#include "styles/style_window.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
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
	void attach();
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
	bool _attachSent = false;
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
	_status = Ui::CreateChild<Ui::FlatLabel>(
		_body,
		_statusText.value(),
		st::boxLabel);
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
	for (const auto button : { _save, _copy, _insert }) {
		if (button->isHidden()) {
			continue;
		}
		button->moveToRight(right, skip, width);
		right += button->width() + skip;
		height = std::max(height, button->height());
	}
	_status->resizeToWidth(std::max(width - right - skip * 4, 1));
	const auto bar = std::max(
		std::max(height, _status->height()) + skip * 2,
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
	_attachSent = false;
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
	attach();
}

// The attachments into the page once both they and the text are there.
void AiWindow::attach() {
	if (_attachSent
		|| !_textInserted
		|| !_webview
		|| _request.attachments.empty()
		|| !loadingDone()) {
		return;
	}
	_attachSent = true;
	auto index = 0;
	for (const auto &attachment : _request.attachments) {
		if (attachment.bytes.isEmpty()) {
			continue;
		}
		_webview->eval(FileStartScript(index, attachment.name, attachment.mime));
		for (auto from = 0; from < attachment.bytes.size(); from += kChunkSize) {
			_webview->eval(FileChunkScript(
				index,
				attachment.bytes.mid(from, kChunkSize)));
		}
		++index;
	}
	if (!index) {
		setStatus(TrNow("ai_status_no_attach"));
		return;
	}
	_webview->eval(AttachScript(_provider));
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
	if (const auto attached = ParseAttach(message)) {
		setStatus(attached->attached
			? TrNow("ai_status_attached").arg(attached->count)
			: TrNow("ai_status_no_attach"));
	} else {
		switch (ParseStatus(message)) {
		case Status::Inserted:
			_waiting.cancel();
			_textInserted = true;
			if (_request.attachments.empty()) {
				setStatus(TrNow("ai_status_inserted"));
			} else if (loadingDone()) {
				attach();
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
		.text = TrNow("ai_privacy_text").arg(Name(provider)),
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

void AddAskAiSelectedAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		MessageIdsList ids,
		QString text,
		bool copyRestricted) {
	if (!AskAiAction() || copyRestricted || ids.empty()) {
		return;
	}
	auto items = std::vector<not_null<HistoryItem*>>();
	auto &data = controller->session().data();
	for (const auto &id : ids) {
		if (const auto item = data.message(id)) {
			if (!item->isService()) {
				items.push_back(item);
			}
		}
	}
	// In the order of the chat, as the text is
	ranges::sort(items, [](not_null<HistoryItem*> a, not_null<HistoryItem*> b) {
		return (a->date() != b->date())
			? (a->date() < b->date())
			: (a->id < b->id);
	});
	AddAskAction(menu, controller, Collect(items, std::move(text)));
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
	// like the site's, a file input (Gemini: none, its field takes a paste)
	// and a Send button that marks a send. Two files go with the text. Only
	// a build configured with MZGRAM_TEST_HOOKS has it.
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
	const auto page = [=](const ProviderInfo &info) -> QByteArray {
		const auto input = (info.id == Provider::Gemini)
			? QByteArray()
			: QByteArray("<input type=\"file\" id=\"files\" multiple "
				"style=\"display:none\">");
		return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>"
			"textarea,[contenteditable]{display:block;width:600px;"
			"min-height:80px;border:1px solid #888}</style></head><body>"
			"<form id=\"form\">" + field(info) + input + "<button id=\"send\" "
			"type=\"submit\">Send</button></form><script>"
			"window.mzgramSent=false;window.mzgramFiles=[];"
			"const record=(list)=>{window.mzgramFiles=Array.from(list)"
			".map(f=>f.name+':'+f.size);};"
			"const files=document.getElementById('files');"
			"if(files){files.addEventListener('change',()=>record(files.files));}"
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
	const auto check = [](const ProviderInfo &info) -> QByteArray {
		return "(function(){const e=document.querySelector("
			+ ScriptString(QString::fromLatin1(info.inputSelectors.front()))
			+ ");window.external.invoke(JSON.stringify({mzgram_ai_test:{"
			"value:e?((e.tagName==='TEXTAREA')?e.value:e.innerText):null,"
			"files:window.mzgramFiles,"
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
		if (index >= int(providers.size())) {
			CloseWindow();
			LOG(("MZGram AI test: done"));
			base::call_delayed(1000, [] {
				Core::Quit();
			});
			return;
		}
		const auto &info = providers[index];
		const auto key = QString::fromLatin1(info.key);
		const auto finished = std::make_shared<bool>(false);
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
		const auto window = OpenWindow(info.id, request(), page(info));
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
		window->setObserver([=](QByteArray message) {
			if (const auto attached = ParseAttach(message)) {
				if (!attached->attached) {
					finish(u"FAIL %1: no place for the files"_q.arg(key));
					return;
				}
				LOG(("MZGram AI test: %1 files given by %2: %3"
					).arg(key
					).arg(attached->how
					).arg(attached->count));
				window->eval(check(info));
				return;
			} else if (ParseStatus(message) == Status::NoField) {
				finish(u"FAIL %1: no input field"_q.arg(key));
				return;
			}
			const auto result = QJsonDocument::fromJson(message).object().value(
				u"mzgram_ai_test"_q).toObject();
			if (result.isEmpty()) {
				return;
			}
			const auto value = result.value(u"value"_q).toString();
			const auto sent = result.value(u"sent"_q).toBool();
			auto files = QStringList();
			for (const auto &file : result.value(u"files"_q).toArray()) {
				files.push_back(file.toString());
			}
			if (value.simplified() == text.simplified()
				&& files == expectedFiles
				&& !sent) {
				finish(u"PASS %1: the field has the text, 2 files attached, nothing sent"_q.arg(key));
			} else {
				finish(u"FAIL %1: the field has \"%2\", files %3, sent %4"_q
					.arg(key)
					.arg(value)
					.arg(files.join(u", "_q))
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
