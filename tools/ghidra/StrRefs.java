// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript StrRefs.java <substr> <out_file> [maxFuncs]  -> escribe xrefs y decompilación de funciones que usan cadenas con <substr>
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.util.DefinedDataIterator;
import java.util.*;
import java.io.*;

public class StrRefs extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        String q = a[0].toLowerCase();
        PrintWriter w = new PrintWriter(new FileWriter(a[1]));
        int max = a.length > 2 ? Integer.parseInt(a[2]) : 3;
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Set<Function> done = new LinkedHashSet<>();
        for (Data d : currentProgram.getListing().getDefinedData(true)) {
            if (!d.hasStringValue()) continue; String s = String.valueOf(d.getValue());
            if (!s.toLowerCase().contains(q)) continue;
            w.println("STR " + d.getAddress() + " \"" + s + "\"");
            for (Reference r : getReferencesTo(d.getAddress())) {
                Function f = getFunctionContaining(r.getFromAddress());
                w.println("  xref " + r.getFromAddress() + " in " + (f == null ? "?" : f.getName()));
                if (f != null && done.size() < max) done.add(f);
            }
        }
        for (Function f : done) {
            DecompileResults res = di.decompileFunction(f, 60, monitor);
            w.println("==== " + f.getName() + " @ " + f.getEntryPoint());
            if (res.decompileCompleted()) w.println(res.getDecompiledFunction().getC());
        }
        w.close();
    }
}
