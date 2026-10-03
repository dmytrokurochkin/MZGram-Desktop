/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

namespace MZGram {

// Settings > MZGram > Message menu > "Forward and save protected content":
// in chats and channels whose owner turned on "Restrict saving content",
// messages and media can be forwarded, saved, copied and screenshotted like
// anywhere else.
//
// The app's own checks (PeerData::allowsForwarding, HistoryItem::
// forbidsForward, Data::AllowsForwardingValue) ask AllowsForwarding and
// ForbidsForward below, which say "not restricted" while the switch is on.
// The server still refuses to forward such messages, so ApiWrap::
// forwardMessages hands them to SendProtectedCopies (mzgram_protected_copy):
// each one is sent again as a new message with its text and a fresh upload
// of its file, downloading the file first when it is not on disk yet.
//
// Kept free of session/history dependencies so the decisions below can be
// checked by the headless unit test.
extern const char kOptionSaveProtectedContent[];
[[nodiscard]] bool SaveProtectedContent();

// What the app allows, given what the server says.
[[nodiscard]] bool AllowsForwarding(bool allowedOnServer);
[[nodiscard]] bool ForbidsForward(bool forbiddenOnServer);

// A protected message goes out as a copy only while the switch is on;
// with the switch off it is forwarded as before, and the server refuses.
[[nodiscard]] bool SendAsCopy(bool protectedOnServer);

// What a message holds, as far as copying it is concerned.
enum class CopyMedia {
	None, // Text only, or a link preview.
	Photo,
	Voice,
	Round,
	File, // Any other document: video, music, GIF, sticker, file.
	Other, // Poll, location, contact, game, paid media...
};

struct CopySource {
	CopyMedia media = CopyMedia::None;
	bool hasText = false;
	bool fileOnDisk = false;
	bool dropCaption = false;
};

enum class CopyKind {
	Skip, // Nothing that can be sent again.
	Text,
	Photo,
	Voice,
	Round,
	File,
};

struct CopyPlan {
	CopyKind kind = CopyKind::Skip;
	bool download = false; // Download the file first.
	bool caption = false; // Send the text with the file.
};

[[nodiscard]] CopyPlan PlanCopy(const CopySource &source);

} // namespace MZGram
