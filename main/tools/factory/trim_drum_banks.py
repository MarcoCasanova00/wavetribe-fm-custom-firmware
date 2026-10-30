#!/usr/bin/env python3
"""
Trim drum sample banks so the firmware fits the hardware XIP flash region.

The original samples are 16-bit and can be up to 20000 samples each; the six
factory kits alone are ~730 KB, which overflows the FM-1's XIP region
(0x8DFBC ~ 568 KB). This caps every drum array to MAX_SMP samples (keeping the
attack/body, dropping the decay tail) and rewrites the bank tables' counts.

Run:  python3 tools/factory/trim_drum_banks.py [max_samples]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MAX_SMP = int(sys.argv[1]) if len(sys.argv) > 1 else 6800

ARRAY = re.compile(
    r'static const int16_t (\w+)\[(\d+)\] = \{(.*?)\};',
    re.S)
COUNT = re.compile(r'\{ (\d+)u, (\w+) \}')
def truncate(path: Path):
    txt = path.read_text()
    lens = {}

    def fix_array(m):
        name, size, body = m.group(1), int(m.group(2)), m.group(3)
        vals = [int(x) for x in re.findall(r'-?\d+', body)]
        vals = vals[:MAX_SMP]
        lens[name] = len(vals)
        lines = []
        for i in range(0, len(vals), 12):
            lines.append('    ' + ', '.join(str(v) for v in vals[i:i + 12]) + ',')
        return 'static const int16_t %s[%d] = {\n%s\n};' % (name, len(vals), '\n'.join(lines))

    txt = ARRAY.sub(fix_array, txt)

    def fix_count(m):
        return '{ %du, %s }' % (lens[m.group(2)], m.group(2))

    txt = COUNT.sub(fix_count, txt)
    path.write_text(txt)
    print('trimmed %s -> %d arrays, max %d smp' % (path.name, len(lens), MAX_SMP))
if __name__ == '__main__':
    truncate(ROOT / 'main/firmware/gen/factory_drum_kits.h')
    truncate(ROOT / 'main/firmware/gen/abuse_drum_kit.h')
