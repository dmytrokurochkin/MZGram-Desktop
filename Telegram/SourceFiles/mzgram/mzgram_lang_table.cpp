/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_lang.h"

#include "base/basic_types.h"
#include "base/options.h"

#include <cstring>
#include <map>
#include <string>

namespace MZGram {
namespace {

// Every MZGram text shown to the user, in English and Ukrainian; the
// other languages are in mzgram_lang_translations.cpp.
// Placeholders are Qt's %1, %2.
const std::vector<Phrase> kPhrases = {
	// Settings > MZGram: sections.
	{ "settings_title", "MZGram", "MZGram" },
	{ "section_archive", "Archive", "Архів" },
	{ "section_ghost_mode", "Ghost Mode", "Режим привида" },
	{ "section_message_menu", "Message menu", "Меню повідомлення" },
	{ "section_media_calls", "Media and calls", "Медіа і дзвінки" },
	{ "section_interface", "Interface", "Інтерфейс" },
	{ "section_ads_filters", "Ads and filters", "Реклама і фільтри" },

	// Archive.
	{ "save_deleted_messages",
		"Save deleted messages",
		"Зберігати видалені повідомлення" },
	{ "save_edit_history", "Save edit history", "Зберігати історію редагувань" },
	{ "save_archive_media", "Save media", "Зберігати медіа" },
	{ "save_formatting", "Save formatting", "Зберігати форматування" },
	{ "save_reactions", "Save reactions", "Зберігати реакції" },
	{ "save_for_bots", "Save in chats with bots", "Зберігати в чатах з ботами" },
	{ "keep_from_title", "Keep messages", "Зберігати повідомлення" },
	{ "keep_from_users", "From users", "Від користувачів" },
	{ "keep_from_bots", "From bots", "Від ботів" },
	{ "keep_from_groups", "In groups", "У групах" },
	{ "keep_from_channels", "In channels", "У каналах" },
	{ "keep_from_info", "A bot's messages follow From bots in every chat; other messages follow the kind of chat: a private chat, a group or a channel. Turning one off stops new saves; what is already kept stays.", "Повідомлення бота в будь-якому чаті йдуть за «Від ботів»; інші за типом чату: особистий чат, група чи канал. Вимкнення зупиняє нове збереження; уже збережене лишається." },
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
	{ "erase_local_database",
		"Clear Telegram local database",
		"Стерти локальну базу Telegram" },
	{ "erase_local_database_button", "Clear and restart", "Стерти й перезапустити" },
	{ "erase_local_database_info",
		"Clears Telegram's cache on this device and restarts the app; "
		"messages and chats load again from the server. The MZGram archive "
		"is not cleared.",
		"Стирає кеш Telegram на цьому пристрої і перезапускає застосунок; "
		"повідомлення й чати знову завантажаться з сервера. Архів MZGram не "
		"стирається." },
	{ "erase_local_database_confirm",
		"This clears Telegram's local cache on this device and restarts the "
		"app; messages and chats then load again from the server. The MZGram "
		"archive (kept deleted and edited messages, and the Saved Attachments "
		"folder) is NOT cleared; use \"Clear archive\" for that. A message "
		"deleted on the server while the app restarts is not kept.",
		"Буде стерто локальний кеш Telegram на цьому пристрої і "
		"перезапущено застосунок; повідомлення й чати потім знову "
		"завантажаться з сервера. Архів MZGram (збережені видалені й "
		"редаговані повідомлення і тека Saved Attachments) НЕ стирається; "
		"для нього є \"Очистити архів\". Повідомлення, видалене на сервері, "
		"поки застосунок перезапускається, не збережеться." },
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
	{ "archive_cleared", "Archive cleared.", "Архів очищено." },
	{ "archive_files_deleted", "Archive files deleted, %1 freed.", "Файли архіву видалено, звільнено %1." },
	{ "archive_files_left", "Some archive files could not be deleted. MZGram will try again at the next start.", "Деякі файли архіву не вдалося видалити. MZGram спробує ще раз під час наступного запуску." },
	{ "archive_size", "Archive size", "Розмір архіву" },
	{ "archive_size_counting", "Counting…", "Рахую…" },
	{ "archive_size_database", "Messages", "Повідомлення" },
	{ "archive_size_media", "Media", "Медіа" },
	{ "archive_size_attachments", "Saved Attachments", "Збережені вкладення" },
	{ "archive_size_other", "Other", "Інше" },

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
	{ "details_read", "Read", "Прочитано" },
	{ "details_read_local",
		"%1 (when this device learned of it)",
		"%1 (коли про це дізнався цей пристрій)" },
	{ "details_read_unknown", "time unknown", "час невідомий" },
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
		"чату. Якщо останнє повідомлення це фото або відео, натомість "
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
	{ "mzgram_language", "MZGram language", "Мова MZGram" },
	{ "mzgram_language_system", "System", "Як у системі" },
	{ "mzgram_language_info", "The language of MZGram's own settings and features. Same as Telegram: Telegram's language when MZGram has it, otherwise English. System: the system's language when MZGram has it, otherwise English.", "Мова власних налаштувань і функцій MZGram. Як у Telegram: мова Telegram, якщо MZGram її має, інакше англійська. Як у системі: мова системи, якщо MZGram її має, інакше англійська." },
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
	{ "check_updates", "Check for MZGram updates", "Перевіряти оновлення MZGram" },
	{ "check_updates_info",
		"Every few hours MZGram looks for its latest stable release on GitHub "
		"and shows a bar at the bottom of the chat list when it is newer. "
		"Update downloads that release from GitHub, checks its SHA-256 and "
		"installs it on restart, keeping the old files until the new ones "
		"are in place. Check for updates looks at once and says what it "
		"found, also when this is off. The updater of Telegram Desktop is "
		"off: it installs the official app.",
		"Раз на кілька годин MZGram шукає свій останній стабільний реліз на "
		"GitHub і показує плашку внизу списку чатів, якщо він новіший. Оновити "
		"завантажує цей реліз із GitHub, перевіряє його SHA-256 і встановлює "
		"під час перезапуску, зберігаючи старі файли, доки нові не стануть на "
		"місце. Перевірити оновлення шукає одразу і каже, що знайшло, навіть "
		"коли це вимкнено. Оновлювач Telegram Desktop вимкнений: він "
		"встановлює офіційний застосунок." },
	{ "check_updates_now", "Check for updates", "Перевірити оновлення" },
	{ "update_check_latest", "You have the latest version of MZGram", "У вас остання версія MZGram" },
	{ "update_check_available", "MZGram %1 is available", "Доступна версія MZGram %1" },
	{ "update_check_failed",
		"Could not check for updates. Check the connection and try again.",
		"Не вдалося перевірити оновлення. Перевірте з'єднання і спробуйте ще "
		"раз." },
	{ "update_available", "MZGram %1 update available", "Доступне оновлення MZGram %1" },
	{ "update_open", "Update", "Оновити" },
	{ "update_close", "Close", "Закрити" },
	{ "update_retry", "Retry", "Повторити" },
	{ "update_page", "Release page", "Сторінка релізу" },
	{ "update_downloading",
		"Downloading MZGram %1: %2%",
		"Завантаження MZGram %1: %2%" },
	{ "update_verifying",
		"Checking the SHA-256 of MZGram %1...",
		"Перевірка SHA-256 для MZGram %1..." },
	{ "update_installing",
		"Restarting to install MZGram %1...",
		"Перезапуск для встановлення MZGram %1..." },
	{ "update_failed_network",
		"MZGram %1 could not be downloaded. Check the connection and try "
		"again.",
		"Не вдалося завантажити MZGram %1. Перевірте з’єднання і спробуйте "
		"ще раз." },
	{ "update_failed_server",
		"GitHub did not give MZGram %1 right now, often because of its "
		"hourly limit. Try again later or use the release page.",
		"GitHub зараз не віддав MZGram %1, часто через погодинне "
		"обмеження. Спробуйте пізніше або скористайтеся сторінкою "
		"релізу." },
	{ "update_failed_address",
		"MZGram %1 was not downloaded: GitHub sent it from an address "
		"MZGram does not trust. Nothing was installed.",
		"MZGram %1 не завантажено: GitHub віддав його з адреси, якій "
		"MZGram не довіряє. Нічого не встановлено." },
	{ "update_failed_release",
		"The MZGram %1 release on GitHub is not the one that was found. "
		"Check for updates again.",
		"Реліз MZGram %1 на GitHub не той, що був знайдений. Перевірте "
		"оновлення ще раз." },
	{ "update_failed_disk",
		"MZGram %1 could not be saved: the disk may be full or the "
		"update folder cannot be written. Nothing was installed.",
		"Не вдалося зберегти MZGram %1: можливо, диск заповнений або в "
		"папку оновлень не можна записувати. Нічого не встановлено." },
	{ "update_failed_checksum",
		"The MZGram %1 archive did not pass the SHA-256 check and was "
		"deleted. Nothing was installed.",
		"Архів MZGram %1 не пройшов перевірку SHA-256 і був видалений. "
		"Нічого не встановлено." },
	{ "update_failed_unpack",
		"The MZGram %1 archive could not be prepared for installing. "
		"Nothing was installed.",
		"Архів MZGram %1 не вдалося підготувати до встановлення. Нічого не "
		"встановлено." },
	{ "update_not_writable",
		"MZGram cannot update itself here: its folder cannot be written. "
		"MZGram %1 can be installed from the release page.",
		"MZGram не може оновитися тут: у його папку не можна записувати. "
		"MZGram %1 можна встановити зі сторінки релізу." },
	{ "update_rolled_back",
		"MZGram %1 could not be installed, so the previous version was "
		"put back and kept. It can be installed from the release page.",
		"Не вдалося встановити MZGram %1, тому попередню версію повернуто "
		"і збережено. Нову можна встановити зі сторінки релізу." },
	{ "pin_locally", "Pin in MZGram only", "Закріпити лише в MZGram" },
	{ "unpin_locally", "Unpin from MZGram", "Відкріпити з MZGram" },
	{ "pinned_locally",
		"Pinned in MZGram only: the server allows no more pinned chats, so "
		"this one is pinned on this device and not on your other devices.",
		"Закріплено лише в MZGram: сервер не дозволяє більше закріплених "
		"чатів, тому цей закріплено на цьому пристрої, а на інших ваших "
		"пристроях ні." },

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
	// Buttons of the boxes MZGram shows (not Telegram's, so they follow the MZGram language).
	{ "button_cancel", "Cancel", "Скасувати" },
	{ "button_ok", "OK", "Гаразд" },
	{ "button_save", "Save", "Зберегти" },
	{ "button_delete", "Delete", "Видалити" },
	{ "button_close", "Close", "Закрити" },
	{ "button_send", "Send", "Надіслати" },
	// Branding (mzgram_branding.cpp, about_box.cpp)
	{ "brand_about_text", "Unofficial messaging client based on Telegram Desktop {version}.", "Неофіційний клієнт для обміну повідомленнями на базі Telegram Desktop {version}." },
	{ "brand_intro_about", "Welcome to MZGram Desktop,\nan unofficial client for Telegram.", "Ласкаво просимо до MZGram Desktop,\nнеофіційного клієнта Telegram." },
	// Ask AI (mzgram_ai.cpp, Settings > MZGram > AI)
	{ "section_ai", "AI", "ШІ" },
	{ "ai_show_action", "Show \"Ask AI\" in the message menu", "Показувати «Запитати ШІ» в меню повідомлення" },
	{ "ai_default_provider", "Default AI", "ШІ за замовчуванням" },
	{ "ai_sign_out", "Sign out of AI sites", "Вийти з сайтів ШІ" },
	{ "ai_sign_out_confirm", "Sign out of every AI site in MZGram? Your Telegram accounts stay signed in.", "Вийти з усіх сайтів ШІ в MZGram? Ваші акаунти Telegram лишаться в системі." },
	{ "ai_signed_out", "Signed out of the AI sites.", "Ви вийшли з сайтів ШІ." },
	{ "ai_ask", "Ask AI", "Запитати ШІ" },
	{ "ai_message", "Message", "Повідомлення" },
	{ "ai_prompt", "Your prompt (optional)", "Ваш промпт (необов'язково)" },
	{ "ai_provider", "AI site", "Сайт ШІ" },
	{ "ai_open", "Open", "Відкрити" },
	{ "ai_privacy_title", "The text leaves Telegram", "Текст залишає Telegram" },
	{ "ai_continue", "Continue", "Продовжити" },
	{ "ai_status_opening", "Opening %1…", "Відкриваю %1…" },
	{ "ai_status_inserted", "The text is in the input field. Check it and press Send yourself.", "Текст у полі вводу. Перевірте його і натисніть «Надіслати» самі." },
	{ "ai_status_no_field", "No input field found. Sign in if the site asks, then press Insert text, or copy the text and paste it.", "Поле вводу не знайдено. Увійдіть, якщо сайт просить, і натисніть «Вставити текст» або скопіюйте текст і вставте його." },
	{ "ai_insert", "Insert text", "Вставити текст" },
	{ "ai_copy", "Copy text", "Копіювати текст" },
	{ "ai_copied", "Text copied.", "Текст скопійовано." },
	{ "ai_no_webview", "The built-in browser cannot start here. On Windows install Microsoft Edge WebView2 Runtime, on Linux WebKitGTK, then try again.", "Вбудований браузер тут не запускається. На Windows встановіть Microsoft Edge WebView2 Runtime, на Linux WebKitGTK, і спробуйте ще раз." },
	// The 1.0.9 batch: Ask AI attachments, hidden rows, AI menu buttons,
	// Other, the moved message menu items, the unblock button
	{ "mzgram_language_telegram", "Same as Telegram", "Як у Telegram" },
	{ "section_other", "Other", "Інше" },
	{ "hide_rows_title", "Hide in Telegram's Settings", "Приховати в налаштуваннях Telegram" },
	{ "hide_rows_info", "A hidden row is gone from Telegram's Settings and from its search; nothing else changes.", "Прихований рядок зникає з налаштувань Telegram і з пошуку в них; більше нічого не змінюється." },
	{ "hide_row_premium", "Telegram Premium", "Telegram Premium" },
	{ "hide_row_stars", "My Stars", "Мої зірки" },
	{ "hide_row_currency", "Gram Earnings", "Заробіток у Gram" },
	{ "hide_row_business", "Telegram Business", "Telegram Business" },
	{ "hide_row_send_gift", "Premium Gifting", "Подарувати Premium" },
	{ "hide_row_faq", "Telegram FAQ", "Часті запитання про Telegram" },
	{ "hide_row_features", "Telegram Features", "Можливості Telegram" },
	{ "hide_row_ask_question", "Ask a Question", "Поставити запитання" },
	{ "unblock_files", "Unblock MZGram files", "Розблокувати файли MZGram" },
	{ "unblock_files_confirm", "MZGram will remove the \"downloaded from the internet\" mark from the files in its folder, so Windows stops asking before they run. Windows protection stays on. Do this only if you downloaded MZGram from its GitHub page. Continue?", "MZGram зніме з файлів у своїй теці позначку «завантажено з інтернету», і Windows перестане питати перед їх запуском. Захист Windows лишається увімкненим. Робіть це, лише якщо завантажили MZGram з його сторінки на GitHub. Продовжити?" },
	{ "unblock_files_done", "Files unblocked: %1.", "Розблоковано файлів: %1." },
	{ "unblock_files_denied", "Windows did not allow it: MZGram's folder needs administrator rights. Run PowerShell as administrator and use Unblock-File on that folder.", "Windows не дозволив: для теки MZGram потрібні права адміністратора. Запустіть PowerShell від імені адміністратора і виконайте Unblock-File для цієї теки." },
	{ "unblock_files_info", "If Windows asks before starting MZGram (\"Windows protected your PC\"), this removes the \"downloaded from the internet\" mark from MZGram's own files. Windows protection stays on, and nothing happens until you press the button.", "Якщо Windows питає перед запуском MZGram («Windows protected your PC»), ця кнопка знімає позначку «завантажено з інтернету» з власних файлів MZGram. Захист Windows лишається увімкненим, і нічого не відбувається, поки ви не натиснете кнопку." },
	{ "show_copy_photo", "Copy Photo", "Копіювати фото" },
	{ "photo_copied", "Photo copied to clipboard", "Фото скопійовано в буфер обміну" },
	{ "show_delete_downloaded", "Delete Downloaded File", "Видалити завантажений файл" },
	{ "show_forward_no_quote", "Forward without sender", "Переслати без автора" },
	{ "message_menu_more_info", "Copy Photo copies a photo or a picture sent as a file; while it is on, it stands for Telegram's Copy Image. Delete Downloaded File removes a message's file from this computer; the message stays. Forward without sender opens the forward box with the sender's name hidden.", "Копіювати фото копіює фото або картинку, надіслану файлом; поки перемикач увімкнено, він заміняє «Копіювати зображення» Telegram. Видалити завантажений файл прибирає файл повідомлення з цього комп'ютера; повідомлення лишається. Переслати без автора відкриває вікно пересилання з уже прихованим іменем автора." },
	{ "downloaded_file_deleted", "The downloaded file is deleted.", "Завантажений файл видалено." },
	{ "delete_downloaded_confirm", "Delete the downloaded file from this computer? The message stays, and the file can be downloaded again.", "Видалити завантажений файл з цього комп'ютера? Повідомлення лишиться, а файл можна буде завантажити знову." },
	{ "delete_downloaded_confirm_path", "Delete %1 from this computer? The message stays, and the file can be downloaded again.", "Видалити %1 з цього комп'ютера? Повідомлення лишиться, а файл можна буде завантажити знову." },
	{ "ai_attachments", "Attachments (%1): %2", "Вкладення (%1): %2" },
	{ "ai_left_out", "Not attached: %1", "Не додано: %1" },
	{ "ai_left_over_limit", "over the limit of 10: %1", "понад ліміт у 10: %1" },
	{ "ai_left_too_large", "larger than 30 MB: %1", "більші за 30 МБ: %1" },
	{ "ai_left_archives", "archives: %1", "архіви: %1" },
	{ "ai_left_media", "video, GIFs, stickers, voice and music: %1", "відео, GIF, стікери, голосові й музика: %1" },
	{ "ai_left_other", "other file types: %1", "інші типи файлів: %1" },
	{ "ai_status_downloading", "Downloading attachments: %1 of %2…", "Завантажую вкладення: %1 з %2…" },
	{ "ai_status_attached", "Attachments added: %1. Check the message on the site and press Send yourself.", "Додано вкладень: %1. Перевірте повідомлення на сайті й натисніть «Надіслати» самі." },
	{ "ai_status_no_attach", "The site took no attachments: press Save files and add them on the site by hand.", "Сайт не прийняв вкладення: натисніть «Зберегти файли» і додайте їх на сайті вручну." },
	{ "ai_save_files", "Save files", "Зберегти файли" },
	{ "ai_files_saved", "Files saved to %1", "Файли збережено в %1" },
	{ "ask_selected", "Ask AI about the selected (%1)", "Запитати ШІ про виділені (%1)" },
	{ "ai_restricted", "This chat does not allow copying, so its messages are not sent to AI.", "У цьому чаті заборонено копіювання, тому його повідомлення не передаються ШІ." },
	{ "peer_id", "Show ID", "Показувати ID" },
	{ "peer_id_hidden", "Hidden", "Сховано" },
	{ "peer_id_telegram", "As in Telegram API", "Як у Telegram API" },
	{ "peer_id_bot", "As in Bot API (-100…)", "Як у Bot API (-100…)" },
	{ "peer_id_info", "The ID of a user, bot, group or channel in its profile. Bot API puts -100 before the ID of a channel or supergroup and - before the ID of a group. Click the ID to copy it.", "ID користувача, бота, групи чи каналу в їхньому профілі. Bot API ставить -100 перед ID каналу чи супергрупи і - перед ID групи. Клацніть ID, щоб скопіювати." },
	{ "peer_id_copied", "ID copied", "ID скопійовано" },
	{ "archive_limits", "Download in advance", "Завантажувати наперед" },
	{ "archive_limit_users", "Users", "Користувачі" },
	{ "archive_limit_bots", "Bots", "Боти" },
	{ "archive_limit_groups", "Groups", "Групи" },
	{ "archive_limit_channels", "Channels", "Канали" },
	{ "archive_limit_none", "No limit", "Без обмежень" },
	{ "archive_limits_info", "Files up to this size are downloaded as soon as a message comes. When a message is deleted, a file up to 100 MB larger is downloaded then, if the server still gives it. Files saved earlier stay.", "Файли до цього розміру завантажуються одразу, щойно приходить повідомлення. Коли повідомлення видаляють, тоді завантажується файл, більший на 100 МБ, якщо сервер ще його віддає. Збережені раніше файли лишаються." },
	{ "archive_counters", "Saved files", "Збережені файли" },
	{ "archive_counter_saved", "Saved: %1 in advance, %2 when deleted", "Збережено: %1 наперед, %2 при видаленні" },
	{ "archive_counter_failed", "Not downloaded: %1", "Не завантажено: %1" },
	{ "archive_counter_expired", "Link expired: %1", "Посилання застаріло: %1" },
	{ "archive_counters_reset", "Reset counters", "Скинути лічильники" },
	{ "archive_counters_info", "These numbers show how the limits work on this device. Link expired means the server no longer gives the file of a deleted message.", "Ці числа показують, як працюють ліміти на цьому пристрої. «Посилання застаріло» означає, що сервер уже не віддає файл видаленого повідомлення." },
	{ "ai_warning_files", "To add the photos and files of the messages, press the site's own button for adding files: MZGram then shows them to choose from.", "Щоб додати фото й файли з повідомлень, натисніть на сайті його кнопку додавання файлів: MZGram покаже їх для вибору." },
	{ "ai_files_title", "Files for %1", "Файли для %1" },
	{ "ai_files_hint", "Tick the files to give the site, at most 10 at once.", "Позначте файли, які віддати сайту, до 10 за раз." },
	{ "ai_files_add", "Add (%1)", "Додати (%1)" },
	{ "ai_files_other", "Choose another file on this computer", "Вибрати інший файл на комп'ютері" },
	{ "ai_files_photos_only", "The site takes only photos here.", "Тут сайт приймає лише фото." },
	{ "ai_files_downloading", "Downloading…", "Завантаження…" },
	{ "ai_status_press_add", "Files ready: %1. Press the site's button for adding files to choose them.", "Файли готові: %1. Натисніть на сайті кнопку додавання файлів, щоб вибрати їх." },
	{ "ai_status_one_by_one", "The site takes one file at a time: press its button for adding files again (%1 left).", "Сайт приймає по одному файлу: натисніть його кнопку додавання файлів ще раз (лишилося %1)." },
	{ "ai_menu_buttons", "Buttons in the main menu", "Кнопки в головному меню" },
	{ "ai_menu_buttons_info", "A button opens that AI site in MZGram's window, without a message.", "Кнопка відкриває цей сайт ШІ у вікні MZGram, без повідомлення." },
	{ "ai_model_on_site", "Model: chosen on the %1 site.", "Модель: обирається на сайті %1." },
	{ "ai_privacy_text", "The text, the attachments and your prompt will go into the chat of %1, a service outside Telegram and MZGram, under the account you sign in with there. The site uploads attachments to its servers as soon as they are added, before you press Send; the text is sent only when you press Send. This warning is shown once.", "Текст, вкладення і ваш промпт потраплять у чат %1, сервісу поза Telegram і MZGram, під акаунтом, з яким ви там увійдете. Сайт вивантажує вкладення на свої сервери одразу, щойно їх додано, ще до натискання «Надіслати»; текст надсилається, лише коли ви натиснете «Надіслати». Це попередження показується один раз." },
	{ "ai_info", "Ask AI opens the chosen AI site in an MZGram window, with a sign-in of its own apart from Telegram's. No paid API is used: you sign in to the site with your own account. The text, your prompt and up to 10 photos and files go into the site's message, and you send it there yourself. Messages in chats that restrict copying have no Ask AI.", "«Запитати ШІ» відкриває вибраний сайт ШІ у вікні MZGram з окремим входом, не пов'язаним із Telegram. Платний API не використовується: ви входите на сайт зі своїм акаунтом. Текст, ваш промпт і до 10 фото та файлів потрапляють у повідомлення на сайті, і ви надсилаєте його там самі. У чатах, що забороняють копіювання, «Запитати ШІ» немає." },
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

const char kOptionLanguage[] = "mzgram-language";

base::options::option<QString> OptionLanguage({
	.id = kOptionLanguage,
	.name = "MZGram language",
	.description = "The language of MZGram's own settings and features: "
		"empty for Telegram's language, system for the system's, or en, uk, "
		"ru, pl, es, de, fr, pt-BR, tr, ar, be, kk",
	.defaultValue = QString(),
});

QString LanguageChoice() {
	const auto value = OptionLanguage.value();
	if (value == QString::fromLatin1(kLanguageSystem)) {
		return value;
	}
	for (const auto code : kLanguages) {
		if (value == QString::fromLatin1(code)) {
			return value;
		}
	}
	return QString();
}

QString ResolveLanguage(
		const QString &choice,
		const QString &telegramLanguage,
		const QString &systemLocale) {
	for (const auto code : kLanguages) {
		if (choice == QString::fromLatin1(code)) {
			return choice;
		}
	}
	return SupportedLanguage((choice == QString::fromLatin1(kLanguageSystem))
		? systemLocale
		: telegramLanguage);
}

QString SupportedLanguage(const QString &localeName) {
	const auto language = localeName.section(QChar('_'), 0, 0)
		.section(QChar('-'), 0, 0)
		.toLower();
	if (language == u"pt"_q) {
		// Brazilian is MZGram's only Portuguese
		return u"pt-BR"_q;
	}
	for (const auto code : kLanguages) {
		if (language == QString::fromLatin1(code)) {
			return language;
		}
	}
	return u"en"_q;
}

QString TranslateTo(const char *key, const QString &language) {
	if (language == u"uk"_q) {
		return Translate(key, true);
	}
	// Asked while painting: an index per language, made once.
	static auto indexes = std::map<QString, std::map<std::string, const char*>>();
	auto i = indexes.find(language);
	if (i == end(indexes)) {
		auto index = std::map<std::string, const char*>();
		for (const auto &translation : Translations(language)) {
			index.emplace(translation.key, translation.text);
		}
		i = indexes.emplace(language, std::move(index)).first;
	}
	const auto j = i->second.find(key);
	return (j != end(i->second))
		? QString::fromUtf8(j->second)
		: Translate(key, false);
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
