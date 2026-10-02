// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript GrepInstr.java <out_file> <desde_hex> <hasta_hex> <subcadena>...  -> instrucciones en [desde,hasta) cuyo texto contiene alguna subcadena (p. ej. "0x230(")
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;

public class GrepInstr extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long lo = Long.parseLong(a[1], 16), hi = Long.parseLong(a[2], 16);
        PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        for (Instruction in : currentProgram.getListing().getInstructions(true)) {
            long e = in.getAddress().getOffset();
            if (e < lo || e >= hi) continue;
            String s = in.toString();
            for (int i = 3; i < a.length; i++) if (s.contains(a[i])) {
                Function f = getFunctionContaining(in.getAddress());
                w.println(in.getAddress() + "  " + s + "  in " + (f == null ? "?" : f.getName()));
                break;
            }
        }
        w.close();
    }
}
