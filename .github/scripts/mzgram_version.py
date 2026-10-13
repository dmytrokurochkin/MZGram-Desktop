#!/usr/bin/env python3
"""MZGram version numbers from git tags (the same scheme as MZGram for Android).

Tags: v1.2.0 (stable) and v1.3.0-beta.2 (beta, a GitHub prerelease).
The code = MAJOR * 1000000 + MINOR * 10000 + PATCH * 100 + N, N = the beta
number (1..98) or 99 for the stable release, orders the tags: every beta of
a version comes before it and after the version before. Desktop has no
versionCode; the code only keeps a new release above every earlier tag.
The repository holds no upstream v* tags (upstream tags are not pushed here).

Subcommands (tags of the repo on stdin, one per line):
  release TAG --upstream 12.10.1 --floor 7038
      checks TAG and prints GitHub step outputs: version, code, name, prerelease
  dev --upstream 12.10.1 --floor 7038
      code and name for a build that is not a release: the code of the newest
      tag, so a push build installs over the last release and never below it
  previous TAG
      the tag the release notes of TAG start from
  notes
      commit subjects on stdin, the "Changes" list of the release notes out:
      the [mzgram] commits users see, without the internal CI:, Test:,
      Release workflow:, Docs: and Spec: ones, without any line naming
      another client, and without commit hashes in parentheses
"""

import argparse
import re
import sys

TAG = re.compile(r"^v(0|[1-9]\d*)\.(0|[1-9]\d?)\.(0|[1-9]\d?)(?:-beta\.([1-9]\d?))?$")
MAX_BASE = (2100000000 - 9) // 10  # Android's versionCode limit after * 10 + 9
INTERNAL = re.compile(r"^(CI|Test|Tests|Release workflow|Docs|Spec)\s*:")
NO_CHANGES = "- Build, test and CI changes only."
# Names of other clients never go into the notes, whatever case
FORBIDDEN = re.compile(r"ayu|neko|mercury", re.IGNORECASE)
# A commit hash in parentheses, e.g. "(0b331573e)" or "(1caf86c27, ec74fd73e8)"
HASHES = re.compile(r"\s*\((?:[0-9a-f]{7,40})(?:,\s*[0-9a-f]{7,40})*\)")


class VersionError(ValueError):
    pass


def parse(tag: str) -> tuple[int, int, int, int | None]:
    match = TAG.match(tag)
    if not match:
        raise VersionError(f"{tag}: expected vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-beta.N (MINOR, PATCH 0..99)")
    major, minor, patch = (int(part) for part in match.group(1, 2, 3))
    beta = int(match.group(4)) if match.group(4) else None
    if beta is not None and beta > 98:
        raise VersionError(f"{tag}: beta numbers go up to 98")
    return major, minor, patch, beta


def code(tag: str) -> int:
    major, minor, patch, beta = parse(tag)
    value = major * 1000000 + minor * 10000 + patch * 100 + (99 if beta is None else beta)
    if value > MAX_BASE:
        raise VersionError(f"{tag}: too large for an Android versionCode")
    return value


def version(tag: str) -> str:
    return tag[1:]


def valid_tags(tags: list[str]) -> list[str]:
    result = []
    for tag in tags:
        try:
            code(tag)
        except VersionError:
            continue
        result.append(tag)
    return result


def release(tag: str, upstream: str, floor: int, tags: list[str]) -> dict[str, str]:
    value = code(tag)
    if value <= floor:
        raise VersionError(f"{tag}: versionCode {value} is not above the current {floor}; use a higher version")
    others = [other for other in valid_tags(tags) if other != tag]
    higher = [other for other in others if code(other) >= value]
    if higher:
        raise VersionError(f"{tag}: versionCode {value} is not above {max(higher, key=code)} ({code(max(higher, key=code))})")
    return {
        "version": version(tag),
        "code": str(value),
        "name": f"{version(tag)} ({upstream})",
        "prerelease": "true" if parse(tag)[3] is not None else "false",
    }


def dev(upstream: str, floor: int, tags: list[str]) -> dict[str, str]:
    tags = valid_tags(tags)
    if not tags:
        return {"code": str(floor), "name": ""}
    newest = max(tags, key=code)
    return {"code": str(max(floor, code(newest))), "name": f"{version(newest)}-dev ({upstream})"}


def previous(tag: str, tags: list[str]) -> str:
    """A stable release lists the changes since the last stable one, a beta since the last tag."""
    value = code(tag)
    stable = parse(tag)[3] is None
    older = [other for other in valid_tags(tags) if code(other) < value and (not stable or parse(other)[3] is None)]
    return max(older, key=code) if older else ""


def notes(subjects: list[str]) -> list[str]:
    """The release notes list: one line per [mzgram] commit a user would notice."""
    lines = []
    for subject in subjects:
        subject = subject.strip()
        if not subject.startswith("[mzgram] "):
            continue
        text = subject[len("[mzgram] "):].strip()
        if not text or INTERNAL.match(text) or FORBIDDEN.search(text):
            continue
        text = HASHES.sub("", text).strip()
        if text:
            lines.append(f"- {text}")
    return lines or [NO_CHANGES]


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    rel = sub.add_parser("release")
    rel.add_argument("tag")
    rel.add_argument("--upstream", required=True)
    rel.add_argument("--floor", type=int, required=True)
    dv = sub.add_parser("dev")
    dv.add_argument("--upstream", required=True)
    dv.add_argument("--floor", type=int, required=True)
    prev = sub.add_parser("previous")
    prev.add_argument("tag")
    sub.add_parser("notes")
    args = parser.parse_args()

    if args.command == "notes":
        print("\n".join(notes(sys.stdin.read().splitlines())))
        return 0
    tags = [line.strip() for line in sys.stdin if line.strip()]
    # git ls-remote prints "<sha>\trefs/tags/v1.0.0" and "...^{}" for annotated tags
    tags = sorted({tag.split("refs/tags/")[-1].removesuffix("^{}") for tag in tags})
    try:
        if args.command == "release":
            out = release(args.tag, args.upstream, args.floor, tags)
        elif args.command == "dev":
            out = dev(args.upstream, args.floor, tags)
        else:
            print(previous(args.tag, tags))
            return 0
    except VersionError as exc:
        print(f"::error::{exc}")
        return 1
    for key, value in out.items():
        print(f"{key}={value}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
