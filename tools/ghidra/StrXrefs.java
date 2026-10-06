// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript StrXrefs.java <regex_con_alternancia> <out_file>  -> cadenas que casan y las funciones que las referencian (sin decompilar; una sola apertura del proyecto)
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.regex.*;
import java.io.*;

public class StrXrefs extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Pattern p = Pattern.compile(a[0], Pattern.CASE_INSENSITIVE);
        PrintWriter w = new PrintWriter(new FileWriter(a[1]));
        for (Data d : currentProgram.getListing().getDefinedData(true)) {
            if (!d.hasStringValue()) continue;
            String s = String.valueOf(d.getValue());
            if (!p.matcher(s).find()) continue;
            w.println("STR " + d.getAddress() + " \"" + s + "\"");
            for (Reference r : getReferencesTo(d.getAddress())) {
                Function f = getFunctionContaining(r.getFromAddress());
                w.println("  xref " + r.getFromAddress() + " in " + (f == null ? "?" : f.getName() + " @ " + f.getEntryPoint() + " size " + f.getBody().getNumAddresses()));
            }
        }
        w.close();
    }
}
