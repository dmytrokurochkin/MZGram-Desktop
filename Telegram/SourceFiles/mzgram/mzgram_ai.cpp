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
#include "core/application.h"
#include "history/history_item.h"
#include "mzgram/mzgram_lang.h"
#include "settings.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/separate_panel.h"
#include "webview/webview_data_stream_memory.h"
#include "webview/webview_embed.h"
#include "webview/webview_interface.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_widgets.h"

#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>

namespace MZGram::Ai {
namespace {

// The message as the box shows it; all of it goes to the site.
constexpr auto kPreviewLength = 600;
// A site that never showed a field, as when it asks to sign in first.
constexpr auto kInsertTimeout = crl::time(35000);

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
	.description = "The text of \"Ask AI\" leaves Telegram for the AI site",
	.defaultValue = false,
});

[[nodiscard]] QString Name(Provider provider) {
	return QString::fromLatin1(Info(provider).name);
}

[[nodiscard]] QString ProfilesFolder() {
	return cWorkingDir() + u"tdata/"_q + QString::fromLatin1(kStorageFolder);
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

// The site in a window of its own: a bar with what happened and the
// Insert text and Copy text buttons above the page.
class AiWindow final {
public:
	AiWindow(Provider provider, QString text, QByteArray localPage);
	~AiWindow();

	[[nodiscard]] bool created() const;
	void show(Provider provider, QString text);
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

	Provider _provider;
	QString _text;
	// The CI test's page in place of the site, see StartTestHook.
	const QByteArray _localPage;
	std::unique_ptr<Ui::SeparatePanel> _panel;
	Ui::RpWidget *_body = nullptr;
	Ui::FlatLabel *_status = nullptr;
	Ui::RoundButton *_insert = nullptr;
	Ui::RoundButton *_copy = nullptr;
	Ui::RpWidget *_container = nullptr;
	std::unique_ptr<Webview::Window> _webview;
	rpl::variable<QString> _statusText;
	base::Timer _waiting;
	bool _insertOnLoad = false;
	Fn<void(QByteArray)> _observer;
	rpl::event_stream<> _closeRequests;
	rpl::lifetime _lifetime;

};

AiWindow *Opened = nullptr;

AiWindow::AiWindow(Provider provider, QString text, QByteArray localPage)
: _provider(provider)
, _text(std::move(text))
, _localPage(std::move(localPage))
, _panel(std::make_unique<Ui::SeparatePanel>())
, _waiting([=] {
	setStatus(TrNow("ai_status_no_field"));
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
	show(_provider, _text);
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
	_container = Ui::CreateChild<Ui::RpWidget>(_body);
	_insert->setClickedCallback([=] {
		insert();
	});
	_copy->setClickedCallback([=] {
		QGuiApplication::clipboard()->setText(_text);
		Ui::Toast::Show(TrNow("ai_copied"));
	});
	_body->paintRequest() | rpl::on_next([=](QRect clip) {
		QPainter(_body).fillRect(clip, st::windowBg);
	}, _body->lifetime());
	rpl::combine(
		_body->sizeValue(),
		_insert->widthValue(),
		_copy->widthValue(),
		_statusText.value()
	) | rpl::to_empty | rpl::on_next([=] {
		relayout();
	}, _body->lifetime());
}

void AiWindow::relayout() {
	const auto width = _body->width();
	const auto skip = st::boxRowPadding.left() / 2;
	_copy->moveToRight(skip, skip, width);
	_insert->moveToRight(skip * 2 + _copy->width(), skip, width);
	const auto buttons = _insert->width() + skip + _copy->width();
	_status->resizeToWidth(std::max(width - buttons - skip * 5, 1));
	const auto bar = std::max(_insert->height(), _status->height()) + skip * 2;
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

void AiWindow::show(Provider provider, QString text) {
	_provider = provider;
	_text = std::move(text);
	_panel->setTitle(rpl::single(Name(_provider)));
	if (!_webview) {
		return;
	}
	setStatus(TrNow("ai_status_opening").arg(Name(_provider)));
	navigate();
	_panel->showAndActivate();
}

void AiWindow::navigate() {
	_insertOnLoad = true;
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
	_webview->eval(InsertScript(_provider, _text));
	_waiting.callOnce(kInsertTimeout);
}

void AiWindow::eval(const QByteArray &script) {
	if (_webview) {
		_webview->eval(script);
	}
}

void AiWindow::setStatus(const QString &text) {
	_statusText = text;
}

void AiWindow::handleMessage(const QByteArray &message) {
	switch (ParseStatus(message)) {
	case Status::Inserted:
		_waiting.cancel();
		setStatus(TrNow("ai_status_inserted"));
		break;
	case Status::NoField:
		_waiting.cancel();
		setStatus(TrNow("ai_status_no_field"));
		break;
	case Status::None:
		break;
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
		const QString &text,
		QByteArray localPage = QByteArray()) {
	if (Opened && localPage.isEmpty()) {
		Opened->show(provider, text);
		return Opened;
	}
	CloseWindow();
	RemoveOldProfiles();
	const auto result = new AiWindow(provider, text, std::move(localPage));
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

// The text leaves Telegram: said once, before the first time.
void Open(
		not_null<Window::SessionController*> controller,
		Provider provider,
		const QString &text) {
	if (OptionAiWarned.value()) {
		OpenWindow(provider, text);
		return;
	}
	controller->show(Ui::MakeConfirmBox({
		.text = TrNow("ai_privacy_text").arg(Name(provider)),
		.confirmed = [=](Fn<void()> close) {
			OptionAiWarned.set(true);
			close();
			OpenWindow(provider, text);
		},
		.confirmText = Tr("ai_continue"),
		.cancelText = Tr("button_cancel"),
		.title = Tr("ai_privacy_title"),
	}));
}

void AskBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		QString message) {
	box->setTitle(Tr("ai_ask"));
	box->setWidth(st::boxWideWidth);
	const auto container = box->verticalLayout();

	Ui::AddSubsectionTitle(container, Tr("ai_message"));
	const auto preview = (message.size() > kPreviewLength)
		? (message.left(kPreviewLength) + QChar(0x2026))
		: message;
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		rpl::single(preview),
		st::boxLabel));
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

	// No site has a model parameter MZGram could rely on: say so.
	Ui::AddSkip(container);
	Ui::AddSubsectionTitle(container, Tr("ai_model"));
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		rpl::combine(
			Tr("ai_model_on_site"),
			chosen->value()
		) | rpl::map([](QString text, Provider provider) {
			return text.arg(Name(provider));
		}),
		st::boxLabel));
	Ui::AddSkip(container);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		Tr("ai_privacy_note"),
		st::boxLabel));

	box->setFocusCallback([=] {
		prompt->setFocusFast();
	});
	box->addButton(Tr("ai_open"), [=] {
		const auto text = ComposeText(prompt->getLastText(), message);
		const auto provider = chosen->current();
		box->closeBox();
		Open(controller, provider, text);
	});
	box->addButton(Tr("button_cancel"), [=] {
		box->closeBox();
	});
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
	const auto message = item->originalText().text;
	if (message.trimmed().isEmpty()) {
		return;
	}
	menu->addAction(TrNow("ai_ask"), crl::guard(controller, [=] {
		controller->show(Box(AskBox, controller, message));
	}), &st::menuIconBot);
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
	// like the site's and a Send button that marks a send. Only a build
	// configured with MZGRAM_TEST_HOOKS has it.
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
		return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>"
			"textarea,[contenteditable]{display:block;width:600px;"
			"min-height:80px;border:1px solid #888}</style></head><body>"
			"<form id=\"form\">" + field(info) + "<button id=\"send\" "
			"type=\"submit\">Send</button></form><script>"
			"window.mzgramSent=false;"
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
			"sent:window.mzgramSent===true}}));})();";
	};
	const auto text = ComposeText(
		u"Test prompt: \"quotes\" & <tags>"_q,
		QString::fromUtf8("Line one\n\xd0\x94\xd1\x80\xd1\x83\xd0\xb3\xd0\xb8\xd0\xb9 "
			"\xf0\x9f\x91\x8b"));
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
		const auto window = OpenWindow(info.id, text, page(info));
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
			const auto status = ParseStatus(message);
			if (status == Status::Inserted) {
				window->eval(check(info));
				return;
			} else if (status == Status::NoField) {
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
			if (value.simplified() == text.simplified() && !sent) {
				finish(u"PASS %1: the field has the text, nothing sent"_q.arg(key));
			} else {
				finish(u"FAIL %1: the field has \"%2\", sent %3"_q
					.arg(key)
					.arg(value)
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
