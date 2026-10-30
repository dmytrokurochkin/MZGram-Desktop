# MZGram: передача роботи наступній сесії

Стан на 2026-10-30. Тут лише технічні факти. Для секретів, токенів і ключів названо тільки імена змінних. Повна історія рішень і прогонів CI лежить у `handoff/mzgram-bug-spec.md` (див. розділ ж).

## а) Репозиторії, гілки, бази, версії

| | Desktop | Android |
|---|---|---|
| Репозиторій | `dmytrokurochkin/MZGram-Desktop` | `dmytrokurochkin/MZGram-Android` |
| Робоча гілка | `mzgram` | `mzgram` |
| HEAD `mzgram` | `a5b1864afe` | `088bc6db7` |
| База апстріму | Telegram Desktop 7.3.1 (`v7.3.1` = `6fed91ffab`) | Telegram for Android 13.0.3 (DrKLO/Telegram `0aae95980`, versionCode 7183) |
| Останній реліз | `v1.0.7` (2026-10-28, тег-об'єкт `0b2ee190b1` -> `a5b1864afe`) | `v1.0.8` (2026-10-28, тег-об'єкт `67112a68e5` -> `088bc6db7`) |
| Теги MZGram на GitHub | v1.0.0, v1.0.1, v1.0.2, v1.0.3, v1.0.4, v1.0.6, v1.0.7 (1.0.5 не випускався, його виправлення в 1.0.6) | v1.0.0 ... v1.0.8, усі стабільні |
| Наступна версія | 1.0.8 | 1.0.9 |

Інші гілки на GitHub: у Desktop лише `handoff` (цей файл) і 4 гілки dependabot (`actions/download-artifact-8`, `al-cheb/configure-pagefile-action-1.5`, `azure/login-3.1.0`, `jlumbroso/free-disk-space-2.0.0`), які ніхто не розбирав. В Android лише `mzgram`.

Пакет Android: `org.mzgram.messenger` (`MZGRAM_PACKAGE` у `gradle.properties`). Java-пакети й namespace лишились `org.telegram.*`. Підпис v3 з ротацією, SHA-256 сертифіката `0b6801b8606f1d2dd6d7511600d834cf6cf5d3791f78601b216a49c3777f1560`. minSdk 23 (з 1.0.7; на Android 5 лишається 1.0.6). Мінімальна Windows: 10 версії 1809, бо Qt 6.

Історія Desktop: `mzgram` не має спільної історії з апстрімом до join-коміту (`git merge -s ours v7.2.8`, дерево не змінювалось). Відтоді апстрім вливається звичайним merge (7.2.9, 7.3.1). Історія Android лінійна від DrKLO, злиття 13.0.2 `ae9e86178` і 13.0.3 `95b796087`.

Код MZGram:
- Desktop: `Telegram/SourceFiles/mzgram/` (модулі `mzgram_*.{h,cpp}`, `*_rules.*` з чистою логікою під тести), тести `mzgram/tests/`, ціль `mzgram_tests` (`cmake/mzgram_tests.cmake`, консольна, PASS/FAIL на кожен випадок).
- Android: `TMessagesProj/src/main/java/org/telegram/messenger/mzgram/` (налаштування `MZGramConfig`, архів `MZGramHistoryController`/`MZGramHistoryDatabase`, мова `MZGramLanguage`), UI `.../ui/mzgram/` (`MZGramSettingsActivity`), рядки `res/values*/mzgram_strings.xml`, instrumented-тести `TMessagesProj_AppTests/src/androidTest/kotlin/org/telegram/messenger/mzgram/test/` (38 файлів, 217 тестів).

Окремий публічний репозиторій `dmytrokurochkin/mzgram-release-bot` (Python, Conventional Commits, без префікса `[mzgram]`).

## б) Збірка, CI, реліз

Локально нічого важкого не збирається: ноутбук власника Windows ARM64, тому все збирається в GitHub Actions.

Workflow Desktop (`.github/workflows/`):
- `mzgram-windows.yml`: push у `mzgram` і `mzgram/**`, ручний запуск. Матриця x64 (`windows-latest`) і arm64 (`windows-11-arm`). Debug-збірка, `mzgram_tests.exe`, `mzgram_check_imports.py` (кожна DLL з імпортів має бути з Windows), `mzgram_self_update_test.ps1` (самооновлення через Updater зі збереженням даних). `cancel-in-progress: true`.
- `mzgram-linux.yml`: те саме для Linux x64 і arm64 (збірка в Docker), `cancel-in-progress: true`. Юніт-тестів у Linux CI немає, лише збірка.
- `mzgram-desktop-release.yml`: тег `v*` (або ручний запуск з `tag` без публікації). Кроки: Version, Windows x64/arm64 Release, Linux x64/arm64 Release, публікація (чернетка, файли, `SHA256SUMS`, публікація). Ліміт джоба 360 хв.
- Решта workflow апстріму (`win.yml`, `linux.yml`, `mac.yml` тощо) вимкнені вручну в Actions. Там же лишився зареєстрований `MZGram diag libpq` (`mzgram-diag-libpq.yml` з видаленої гілки `w-diag-libpq`); він нешкідливий, його можна вимкнути.

Workflow Android:
- `mzgram-android.yml`: push у `mzgram` і `mzgram/**`, ручний запуск. Джоб `Android` (збірка, Python-тести скриптів) і джоб `MZGram instrumented tests` (емулятор, `.github/scripts/mzgram_tests.sh`, ліміт 120 хв). Обидва мають бути зелені.
- `mzgram-android-release.yml`: тег `v*`, або ручний запуск з `tag` і `throwaway_key`.

Скрипти `.github/scripts/`:
- Обидва репо: `mzgram_version.py` (версії, коди, нотатки) і `test_mzgram_version.py`.
- Desktop: `mzgram_check_imports.py` + тест, `mzgram_self_update_test.ps1`.
- Android: `mzgram_tests.sh` (усі instrumented-тести крім живого; окремо `mzgram_push_cold.sh` зі справжнім FCM-пушем і `mzgram_appearance_restart.sh`), `mzgram_e2e_peer.py` (другий акаунт тестового сервера Telegram для живого E2E), `mzgram_rotation_check.sh` (ротація ключа на емуляторі), `mzgram_sign.sh` і `mzgram_sign_ci.sh` (apksigner rotate), `mzgram_fcm_push.py`. Тести: `test_strings.py` (12 мов, ~150 с), `test_texts.py` (рядки MZGram на екранах MZGram), `test_features.py`, `test_manifest.py`. Запускати так: `python .github/scripts/test_strings.py`; `python -m unittest <шлях>` падає з `Empty module name`.

Версії (`mzgram_version.py`):
- Тег `vX.Y.Z` (стабільний) або `vX.Y.Z-beta.N` (prerelease, N від 1 до 98).
- base = MAJOR\*1000000 + MINOR\*10000 + PATCH\*100 + (N або 99 для стабільного).
- Android: versionCode = base\*10 + 9 (остання цифра означає набір ABI). Workflow відмовляє тегу, чий код не вищий за всі теги і за `APP_VERSION_CODE` з `gradle.properties`. versionName: `X.Y.Z (13.0.3)`. Push-збірки беруть код найновішого тегу й назву `X.Y.Z-dev`. Приклад: 1.0.8 = 10008999.
- Desktop: код лише впорядковує теги, versionCode немає. Апстрім береться з `Telegram/build/version` (`AppVersionStr`).
- Опис: `docs/releasing.md` в Android.

Нотатки релізу:
- Workflow складає текст: рядок «MZGram ... X.Y.Z, based on ...», потім вміст `.github/release_notes/X.Y.Z.md` (необов'язковий вступ), потім `## Changes`, потім `## Install` (генерує `mzgram_version.py install`).
- `## Changes`: якщо є `.github/release_notes/X.Y.Z.changes.md` з хоча б одним рядком, береться він. Інакше беруться теми комітів `[mzgram]` від попереднього тегу (`mzgram_version.py previous`), відфільтровані `notes`: без `CI:`, `Test:`, `Docs:`, `Release workflow:`, `Release notes:`, `Spec:`, рядків про CI/FEATURES.md/README, хешів у дужках і назв інших клієнтів.
- Фільтр не розуміє, що «Updater ...» чи «languages ...» внутрішні зміни, тому для релізу краще завжди писати `.changes.md` руками, лише зміни для користувача, англійською.
- Перевірка локально: `git log --no-merges --format=%s <prev>..HEAD | python .github/scripts/mzgram_version.py notes --own .github/release_notes/X.Y.Z.changes.md`.

Як випускати реліз (лише з дозволу власника):
1. `.changes.md` і FEATURES.md («Next release» -> номер версії, рядок 5 FEATURES.md -> наступні номери) комітом у `mzgram`, зелений push CI.
2. Тег-об'єкт через `git mktag` (у локальному клоні Desktop є старі теги апстріму v1.0.x, тому `git tag` не використовувати):
   ```
   printf 'object <sha>\ntype commit\ntag vX.Y.Z\ntagger <name> <email> <unix> <tz>\n\nMZGram Desktop X.Y.Z\n' | git mktag
   git push origin <tag-object>:refs/tags/vX.Y.Z
   ```
3. Release workflow публікує сам: GitHub release не чернетка, `releases/latest`.
4. Перевірити: текст нотаток, файли проти `SHA256SUMS` і `digest` у `gh api .../releases/tags/vX.Y.Z`, пакет/підпис/versionCode APK, `Updater` в архівах Desktop, відсутність `libpq.dll` в імпортах.

Секрети GitHub (лише імена):
- Обидва репо: `TELEGRAM_API_ID`, `TELEGRAM_API_HASH`.
- Android: `ANDROID_KEYSTORE_B64`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD`. Без них збірка лишається на debug-ключі й реліз стає prerelease «debug key».
- У локальному мета-репо `.env`: `TELEGRAM_API_ID`, `TELEGRAM_API_HASH`, `ANDROID_HOME`, `MZGRAM_KEYSTORE_PATH`, `MZGRAM_KEYSTORE_PASSWORD`, `MZGRAM_KEY_ALIAS`, `MZGRAM_KEY_PASSWORD`. Значення ніде не копіювати.

mzgram-release-bot (працює на сервері власника як systemd-сервіс `mzgram-release-bot`; на сервер не заходити і нічого там не чіпати):
- Кожні 10 хв питає GitHub про нові релізи обох репо, без вебхуків.
- Завантажує файли й перевіряє SHA-256 за `SHA256SUMS` і за дайджестом GitHub.
- Публікує в канал один пост на реліз: файли `.apk`, `.zip`, `.tar.xz`, `SHA256SUMS` (список `CHANNEL_FILES`; `-source.tar.xz` у канал не йде), під ними англійською назва, зміни й посилання на реліз. Пост закріплюється.
- Не публікує, якщо бракує файлу, сума не збігається, файлів більше 10 або файл більший за 2000 МБ. Тоді пише адміну.
- Ніколи не публікує двічі (SQLite `UNIQUE (repo, tag)`). Бета-релізи пропускає (`PUBLISH_PRERELEASES`).
- Звідси обмеження для релізів: до 10 файлів разом із `SHA256SUMS`, кожен до 2000 МБ.
- Текст, який бот уже опублікував, не зміниться від `gh release edit`.

## в) Тривалості CI і інфраструктурні збої

Типові тривалості (жовтень 2026):

| Прогін | Тривалість |
|---|---|
| Android push: `Android` | 47-57 хв |
| Android push: instrumented | 55-65 хв |
| Windows push: x64 | 42-56 хв |
| Windows push: arm64 | 79-82 хв |
| Linux push: x64 | ~16 хв |
| Linux push: arm64 | ~75 хв |
| Android release | ~73 хв |
| Desktop release з теплим кешем | ~4 год (Windows arm64 найдовший) |
| Desktop release з холодним кешем | ~5 год (x64 290 хв, arm64 245 хв, ліміт 360) |

Відомі збої, не пов'язані з кодом:
- **Холодний кеш Release-бібліотек Windows.** GitHub вичищає кеш, яким не користувались 7 днів. Тоді крок `Libraries` збирає все з нуля, 2-2,5 год на кожній архітектурі. Кеші прив'язані до гілки: кеш з `w-*` гілці `mzgram` недоступний. Перед релізом після тижневої паузи варто прогріти кеш ручним запуском `mzgram-desktop-release.yml` з `tag` (без публікації), щоб не впертися в 360 хв.
- **Диск Windows.** На x64-раннері C: має ~31 ГБ вільних, D: ~147 ГБ. Збірка йде на диску з найбільшим вільним місцем (`D:\TBuild`). На C: Debug-лінкування падало з `LNK1180: insufficient disk space`. Крок «Free up some disk space» не повинен видаляти `*/tdesktop_rust/out/src/*`, а з образу не видаляти PostgreSQL.
- **Linux: дзеркала EPEL.** У Docker-образі буває помилка SSL дзеркала EPEL. Лікується перезапуском джоба.
- **Емулятор Android.**
  - `MZGramAttachGalleryTest` періодично падає не через код: `Targeted input event injection ... not directed at a window owned by uid` (системне вікно поверх застосунку), або емулятор вмирає.
  - Відрізнити так: падає тест, якого зміна не стосується; повідомлення про injection, вікно, емулятор, OOM; той самий тест зелений у сусідніх прогонах. Тоді `gh run rerun <id> --failed`.
  - Справжня помилка коду повторюється і зазвичай падає в тесті, що стосується зміни. Приклад: closed pool в архіві був справжнім багом, хоч і рідкісним.
- **Мережа до GitHub.** `gh run watch` іноді обривається з TLS handshake timeout. Результат треба перечитати через `gh run view --json`, а не довіряти виводу watcher.
- **Скасування прогонів.** Windows і Linux CI мають `cancel-in-progress: true`: новий push у `mzgram` скасовує поточний push-прогін. Не пушити в `mzgram`, поки йде довгий Linux arm64 чи інший потрібний прогін.

## г) Методика red/green і перевірки

1. Робота йде в окремій гілці `w-<назва>` з окремої робочої копії (див. «Локальні копії» нижче). Ім'я `mzgram/...` не годиться: конфлікт ref з гілкою `mzgram`.
2. Червоний коміт: лише тест (плюс мінімум, щоб компілювалось), `[mzgram] Test: ...`. Його пушу в `w-red-<назва>` і запускаю `gh workflow run <workflow>.yml --ref w-red-<назва>`. Push в інші гілки CI не запускає. Червоний прогін має впасти рівно на нових тестах.
3. Зелений: коміти виправлення поверх червоного в `w-<назва>`, той самий ручний запуск, усе зелене.
4. FEATURES.md однаковий в обох репо й іде в тому ж коміті, що і фіча. Коли фіча лише Android, у Desktop іде окремий коміт тільки з FEATURES.md.
5. Злиття лише з дозволу власника, fast-forward:
   ```
   git fetch origin mzgram
   git merge-base --is-ancestor origin/mzgram <sha>
   git push origin <sha>:refs/heads/mzgram
   ```
   Потім дочекатися зеленого push CI.
6. Видалити `w-*` і `w-red-*` на GitHub і локально, оновити специфікацію.

Перевірки до CI (локально, легкі):
- Android: `python .github/scripts/test_strings.py`, `test_texts.py`, `test_features.py`, `test_mzgram_version.py`.
- Desktop: `python .github/scripts/test_mzgram_version.py`, `test_mzgram_check_imports.py`.
- Перевірка синтаксису C++ на ноутбуці: `cl /Zs` з MSVC (vcvarsarm64) і include Qt (`C:/TBuild/Libraries/Qt-6.11.2/include`) та lib_base/lib_rpl/lib_crl/GSL з `C:/TBuild/tdesktop`. Лише синтаксис, не збірка.

Уроки:
- **CRLF.** Скрипт заміни шукав `\n`, а `application.cpp` має CRLF. Заміна мовчки не відбулась, `#include` не додався, збірка впала в CI. Тепер кожен скрипт заміни читає байти, нормалізує `\r\n`, перевіряє `assert text.count(old) == 1` і повертає CRLF назад.
- **Лічильник перемикачів.** `MZGramSettingsTest` перевіряє кількість перемикачів і кнопок у Settings > MZGram (зараз 51). Новий перемикач = +1 там, інакше CI червоний.
- **Червоний прогін на іншому кроці.** Якщо червоний прогін зупинився раніше на іншому кроці, тест не перевірено як червоний. Це треба записати чесно.

Локальні копії (Windows ARM64, `C:\TBuild`):
- Desktop: `C:\TBuild\tdesktop` (основний клон, локальна `mzgram` = `5ddea65cca`, стара історія до join, відстає від `origin/mzgram`; є `stash@{0}` «WIP on mzgram: d4f39243e2 ... auto pause video», 4 файли). Робоча копія `C:\TBuild\mzw-trial` (worktree того ж репо, зараз detached HEAD на `origin/mzgram` = `a5b1864afe`; там же локальна гілка `handoff`). Мета-репо має посилання `clients/tdesktop` -> `C:\TBuild\tdesktop`.
- Android: `<мета-репо>/clients/android` (основний клон, локальна `mzgram` = `99350172e`, відстає на 61; три невідстежені `.pyc`), робоча копія `C:\TBuild\mza-trial` (detached HEAD на `origin/mzgram` = `088bc6db7`).
- Гілка `mzgram` зайнята основними клонами, тому в робочих копіях працювати на `w-*` або detached HEAD (`git checkout --detach origin/mzgram`) і пушити `<sha>:refs/heads/mzgram`.
- Локальні гілки без власних змін (усі патчі вже в `mzgram`): Desktop `k3-desktop-red`, `dev`; Android `k1-red-check`, `k3-red-check`, `themes-red-check`, `themes2-red-check`, `master`.

## д) Правила власника

- Коміти з префіксом `[mzgram] `, англійською, від імені власника. Без `Co-authored-by` і будь-яких трейлерів, без згадок асистентів чи ботів, навіть якщо інструмент просить додати атрибуцію. Репо бота: Conventional Commits з тим самим правилом.
- Назви інших клієнтів-форків ніде в репо: код, коментарі, назви тестів, UI, коміти, нотатки. `mzgram_version.py` тримає їх задом наперед. Код інших GPL-форків портувати дозволено (власник скаже, яких), зі збереженням заголовків copyright, але без назв.
- FEATURES.md: однаковий в обох репо, розділи `## Android`, `## Desktop`, `## Android and Desktop compared`, англійською, без хешів, лише реалізоване. У колонці «Since» перша версія з фічею або «Next release».
- 12 мов для кожного рядка MZGram: en, uk, ru, pl, es, de, fr, pt-BR, tr, ar, be, kk.
  - Android: `values{,-uk,-ru,-pl,-es,-de,-fr,-pt-rBR,-tr,-ar,-be,-kk}/mzgram_strings.xml`.
  - Desktop: `mzgram_lang_table.cpp` (en, uk) і `mzgram_lang_translations.cpp` (решта).
  - Терміни як в офіційній локалізації Telegram кожною мовою.
  - Тести: `test_strings.py` і Desktop `TestEveryPhraseInEveryLanguage`.
  - Мова MZGram вибирається окремо від мови Telegram (Settings > MZGram > Interface). Екрани MZGram показують лише рядки MZGram (`test_texts.py`).
- У рядках MZGram немає довгого тире (`test_strings.py` це перевіряє). У документах теж краще без нього.
- Не комітити `BuildVars`. Premium і ghost mode не чіпати.
- Мережа: жодних сторонніх сервісів і запитів, крім Telegram. Єдиний виняток: шлюз UnifiedPush (Android), лише константою в коді. Перекладача немає.
- `applicationId` `org.mzgram.messenger` не змінювати. versionCode ніколи не зменшувати.
- Злиття в `mzgram`, теги й релізи лише з явного дозволу власника.
- Важке збирати лише в CI. На ноутбуці власника не запускати GUI-програм і нічого не видаляти.
- Тести обов'язкові, спершу червоний, потім зелений. Тимчасові гілки видаляти.
- Звіт власнику українською, коротко: номери прогонів, коміти, що НЕ перевірено. Роботу незалежно перевіряє супервайзер на GitHub.
- Архів повідомлень: усі типи чатів, перемикачі за замовчуванням увімкнені, власні й службові повідомлення не зберігаються, медіа без ліміту в Downloads/MZGram/Saved Attachments, позначки 🧹 і ✏️.

## е) Що перевірено, а що ні

Автоматично на кожному push:
- Android: 217 instrumented-тестів на емуляторі (зі знімками екрана), перевірка ротації ключа, справжній FCM-пуш до вбитого процесу, тема після перезапуску й оновлення, Python-тести рядків/текстів/FEATURES/версій.
- Desktop: `mzgram_tests` на Windows x64 і arm64 (103 випадки на 2026-10-28, реєстр під `HKCU\Software\MZGramTests\`), імпорти DLL, самооновлення через Updater на CI-машині. Linux лише збирається.

Не перевірялося ніколи:
- Живі пристрої: телефон і ПК власника вручну нічого не перевіряли.
- Живий E2E видалених повідомлень (`MZGramDeletedArchiveLiveTest`): пропускається в усіх останніх прогонах, бо вхід на тестовий сервер Telegram у preflight не проходить.
- Справжнє «видалити для всіх» на сервері для перемикача 1.0.8 (тест лише перевіряє галочку в діалозі).
- Запис голосових V2 у 13.0.3 з підтвердженням відправки: лише читання коду.
- Самооновлення з 1.0.4 на справжній машині (лише CI-тест з позначеною копією).
- Linux: `tg://` (`mzgram_url_scheme_linux.cpp`, Gio) і самооновлення лише скомпільовані.
- Реєстр ноутбука власника не чіпався. Там `tg` може вказувати на старий MZGram, поки не запуститься 1.0.6+.
- Пост бота в каналі для 1.0.8 і 1.0.7 не дивився.
- APK 1.0.8 (пакет, підпис, versionCode) і архіви Desktop 1.0.7 (`Updater`, імпорти) після публікації окремо не розпаковувались. Відповідні кроки релізу зелені, суми файлів збігаються.

## є) Відкриті питання й борги

- **Desktop-реліз на межі ліміту.** З холодним кешем x64 зайняв 290 з 360 хв (`mzgram-desktop-release.yml`, `timeout-minutes: 360`). Варіанти: прогрівати кеш перед релізом або зберігати кеш частіше.
- **Експорт архіву Android тримає транзакцію.** `MZGramHistoryDatabase.exportTo` тримає транзакцію на живій базі, поки копіює всі рядки. Інші потоки архіву чекають. Якщо десь доступ до архіву йде з UI-потоку, на великому архіві під час експорту можливе підвисання. Не перевіряв, де саме викликається архів з main thread.
- **Імпорт дуже старого архіву.** `MZGramHistoryDatabase.importFrom` копіює спільні колонки. Архів схеми v1 (до `DB_VERSION` 2) з колонками NOT NULL, яких там немає, не імпортується: транзакція відкочується, архів лишається. Раніше v1 просто втрачав таблицю. Практично таких файлів немає.
- **Слово «Telegram» у заголовку списку чатів Android.** Drawable `telegram_logo_2` показує його з 12.4.0, тобто з MZGram 1.0.0. Власнику сказано, не виправлено.
- **Іконка Desktop.** Досі `icon256.ico` Telegram. Власник обіцяв малюнки (лого SVG 1024x1024 і 3-5 іконок 24x24), потім одна задача для обох клієнтів.
- **Dependabot у Desktop.** 4 гілки (оновлення actions) не розглянуті.
- **Workflow `MZGram diag libpq`.** Активний запис у Actions без файлу в `mzgram`.
- **Нестабільний тест галереї.** `MZGramAttachGalleryTest` нестабільний на емуляторі, причина не встановлена (камери вимкнені, демони збірки зупиняються до старту емулятора, є крок про OOM).
- **Перевірка оновлень Telegram.** Відключення `LaunchActivity.checkAppUpdate`, бо воно пропонує офіційний Telegram з іншим підписом: було в планах J, стан не перевіряв.
- **Плани на потім:** APK під кожну ABI, F-Droid, бета в канал.
- **Старий stash.** `stash@{0}` у `C:\TBuild\tdesktop` на старій історії: невідомо, чи потрібен. Не видаляти без власника.
- **Друга копія специфікації.** Правило каже «оновлювати обидві копії `mzgram-bug-spec.md`», але на ноутбуці знайдено лише одну (мета-репо). Друга, ймовірно, у супервайзера. Уточнити у власника.

## ж) Специфікація і беклог

- **Оригінал.** Локальний мета-репо власника `MZGram` (git без remote, гілка `main`, останній коміт `c87875a`), файл `mzgram-bug-spec.md`, українською, ~190 КБ. Там же `docs/` (налаштування API, процес форка, збірка, CI, план портування фіч) і `scripts/` (`doctor.ps1`, `apply-credentials.ps1`, `build-desktop.cmd`, `prepare-desktop.cmd`).
- **Копія в гілці `handoff`.** `handoff/mzgram-bug-spec.md` з цим станом. Назви інших клієнтів у копії замінено позначками (див. перший рядок копії), бо репо публічне.
- **Структура.**
  - Розділи 1-6: вихідний баг архіву і обмеження.
  - 6a-6d: рішення власника.
  - **6e «Беклог»**: план від 2026-10-05.
  - **«Стан виконання 6e»**: хронологія всіх задач (від H до останнього запису 2026-10-28) з комітами, номерами прогонів, червоними/зеленими і «Не перевірено».
  - 7-9: вимоги до верифікації і процесу.
- **Як користуватись.** Перед задачею прочитати останні записи «Стану виконання» і рішення 6a-6d. Після задачі дописати запис у той самий формат: що просили; що знайдено; коміти; червоний і зелений прогони; злиття; релізи; «Не перевірено». Закомітити в мета-репо з повідомленням `[mzgram] Bug spec: ...`.
- **Відкритих задач від власника зараз немає.** Останнє прохання (архів скролом, «видалити для всіх») закрите релізами 1.0.8 і 1.0.7.

## з) Пастки

- **Старі теги апстріму в локальному клоні Desktop.** `git log v1.0.6..mzgram` у ньому дає 2,5 МБ історії Telegram. Межу брати за комітом тегу MZGram з GitHub: `gh api repos/dmytrokurochkin/MZGram-Desktop/git/refs/tags/v1.0.7 -q .object.sha` дає тег-об'єкт, `gh api repos/dmytrokurochkin/MZGram-Desktop/git/tags/<він> -q .object.sha` дає коміт (`v1.0.7` = `a5b1864afe`). Теги ставити лише через `git mktag`.
- **Push в інші гілки не запускає CI.** Лише `mzgram` і `mzgram/**`. Для `w-*` запускати `gh workflow run ... --ref`.
- **Push у `mzgram` скасовує поточний Windows/Linux прогін** (`cancel-in-progress`).
- **Гілка `mzgram` зайнята основними клонами.** `git checkout mzgram` у робочій копії не вийде.
- **13.0 шукає рядок лише за id ресурсу (ключ null).** `MZGramLanguage.key(key, res)` бере ім'я ресурсу. Без цього `LOC_ERR: null`.
- **13.0 ставить `localeFilters ["zz"]`.** Мови MZGram тримаються одним списком `MZGRAM_LOCALES` у `gradle.properties` для всіх модулів, плюс `noCompress "pack"`.
- **Статичний Qt 6 на Windows x64 тягнув `libpq.dll`.** Через це 1.0.0-1.0.4 x64 не стартували ніде. Тепер `qt6_import_plugins(... INCLUDE_BY_TYPE sqldrivers Qt6::QSQLiteDriverPlugin)` і `mzgram_check_imports.py`.
- **`tg://` на Windows.** Не писати в ключі Telegram Desktop. Власні ключі `Classes\mzgram.<p>` і `Software\MZGramDesktop\Capabilities`, маркер `Software\MZGramDesktop\TakenUrlSchemes`. `Classes\<p>` брати лише якщо порожній, вказує на MZGram.exe чи на зниклий exe, або з `registertg`. Обробники REG_EXPAND_SZ і без лапок не вважати зниклими.
- **Спільний `SQLiteOpenHelper` архіву Android.** Ніколи не викликати `close()`, поки застосунок працює: інший потік отримає `connection pool has been closed`. Експорт та імпорт ідуть через транзакцію.
- **Паралельні потоки в тестах.** Для гонок у тестах проганяти дію в циклі (наприклад 10 разів експорт та імпорт), поки другий потік працює з тим самим ресурсом. Одиночна перевірка гонку ловить випадково.
- **Автоматичні нотатки.** Внутрішні коміти з «Updater», «languages» чи «tg://» фільтр пропускає в нотатки. Писати `.changes.md`. Після публікації текст у каналі вже не змінити.
- **Ноутбук власника ARM64.** x64-збірки там не запускати: старий x64 показав помилку `libpq.dll`. GUI не запускати взагалі.
- **Інструмент оболонки.** Довгий `sleep` блокується. Чекати прогони фоновою командою з `gh run watch`, потім перечитувати `gh run view`.
