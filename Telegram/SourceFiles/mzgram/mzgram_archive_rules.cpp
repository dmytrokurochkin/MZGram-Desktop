/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_archive_rules.h"

#include "base/options.h"

namespace MZGram {
namespace {

base::options::toggle OptionSaveDeletedAndEdited({
	.id = kOptionSaveDeletedAndEdited,
	.name = "Save deleted and edited messages",
	.description = "Keep messages others delete, earlier versions of edited "
		"messages and view-once media, in every chat.",
	.defaultValue = true,
});

} // namespace

const char kOptionSaveDeletedAndEdited[] = "mzgram-save-deleted-and-edited";

bool SaveDeletedAndEdited() {
	return OptionSaveDeletedAndEdited.value();
}

bool KeepsMessage(bool service, bool own) {
	return SaveDeletedAndEdited() && !service && !own;
}

} // namespace MZGram
