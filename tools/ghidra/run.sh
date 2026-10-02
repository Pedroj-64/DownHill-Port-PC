#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Uso: tools/ghidra/run.sh Script.java args...   (imprime solo la salida println del script)
ghidra-analyzeHeadless ghidra_projects dh -process SLES_522.02 -noanalysis -scriptPath tools/ghidra -postScript "$@" 2>&1 | sed -n 's/^INFO  [A-Za-z]*\.java> \?//p'
