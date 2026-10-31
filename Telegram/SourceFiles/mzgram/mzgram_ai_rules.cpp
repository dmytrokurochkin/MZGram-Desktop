/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_ai_rules.h"

#include "base/basic_types.h"
#include "base/options.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

namespace MZGram::Ai {
namespace {

// The input fields as the sites had them in 2026. A site may change its
// page: then the window says no field was found and the text can still be
// copied and pasted by hand.
const auto kProviders = std::vector<ProviderInfo>{
	{
		.id = Provider::Gemini,
		.key = "gemini",
		.name = "Gemini",
		.startUrl = "https://gemini.google.com/app",
		.hosts = { "gemini.google.com" },
		.inputSelectors = {
			"rich-textarea .ql-editor[contenteditable=\"true\"]",
			"div.ql-editor[contenteditable=\"true\"]",
		},
	},
	{
		.id = Provider::Claude,
		.key = "claude",
		.name = "Claude",
		.startUrl = "https://claude.ai/new",
		.hosts = { "claude.ai" },
		.inputSelectors = {
			"div.ProseMirror[contenteditable=\"true\"]",
			"[data-testid=\"chat-input\"]",
		},
	},
	{
		.id = Provider::ChatGpt,
		.key = "chatgpt",
		.name = "ChatGPT",
		.startUrl = "https://chatgpt.com/",
		.hosts = { "chatgpt.com", "openai.com" },
		.inputSelectors = {
			"#prompt-textarea",
			"div.ProseMirror[contenteditable=\"true\"]",
		},
	},
	{
		.id = Provider::DeepSeek,
		.key = "deepseek",
		.name = "DeepSeek",
		.startUrl = "https://chat.deepseek.com/",
		.hosts = { "deepseek.com" },
		.inputSelectors = {
			"textarea#chat-input",
			"textarea",
		},
	},
	{
		.id = Provider::Grok,
		.key = "grok",
		.name = "Grok",
		.startUrl = "https://grok.com/",
		.hosts = { "grok.com", "x.ai", "x.com" },
		.inputSelectors = {
			"textarea[aria-label]",
			"div.ProseMirror[contenteditable=\"true\"]",
			"textarea",
		},
	},
};

// Pages the sites sign in with; they stay in the window.
const auto kSignInHosts = {
	"accounts.google.com",
	"appleid.apple.com",
	"login.microsoftonline.com",
	"login.live.com",
};

base::options::option<QString> OptionAiProvider({
	.id = kOptionAiProvider,
	.name = "MZGram: the default AI",
	.description = "The site \"Ask AI\" starts with: gemini, claude, "
		"chatgpt, deepseek or grok",
	.defaultValue = u"gemini"_q,
});

base::options::option<QString> OptionAiStorageToken({
	.id = kOptionAiStorageToken,
	.name = "MZGram: the AI sites' profile",
	.description = "The Webview profile the AI sites sign in to, apart "
		"from Telegram's",
	.defaultValue = QString(),
});

[[nodiscard]] bool HostIn(const QString &host, const char *domain) {
	const auto name = QString::fromLatin1(domain);
	return (host == name) || host.endsWith(u"."_q + name);
}

// Generic fields after the site's own: a site that changed its page may
// still have one of these.
const auto kAnyInput = {
	"textarea:not([readonly]):not([disabled])",
	"[contenteditable=\"true\"]",
};

} // namespace

const char kOptionAiProvider[] = "mzgram-ai-provider";
const char kOptionAiStorageToken[] = "mzgram-ai-storage-token";

const std::vector<ProviderInfo> &Providers() {
	return kProviders;
}

const ProviderInfo &Info(Provider provider) {
	for (const auto &info : kProviders) {
		if (info.id == provider) {
			return info;
		}
	}
	return kProviders.front();
}

std::optional<Provider> ProviderByKey(const QString &key) {
	for (const auto &info : kProviders) {
		if (key == QString::fromLatin1(info.key)) {
			return info.id;
		}
	}
	return std::nullopt;
}

Provider DefaultProvider() {
	return ProviderByKey(OptionAiProvider.value()).value_or(Provider::Gemini);
}

void SetDefaultProvider(Provider provider) {
	OptionAiProvider.set(QString::fromLatin1(Info(provider).key));
}

QString ComposeText(const QString &prompt, const QString &message) {
	const auto clean = [](QString text) {
		return text.replace(u"\r\n"_q, u"\n"_q).replace('\r', '\n').trimmed();
	};
	const auto first = clean(prompt);
	const auto second = clean(message);
	return first.isEmpty()
		? second
		: second.isEmpty()
		? first
		: (first + u"\n\n"_q + second);
}

QByteArray ScriptString(const QString &text) {
	auto result = QByteArray("\"");
	result.reserve(text.size() + 2);
	for (const auto ch : text) {
		const auto code = ch.unicode();
		const auto plain = (code >= 0x20)
			&& (code < 0x7F)
			&& (code != '"')
			&& (code != '\\')
			&& (code != '\'')
			&& (code != '<')
			&& (code != '>')
			&& (code != '&');
		if (plain) {
			result.append(char(code));
		} else {
			result.append("\\u");
			result.append(QByteArray::number(int(code), 16).rightJustified(4, '0'));
		}
	}
	result.append('"');
	return result;
}

QByteArray InsertScript(Provider provider, const QString &text) {
	auto selectors = QByteArray();
	const auto add = [&](const char *selector) {
		if (!selectors.isEmpty()) {
			selectors.append(',');
		}
		selectors.append(ScriptString(QString::fromLatin1(selector)));
	};
	for (const auto selector : Info(provider).inputSelectors) {
		add(selector);
	}
	for (const auto selector : kAnyInput) {
		add(selector);
	}
	// Puts the text into the first visible field, the way typing or pasting
	// would, and reads it back. Nothing here presses a key or a button.
	return R"JS((function(){
const text = )JS" + ScriptString(text) + R"JS(;
const selectors = [)JS" + selectors + R"JS(];
const tell = (status) => {
	try { window.external.invoke(JSON.stringify({ mzgram_ai: status })); } catch (e) {}
};
const shown = (e) => {
	const r = e.getBoundingClientRect();
	return r.width > 0 && r.height > 0;
};
const field = () => {
	for (const s of selectors) {
		for (const e of document.querySelectorAll(s)) {
			if (shown(e)) return e;
		}
	}
	return null;
};
const flat = (s) => (s || '').replace(/\s+/g, ' ').trim();
const plain = (e) => (e.tagName === 'TEXTAREA' || e.tagName === 'INPUT');
const holds = (e) => flat(plain(e) ? e.value : e.innerText).includes(flat(text));
const put = (e) => {
	e.focus();
	if (plain(e)) {
		const proto = (e.tagName === 'TEXTAREA')
			? HTMLTextAreaElement.prototype
			: HTMLInputElement.prototype;
		Object.getOwnPropertyDescriptor(proto, 'value').set.call(e, text);
		e.dispatchEvent(new Event('input', { bubbles: true }));
		return;
	}
	const range = document.createRange();
	range.selectNodeContents(e);
	const selection = window.getSelection();
	selection.removeAllRanges();
	selection.addRange(range);
	document.execCommand('insertText', false, text);
	if (holds(e)) return;
	try {
		const data = new DataTransfer();
		data.setData('text/plain', text);
		e.dispatchEvent(new ClipboardEvent('paste', {
			clipboardData: data,
			bubbles: true,
			cancelable: true,
		}));
	} catch (x) {}
	if (holds(e)) return;
	e.textContent = text;
	e.dispatchEvent(new InputEvent('input', {
		bubbles: true,
		inputType: 'insertText',
		data: text,
	}));
};
let tries = 0;
const step = () => {
	const e = field();
	if (e) {
		put(e);
		tell(holds(e) ? 'inserted' : 'no_field');
	} else if (++tries < 60) {
		setTimeout(step, 500);
	} else {
		tell('no_field');
	}
};
step();
})();)JS";
}

Status ParseStatus(const QByteArray &message) {
	const auto document = QJsonDocument::fromJson(message);
	if (!document.isObject()) {
		return Status::None;
	}
	const auto status = document.object().value(u"mzgram_ai"_q).toString();
	return (status == u"inserted"_q)
		? Status::Inserted
		: (status == u"no_field"_q)
		? Status::NoField
		: Status::None;
}

bool AllowedNavigation(const QString &url) {
	if (url == u"about:blank"_q) {
		return true;
	}
	const auto parsed = QUrl(url, QUrl::StrictMode);
	return parsed.isValid()
		&& (parsed.scheme() == u"https"_q)
		&& !parsed.host().isEmpty();
}

NewWindow NewWindowTarget(Provider provider, const QString &url) {
	if (url == u"about:blank"_q || !AllowedNavigation(url)) {
		return NewWindow::Refuse;
	}
	const auto host = QUrl(url, QUrl::StrictMode).host().toLower();
	for (const auto domain : Info(provider).hosts) {
		if (HostIn(host, domain)) {
			return NewWindow::Here;
		}
	}
	for (const auto domain : kSignInHosts) {
		if (host == QString::fromLatin1(domain)) {
			return NewWindow::Here;
		}
	}
	return NewWindow::Browser;
}

} // namespace MZGram::Ai
