// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript ListFns.java <out_file> <desde_hex> <hasta_hex>  -> entradas de función en [desde, hasta) con su tamaño
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;

public class ListFns extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long lo = Long.parseLong(a[1], 16), hi = Long.parseLong(a[2], 16);
        PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            long e = f.getEntryPoint().getOffset();
            if (e >= lo && e < hi) w.println(f.getEntryPoint() + " " + f.getName() + " size " + f.getBody().getNumAddresses());
        }
        w.close();
    }
}
