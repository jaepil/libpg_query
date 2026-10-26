#!/usr/bin/env python3
"""Emit type-correct descriptor callbacks after protobuf-c code generation."""

import argparse
from pathlib import Path
import re

INITIALIZER = re.compile(
    r"^(void[ \t]+(?P<name>\w+__init)\s+"
    r"\((?P<type>\w+)[ \t]+\*message\)\n\{[^}]*\n\})", re.M)


def adapt(source):
    initializers = list(INITIALIZER.finditer(source))
    if not initializers:
        raise ValueError("No typed protobuf-c initializers found")
    # Work backwards so insertions leave earlier match offsets valid.
    for match in reversed(initializers):
        name, message_type = match.group("name", "type")
        callback = name + "_callback"
        adapter = (f"\nstatic void {callback}(ProtobufCMessage *message)\n{{\n"
                   f"  {name}(({message_type} *) message);\n}}")
        if not source[match.end():].startswith(adapter):
            if re.search(r"\b" + callback + r"\s*\(", source):
                raise ValueError(f"Unexpected existing callback: {callback}")
            source = source[:match.end()] + adapter + source[match.end():]
    for match in initializers:
        name = match.group("name")
        cast = "(ProtobufCMessageInit) " + name
        callback = name + "_callback"
        if source.count(cast) == 1:
            source = source.replace(cast, callback)
        elif source.count("  " + callback + ",") != 1:
            raise ValueError(f"Missing or duplicated descriptor initializer: {name}")
    if "(ProtobufCMessageInit)" in source:
        raise ValueError("Unrecognized incompatible initializer cast")
    return source, len(initializers)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    source = args.source.read_text(encoding="utf-8")
    adapted, count = adapt(source)
    if args.check:
        if adapted != source:
            raise SystemExit("Regenerate the protobuf-c descriptor callbacks")
    else:
        args.source.write_text(adapted, encoding="utf-8")
    print(f"Verified {count} typed protobuf-c descriptor callbacks")


if __name__ == "__main__":
    main()
