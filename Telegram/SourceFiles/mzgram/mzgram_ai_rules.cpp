/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_ai_rules.h"

#include "base/basic_types.h"
#include "base/options.h"

namespace MZGram::Ai {
namespace {

const auto kNone = ProviderInfo();

} // namespace

const char kOptionAiProvider[] = "mzgram-ai-provider";
const char kOptionAiStorageToken[] = "mzgram-ai-storage-token";

const std::vector<ProviderInfo> &Providers() {
	static const auto result = std::vector<ProviderInfo>();
	return result;
}

const ProviderInfo &Info(Provider provider) {
	return kNone;
}

std::optional<Provider> ProviderByKey(const QString &key) {
	return std::nullopt;
}

Provider DefaultProvider() {
	return Provider::Gemini;
}

void SetDefaultProvider(Provider provider) {
}

QString ComposeText(const QString &prompt, const QString &message) {
	return QString();
}

QByteArray ScriptString(const QString &text) {
	return QByteArray();
}

QByteArray InsertScript(Provider provider, const QString &text) {
	return QByteArray();
}

Status ParseStatus(const QByteArray &message) {
	return Status::None;
}

bool AllowedNavigation(const QString &url) {
	return true;
}

NewWindow NewWindowTarget(Provider provider, const QString &url) {
	return NewWindow::Here;
}

} // namespace MZGram::Ai
