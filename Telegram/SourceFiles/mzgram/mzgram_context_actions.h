/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;
class DocumentData;

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

// Ported from MZGram Android (showAddToSavedMessages, itself ported from
// Nekogram). Forwards the message to Saved Messages, keeping the
// "Forwarded from" header, same as a normal forward. AyuGram Desktop has
// no equivalent.
void AddSaveMessageAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

// Ported from MZGram Android (showSetReminder, itself ported from
// Nekogram). Opens tdesktop's own date/time picker (the same one used for
// "Reminder" scheduling to Saved Messages) and forwards the message to
// Saved Messages scheduled for that time. AyuGram Desktop has no
// equivalent.
void AddSetReminderAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

// Ported from MZGram Android (showOpenIn, itself ported from Nekogram).
// Opens the OS "Open with" dialog for an already-downloaded document, the
// same native chooser tdesktop already uses for outgoing attachments
// (Core::File::OpenWith). Not applicable on Desktop's Android-equivalent
// media viewer without a matching in-place "Open in" concept, so this adds
// a plain menu item next to "Show in folder" instead.
void AddOpenInAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<DocumentData*> document);

} // namespace MZGram
