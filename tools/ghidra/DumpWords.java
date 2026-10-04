// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript DumpWords.java <out_file> <addr_hex> <n_palabras>  -> n palabras de 32 bits desde addr; si la palabra es una dirección de función, la marca (tablas de funciones/vtables)
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import java.io.*;

public class DumpWords extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs(); PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        Address at = toAddr(Long.parseLong(a[1], 16)); int n = Integer.parseInt(a[2]);
        for (int i = 0; i < n; i++, at = at.add(4)) {
            long v = currentProgram.getMemory().getInt(at) & 0xffffffffL; ghidra.program.model.listing.Function f = v < 0x10000000L ? getFunctionAt(toAddr(v)) : null;
            w.println(at + "  " + String.format("%08x", v) + (f != null ? "  " + f.getName() : ""));
        }
        w.close();
    }
}
