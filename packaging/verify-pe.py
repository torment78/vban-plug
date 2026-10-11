"""Verify Authenticode signing preserved the original executable payload."""
import struct
import sys
from pathlib import Path

before = bytearray(Path(sys.argv[1]).read_bytes())
after = bytearray(Path(sys.argv[2]).read_bytes())
pe = struct.unpack_from('<I', before, 0x3c)[0]
optional = pe + 24
magic = struct.unpack_from('<H', before, optional)[0]
assert magic in (0x10b, 0x20b), 'Expected PE executable'
security = optional + (112 if magic == 0x20b else 96) + 8 * 4
assert struct.unpack_from('<II', before, security) == (0, 0), 'Expected unsigned input'
start, length = struct.unpack_from('<II', after, security)
assert start >= len(before) and start + length == len(after), 'Unexpected certificate range'
assert all(v == 0 for v in after[len(before):start]), 'Unexpected extra content'
for pos, count in ((optional + 64, 4), (security, 8)):
    before[pos:pos + count] = b'\0' * count
    after[pos:pos + count] = b'\0' * count
assert before == after[:len(before)], 'Executable content changed during signing'
print('PASS: original executable bytes preserved; only signature metadata and certificate appended.')
