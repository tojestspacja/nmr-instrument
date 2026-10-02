#!/usr/bin/env python3
"""Inspect a local NMR checkout for handoff. Standard library only; JSON to stdout.

No network, source/Git writes, builds, installation, flashing, or credential reads.
Exit 0: continuation files found (UNVERIFIED); 3: recovery needed; 2: bad input.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

ARCHIVE_HEAD = "0914edbc5f9424503e6046394dd632a815474fc3"
REQUIRED_V2 = (
    "design/instrument.yaml",
    "tools/gen_config.py",
    "CMakeLists.txt",
    "physics/include/nmr/capi.h",
    "physics/src/pipeline.cpp",
    "pulse/include/pulse/backend.hpp",
    "pulse/src/backend.cpp",
    "firmware/platformio.ini",
    "firmware/components/engine/engine.cpp",
    "firmware/components/adc/adc_stream.cpp",
    "firmware/components/rf/rf.cpp",
)
OTHER_KEYS = (
    "README.md", "docs/audit.md", "docs/CURRENT_STATE.md",
    "docs/PROJECT_CONTRACT.md", "docs/AGENT_PLAN.md",
    "generated/instrument_config.hpp", "generated/instrument_config.json",
    "generated/instrument_config.py", "tests/test_config.py",
    "tests/test_golden.py", "tests/test_wasm_parity.py",
    "simulator/web/nmrcore.js", "simulator/web/nmrcore.wasm",
    "simulator/wasm/build.py", "physics/tests/test_backend.cpp",
    "firmware/CMakeLists.txt", "firmware/sdkconfig.defaults",
    "firmware/main/CMakeLists.txt", "firmware/main/main.cpp",
    "firmware/main/main.c", "firmware/components/rf/CMakeLists.txt",
    "legacy/README.md", "legacy/simulator/physics.js",
    "simulator/physics.js", "mechanical/probe.scad", "hardware/README.md",
)


def file_info(root: Path, rel: str) -> dict[str, Any]:
    """Hash only explicitly selected regular files inside this project root."""
    result: dict[str, Any] = {"path": rel, "exists": False}
    p = root / rel
    try:
        if not p.exists():
            return result
        result["exists"] = True
        try:
            p.resolve().relative_to(root)
        except ValueError:
            result["error"] = "external symlink target: not read"
            return result
        if not p.is_file():
            result["error"] = "not a regular file"
            return result
        result["size_bytes"] = p.stat().st_size
        h = hashlib.sha256()
        with p.open("rb") as f:
            for chunk in iter(lambda: f.read(1024 * 1024), b""):
                h.update(chunk)
        result["sha256"] = h.hexdigest()
    except OSError as exc:
        result["error"] = f"{type(exc).__name__}: {exc}"
    return result


def git_query(root: Path, args: list[str]) -> dict[str, Any]:
    """Run only the read-only commands fixed by inspect(); never use a shell."""
    git = shutil.which("git")
    if not git:
        return {"args": args, "exit_code": None, "stdout": "", "stderr": "git not available"}
    env = os.environ.copy()
    for key in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_OBJECT_DIRECTORY",
                "GIT_ALTERNATE_OBJECT_DIRECTORIES"):
        env.pop(key, None)
    env["GIT_OPTIONAL_LOCKS"] = "0"
    env["GIT_TERMINAL_PROMPT"] = "0"
    command = [git, "--no-optional-locks", "-c", f"safe.directory={root}",
               "-c", "core.fsmonitor=false", "-c", "core.quotePath=false",
               "-c", f"core.hooksPath={root / '.handoff-no-hooks'}", "-C", str(root), *args]
    try:
        p = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True,
                           text=True, encoding="utf-8", errors="replace", env=env, timeout=20,
                           check=False)
        return {"args": args, "exit_code": p.returncode,
                "stdout": p.stdout, "stderr": p.stderr}
    except subprocess.TimeoutExpired:
        return {"args": args, "exit_code": None, "stdout": "", "stderr": "query timed out after 20 seconds"}
    except OSError as exc:
        return {"args": args, "exit_code": None, "stdout": "", "stderr": str(exc)}


def inspect(root: Path) -> tuple[dict[str, Any], int]:
    root = root.expanduser().resolve()
    if not root.is_dir():
        return {"schema_version": 1, "error": "project directory does not exist", "root": str(root)}, 2
    keys = {rel: file_info(root, rel) for rel in (*REQUIRED_V2, *OTHER_KEYS)}
    present = [rel for rel in REQUIRED_V2
               if keys[rel].get("sha256") and keys[rel].get("size_bytes", 0) > 0]
    missing = [rel for rel in REQUIRED_V2 if rel not in present]
    # Do not accidentally inspect an unrelated parent repository for an extracted directory.
    git_marker = root / ".git"
    has_git = git_marker.is_dir() or git_marker.is_file()
    queries: dict[str, Any] = {}
    if has_git:
        commands = {
            "head": ["rev-parse", "HEAD"],
            "branch": ["branch", "--show-current"],
            "refs": ["for-each-ref", "--format=%(refname) %(objectname)", "refs/heads", "refs/remotes"],
            "status": ["status", "--porcelain=v1", "--untracked-files=all", "--ignore-submodules=all"],
            "log": ["log", "-12", "--format=%H %cI %s"],
            "worktrees": ["worktree", "list", "--porcelain"],
            "reflog": ["reflog", "--all", "-20", "--format=%H %gs"],
        }
        queries = {label: git_query(root, args) for label, args in commands.items()}
    head = queries.get("head", {}).get("stdout", "").strip()
    refs = queries.get("refs", {}).get("stdout", "")
    if not missing:
        classification, exit_code = "V2_FILES_PRESENT_UNVERIFIED", 0
        next_step = "Protect WIP, read the live interfaces, run baseline checks, then continue firmware integration. File presence is not build/test validation."
    elif present:
        classification, exit_code = "PARTIAL_V2_OR_MIXED_TREE", 3
        next_step = "Preserve all existing work; locate missing files or the correct worktree before integration."
    elif "refs/heads/v2 " in refs or "refs/remotes/origin/v2 " in refs:
        classification, exit_code = "V2_REF_NOT_CHECKED_OUT", 3
        next_step = "Inspect the v2 worktree/ref safely after preserving current WIP; do not force-switch a dirty tree."
    elif head == ARCHIVE_HEAD and keys["simulator/physics.js"]["exists"]:
        classification, exit_code = "LEGACY_SNAPSHOT_ONLY", 3
        next_step = "Use RECOVERY_PLAN.md. The supplied legacy snapshot cannot establish the recorded M4 working files."
    else:
        classification, exit_code = "SOURCE_STATE_UNESTABLISHED", 3
        next_step = "Verify the actual project root and recover the intended v2 state without overwriting existing files."
    cfg_hash = None
    cfg = root / "generated/instrument_config.json"
    if keys["generated/instrument_config.json"].get("sha256"):
        try:
            payload = json.loads(cfg.read_text(encoding="utf-8-sig"))
            cfg_hash = payload.get("meta", {}).get("config_sha256")
        except (OSError, ValueError, AttributeError):
            pass
    report = {
        "schema_version": 1,
        "observed_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "root": str(root), "classification": classification, "exit_code": exit_code,
        "git_marker_present": has_git, "git_observations": queries,
        "required_v2_present": present, "required_v2_missing_or_unreadable": missing,
        "key_files": list(keys.values()), "declared_generated_config_hash": cfg_hash,
        "directories": {rel: (root / rel).is_dir() for rel in
                        ("legacy", "design/adr", "validation/legacy-audit", "firmware/main", "build")},
        "tool_paths_only": {tool: shutil.which(tool) for tool in
                            ("git", "cmake", "ninja", "node", "pio", "clang++", "g++")},
        "python": sys.version.split()[0], "platform": platform.platform(),
        "next_step": next_step,
        "limits": [
            "This is a local source-state observation, not a test of instrument function.",
            "No source, Git refs/index/config, credentials or hardware were modified.",
            "No network, package installation, builds or tests were run.",
            "Tool paths do not establish tool versions or reproducibility.",
            "No account/session/process state was transferred or assumed.",
        ],
    }
    return report, exit_code


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=Path.cwd(), help="Actual nmr-instrument root (default: current directory)")
    args = ap.parse_args()
    report, code = inspect(args.root)
    # ASCII JSON works in legacy Windows consoles, while retaining Unicode via escapes.
    print(json.dumps(report, indent=2, ensure_ascii=True))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
