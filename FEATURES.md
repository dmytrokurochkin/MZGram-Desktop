# MZGram features

What MZGram adds to the regular Telegram apps, and how it differs from them. MZGram is an unofficial client: MZGram for Android is based on Telegram for Android, MZGram Desktop on Telegram Desktop. Everything here is in the code today; nothing planned is listed.

"Since" is the first MZGram release with the feature. "Next release" means it is done and tested but not released yet (Android 1.0.6, Desktop 1.0.4).

Most switches are under **Settings > MZGram**, grouped into topic pages. This file is the same in both repositories; it is updated together with every change to a feature.

- [Android](#android)
- [Desktop](#desktop)
- [Android and Desktop compared](#android-and-desktop-compared)

## Android

### Message archive

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Deleted messages are kept | In every private chat, group, channel and secret chat, another person's message stays in the chat after they delete it, from a local copy. Telegram removes it. Your own messages are never kept. On by default. | 1.0.0 |
| Edit history | Every earlier version of another person's edited message is kept; the message history screen shows the revisions, and a tap copies one. Telegram shows only the last version. | 1.0.0 |
| View-once media kept | View-once photos and videos are kept with their file, also unopened ones. Deleted messages and removed media also stay in topics, threads and secret chats. | 1.0.0 |
| Chats with only deleted messages stay | When the other side deletes the only message of a chat or its whole history (also by clearing it), the chat stays in the chat list and shows the newest kept message, also after a restart. Telegram removes such a chat. Without kept messages the chat goes as usual. | 1.0.4 |
| Archive switches | Separate switches for deleted messages, edit history, media files, formatting, reactions and chats with bots. | 1.0.0 |
| Media without limits | Files of kept messages are copied to Downloads/MZGram/Saved Attachments, of any size and with no total limit, so they stay after the sender removes them. | 1.0.0 |
| Marks for deleted and edited messages | A mark before the time of a deleted or edited message (any text, or none); kept deleted messages are drawn at 75% opacity. | 1.0.0 |
| Export, import and wipe | Export the archive database to a file, import it back (replacing the current archive), or delete the whole archive and its saved media after a confirmation. | 1.0.0 |
| Export with Save as | Export opens Android's Save as, so the archive file goes where you choose (Downloads, a file manager, a cloud drive), and says when it is saved. Import takes only an MZGram archive file and keeps the current archive otherwise. Up to 1.0.4 Export closed the app, so the archive of 1.0.3 and older cannot be moved. | 1.0.5 |
| Clear the local database | Clears Telegram's cache on the device (messages, chat list); it loads again from the server. The MZGram archive is kept. | 1.0.0 |

### Privacy and ghost mode

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Ghost mode | Other people no longer see that you read their messages or that you are typing. A message is marked read only when you reply or react. Telegram always sends both. | 1.0.0 |
| Delayed sending | While ghost mode is on, an outgoing message waits about 12 seconds (longer for media) before it is sent, so writing and sending at once does not show you online. | 1.0.0 |
| Send without sound | While ghost mode is on, every outgoing message is sent without a notification sound. | 1.0.0 |
| Ghost mode before stories | Before you open a story, MZGram offers to turn ghost mode on, so the story is not marked as seen. | 1.0.0 |
| Read time | Message details show when the other person read your message. | 1.0.0 |
| Approximate last seen | For a person who hides their last seen, MZGram shows an estimate from what this device saw (for example, their new messages), like any last seen time; up to 1.0.3 it was marked "(approx.)". | 1.0.0 |
| Protected content | In chats and channels that restrict saving, you can forward, save and copy messages and media and take screenshots. Messages that cannot be forwarded are sent as new copies. Off by default. | 1.0.0 |

### Message menu

Each item has its own switch.

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Copy Photo | Copies a downloaded photo, to paste it into other apps. | 1.0.0 |
| Delete Downloaded File | Removes the downloaded file of a message from the device; the message stays and the file can be downloaded again. | 1.0.0 |
| Save Message | Forwards a message to Saved Messages in one tap. | 1.0.0 |
| Set a reminder | Schedules a copy of the message in Saved Messages at the time you pick, which notifies you then. | 1.0.0 |
| Repeat | Sends the same message into the chat again (or its text or sticker where forwarding is not allowed). | 1.0.0 |
| Open in... | Opens a video in another app, streaming it if it is not downloaded yet (Android 8 and newer). | 1.0.0 |
| Details | Ids, sender, dates, forward source, file, media and data center of a message; a long press copies a value. | 1.0.0 |
| QR Code | When a photo holds a QR code, shows its link or text. The photo is scanned on the device. | 1.0.0 |
| Forward without sender | Opens the forward screen with the sender's name already hidden. | 1.0.0 |

### Chats and pinning

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Pin in MZGram only | When the server allows no more pinned chats, more chats can be pinned on this device only, up to 10 in all: the server's pins first, then MZGram's, marked with their own pin. Nothing is sent to the server, and Premium is not needed or changed. Telegram stops at the server's limit. | 1.0.1 |
| Open archive on pull | Pulling the chat list down past the archive row opens the archive. | 1.0.0 |
| Media preview | A long press on a chat's avatar shows its last photo or video instead of the chat preview. | 1.0.0 |

### Media and calls

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Ask before calling | Asks for confirmation before a voice or video call starts. | 1.0.0 |
| Confirm voice and video messages | After recording, a voice or round video message is shown for review with a Send button instead of being sent at once. | 1.0.0 |
| Instant camera off | The camera tile in the attach menu shows no live preview; the camera starts only on a tap. | 1.0.0 |
| Hide camera tile | Removes the camera tile, so the attach gallery starts with your first photo. | 1.0.0 |
| Original quality | Videos offered in several qualities play in the original one. | 1.0.0 |
| Auto-pause video | A video in the media viewer pauses when the app goes to the background and resumes when you return. | 1.0.0 |

### Interface and appearance

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Folder tabs at the bottom | Chat folder tabs sit at the bottom of the chat list, within reach of the thumb. | 1.0.0 |
| No bottom navigation bar | Removes the tab bar under the chat list; its pages move to the chat list menu. | 1.0.0 |
| Accounts in the chat list menu | The menu of the three dots at the top of the chat list ends with every signed-in account (avatar and name, the current one marked, the unread count of the others); a tap switches to it at once, without opening Settings. Add Account is there while there is room for one (more with Premium). The standard items stay. On by default, can be switched off. | 1.0.3 |
| Hide stories | Removes the stories row from the chat list. | 1.0.0 |
| Seconds in message time | Message times show seconds, such as 12:34:56. | 1.0.0 |
| Exact numbers | Views, reactions and members show exact counts (4777 instead of 4.8K). | 1.0.0 |
| No greeting sticker | An empty chat shows no suggested sticker. | 1.0.0 |
| Hide channel bottom button | In channels where you cannot post, hides the bottom bar with the mute button. | 1.0.0 |
| Animation switches | Switches for the predictive back animation and the gooey avatar animation in profiles. | 1.0.0 |
| MZGram first in Settings | The MZGram row ("Features of this client") is the first row of Telegram's settings list, in the same group and with the same look as My Account and the other settings, right above My Account (next release; in 1.0.5 it was a group of its own above the accounts). "Features of this client" is only that row's subtitle, not a text on the MZGram page. | 1.0.5 |
| 12 languages | The MZGram interface is available in 12 languages: English, Ukrainian, Russian, Polish, Spanish, German, French, Portuguese (Brazil), Turkish, Arabic, Belarusian and Kazakh. Every MZGram switch, setting, menu item, dialog, error, the update banner and the push diagnostics are complete in each; for any other language they are in English, never a mix. From the next release every MZGram text is in the chosen MZGram language, never in Telegram's: the buttons of MZGram's dialogs (Cancel, OK, Save, Delete), the update banner and the install permission dialog (also right after the language is changed), the MZGram items of the message menu, the QR code and message details screens and their dates. English and Ukrainian since 1.0.2. | 1.0.4 |
| MZGram language | Settings > MZGram > Interface > MZGram language: System (the phone's language when MZGram has it, otherwise English) or one of the 12, apart from Telegram's language. Before, some MZGram texts could switch to the phone's language. | 1.0.4 |
| Standard color themes | The color theme row in Appearance always lists the home theme and 8 standard themes built on the device, also without a server answer and after an update; themes from the server are added after them. The theme in use stays selected after Switch to Night Mode. | 1.0.1 |

### Ads and filters

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| No sponsored messages | Sponsored messages in channels and the promo or proxy banner in the chat list are neither requested nor shown. | 1.0.0 |
| Zalgo filter | Removes stacked Unicode combining marks (corrupted "Zalgo" text) from names, chat titles and message text. | 1.0.0 |

### Notifications

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| UnifiedPush | Notifications come through a UnifiedPush distributor: the built-in Google FCM, or an app such as ntfy, microG or Sunup. Telegram's own Google push does not reach unofficial builds. | 1.0.0 |
| Push gateway | A gateway the user can change; it cannot read notifications (the keys stay on the device). | 1.0.0 |
| Push diagnostics and test push | The UnifiedPush page shows what happened to the last notifications and can send a test push through the whole path. | 1.0.0 |

### Storage

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| 1 GB cache limit | Maximum cache size can be set to 1 GB (Telegram starts at 5 GB); a lower limit clears the cache at once. | 1.0.1 |

### Updates and branding

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| MZGram update banner | Every few hours MZGram checks its latest stable release on GitHub and shows a banner when a newer one is out; Close hides that version. Telegram's own update check is turned off. Can be switched off. | 1.0.1 |
| Self-update | Update in the banner downloads the APK of that release (for the device's architecture, else the universal one) only from the MZGram-Android releases on GitHub over HTTPS, checks its SHA-256 against the release's SHA256SUMS and GitHub's digest, and installs it with Android's package installer; Android may ask to confirm, and the first time asks to allow installing apps from MZGram. Progress is shown in the banner; a broken download resumes; a file that fails the check is deleted and nothing is installed. On an error: Retry or the release page. When a release has no APK that can be checked, the release page opens. Only an APK of MZGram's own package is installed. The switch of the update banner turns off the automatic check; from the next release Update also works in a banner shown by Check for updates with that switch off (before, it did nothing then). The version before it is installed by hand once. | 1.0.2 |
| Check for updates | Settings > MZGram > Other > Check for updates looks at once, outside the interval and also with the banner switched off, and says what it found: the latest version is installed, a newer one is available (it also gets the banner), or the check failed (no network, a GitHub error). | 1.0.4 |
| MZGram name | The app calls itself MZGram in dialogs, permission requests, call notifications, contacts and in the version line of Settings ("MZGram 1.0.2 (based on Telegram 12.10.1)"), in every language. The Telegram service keeps its name. | 1.0.2 |
| Own package | MZGram is org.mzgram.messenger, a separate app beside Telegram from Google Play and from telegram.org (up to 1.0.3 MZGram had the package of Telegram from telegram.org, so Android took it for that app's update and did not install it). Its content providers, permissions and contacts account are its own, and its launcher icon is violet, not Telegram's blue. tg:// and t.me links may ask which app opens them. MZGram 1.0.3 and older do not update to it: install it by hand once and sign in. The archive of the old MZGram is not moved (Export did not work in 1.0.3), so the new MZGram starts with an empty archive; files of archived messages stay in Downloads/MZGram/Saved Attachments. | 1.0.4 |

### Releases

| Feature | What it does | Since |
|---|---|---|
| Signed releases | Every release APK is signed with the MZGram key, with a key rotation from the earlier key so it installs over earlier MZGram builds and keeps their data (up to 1.0.3; the next release has its own package and is installed beside them). `SHA256SUMS` and the signing certificate are published with each release. | 1.0.0 |

## Desktop

### Message archive

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Deleted messages are kept | In every chat, another person's message stays after they delete it, from a local copy. Telegram removes it. Your own messages are never kept. On by default. | 1.0.0 |
| Edit history | Every earlier version of another person's edited message is kept and shown in an edit history window opened from the message. | 1.0.0 |
| View-once media kept | View-once media of other people is kept with its file. | 1.0.0 |
| Chats with only deleted messages stay | When the other side deletes the only message of a chat or its whole history, the chat stays in the chat list with the newest kept message while the app runs. After a restart the chat list comes from the server, which no longer lists such a chat; the kept messages are there again when the chat is opened. | 1.0.2 |
| Archive switches | Separate switches for deleted messages, edit history, media files, formatting, reactions and chats with bots. | 1.0.0 |
| Media without limits | Files of kept messages are saved in a Saved Attachments folder, of any size and with no total limit. | 1.0.0 |
| Marks for deleted and edited messages | A mark before the time of a deleted or edited message (any text, or none); kept deleted messages are dimmed. | 1.0.0 |
| Export, import and wipe | Export the archive database, import it back, or delete the whole archive after a confirmation. | 1.0.0 |
| Clear Telegram's local cache | Clears the local cache of Telegram on this computer and restarts; the MZGram archive is kept. | 1.0.0 |

### Privacy and ghost mode

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Ghost mode | Hides your read receipts, typing and online status; a switch for each lets it through. A message is marked read only when you reply or react. | 1.0.0 |
| Delayed sending | While ghost mode is on, an outgoing message waits before it is sent, so writing and sending at once does not show you online. | 1.0.0 |
| Send without sound | While ghost mode is on, every outgoing message is sent without a notification sound. | 1.0.0 |
| Ghost mode before stories | Before you open a story, MZGram offers to turn ghost mode on. | 1.0.0 |
| Read time | Message details show when the other person read your message. | 1.0.0 |
| Approximate last seen | For a person who hides their last seen, MZGram shows an estimate from what this computer saw, like any last seen time; up to 1.0.1 it was marked "(approx.)". | 1.0.0 |
| Protected content | In chats and channels that restrict saving, you can forward, save and copy their messages and media; they are sent as new messages without the original sender. Off by default. | 1.0.0 |

### Message menu

Each item has its own switch.

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Save Message | Forwards a message to Saved Messages in one click. | 1.0.0 |
| Set a reminder | Schedules a copy of the message in Saved Messages at the time you pick. | 1.0.0 |
| Repeat | Sends the same message into the chat again. | 1.0.0 |
| Open in... | Opens a downloaded file in another program. | 1.0.0 |
| Message details | Ids, dates, views, file and media of a message. | 1.0.0 |
| Scan QR code | When a downloaded photo holds a QR code, shows its link or text. Scanned on the computer. | 1.0.0 |

### Chats and pinning

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Pin in MZGram only | When the server allows no more pinned chats, more chats can be pinned on this computer only, up to 10 in all, after the server's pins and marked with their own pin. Nothing is sent to the server; Premium is not needed or changed. | 1.0.0 |
| Open archive on pull | Pulling the chat list down past the Archive row opens it. | 1.0.0 |
| Media preview | Holding a chat's avatar shows its last photo or video instead of the chat preview. | 1.0.0 |

### Media

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Confirm voice messages | Asks before a recorded voice message is sent. | 1.0.0 |
| Confirm round video messages | Asks before a recorded round video message is sent. | 1.0.0 |
| Auto-pause video | A video in the media viewer pauses when its window loses focus or is minimized, and resumes when you return. | 1.0.0 |

### Interface

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| Seconds in message time | Message times show seconds. | 1.0.0 |
| Hide stories | Removes stories from the chat list. | 1.0.0 |
| No greeting sticker | An empty chat shows no suggested sticker. | 1.0.0 |
| Hide channel bottom button | Hides the bottom button of channels where you cannot post. | 1.0.0 |
| 12 languages | The MZGram interface is available in 12 languages: English, Ukrainian, Russian, Polish, Spanish, German, French, Portuguese (Brazil), Turkish, Arabic, Belarusian and Kazakh. Every MZGram setting, menu item, dialog and the update bar are complete in each; for any other language they are in English, never a mix. From the next release every MZGram text is in the chosen MZGram language, never in Telegram's: the buttons of MZGram's boxes (Cancel, OK, Save, Delete, Close, Send), the dates on them, and the update bar, which also follows a change of the language at once. English and Ukrainian since 1.0.0. | 1.0.2 |
| MZGram language | Settings > MZGram > Interface > MZGram language: System (the system's language when MZGram has it, otherwise English) or one of the 12, apart from Telegram Desktop's language. | 1.0.2 |

### Ads and filters

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| No sponsored messages | Sponsored messages in channels and the promo banner in the chat list are not shown. | 1.0.0 |
| Zalgo filter | Removes stacked Unicode combining marks from names, chat titles and message text. | 1.0.0 |

### Updates and branding

| Feature | What it does, and how it differs from Telegram | Since |
|---|---|---|
| MZGram update bar | Every few hours MZGram checks its latest stable release on GitHub and shows a bar in the chat list when a newer one is out; Close hides that version. Telegram Desktop's own update checks are turned off. Can be switched off. | 1.0.0 |
| Self-update | Update in the bar downloads the archive of that release for this system and architecture (Windows x64 or ARM64 zip, Linux x64 or ARM64 tar.xz) only from the MZGram-Desktop releases on GitHub over HTTPS, checks its SHA-256 against the release's SHA256SUMS and GitHub's digest, unpacks it and restarts; the Updater copies the new files, backs up every file it replaces and puts them back if a copy fails, so the old version keeps working. The data folder is not touched. Progress is shown in the bar; a file that fails the check is deleted and nothing is installed; on an error: Retry or the release page. When the MZGram folder cannot be written, the bar says so and offers the release page. Turned off with the update bar. The version before it is installed by hand once. | 1.0.1 |
| Check for updates | Settings > MZGram > Interface > Check for updates looks at once, outside the interval and also with the bar switched off, and says what it found: the latest version is installed, a newer one is available (it also gets the bar), or the check failed. | 1.0.2 |
| Own data folder | MZGram keeps its data apart from Telegram Desktop, so both can be installed. | 1.0.0 |
| MZGram name | Window titles, the About box ("MZGram Desktop X.Y.Z, an unofficial client based on Telegram Desktop", GPL, GitHub), menus, tray, notifications, the Windows app id and Start menu shortcut, and the exe properties say MZGram, in every language. The Telegram service keeps its name. | 1.0.1 |

### Releases

| Feature | What it does | Since |
|---|---|---|
| Windows x64 and ARM64 | A zip for each, with `MZGram.exe`. | 1.0.0 |
| Linux x64 | A tar.xz, built against glibc 2.28 so it runs on current distributions. | 1.0.0 |
| Linux ARM64 | A tar.xz for aarch64 machines such as Snapdragon laptops. | 1.0.1 |
| Source archive and checksums | Every release has the complete source (`-source.tar.xz`) and `SHA256SUMS`. | 1.0.0 |

## Android and Desktop compared

Yes: present. No: absent. Partly: present in a narrower form.

| Feature | Android | Desktop |
|---|---|---|
| Deleted messages, edit history, view-once media kept | Yes | Yes |
| A chat with only deleted messages stays in the list | Yes | Yes (until a restart) |
| Archive export, import, wipe | Yes | Yes |
| Ghost mode: read receipts and typing | Yes | Yes |
| Ghost mode: online status | No | Yes |
| Ghost mode: delayed and silent sending, before stories | Yes | Yes |
| Read time, approximate last seen | Yes | Yes |
| Protected content | Yes | Yes |
| Message menu: Save, Reminder, Repeat, Open in, Details, QR | Yes | Yes |
| Message menu: Copy Photo, Delete Downloaded File, Forward without sender | Yes | No |
| Pin in MZGram only | Yes | Yes |
| Confirm voice and round video messages | Yes (one switch) | Yes (two switches) |
| Ask before calling | Yes | No |
| Instant camera and camera tile switches | Yes | No |
| Original video quality | Yes | No |
| Folder tabs at the bottom, no bottom bar | Yes | No |
| Accounts in the menu, one tap to switch | Yes | Yes (Telegram Desktop's side menu) |
| Exact numbers, animation switches | Yes | No |
| Seconds in time, hide stories, no greeting sticker, channel button | Yes | Yes |
| No sponsored messages, Zalgo filter | Yes | Yes |
| Standard color themes without the server | Yes | No (Telegram Desktop has them built in) |
| 1 GB cache limit | Yes | No (Telegram Desktop already offers it) |
| UnifiedPush notifications | Yes | No |
| Interface in 12 languages, complete in each, chosen apart from Telegram's | Yes | Yes |
| MZGram update banner, Telegram's updater off | Yes | Yes |
| Check for updates by hand, with the result | Yes | Yes |
| Self-update from GitHub with a SHA-256 check | Yes | Yes |
| MZGram name everywhere | Yes | Yes |
| ARM64 builds | Yes (the APK) | Yes |
