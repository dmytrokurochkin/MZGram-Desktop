# MZGram Desktop

MZGram is an unofficial Telegram client for desktop that keeps deleted and edited messages, adds a ghost mode and many small extras.

> **Disclaimer.** MZGram is an unofficial client. It is not affiliated with, endorsed by or supported by Telegram. It is a fork of [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) (the Android version is a fork of [Telegram for Android](https://github.com/DrKLO/Telegram): [MZGram for Android](https://github.com/dmytrokurochkin/MZGram-Android)). MZGram uses its own name and its own `api_id`. The Telegram name and logo are trademarks of Telegram, and MZGram does not use the Telegram logo as its own; the app icon is still the one inherited from upstream and is to be replaced.

Built in CI for Windows x64, Windows ARM64 and Linux x64.

## Features

All MZGram features are in **Settings > MZGram**, grouped by topic. Every MZGram text is available in English and Ukrainian.

### Archive of deleted and edited messages

- Keeps other people's deleted messages, earlier versions of their edited messages and their view-once media, in every chat. On by default.
- Deleted messages stay in the chat; the edit history of a message can be opened from it.
- Separate switches for media, formatting, reactions and chats with bots.
- Media of saved messages is copied to `Downloads/MZGram/Saved Attachments`, with no size limit and no total quota.
- Your own messages are never saved.
- Editable marks before the time of a deleted or edited message; deleted messages are drawn at 75% opacity (switchable).
- Clear the archive, export it to a file and import it back.
- Clear Telegram's local cache and restart, without touching the archive.

### Protected content

- In chats and channels that restrict saving content: forward, save and copy messages and media. Such messages are sent as new messages without the original sender.

### Ghost mode

- Separate switches for read receipts, typing status and online status.
- Delay sending outgoing messages, so sending right away does not show you online.
- Send every message without sound while ghost mode is on.
- Offer to turn ghost mode on before opening a story.

### Message menu

- Repeat, Save message (to Saved Messages), Set a reminder, Open in... (downloaded files).
- Message details: ids, dates, views, file and media of a message. For your own messages it shows when the other side read them.
- Scan for QR code: decodes a QR code in a downloaded photo, on the device.

### Media and calls

- Auto pause video in the media viewer.
- Confirm before sending voice messages and round videos.
- Media preview instead of Chat Preview on a chat's avatar.

### Interface

- Message times with seconds.
- Hide Stories.
- Disable the greeting sticker in empty chats.
- Hide the bottom button in channels where you cannot post.
- Open Archive on pull down.
- For people who hide their last seen, an approximate last seen from what this device saw.

### Ads and filters

- Disable sponsored messages in channels and the promo banner in the chat list.
- Zalgo filter: removes stacked combining marks from names, chat titles and message text.

## Download

Releases will be published on the [Releases](https://github.com/dmytrokurochkin/MZGram-Desktop/releases) page. There are no releases yet.

## Build

The working branch is `mzgram`.

1. Get your own `api_id` and `api_hash` at https://my.telegram.org/apps (see [docs/api_credentials.md](docs/api_credentials.md)). Do not commit them.
2. Follow the upstream instructions for your system and pass your credentials to `configure`:
   ```
   configure.bat x64 -D TDESKTOP_API_ID=YOUR_API_ID -D TDESKTOP_API_HASH=YOUR_API_HASH
   ```
   - [Windows](docs/building-win.md). On an ARM64 host, initialize the shell with `vcvarsarm64.bat` and pass `arm` to `configure.bat`.
   - [GNU/Linux using Docker](docs/building-linux.md)
   - [macOS](docs/building-mac.md) (not built or tested for MZGram)

CI builds Windows x64, Windows ARM64 and Linux x64 on every push to `mzgram` (`.github/workflows/mzgram-windows.yml`, `.github/workflows/mzgram-linux.yml`). It reads the credentials from the repository secrets `TELEGRAM_API_ID` and `TELEGRAM_API_HASH`. The other workflows in `.github/workflows` are inherited from upstream and are not used. More about the fork is in [MZGRAM.md](MZGRAM.md).

<details>
<summary>Links from the upstream README</summary>

- Telegram API: https://core.telegram.org
- MTProto protocol: https://core.telegram.org/mtproto
- Obtaining an api_id: https://core.telegram.org/api/obtaining_api_id
- Official Telegram Desktop downloads and supported systems: https://desktop.telegram.org

</details>

## Upstream

MZGram Desktop is based on Telegram Desktop **7.2.8** from [telegramdesktop/tdesktop](https://github.com/telegramdesktop/tdesktop). Upstream updates are taken from that repository: the `mzgram` branch is rebased onto a newer upstream release tag, so the fork stays a readable set of patches on top of it.

Every fork commit has the `[mzgram]` prefix. To list all changes against upstream:

```bash
git log --grep='^\[mzgram\]'
```

## License

MZGram Desktop is free software under the GNU General Public License v3 with the OpenSSL exception, inherited from Telegram Desktop. See [LICENSE](LICENSE) and [LEGAL](LEGAL). If you distribute a modified build, you must publish its source code under the same license.

MZGram for Android is licensed under GPL v2, see its [repository](https://github.com/dmytrokurochkin/MZGram-Android).

## Third-party

* Qt 6 ([LGPL](http://doc.qt.io/qt-6/lgpl.html)) and Qt 5.15 ([LGPL](http://doc.qt.io/qt-5/lgpl.html)) slightly patched
* OpenSSL 3.2.1 ([Apache License 2.0](https://openssl-library.org/source/license/apache-license-2.0.txt))
* WebRTC ([New BSD License](https://github.com/desktop-app/tg_owt/blob/master/LICENSE))
* zlib ([zlib License](http://www.zlib.net/zlib_license.html))
* LZMA SDK 9.20 ([public domain](http://www.7-zip.org/sdk.html))
* liblzma ([public domain](http://tukaani.org/xz/))
* Google Breakpad ([License](https://chromium.googlesource.com/breakpad/breakpad/+/master/LICENSE))
* Google Crashpad ([Apache License 2.0](https://chromium.googlesource.com/crashpad/crashpad/+/master/LICENSE))
* GYP ([BSD License](https://github.com/bnoordhuis/gyp/blob/master/LICENSE))
* Ninja ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))
* OpenAL Soft ([LGPL](https://github.com/kcat/openal-soft/blob/master/COPYING))
* Opus codec ([BSD License](http://www.opus-codec.org/license/))
* FFmpeg ([LGPL](https://www.ffmpeg.org/legal.html))
* Guideline Support Library ([MIT License](https://github.com/Microsoft/GSL/blob/master/LICENSE))
* Range-v3 ([Boost License](https://github.com/ericniebler/range-v3/blob/master/LICENSE.txt))
* Open Sans font ([Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0.html))
* Vazirmatn font ([SIL Open Font License 1.1](https://github.com/rastikerdar/vazirmatn/blob/master/OFL.txt))
* Emoji alpha codes ([MIT License](https://github.com/emojione/emojione/blob/master/extras/alpha-codes/LICENSE.md))
* xxHash ([BSD License](https://github.com/Cyan4973/xxHash/blob/dev/LICENSE))
* QR Code generator ([MIT License](https://github.com/nayuki/QR-Code-generator#license))
* CMake ([New BSD License](https://github.com/Kitware/CMake/blob/master/Copyright.txt))
* Hunspell ([LGPL](https://github.com/hunspell/hunspell/blob/master/COPYING.LESSER))
* Ada ([Apache License 2.0](https://github.com/ada-url/ada/blob/main/LICENSE-APACHE))
* quirc ([ISC License](Telegram/SourceFiles/mzgram/quirc/LICENSE-quirc.txt)), used by MZGram for QR code scanning

## Credits

- [Telegram](https://telegram.org) and the Telegram Desktop Authors, on whose code MZGram is built.
