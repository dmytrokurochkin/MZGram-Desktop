/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace MZGram {

// Ported from AyuGram Desktop (ayu/ui/context_menu/context_menu.cpp,
// AddRepeatMessageAction). Resends the message's content to the same chat
// as a new message, without a "Forwarded from" header.
void AddRepeatMessageAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

// Ported from AyuGram Desktop (ayu/ui/context_menu/context_menu.cpp,
// AddMessageDetailsAction). Shows a box with the message id, dates, view
// count and, for media, its size, MIME type, file name and resolution.
void AddMessageDetailsAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

} // namespace MZGram
