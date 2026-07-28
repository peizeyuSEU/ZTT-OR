#!/usr/bin/env python3
"""Add -DCG_DIAG to Build.mk CXXFLAGS"""
with open('src/Build.mk', 'r') as f:
    lines = f.readlines()
for i, line in enumerate(lines):
    if 'CXXFLAGS' in line and '-DCG_DIAG' not in line:
        lines[i] = line.rstrip() + ' -DCG_DIAG\n'
        print(f"Modified line {i+1}: {lines[i]}")
        break
with open('src/Build.mk', 'w') as f:
    f.writelines(lines)
print("Done")