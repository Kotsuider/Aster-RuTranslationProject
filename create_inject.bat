@echo off
py -3.12 g2dump.py from-xlsx ORIG_STR aster.xlsx Translated
G2CryptTool.exe inject ORIG Translated script.pak
pause