/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mzgram/mzgram_branding_rules.h"

namespace MZGram::Branding {

// "1.0.0" for a release build, "Telegram Desktop 7.2.8" when the build has
// no MZGram version (a local build).
[[nodiscard]] QString CurrentVersionText();

// The Telegram Desktop version MZGram is based on, "7.2.8".
[[nodiscard]] QString BaseVersionText();

// Letters Open Sans and Telegram's substitutes lack, runes (U+16A0-16FF)
// for one, from Windows' own Segoe UI Historic instead of squares. Only
// for the letters no font before it has, so other text stays as it is.
void ApplyFontFallbacks();

} // namespace MZGram::Branding
