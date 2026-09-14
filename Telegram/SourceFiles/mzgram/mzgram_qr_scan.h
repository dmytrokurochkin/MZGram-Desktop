/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class PhotoData;
class HistoryItem;

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace MZGram {

// MZGram's own code, mirroring the QR-code scanning feature already
// present in Telegram for Android (which uses zxing there). Decodes a QR
// code found in a photo that is already downloaded to this device, fully
// offline, using the bundled quirc library. Not present in AyuGram
// Desktop, which has no equivalent feature.
void AddScanQrCodeAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<PhotoData*> photo,
	HistoryItem *item,
	not_null<Window::SessionController*> controller);

} // namespace MZGram
