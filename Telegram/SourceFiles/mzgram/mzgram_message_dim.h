/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class Painter;

namespace Ui {
struct ChatPaintContext;
} // namespace Ui

namespace HistoryView {
class Element;
} // namespace HistoryView

namespace MZGram {

// Draws a message view, semi-transparent when it is a kept deleted message.
// Stands in for view->draw(p, context) in the chat lists.
void DrawMessage(
	Painter &p,
	not_null<const HistoryView::Element*> view,
	const Ui::ChatPaintContext &context);

} // namespace MZGram
