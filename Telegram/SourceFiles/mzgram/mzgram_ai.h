/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mzgram/mzgram_ai_rules.h"

class HistoryItem;

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace MZGram::Ai {

// Settings > MZGram > AI > Show "Ask AI" in the message menu.
extern const char kOptionAiAction[];
[[nodiscard]] bool AskAiAction();

// The warning that the text leaves Telegram was accepted once.
extern const char kOptionAiWarned[];

// "Ask AI" in the menu of a message with text, where "Copy text" is: not
// for a message whose chat restricts copying (copyRestricted, the menu's
// own hasCopyRestriction, which follows "Forward and save protected
// content"). It opens a box with the message, a prompt of the user's own
// and the site; Open shows the site in MZGram's AI window with the text in
// its input field.
void AddAskAiAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller,
	bool copyRestricted);

// Signs out of every AI site: the AI window's profile is cleared.
void ClearSignIns();

// The AI window test of the Windows CI (mzgram_ai_window_test.ps1); does
// nothing outside a build with MZGRAM_TEST_HOOKS.
void StartTestHook();

} // namespace MZGram::Ai
