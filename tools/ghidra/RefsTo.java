// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript RefsTo.java <out_file> <addr_hex>...  -> referencias a esas direcciones (con función contenedora)
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.listing.Function;
import java.io.*;

public class RefsTo extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        for (int i = 1; i < a.length; i++) {
            w.println("== " + a[i]);
            for (Reference r : getReferencesTo(toAddr(Long.parseLong(a[i], 16)))) {
                Function f = getFunctionContaining(r.getFromAddress());
                w.println("  from " + r.getFromAddress() + " in " + (f == null ? "?" : f.getName() + " @ " + f.getEntryPoint()));
            }
        }
        w.close();
    }
}
