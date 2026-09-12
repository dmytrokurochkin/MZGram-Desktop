# MZGram Desktop

A fork of [Telegram Desktop](https://github.com/telegramdesktop/tdesktop).

## Relation to upstream

`upstream/master` is tracked unmodified. All fork work lives on the `mzgram`
branch as commits on top of an upstream release tag, each prefixed `[mzgram]`.
To list everything this fork changes:

```
git log --grep='\[mzgram\]'
```

Updating means rebasing onto a newer upstream tag rather than merging, so the
fork stays a readable set of patches instead of an entangled history.

## Building

Windows x64 and ARM64 builds run in CI (`.github/workflows/mzgram-windows.yml`).
The upstream workflows in this directory are inherited from the parent project
and are not used here.

Local builds follow the upstream instructions in `docs/building-win.md`, with
one addition: on an ARM64 host, initialize the shell with `vcvarsarm64.bat` and
pass `arm` to `configure.bat`. The `-vcvars_ver=14.44` toolset that upstream
requires exists to preserve Windows 7 support and does not apply to ARM64.

Building requires your own `api_id` and `api_hash` from
https://my.telegram.org/apps. Credentials are never committed; CI reads them
from repository secrets.

## License

GPLv3 with the OpenSSL exception, inherited from upstream. See `LICENSE` and
`LEGAL`. Distributing a modified binary obliges you to publish its source under
the same terms.

The Telegram name and logo are trademarks of Telegram and are not covered by
that license.
