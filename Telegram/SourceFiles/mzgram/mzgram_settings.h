/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "settings/settings_type.h"

namespace Settings {

// The MZGram section of the main settings: every fork feature in one place,
// so none of them hides behind Advanced > Experimental.
[[nodiscard]] Type MZGramId();

} // namespace Settings
