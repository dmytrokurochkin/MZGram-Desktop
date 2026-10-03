/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_protected_content.h"

#include "base/options.h"

namespace MZGram {
namespace {

base::options::toggle OptionSaveProtectedContent({
	.id = kOptionSaveProtectedContent,
	.name = "Forward and save protected content",
	.description = "In chats and channels that restrict saving content, "
		"forward, save and copy messages and media like anywhere else.",
});

} // namespace

const char kOptionSaveProtectedContent[] = "mzgram-save-protected-content";

bool SaveProtectedContent() {
	return OptionSaveProtectedContent.value();
}

bool AllowsForwarding(bool allowedOnServer) {
	return allowedOnServer || SaveProtectedContent();
}

bool ForbidsForward(bool forbiddenOnServer) {
	return forbiddenOnServer && !SaveProtectedContent();
}

bool SendAsCopy(bool protectedOnServer) {
	return protectedOnServer && SaveProtectedContent();
}

CopyPlan PlanCopy(const CopySource &source) {
	const auto withFile = [&](CopyKind kind) {
		return CopyPlan{
			.kind = kind,
			.download = !source.fileOnDisk,
			.caption = source.hasText && !source.dropCaption,
		};
	};
	switch (source.media) {
	case CopyMedia::None:
		return source.hasText
			? CopyPlan{ .kind = CopyKind::Text }
			: CopyPlan();
	case CopyMedia::Photo: return withFile(CopyKind::Photo);
	case CopyMedia::File: return withFile(CopyKind::File);
	// Telegram sends voice and round messages without a caption.
	case CopyMedia::Voice: {
		auto result = withFile(CopyKind::Voice);
		result.caption = false;
		return result;
	}
	case CopyMedia::Round: {
		auto result = withFile(CopyKind::Round);
		result.caption = false;
		return result;
	}
	case CopyMedia::Other:
		return (source.hasText && !source.dropCaption)
			? CopyPlan{ .kind = CopyKind::Text }
			: CopyPlan();
	}
	return CopyPlan();
}

} // namespace MZGram
