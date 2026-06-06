#!/usr/bin/env python3
"""Mark unused pins (10-14) on display FPC connector with NOT net."""

import re

# Read the PCB file
with open(r'C:\Users\JGril0\Desktop\CyberGame\hardware\pcb\PCB\PCB.kicad_pcb', 'r') as f:
    content = f.read()

# Find and replace the unused display pads (pads 10-14 in the display FPC at 93.08 113.78)
# These currently have no net attribute, we need to add (net "NOT")

# Pattern for pad "10" in the display connector (around line 3053)
# (pad "10" smd rect
#   (at 1.25 -1.35 270)
#   (size 0.3 1.1)
#   (layers "F.Cu" "F.Mask" "F.Paste")
#   (uuid "...")
# )
# We need to add (net "NOT") before the uuid

display_connector_pattern = r'(pad "([10-9]|1[0-4])" smd rect\s*\(at [^)]+\)\s*\(size [^)]+\)\s*\(layers [^)]+\))(\s*\(uuid)'

def replace_unused_pads(match):
    pad_num = match.group(2)
    prefix = match.group(1)
    uuid_part = match.group(3)

    # Mark pads 10-14 with "NOT"
    if pad_num in ['10', '11', '12', '13', '14']:
        return f'{prefix}\n\t\t\t\t(net "NOT"){uuid_part}'
    else:
        # Pads 1-9 keep their original nets
        return match.group(0)

# Apply the pattern - but we need to be more careful and target just the display connector
# Let me find the display connector first and apply the change only there

# Better approach: find the display connector section (starts with uuid 9331cd20-c48a-4678-8e68-a1d26e65a9ff)
# and replace the unused pads within that section

display_connector_start = content.find('(uuid "9331cd20-c48a-4678-8e68-a1d26e65a9ff")')
if display_connector_start == -1:
    print("ERROR: Display connector UUID not found!")
    exit(1)

# Find the end of this footprint (next (footprint or end of (kicad_pcb))
next_footprint_start = content.find('\n\t(footprint', display_connector_start + 100)
if next_footprint_start == -1:
    next_footprint_start = content.find('\n\t(footprint', display_connector_start + 100)

footprint_section = content[display_connector_start:next_footprint_start]

# Replace unused pads in this section
# Pattern for pads 10-14 that currently have no net
pad_pattern = r'(\(pad "([10-9]|1[0-4])" smd rect\s*\(at [^)]+\)\s*\(size [^)]+\)\s*\(layers "[^"]+"\))(\s*\(uuid)'

def replace_in_display(match):
    pad_num = match.group(2)
    prefix = match.group(1)
    uuid_part = match.group(3)

    if pad_num in ['10', '11', '12', '13', '14']:
        return f'{prefix}\n\t\t\t\t(net "NOT"){uuid_part}'
    return match.group(0)

new_footprint_section = re.sub(pad_pattern, replace_in_display, footprint_section)

# Replace in original content
content = content[:display_connector_start] + new_footprint_section + content[next_footprint_start:]

# Write back
with open(r'C:\Users\JGril0\Desktop\CyberGame\hardware\pcb\PCB\PCB.kicad_pcb', 'w') as f:
    f.write(content)

print("[OK] Marked display FPC pins 10-14 as NOT")
