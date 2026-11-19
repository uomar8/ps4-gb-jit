#!/usr/bin/env python3
import sys
import xml.etree.ElementTree as ET
from datetime import datetime, timezone

def main() -> None:
    if len(sys.argv) < 3:
        print("Usage: make_gp4.py <content_id> <output.gp4>")
        sys.exit(1)

    content_id = sys.argv[1]
    out_gp4 = sys.argv[2]
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%d")

    files = [
        ("eboot.bin", "eboot.bin"),
        ("sce_sys/param.sfo", "sce_sys/param.sfo"),
        ("sce_sys/icon0.png", "sce_sys/icon0.png"),
        ("sce_sys/pic0.png", "sce_sys/pic0.png"),
        ("sce_module/libc.prx", "sce_module/libc.prx"),
        ("sce_module/libSceFios2.prx", "sce_module/libSceFios2.prx"),
    ]

    lines = [
        '<?xml version="1.0" encoding="utf-8" standalone="yes"?>',
        '<psproject fmt="gp4" version="1000">',
        '  <volume>',
        '    <volume_type>pkg_ps4_app</volume_type>',
        '    <volume_id>PS4VOLUME</volume_id>',
        f'    <volume_ts>{ts} 00:00:00</volume_ts>',
        f'    <package content_id="{content_id}" passcode="00000000000000000000000000000000" storage_type="digital50" app_type="full"/>',
        '    <chunk_info chunk_count="1" scenario_count="1">',
        '      <chunks>',
        '        <chunk id="0" layer_no="0" label="Chunk #0"/>',
        '      </chunks>',
        '      <scenarios default_id="0">',
        '        <scenario id="0" type="sp" initial_chunk_count="1" label="Scenario #0">0</scenario>',
        '      </scenarios>',
        '    </chunk_info>',
        '  </volume>',
        '  <files img_no="0">',
    ]
    for targ, orig in files:
        lines.append(f'    <file targ_path="{targ}" orig_path="{orig}"/>')
    lines.extend([
        '  </files>',
        '  <rootdir>',
        '    <dir targ_name="sce_sys"/>',
        '    <dir targ_name="sce_module"/>',
        '  </rootdir>',
        '</psproject>',
        '',
    ])

    with open(out_gp4, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

if __name__ == "__main__":
    main()
