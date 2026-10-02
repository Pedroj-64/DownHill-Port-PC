// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript FindOps.java <out_file> <mnemonic>...  -> funciones que contienen alguno de esos mnemónicos (p. ej. vopmula, vdiv), con recuento
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;

public class FindOps extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        Map<String,Integer> cnt = new TreeMap<>();
        for (Instruction in : currentProgram.getListing().getInstructions(true)) {
            String m = in.getMnemonicString().toLowerCase().replace("_", "");
            for (int i = 1; i < a.length; i++) if (m.startsWith(a[i])) {
                Function f = getFunctionContaining(in.getAddress());
                String k = f == null ? "?" : f.getEntryPoint() + " " + f.getName();
                cnt.merge(k + " " + m, 1, Integer::sum);
            }
        }
        for (Map.Entry<String,Integer> e : cnt.entrySet()) w.println(e.getKey() + " x" + e.getValue());
        w.close();
    }
}
