# Lists the error branches that no test reaches (doc/plan-error-branch-coverage.md).
# The rule "Every error branch is marked unlikely" makes each [[unlikely]]
# the start of one error branch. gcov-16 shows whether the body ran.
# Usage: unreached_error_branches.py <source root> <prefix in gcov> <gcov json>...
import json
import pathlib
import re
import sys

SOURCE_DIRECTORIES = ("src", "include", "program")
EXIT = re.compile(r"\breturn\b|\bthrow\b|std::unexpected\(|std::terminate\(|std::abort\(")


def read_counts(paths, prefix):
    counts = {}
    for p in paths:
        for text in pathlib.Path(p).read_text().splitlines():
            if not text.strip():
                continue
            for f in json.loads(text)["files"]:
                name = f["file"]
                if not name.startswith(prefix):
                    continue
                lines = counts.setdefault(name[len(prefix):].lstrip("/"), {})
                for l in f["lines"]:
                    lines[l["line_number"]] = lines.get(l["line_number"], 0) + l["count"]
    return counts


# A brace inside a string or a character literal does not count. The
# tree holds no raw string with a brace in a marked branch.
def strip_literals(line):
    return re.sub(r"'(\\.|[^'])'|\"(\\.|[^\"])*\"", "", line.split("//")[0])


def block_end(lines, start):
    depth = 0
    for i in range(start, len(lines)):
        text = strip_literals(lines[i])
        if i == start:
            text = text.split("[[unlikely]]")[-1]
        for c in text:
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return i
    return None


def classify(lines, counts, i):
    start = i
    while start < len(lines) and "{" not in strip_literals(lines[start]).split("[[unlikely]]")[-1]:
        start += 1
    end = block_end(lines, start)
    if end is None:
        return "not decided", []
    body = [n for n in range(start + 2, end + 1) if n in counts]
    if not body:
        if (i + 1) in counts:
            return "not decided", []
        return "not instantiated", []
    if counts[body[0]] == 0:
        return "not reached", []
    missed = [n for n in body if counts[n] == 0 and EXIT.search(lines[n - 1])]
    return "reached", missed


def unmarked_exits(root, counts):
    rows = []
    for d in SOURCE_DIRECTORIES:
        for p in sorted((root / d).rglob("*")):
            if p.suffix not in (".cpp", ".hpp"):
                continue
            rel = str(p.relative_to(root))
            lines = p.read_text().splitlines()
            for i, l in enumerate(lines):
                if not re.search(r"\bthrow\b|std::unexpected\(|\bcatch\s*\(", l):
                    continue
                if l.strip().startswith("//"):
                    continue
                if "[[unlikely]]" in " ".join(lines[max(0, i - 3):i + 1]):
                    continue
                c = counts.get(rel, {}).get(i + 1)
                state = "not instantiated" if c is None else ("reached" if c else "not reached")
                rows.append((rel, i + 1, state, l.strip()[:80]))
    return rows


def main():
    root = pathlib.Path(sys.argv[1])
    counts = read_counts(sys.argv[3:], sys.argv[2])
    total = {}
    print("## Markers\n")
    for d in SOURCE_DIRECTORIES:
        for p in sorted((root / d).rglob("*")):
            if p.suffix not in (".cpp", ".hpp"):
                continue
            rel = str(p.relative_to(root))
            lines = p.read_text().splitlines()
            rows = []
            for i, l in enumerate(lines):
                if "[[unlikely]]" not in l:
                    continue
                state, missed = classify(lines, counts.get(rel, {}), i)
                total[state] = total.get(state, 0) + 1
                if state != "reached" or missed:
                    rows.append((i + 1, state, ", ".join(map(str, missed)), l.strip()[:80]))
            if rows:
                print(f"### {rel}\n")
                print("| line | state | exits not reached | text |")
                print("|---|---|---|---|")
                for r in rows:
                    print(f"| {r[0]} | {r[1]} | {r[2]} | `{r[3].replace('|', '\\|')}` |")
                print()
    print("## Totals\n")
    for k, v in sorted(total.items()):
        print(f"- {k}: {v}")
    print("\n## Error exits with no marker\n")
    print("| place | state | text |")
    print("|---|---|---|")
    for r in unmarked_exits(root, counts):
        print(f"| {r[0]}:{r[1]} | {r[2]} | `{r[3].replace('|', '\\|')}` |")


main()
