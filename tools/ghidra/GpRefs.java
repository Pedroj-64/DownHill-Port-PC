// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript GpRefs.java <out_file> <offset>...  -> instrucciones que usan "<offset>(gp)" (p. ej. -0x5528), con su función
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;

public class GpRefs extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        for (Instruction in : currentProgram.getListing().getInstructions(true)) {
            String s = in.toString();
            if (!s.contains("(gp)")) continue;
            for (int i = 1; i < a.length; i++) if (s.contains(a[i] + "(gp)")) {
                Function f = getFunctionContaining(in.getAddress());
                w.println(in.getAddress() + "  " + s + "  in " + (f == null ? "?" : f.getName()));
            }
        }
        w.close();
    }
}
