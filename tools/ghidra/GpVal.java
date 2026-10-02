// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Uso: -postScript GpVal.java <out_file> <addr_hex>  -> valor del registro gp que Ghidra asume en esa dirección
import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.*;
import java.io.*;
import java.math.BigInteger;

public class GpVal extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs(); PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        Register gp = currentProgram.getRegister("gp");
        RegisterValue v = currentProgram.getProgramContext().getRegisterValue(gp, toAddr(Long.parseLong(a[1], 16)));
        w.println("gp@" + a[1] + " = " + (v == null ? "null" : v.getUnsignedValueIgnoreMask()));
        w.close();
    }
}
