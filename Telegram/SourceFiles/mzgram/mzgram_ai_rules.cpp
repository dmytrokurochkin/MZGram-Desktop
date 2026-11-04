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

#include <algorithm>

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

// Pictures the sites take as images.
const auto kImageExtensions = { "jpg", "jpeg", "png", "webp" };
const auto kImageTypes = { "image/jpeg", "image/png", "image/webp" };

// Documents an AI reads: text, data, code, pdf and office files.
const auto kFileExtensions = {
	"txt", "text", "md", "markdown", "rst", "csv", "tsv", "json", "jsonl",
	"xml", "yaml", "yml", "toml", "ini", "cfg", "conf", "log", "tex", "srt",
	"vtt", "html", "htm", "css", "js", "mjs", "cjs", "ts", "jsx", "tsx",
	"py", "ipynb", "java", "kt", "kts", "c", "h", "cpp", "cc", "cxx", "hpp",
	"cs", "go", "rs", "rb", "php", "swift", "m", "mm", "scala", "dart",
	"lua", "pl", "r", "sql", "sh", "bash", "zsh", "ps1", "bat", "cmd",
	"gradle", "properties", "vue", "svelte", "pdf", "doc", "docx", "xls",
	"xlsx", "ppt", "pptx", "rtf", "odt", "ods", "odp", "epub",
};
// Text files that usually have no extension.
const auto kFileNames = {
	"readme", "license", "licence", "changelog", "makefile", "dockerfile",
	"notice", "authors", "contributing",
};
const auto kFileTypes = {
	"application/pdf", "application/json", "application/xml",
	"application/rtf", "application/msword", "application/vnd.ms-excel",
	"application/vnd.ms-powerpoint",
};
const auto kFileTypePrefixes = {
	"application/vnd.openxmlformats-officedocument.",
	"application/vnd.oasis.opendocument.",
};

// Archives and packages: large, and the sites read them badly or not at
// all.
const auto kArchiveExtensions = {
	"zip", "rar", "7z", "tar", "gz", "tgz", "bz2", "tbz", "tbz2", "xz",
	"txz", "zst", "lz", "lzma", "lz4", "z", "cab", "iso", "dmg", "apk",
	"apks", "xapk", "aab", "ipa", "jar", "war", "deb", "rpm", "msix",
	"appx",
};
const auto kArchiveTypes = {
	"application/zip", "application/x-zip-compressed", "application/gzip",
	"application/x-gzip", "application/x-tar", "application/x-7z-compressed",
	"application/x-rar-compressed", "application/vnd.rar",
	"application/x-bzip2", "application/x-xz", "application/zstd",
	"application/vnd.android.package-archive", "application/java-archive",
	"application/x-iso9660-image",
};

[[nodiscard]] bool OneOf(
		const QString &value,
		std::initializer_list<const char*> list) {
	for (const auto item : list) {
		if (value == QString::fromLatin1(item)) {
			return true;
		}
	}
	return false;
}

base::options::toggle OptionMenuGemini({
	.id = "mzgram-ai-menu-gemini",
	.name = "MZGram: Gemini in the main menu",
});
base::options::toggle OptionMenuClaude({
	.id = "mzgram-ai-menu-claude",
	.name = "MZGram: Claude in the main menu",
});
base::options::toggle OptionMenuChatGpt({
	.id = "mzgram-ai-menu-chatgpt",
	.name = "MZGram: ChatGPT in the main menu",
});
base::options::toggle OptionMenuDeepSeek({
	.id = "mzgram-ai-menu-deepseek",
	.name = "MZGram: DeepSeek in the main menu",
});
base::options::toggle OptionMenuGrok({
	.id = "mzgram-ai-menu-grok",
	.name = "MZGram: Grok in the main menu",
});

[[nodiscard]] base::options::toggle &MenuOption(Provider provider) {
	switch (provider) {
	case Provider::Gemini: return OptionMenuGemini;
	case Provider::Claude: return OptionMenuClaude;
	case Provider::ChatGpt: return OptionMenuChatGpt;
	case Provider::DeepSeek: return OptionMenuDeepSeek;
	case Provider::Grok: return OptionMenuGrok;
	}
	return OptionMenuGemini;
}

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

AttachmentKind ClassifyFile(const QString &name, const QString &mime) {
	const auto lower = name.trimmed().toLower();
	const auto type = mime.trimmed().toLower();
	const auto dot = lower.lastIndexOf('.');
	const auto extension = (dot >= 0) ? lower.mid(dot + 1) : QString();
	const auto base = (dot > 0) ? lower.left(dot) : lower;
	if (OneOf(extension, kArchiveExtensions) || OneOf(type, kArchiveTypes)) {
		return AttachmentKind::Archive;
	} else if (OneOf(extension, kImageExtensions)
		|| (extension.isEmpty() && OneOf(type, kImageTypes))) {
		return AttachmentKind::Photo;
	} else if (type.startsWith(u"video/"_q)
		|| type.startsWith(u"audio/"_q)
		|| type == u"image/gif"_q
		|| OneOf(extension, {
			"gif", "tgs", "webm", "mp4", "mov", "mkv", "avi", "m4v", "mp3",
			"ogg", "oga", "opus", "m4a", "wav", "flac", "aac" })) {
		return AttachmentKind::Media;
	} else if (OneOf(extension, kFileExtensions)
		|| OneOf(base, kFileNames)
		|| OneOf(lower, kFileNames)
		|| type.startsWith(u"text/"_q)
		|| OneOf(type, kFileTypes)) {
		return AttachmentKind::File;
	}
	for (const auto prefix : kFileTypePrefixes) {
		if (type.startsWith(QString::fromLatin1(prefix))) {
			return AttachmentKind::File;
		}
	}
	return AttachmentKind::Unsupported;
}

AttachmentChoice ChooseAttachments(
		const std::vector<AttachmentCandidate> &candidates) {
	auto result = AttachmentChoice();
	for (auto i = 0; i != int(candidates.size()); ++i) {
		const auto &candidate = candidates[i];
		switch (candidate.kind) {
		case AttachmentKind::Archive: ++result.archives; break;
		case AttachmentKind::Media: ++result.media; break;
		case AttachmentKind::Unsupported: ++result.unsupported; break;
		case AttachmentKind::Photo:
		case AttachmentKind::File:
			if (candidate.size > kMaxAttachmentSize) {
				++result.tooLarge;
			} else if (int(result.chosen.size()) >= kMaxAttachments) {
				++result.overLimit;
			} else {
				result.chosen.push_back(i);
			}
			break;
		}
	}
	return result;
}

QByteArray FileStartScript(int index, const QString &name, const QString &mime) {
	return "(window.__mzgramAiFiles = window.__mzgramAiFiles || [])["
		+ QByteArray::number(index)
		+ "] = { name: " + ScriptString(name)
		+ ", type: " + ScriptString(mime)
		+ ", parts: [] };";
}

QByteArray FileChunkScript(int index, const QByteArray &bytes) {
	return "window.__mzgramAiFiles["
		+ QByteArray::number(index)
		+ "].parts.push(\"" + bytes.toBase64() + "\");";
}

QByteArray PickerHookScript() {
	// The site's file input: the browser's dialog stays closed, MZGram's
	// chooser opens. Nothing here presses a key or a button.
	return R"JS((function(){
if (window.__mzgramAiHook) return;
window.__mzgramAiHook = true;
const tell = (message) => {
	try { window.external.invoke(JSON.stringify(message)); } catch (e) {}
};
const isFile = (e) => !!e
	&& e.tagName === 'INPUT'
	&& String(e.type).toLowerCase() === 'file';
// One press can reach both the click listener and input.click(): the
// second within 400 ms is the same press. Giving the files or cancelling
// ends the press, so the next one always opens the chooser.
const pick = (input) => {
	if (input.disabled) return;
	const now = Date.now();
	if (window.__mzgramAiPick === input && now - (window.__mzgramAiLast || 0) < 400) return;
	window.__mzgramAiLast = now;
	window.__mzgramAiPick = input;
	tell({
		mzgram_ai: 'pick',
		multiple: !!input.multiple,
		accept: input.accept || '',
	});
};
const click = HTMLInputElement.prototype.click;
HTMLInputElement.prototype.click = function () {
	if (isFile(this)) { pick(this); return; }
	return click.apply(this, arguments);
};
const show = HTMLInputElement.prototype.showPicker;
if (show) {
	HTMLInputElement.prototype.showPicker = function () {
		if (isFile(this)) { pick(this); return; }
		return show.apply(this, arguments);
	};
}
document.addEventListener('click', (e) => {
	const path = e.composedPath ? e.composedPath() : [e.target];
	for (const node of path) {
		const input = isFile(node)
			? node
			: (node && node.tagName === 'LABEL' && isFile(node.control))
			? node.control
			: null;
		if (input) {
			e.preventDefault();
			pick(input);
			return;
		}
	}
}, true);
})();)JS";
}

QByteArray GiveFilesScript() {
	return R"JS((function(){
const tell = (message) => {
	try { window.external.invoke(JSON.stringify(message)); } catch (e) {}
};
const files = [];
for (const f of (window.__mzgramAiFiles || [])) {
	if (!f) continue;
	const binary = atob(f.parts.join(''));
	const bytes = new Uint8Array(binary.length);
	for (let i = 0; i < binary.length; ++i) bytes[i] = binary.charCodeAt(i);
	files.push(new File([bytes], f.name, { type: f.type }));
}
window.__mzgramAiFiles = [];
const input = window.__mzgramAiPick;
if (!input || !files.length) {
	tell({ mzgram_ai: 'given', count: 0 });
	return;
}
const data = new DataTransfer();
for (const f of files) data.items.add(f);
input.files = data.files;
input.dispatchEvent(new Event('input', { bubbles: true }));
input.dispatchEvent(new Event('change', { bubbles: true }));
window.__mzgramAiLast = 0;
tell({ mzgram_ai: 'given', count: files.length });
})();)JS";
}

QByteArray CancelPickScript() {
	return "(function(){const i=window.__mzgramAiPick;window.__mzgramAiLast=0;"
		"if(i){i.dispatchEvent(new Event('cancel',{bubbles:true}));}})();";
}

std::optional<Pick> ParsePick(const QByteArray &message) {
	const auto object = QJsonDocument::fromJson(message).object();
	if (object.value(u"mzgram_ai"_q).toString() != u"pick"_q) {
		return std::nullopt;
	}
	return Pick{
		.multiple = object.value(u"multiple"_q).toBool(),
		.accept = object.value(u"accept"_q).toString(),
	};
}

std::optional<int> ParseGiven(const QByteArray &message) {
	const auto object = QJsonDocument::fromJson(message).object();
	if (object.value(u"mzgram_ai"_q).toString() != u"given"_q) {
		return std::nullopt;
	}
	return object.value(u"count"_q).toInt();
}

Give GiveFiles(const std::vector<int> &checked, bool multiple) {
	auto result = Give();
	for (auto i = 0; i != int(checked.size()); ++i) {
		((multiple || !i) ? result.now : result.later).push_back(checked[i]);
	}
	return result;
}

bool AcceptsFile(
		const QString &accept,
		const QString &name,
		const QString &mime) {
	const auto lowerName = name.toLower();
	const auto type = mime.trimmed().toLower();
	auto any = false;
	for (const auto &part : accept.split(',')) {
		const auto item = part.trimmed().toLower();
		if (item.isEmpty()) {
			continue;
		}
		any = true;
		if (item == u"*/*"_q
			|| (item.startsWith('.') && lowerName.endsWith(item))
			|| (item == type)
			|| (item.endsWith(u"/*"_q)
				&& type.startsWith(item.left(item.size() - 1)))) {
			return true;
		}
	}
	// Nothing listed, or nothing known of the file: the site decides
	return !any || (type.isEmpty() && !lowerName.contains('.'));
}

QString MenuButtonOption(Provider provider) {
	return u"mzgram-ai-menu-"_q + QString::fromLatin1(Info(provider).key);
}

bool MenuButton(Provider provider) {
	return MenuOption(provider).value();
}

void SetMenuButton(Provider provider, bool shown) {
	MenuOption(provider).set(shown);
}

int BarHeight(
		bool anyButton,
		bool hasStatus,
		int contentHeight,
		int minHeight) {
	if (!anyButton && !hasStatus) {
		return 0;
	}
	return std::max(contentHeight, minHeight);
}

int StatusWidth(int barWidth, int buttonsWidth, int skip) {
	return std::max(barWidth - buttonsWidth - skip * 4, 1);
}

} // namespace MZGram::Ai
