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

// Resends the message's content to the same chat
// as a new message, without a "Forwarded from" header.
void AddRepeatMessageAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

// Shows a box with the message id, dates, view
// count and, for media, its size, MIME type, file name and resolution.
void AddMessageDetailsAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

// As on MZGram Android (showAddToSavedMessages). Forwards the message to
// Saved Messages, keeping the "Forwarded from" header, same as a normal
// forward.
void AddSaveMessageAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

// As on MZGram Android (showSetReminder). Opens tdesktop's own date/time
// picker (the same one used for "Reminder" scheduling to Saved Messages)
// and forwards the message to Saved Messages scheduled for that time.
void AddSetReminderAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

// As on MZGram Android (showOpenIn). Opens the OS "Open with" dialog for an already-downloaded document, the
// same native chooser tdesktop already uses for outgoing attachments
// (Core::File::OpenWith). Not applicable on Desktop's Android-equivalent
// media viewer without a matching in-place "Open in" concept, so this adds
// a plain menu item next to "Show in folder" instead.
void AddOpenInAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<DocumentData*> document);

// As on MZGram Android (showCopyPhoto): copies a photo, or a jpg, png or
// webp sent as a file, to the clipboard; a photo not downloaded yet is
// downloaded first. While its switch is on it stands for Telegram's own
// "Copy Image" (CopyPhotoAction), so the menu has one such item.
void AddCopyPhotoAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller,
	bool copyRestricted);

// As on MZGram Android (showDeleteDownloadedFile): removes a message's
// downloaded file from this computer after a confirmation; the message
// stays and the file can be downloaded again.
void AddDeleteDownloadedFileAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

// As on MZGram Android (showNoQuoteForward): the forward box with the
// sender's name already hidden.
void AddForwardNoQuoteAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

// The MZGram items of one message's menu, the same in the main chat and in
// topics, replies and other sections: Repeat, Details, Save Message, Set a
// reminder, Forward without sender, Copy Photo, Delete Downloaded File,
// each behind its own switch, and Ask AI where copying is allowed.
void AddMessageMenuActions(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller,
	bool copyRestricted);

} // namespace MZGram
