#!/usr/bin/env python3
"""Exercise every descriptor initializer and its default-message wire round trip."""

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
header = (root / "protobuf/pg_query.pb-c.h").read_text()
initializers = re.findall(r"void\s+(\w+__init)\s*\(\s*(\w+)\s*\*message\s*\);", header)
if not initializers:
    raise SystemExit("No public protobuf-c initializers found")
source = '''/*
 * Unified Query Algebra
 *
 * Copyright (c) 2023-2026 Cognica, Inc.
 */
#include "protobuf/pg_query.pb-c.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
'''
for function, message_type in initializers:
    prefix = function.removesuffix("__init")
    source += f'''  {{
    {message_type} expected;
    {message_type} *actual = malloc(sizeof(*actual));
    assert(actual != NULL);
    {function}(&expected);
    memset(actual, 0x5a, sizeof(*actual));
    protobuf_c_message_init(&{prefix}__descriptor, actual);
    assert(actual->base.descriptor == expected.base.descriptor);
    size_t expected_size = protobuf_c_message_get_packed_size(&expected.base);
    size_t actual_size = protobuf_c_message_get_packed_size(&actual->base);
    assert(expected_size == actual_size);
    unsigned char *expected_bytes = malloc(expected_size + 1);
    unsigned char *actual_bytes = malloc(actual_size + 1);
    assert(expected_bytes != NULL && actual_bytes != NULL);
    assert(protobuf_c_message_pack(&expected.base, expected_bytes) == expected_size);
    assert(protobuf_c_message_pack(&actual->base, actual_bytes) == actual_size);
    assert(memcmp(expected_bytes, actual_bytes, actual_size) == 0);
    ProtobufCMessage *unpacked = protobuf_c_message_unpack(
        &{prefix}__descriptor, NULL, expected_size, expected_bytes);
    assert(unpacked != NULL);
    assert(unpacked->descriptor == expected.base.descriptor);
    assert(protobuf_c_message_get_packed_size(unpacked) == expected_size);
    assert(protobuf_c_message_pack(unpacked, actual_bytes) == expected_size);
    assert(memcmp(expected_bytes, actual_bytes, expected_size) == 0);
    protobuf_c_message_free_unpacked(unpacked, NULL);
    free(expected_bytes);
    free(actual_bytes);
    free(actual);
  }}
'''
source += "  return 0;\n}\n"
with tempfile.TemporaryDirectory(prefix="pg-query-initializers-") as temporary:
    work = Path(temporary)
    fixture, executable = work / "initializers.c", work / "initializers"
    fixture.write_text(source)
    compiler = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(compiler + flags + ["-I" + str(root), "-I" + str(root / "vendor"),
                   str(fixture), str(root / "libpg_query.a"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print(f"All {len(initializers)} descriptor initializers and default-message wire round trips match the typed public initializer defaults")
