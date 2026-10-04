// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript FindPtr.java <out_file> <valor_hex>...  -> direcciones donde aparece esa palabra de 32 bits (little endian) en memoria (punteros a función en tablas de datos) y, si cae en una función, cuál
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import java.io.*;

public class FindPtr extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs(); PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        for (int i = 1; i < a.length; i++) {
            long v = Long.parseLong(a[i], 16); byte[] pat = {(byte) v, (byte) (v >> 8), (byte) (v >> 16), (byte) (v >> 24)};
            w.println("== " + a[i]);
            Address at = currentProgram.getMinAddress();
            while ((at = currentProgram.getMemory().findBytes(at, pat, null, true, monitor)) != null) {
                ghidra.program.model.listing.Function f = getFunctionContaining(at);
                w.println("  " + at + (f != null ? "  in " + f.getName() : "")); at = at.add(1);
            }
        }
        w.close();
    }
}
