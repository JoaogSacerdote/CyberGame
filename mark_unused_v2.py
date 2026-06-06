#!/usr/bin/env python3
"""Mark unused display FPC pins 10-14 with NOT net - simpler approach."""

# Read file
with open(r'C:\Users\JGril0\Desktop\CyberGame\hardware\pcb\PCB\PCB.kicad_pcb', 'r') as f:
    lines = f.readlines()

# Find and mark the display FPC connector pads 10-14
# They are in the section at line ~3053-3082
replacements = {
    '(pad "10" smd rect\n\t\t\t\t(at 1.25 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(uuid "cdd3cf08-ac5d-49b9-9fc0-bbfc99fbd601")\n\t\t\t)':
        '(pad "10" smd rect\n\t\t\t\t(at 1.25 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(net "NOT")\n\t\t\t\t(uuid "cdd3cf08-ac5d-49b9-9fc0-bbfc99fbd601")\n\t\t\t)',
    '(pad "11" smd rect\n\t\t\t\t(at 1.75 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(uuid "ca06f891-4e8c-4261-b97b-87579f6518b5")\n\t\t\t)':
        '(pad "11" smd rect\n\t\t\t\t(at 1.75 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(net "NOT")\n\t\t\t\t(uuid "ca06f891-4e8c-4261-b97b-87579f6518b5")\n\t\t\t)',
    '(pad "12" smd rect\n\t\t\t\t(at 2.25 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(uuid "cd7c1b2e-86f6-45e5-b251-4e119283f9f1")\n\t\t\t)':
        '(pad "12" smd rect\n\t\t\t\t(at 2.25 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(net "NOT")\n\t\t\t\t(uuid "cd7c1b2e-86f6-45e5-b251-4e119283f9f1")\n\t\t\t)',
    '(pad "13" smd rect\n\t\t\t\t(at 2.75 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(uuid "502941f9-2fc8-45c5-a27b-26058040118e")\n\t\t\t)':
        '(pad "13" smd rect\n\t\t\t\t(at 2.75 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(net "NOT")\n\t\t\t\t(uuid "502941f9-2fc8-45c5-a27b-26058040118e")\n\t\t\t)',
    '(pad "14" smd rect\n\t\t\t\t(at 3.25 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(uuid "ea19926b-0cc4-4eaf-885c-f0923a47c38f")\n\t\t\t)':
        '(pad "14" smd rect\n\t\t\t\t(at 3.25 -1.35 270)\n\t\t\t\t(size 0.3 1.1)\n\t\t\t\t(layers "F.Cu" "F.Mask" "F.Paste")\n\t\t\t\t(net "NOT")\n\t\t\t\t(uuid "ea19926b-0cc4-4eaf-885c-f0923a47c38f")\n\t\t\t)',
}

content = ''.join(lines)

for old, new in replacements.items():
    content = content.replace(old, new)

with open(r'C:\Users\JGril0\Desktop\CyberGame\hardware\pcb\PCB\PCB.kicad_pcb', 'w') as f:
    f.write(content)

print("[OK] Marked display FPC pins 10-14 as NOT")
