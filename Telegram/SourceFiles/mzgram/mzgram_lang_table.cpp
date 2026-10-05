/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_lang.h"

#include <cstring>
#include <map>
#include <string>

namespace MZGram {
namespace {

// Every MZGram text shown to the user, in English and Ukrainian.
// Placeholders are Qt's %1, %2.
const std::vector<Phrase> kPhrases = {
	// Settings > MZGram: sections.
	{ "settings_title", "MZGram", "MZGram" },
	{ "section_archive", "Archive", "Архів" },
	{ "section_privacy", "Privacy", "Приватність" },
	{ "section_ghost_mode", "Ghost Mode", "Режим привида" },
	{ "section_message_menu", "Message menu", "Меню повідомлення" },
	{ "section_media_calls", "Media and calls", "Медіа і дзвінки" },
	{ "section_interface", "Interface", "Інтерфейс" },
	{ "section_ads_filters", "Ads and filters", "Реклама і фільтри" },
	{ "settings_topics_info",
		"Every MZGram feature is on one of these pages.",
		"Кожна функція MZGram є на одній із цих сторінок." },

	// Archive.
	{ "save_deleted_messages",
		"Save deleted messages",
		"Зберігати видалені повідомлення" },
	{ "save_edit_history", "Save edit history", "Зберігати історію редагувань" },
	{ "save_archive_media", "Save media", "Зберігати медіа" },
	{ "save_formatting", "Save formatting", "Зберігати форматування" },
	{ "save_reactions", "Save reactions", "Зберігати реакції" },
	{ "save_for_bots", "Save in chats with bots", "Зберігати в чатах з ботами" },
	{ "deleted_mark_text", "Deleted mark", "Позначка видаленого" },
	{ "edited_mark_text", "Edited mark", "Позначка редагованого" },
	{ "semi_transparent_deleted",
		"Semi-transparent deleted messages",
		"Напівпрозорі видалені повідомлення" },
	{ "archive_mark_info",
		"Shown before the time of another person's message. Any text; "
		"leave empty for none.",
		"Показується перед часом чужого повідомлення. Будь-який текст; "
		"порожньо, щоб не показувати." },
	{ "archive_look_info",
		"The marks are shown before the time of another person's deleted "
		"or edited message. Deleted messages kept in the chat are drawn at "
		"75% opacity.",
		"Позначки показуються перед часом чужого видаленого чи "
		"редагованого повідомлення. Видалені повідомлення, що лишаються в "
		"чаті, малюються з непрозорістю 75%." },
	{ "archive_info",
		"In every private chat, group, channel and secret chat, other "
		"people's deleted messages, earlier versions of their edited "
		"messages and their view-once media are kept. Your own messages "
		"are not. Everything is saved on this device, in an unencrypted "
		"database, and stays after a restart.",
		"У всіх приватних чатах, групах, каналах і секретних чатах "
		"зберігаються чужі видалені повідомлення, попередні версії їхніх "
		"відредагованих повідомлень і їхні одноразові медіа. Ваші власні "
		"повідомлення не зберігаються. Усе зберігається на цьому пристрої "
		"в незашифрованій базі і лишається після перезапуску." },
	{ "saved_media_note",
		"Other people's photos, voice messages, round videos, view-once "
		"media, videos and files are saved to Downloads/MZGram/Saved "
		"Attachments as they arrive, of any size. There is no total quota, "
		"and saved files are never deleted on their own. Formatting and "
		"reactions are kept with the text; chats with bots are saved only "
		"while their switch is on.",
		"Чужі фото, голосові, відеоповідомлення, одноразові медіа, відео і "
		"файли зберігаються в Downloads/MZGram/Saved Attachments, щойно "
		"надходять, будь-якого розміру. Загальної квоти немає, і збережені "
		"файли ніколи не видаляються самі. Форматування і реакції "
		"зберігаються разом з текстом; чати з ботами зберігаються, лише поки "
		"їхній перемикач увімкнений." },
	{ "export_archive", "Export archive", "Експортувати архів" },
	{ "import_archive", "Import archive", "Імпортувати архів" },
	{ "export_archive_title", "Export MZGram archive", "Експорт архіву MZGram" },
	{ "import_archive_title", "Import MZGram archive", "Імпорт архіву MZGram" },
	{ "sqlite_database_filter", "SQLite database (*.db)", "База даних SQLite (*.db)" },
	{ "archive_exported", "Archive exported.", "Архів експортовано." },
	{ "archive_export_failed", "Could not export the archive.", "Не вдалося експортувати архів." },
	{ "archive_imported", "Archive imported.", "Архів імпортовано." },
	{ "archive_import_failed", "Could not import the archive.", "Не вдалося імпортувати архів." },
	{ "import_archive_confirm",
		"This replaces the current local archive with a previously "
		"exported file. This cannot be undone. Continue?",
		"Це замінить поточний локальний архів раніше експортованим файлом. "
		"Цю дію не можна скасувати. Продовжити?" },
	{ "import", "Import", "Імпортувати" },
	{ "export_import_info",
		"Export saves the archive database as a file you choose where to "
		"put. Import replaces the current archive with a previously "
		"exported file (media files are not included in either).",
		"Експорт зберігає базу архіву файлом у вибране вами місце. Імпорт "
		"замінює поточний архів раніше експортованим файлом (медіафайли не "
		"входять ні в те, ні в інше)." },
	{ "clear_archive", "Clear archive", "Очистити архів" },
	{ "clear_archive_confirm",
		"This permanently deletes the whole local archive of deleted and "
		"edited messages, including saved media. This cannot be undone. "
		"Continue?",
		"Це назавжди видалить увесь локальний архів видалених і змінених "
		"повідомлень разом зі збереженими медіа. Цю дію не можна скасувати. "
		"Продовжити?" },
	{ "clear_archive_info",
		"Deletes every archived message and saved media file in every "
		"chat. Does not change the settings above.",
		"Видаляє всі збережені повідомлення і медіафайли в усіх чатах. "
		"Не змінює налаштування вище." },

	// Privacy.
	{ "hide_own_online", "Hide own online status", "Приховати свій статус у мережі" },
	{ "hide_own_online_info",
		"Always reports offline to the server, independent of Ghost mode.",
		"Завжди повідомляє серверу, що ви не в мережі, незалежно від "
		"режиму привида." },

	// Ghost Mode.
	{ "ghost_mode", "Ghost mode", "Режим привида" },
	{ "send_read_receipts", "Send read receipts", "Надсилати позначки про прочитання" },
	{ "send_typing", "Send typing status", "Надсилати статус набору" },
	{ "send_online", "Send online status", "Надсилати статус у мережі" },
	{ "ghost_mode_info",
		"Ghost mode hides your read receipts, typing and online status. "
		"The switches above let some of them through.",
		"Режим привида приховує ваші позначки про прочитання, набір тексту "
		"і статус у мережі. Перемикачі вище пропускають частину з них." },
	{ "delay_sending", "Delay sending messages", "Затримувати надсилання повідомлень" },
	{ "delay_sending_info",
		"Holds an outgoing message (about 12 seconds, longer for "
		"photos/videos/files) before actually sending it, so composing and "
		"sending right away does not make you look online. Not recommended "
		"on an unreliable connection: a delayed message can still be "
		"waiting to send if the app closes or the network drops in the "
		"meantime.",
		"Тримає вихідне повідомлення (близько 12 секунд, довше для фото, "
		"відео і файлів), перш ніж справді його надіслати, щоб написання і "
		"надсилання не показували вас у мережі. Не радимо при нестабільному "
		"зʼєднанні: затримане повідомлення може лишитися ненадісланим, якщо "
		"застосунок закриється або зникне мережа." },
	{ "send_without_sound", "Send without sound", "Надсилати без звуку" },
	{ "send_without_sound_info",
		"Sends every outgoing message without a notification sound for the "
		"recipient, regardless of the per-message \"send without sound\" "
		"choice.",
		"Надсилає кожне вихідне повідомлення без звуку сповіщення для "
		"одержувача, незалежно від вибору «Надіслати без звуку» для "
		"окремого повідомлення." },
	{ "offer_ghost_stories", "Offer ghost mode before Stories", "Пропонувати режим привида перед історіями" },
	{ "offer_ghost_stories_info",
		"Before opening a story for the first time (not when swiping to the "
		"next one), asks whether to turn ghost mode on first, so viewing it "
		"does not mark it as seen.",
		"Перед першим відкриттям історії (не під час гортання до "
		"наступної) питає, чи увімкнути режим привида, щоб перегляд не "
		"позначав її переглянутою." },
	{ "ghost_stories_question",
		"Ghost mode is off. Viewing this story will mark it as seen. Turn "
		"ghost mode on first?",
		"Режим привида вимкнено. Перегляд цієї історії позначить її "
		"переглянутою. Спершу увімкнути режим привида?" },
	{ "enable_and_view", "Enable and view", "Увімкнути і переглянути" },
	{ "view_anyway", "View anyway", "Все одно переглянути" },

	// Message menu.
	{ "show_repeat", "Show \"Repeat\" in the message menu", "Показувати «Повторити» в меню повідомлення" },
	{ "show_details", "Show \"Message details\" in the message menu", "Показувати «Деталі повідомлення» в меню повідомлення" },
	{ "show_scan_qr", "Show \"Scan for QR code\" on photos", "Показувати «Сканувати QR-код» на фото" },
	{ "show_save_message", "Show \"Save message\" in the message menu", "Показувати «Зберегти повідомлення» в меню повідомлення" },
	{ "show_set_reminder", "Show \"Set a reminder\" in the message menu", "Показувати «Встановити нагадування» в меню повідомлення" },
	{ "show_open_in", "Show \"Open in...\" for downloaded files", "Показувати «Відкрити в…» для завантажених файлів" },
	{ "save_protected_content", "Forward and save protected content", "Пересилати і зберігати захищений вміст" },
	{ "save_protected_content_info",
		"In chats and channels whose owner restricted saving content, you "
		"can forward, save and copy messages and media and take "
		"screenshots, like anywhere else. Telegram does not forward such "
		"messages, so they are sent as new messages without the original "
		"sender; media not downloaded yet is downloaded first.",
		"У чатах і каналах, де власник обмежив збереження вмісту, ви "
		"можете пересилати, зберігати й копіювати повідомлення і медіа та "
		"робити скріншоти, як і деінде. Telegram не пересилає такі "
		"повідомлення, тож вони надсилаються як нові, без початкового "
		"автора; ще не завантажене медіа спершу завантажується." },
	{ "message_menu_info",
		"Repeat sends the same message to this chat again. Message details "
		"shows its id, dates, views and file. Scan for QR code decodes an "
		"already-downloaded photo entirely offline. Save message forwards "
		"to Saved Messages in one click, Set a reminder forwards there "
		"scheduled for a chosen time, and Open in... shows the system "
		"\"Open with\" dialog for a downloaded file.",
		"«Повторити» знову надсилає те саме повідомлення в цей чат. "
		"«Деталі повідомлення» показують його ідентифікатор, дати, "
		"перегляди і файл. «Сканувати QR-код» розпізнає вже завантажене "
		"фото повністю офлайн. «Зберегти повідомлення» одним кліком "
		"пересилає його в Збережене, «Встановити нагадування» пересилає "
		"туди як заплановане на обраний час, а «Відкрити в…» показує "
		"системне вікно «Відкрити за допомогою» для завантаженого файлу." },
	{ "repeat", "Repeat", "Повторити" },
	{ "message_details", "Message details", "Деталі повідомлення" },
	{ "details_id", "ID", "ID" },
	{ "details_date", "Date", "Дата" },
	{ "details_edited", "Edited", "Змінено" },
	{ "details_originally_sent", "Originally sent", "Спершу надіслано" },
	{ "details_views", "Views", "Перегляди" },
	{ "details_file_size", "File size", "Розмір файлу" },
	{ "details_mime_type", "MIME type", "MIME-тип" },
	{ "details_file_name", "File name", "Назва файлу" },
	{ "details_resolution", "Resolution", "Роздільна здатність" },
	{ "save_message", "Save message", "Зберегти повідомлення" },
	{ "set_reminder", "Set a reminder", "Встановити нагадування" },
	{ "open_in", "Open in...", "Відкрити в…" },
	{ "scan_qr", "Scan for QR code", "Сканувати QR-код" },
	{ "qr_code", "QR code", "QR-код" },
	{ "copy", "Copy", "Копіювати" },
	{ "no_qr_code", "No QR code was found in this photo.", "На цьому фото немає QR-коду." },

	// Media and calls.
	{ "media_preview", "Media preview instead of Chat Preview", "Перегляд медіа замість попереднього перегляду чату" },
	{ "media_preview_info",
		"Press-and-hold a chat's avatar normally opens the Chat Preview "
		"peek. When the last message is a photo or video, this shows it "
		"directly instead.",
		"Утримання аватара чату зазвичай відкриває попередній перегляд "
		"чату. Якщо останнє повідомлення — фото або відео, натомість "
		"показується саме воно." },
	{ "auto_pause_video", "Auto pause video", "Автопауза відео" },
	{ "confirm_voice", "Confirm before sending voice messages", "Підтверджувати надсилання голосових" },
	{ "confirm_round", "Confirm before sending round videos", "Підтверджувати надсилання відеоповідомлень" },
	{ "media_calls_info",
		"Pauses the video viewer when the window is minimized or loses "
		"focus. Voice messages and round videos ask for confirmation before "
		"they are sent.",
		"Ставить переглядач відео на паузу, коли вікно згорнуто або воно "
		"втрачає фокус. Голосові та відеоповідомлення перед надсиланням "
		"просять підтвердження." },
	{ "send_video_message_question", "Send this video message?", "Надіслати це відеоповідомлення?" },
	{ "send_voice_message_question", "Send this voice message?", "Надіслати це голосове повідомлення?" },

	// Interface.
	{ "message_seconds", "Show seconds in message time", "Показувати секунди в часі повідомлень" },
	{ "hide_stories", "Hide Stories", "Приховати історії" },
	{ "disable_greeting_sticker", "Disable greeting sticker", "Вимкнути вітальний стікер" },
	{ "hide_channel_button", "Hide channel bottom button", "Приховати нижню кнопку каналу" },
	{ "interface_info",
		"Message times with seconds; no stories row in the chat list; an "
		"empty chat without the suggested greeting sticker; no bottom bar "
		"with the mute button in channels where you cannot post.",
		"Час повідомлень із секундами; без рядка історій у списку чатів; "
		"порожній чат без запропонованого вітального стікера; без нижньої "
		"панелі з кнопкою вимкнення звуку в каналах, де ви не можете "
		"писати." },
	{ "open_archive_on_pull", "Open Archive on pull down", "Відкривати архів потягуванням униз" },
	{ "open_archive_on_pull_info",
		"Pulling the chat list down past the Archive row opens it.",
		"Якщо потягнути список чатів униз далі за рядок архіву, він "
		"відкривається." },

	// Ads and filters.
	{ "disable_sponsored", "Disable sponsored messages", "Вимкнути рекламні повідомлення" },
	{ "disable_sponsored_info",
		"Stops sponsored (ad) messages in channels from being requested or "
		"shown.",
		"Рекламні повідомлення в каналах більше не запитуються і не "
		"показуються." },
	{ "zalgo_filter", "Zalgo filter", "Фільтр Zalgo" },
	{ "zalgo_filter_info",
		"Removes stacked Unicode combining marks (Zalgo-style corrupted "
		"text) from names and chat titles shown to you.",
		"Прибирає нагромаджені комбіновані знаки Unicode (зіпсований текст "
		"у стилі Zalgo) з імен і назв чатів, які ви бачите." },

	// Chat menu.

	// Messages.
	{ "edit_history", "Edit history", "Історія змін" },
	{ "no_earlier_versions",
		"No earlier versions were recorded for this message.",
		"Для цього повідомлення не збережено попередніх версій." },
	{ "replaced_at", "Replaced %1", "Замінено %1" },
	{ "empty_text", "(empty)", "(порожньо)" },
	{ "current_version", "Current", "Поточна" },
};

const std::map<std::string, const Phrase*> &Index() {
	static const auto result = [] {
		auto map = std::map<std::string, const Phrase*>();
		for (const auto &phrase : kPhrases) {
			map.emplace(phrase.key, &phrase);
		}
		return map;
	}();
	return result;
}

} // namespace

const std::vector<Phrase> &Phrases() {
	return kPhrases;
}

QString Translate(const char *key, bool ukrainian) {
	const auto &index = Index();
	const auto i = index.find(key);
	if (i == end(index)) {
		return QString::fromUtf8(key);
	}
	return QString::fromUtf8(ukrainian ? i->second->ukrainian : i->second->english);
}

} // namespace MZGram
