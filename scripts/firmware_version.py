#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright The Kabot Project Contributors

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

SEMVER_TAG_RE = re.compile(
    r"^v?(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)"
    r"(?:[-+].*)?$"
)


def run_git(args: list[str], *, quiet: bool = False) -> str:
    stderr = subprocess.DEVNULL if quiet else None
    return subprocess.check_output(["git", *args], text=True, stderr=stderr).strip()


def commit_count() -> int:
    return int(run_git(["rev-list", "--count", "HEAD"]))


def parse_tag(tag: str) -> tuple[int, int, int] | None:
    match = SEMVER_TAG_RE.match(tag)
    if match is None:
        return None

    return (
        int(match.group("major")),
        int(match.group("minor")),
        int(match.group("patch")),
    )


def current_branch() -> str:
    try:
        return run_git(["rev-parse", "--abbrev-ref", "HEAD"])
    except subprocess.CalledProcessError:
        return ""


def default_branch() -> str:
    try:
        # Example output: refs/remotes/origin/main
        ref = run_git(["symbolic-ref", "refs/remotes/origin/HEAD"], quiet=True)
    except subprocess.CalledProcessError:
        return ""

    return ref.rsplit("/", 1)[-1]


def sanitize_extra_version(name: str) -> str:
    # Zephyr EXTRAVERSION allows lowercase a-z, 0-9, dot and dash.
    sanitized = re.sub(r"[^a-z0-9.-]+", "-", name.lower())
    sanitized = re.sub(r"-+", "-", sanitized)
    sanitized = re.sub(r"\.+", ".", sanitized)
    return sanitized.strip(".-")


def describe_version() -> str:
    try:
        described = run_git(
            [
                "describe",
                "--tags",
                "--long",
                "--match",
                "v[0-9]*.[0-9]*.[0-9]*",
                "--match",
                "[0-9]*.[0-9]*.[0-9]*",
            ]
        )
    except subprocess.CalledProcessError:
        return f"0.0.0+{commit_count()}"

    tag, distance, _abbrev = described.rsplit("-", 2)
    parsed = parse_tag(tag)
    if parsed is None:
        return f"0.0.0+{commit_count()}"

    major, minor, patch = parsed
    return f"{major}.{minor}.{patch}+{int(distance)}"


def describe_version_fields() -> tuple[int, int, int, int, str]:
    version = describe_version()
    base, tweak = version.split("+", 1)
    major_str, minor_str, patch_str = base.split(".", 2)

    branch = current_branch()
    if branch in ("", "HEAD", default_branch()):
        extra = ""
    else:
        extra = sanitize_extra_version(branch)

    return int(major_str), int(minor_str), int(patch_str), int(tweak), extra


# Standard '-' is used for stdout, avoiding custom pre-processing.


def write_version_file(
    path: Path | None,
    *,
    major: int,
    minor: int,
    patch: int,
    tweak: int,
    extra: str,
) -> None:
    lines = [
        f"VERSION_MAJOR = {major}\n"
        f"VERSION_MINOR = {minor}\n"
        f"PATCHLEVEL = {patch}\n"
        f"VERSION_TWEAK = {tweak}\n",
    ]
    if extra:
        lines.append(f"EXTRAVERSION = {extra}\n")

    content = "".join(lines)

    if path is None:
        print(content, end="")
        return

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate application VERSION file from git tags.",
        allow_abbrev=False,
    )
    parser.add_argument(
        "--version-file",
        type=Path,
        help="Write a Zephyr VERSION file with VERSION_MAJOR/MINOR/PATCHLEVEL/TWEAK/EXTRAVERSION.",
    )
    args = parser.parse_args()

    version = describe_version()
    version_file = args.version_file

    if version_file is not None:
        major, minor, patch, tweak, extra = describe_version_fields()
        write_version_file(
            None if str(version_file) == "-" else version_file,
            major=major,
            minor=minor,
            patch=patch,
            tweak=tweak,
            extra=extra,
        )

    if version_file is None:
        print(version)


if __name__ == "__main__":
    main()
