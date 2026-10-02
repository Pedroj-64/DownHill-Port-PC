// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript RefsSym.java <out_file> <substr>...  -> referencias a símbolos cuyo nombre contiene <substr>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.Function;
import java.io.*;

public class RefsSym extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
        while (it.hasNext()) {
            Symbol s = it.next();
            for (int i = 1; i < a.length; i++) if (s.getName().contains(a[i])) {
                w.println("== " + s.getName() + " " + s.getAddress());
                for (Reference r : getReferencesTo(s.getAddress())) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    w.println("  from " + r.getFromAddress() + " in " + (f == null ? "?" : f.getName()));
                }
            }
        }
        w.close();
    }
}
