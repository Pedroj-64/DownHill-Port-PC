// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript DecompFn.java <out_file> <addr_hex>...  -> decompila esas funciones al archivo
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.Function;
import java.io.*;

public class DecompFn extends GhidraScript {
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        String[] args = getScriptArgs();
        PrintWriter w = new PrintWriter(new FileWriter(args[0]));
        for (int i = 1; i < args.length; i++) { String a = args[i];
            Function f = getFunctionAt(toAddr(Long.parseLong(a, 16)));
            if (f == null) { disassemble(toAddr(Long.parseLong(a, 16))); f = createFunction(toAddr(Long.parseLong(a, 16)), null); }   // si no hay función (p. ej. destino de puntero), la crea
            if (f == null) { w.println("// no function at " + a); continue; }
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            w.println("// ==== " + f.getName() + " @ " + f.getEntryPoint());
            if (r.decompileCompleted()) w.println(r.getDecompiledFunction().getC());
        }
        w.close();
    }
}
