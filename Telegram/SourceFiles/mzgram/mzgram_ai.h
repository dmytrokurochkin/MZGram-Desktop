/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_types.h"
#include "mzgram/mzgram_ai_rules.h"

class HistoryItem;

namespace Ui {
class PopupMenu;
class VerticalLayout;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace MZGram::Ai {

// Settings > MZGram > AI > Show "Ask AI" in the message menu.
extern const char kOptionAiAction[];
[[nodiscard]] bool AskAiAction();

// The warning that the text and attachments leave Telegram was accepted
// once; it is not shown again.
extern const char kOptionAiWarned[];

// "Ask AI" in the menu of a message (an album: all of it) with text or with
// photos or files an AI reads, where "Copy text" is: not for a message
// whose chat restricts copying (copyRestricted, the menu's own
// hasCopyRestriction, which follows "Forward and save protected content").
// It opens a box with the message, its attachments, a prompt of the user's
// own and the site; Open shows the site in MZGram's AI window with the text
// in its input field and the attachments added to its message.
void AddAskAiAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller,
	bool copyRestricted);

// The same for the selected messages: their text as copying gives it (with
// the senders' names) and their photos and files.
void AddAskAiSelectedAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	MessageIdsList ids,
	QString text,
	bool copyRestricted);

// The AI site with no message: nothing leaves Telegram, so no warning.
void OpenSite(Provider provider);

// The buttons of the AI sites in the main menu, each shown while its switch
// in Settings > MZGram > AI is on.
void AddMainMenuButtons(
	not_null<Ui::VerticalLayout*> menu,
	not_null<Window::SessionController*> controller);

// Signs out of every AI site: the AI window's profile is cleared.
void ClearSignIns();

// The AI window test of the Windows CI (mzgram_ai_window_test.ps1); does
// nothing outside a build with MZGRAM_TEST_HOOKS.
void StartTestHook();

} // namespace MZGram::Ai
