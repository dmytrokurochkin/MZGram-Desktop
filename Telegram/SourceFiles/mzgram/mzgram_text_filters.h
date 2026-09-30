/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>

namespace MZGram {

// MZGram's own code. Strips Zalgo-style combining-mark text corruption
// from display names and message text shown to the user. Kept in its own
// translation unit, deliberately free of session/history/network/UI
// dependencies (only base::options and QString -- not even lib_ui, which
// linking in pulls in unrelated code, e.g. Ui::Animations::Manager,
// requiring a full app's crl/Qt event loop integration to link at all), so
// the pure text-transform logic can be exercised by a headless unit test.
extern const char kOptionStripZalgoText[];
[[nodiscard]] bool StripZalgoText();
[[nodiscard]] QString StripZalgo(const QString &text);

} // namespace MZGram
