// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript ListInstr.java <out_file> <addr_hex>...  -> desensamblado (dirección, bytes, mnemónico+operandos) de cada función en esas direcciones (si no existe, la crea)
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;

public class ListInstr extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs(); PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        for (int i = 1; i < a.length; i++) {
            Address ad = toAddr(Long.parseLong(a[i], 16)); Function f = getFunctionAt(ad);
            if (f == null) { disassemble(ad); f = createFunction(ad, null); }
            if (f == null) { w.println("// no function at " + a[i]); continue; }
            w.println("// ==== " + f.getName() + " @ " + f.getEntryPoint() + " size " + f.getBody().getNumAddresses());
            for (Instruction in : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                StringBuilder b = new StringBuilder();
                for (byte x : in.getBytes()) b.append(String.format("%02x", x & 0xff));
                w.println(in.getAddress() + "  " + b + "  " + in);
            }
        }
        w.close();
    }
}
