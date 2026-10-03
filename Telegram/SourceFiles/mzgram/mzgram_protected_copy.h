/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class ApiWrap;

namespace Api {
struct SendAction;
} // namespace Api

namespace Data {
struct ResolvedForwardDraft;
} // namespace Data

namespace MZGram {

// ApiWrap::forwardMessages, before it forwards: while "Forward and save
// protected content" is on (mzgram_protected_content.h), sends the
// draft's protected messages as copies and leaves the others in the draft,
// to be forwarded as usual. With the switch off the draft is not touched.
void SendProtectedCopies(
	not_null<ApiWrap*> api,
	Data::ResolvedForwardDraft &draft,
	const Api::SendAction &action);

} // namespace MZGram
