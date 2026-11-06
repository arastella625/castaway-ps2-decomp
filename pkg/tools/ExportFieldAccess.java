// Exports every load/store off the `this` pointer in the methods of the given classes.
// Output CSV: class,method,wii_symbol,addr,offset,width,kind (kind L or S; offset in hex).
// Linear scan, so branches can make `this` tracking slightly wrong: treat results as evidence.
// Args: <out.csv> <class> [<class> ...]
//@category Castaway

import java.io.*;
import java.util.*;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;

public class ExportFieldAccess extends GhidraScript {
    private static final Map<String, Integer> WIDTH = new HashMap<>();
    static {
        for (String m : new String[] {"lb", "lbu", "sb"}) WIDTH.put(m, 1);
        for (String m : new String[] {"lh", "lhu", "sh"}) WIDTH.put(m, 2);
        for (String m : new String[] {"lw", "lwu", "sw", "lwc1", "swc1"}) WIDTH.put(m, 4);
        for (String m : new String[] {"ld", "sd"}) WIDTH.put(m, 8);
        for (String m : new String[] {"lq", "sq"}) WIDTH.put(m, 16);
    }

    private static String bare(String mnemonic) {
        return mnemonic.startsWith("_") ? mnemonic.substring(1) : mnemonic;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        Set<String> classes = new HashSet<>(Arrays.asList(args).subList(1, args.length));
        Listing listing = currentProgram.getListing();
        int rows = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(args[0]))) {
            w.println("class,method,wii_symbol,addr,offset,width,kind");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                String cls = f.getParentNamespace().getName(true);
                if (!classes.contains(cls)) continue;
                String plate = listing.getComment(ghidra.program.model.listing.CommentType.PLATE, f.getEntryPoint());
                String sym = plate == null ? "" : plate.split("\n")[0].replace("Wii symbol: ", "");
                Set<String> self = new HashSet<>(Collections.singleton("a0"));
                Set<String> indexed = new HashSet<>(); // this + a runtime index: array access
                int calledAgo = -1;
                for (Instruction i : listing.getInstructions(f.getBody(), true)) {
                    String m = bare(i.getMnemonicString());
                    // the delay slot still runs before the call clobbers anything
                    if (calledAgo == 0) {
                        self.removeIf(r -> !r.startsWith("s"));
                        indexed.removeIf(r -> !r.startsWith("s"));
                    }
                    calledAgo = (m.equals("jal") || m.equals("jalr")) ? 1 : calledAgo - 1;
                    String prefix = cls + "," + f.getName().replace(",", ";") + "," + sym.replace(",", ";") + "," + i.getAddress();
                    if (WIDTH.containsKey(m) && i.getNumOperands() == 2) {
                        Object[] o = i.getOpObjects(1);
                        if (o.length == 2 && o[0] instanceof Scalar s && o[1] instanceof Register b) {
                            char kind = indexed.contains(b.getName()) ? 'I' : m.startsWith("l") ? 'L' : 'S';
                            if (kind == 'I' || self.contains(b.getName())) {
                                w.printf("%s,%x,%d,%c%n", prefix, s.getSignedValue(), WIDTH.get(m), kind);
                                rows++;
                            }
                        }
                    }
                    // follow copies of this, drop registers that get overwritten
                    Object[] dst = i.getNumOperands() > 0 ? i.getOpObjects(0) : new Object[0];
                    if ((m.equals("addiu") || m.equals("daddiu")) && i.getNumOperands() == 3) {
                        Object[] src = i.getOpObjects(1), imm = i.getOpObjects(2);
                        if (src.length == 1 && self.contains(src[0].toString()) && imm.length == 1
                                && imm[0] instanceof Scalar s) {
                            // address of an embedded member or array
                            w.printf("%s,%x,0,A%n", prefix, s.getSignedValue());
                            rows++;
                        }
                    }
                    if ((m.equals("addu") || m.equals("daddu")) && i.getNumOperands() == 3 && dst.length == 1) {
                        String a = i.getOpObjects(1)[0].toString(), b = i.getOpObjects(2)[0].toString();
                        if (self.contains(a) || self.contains(b)) {
                            boolean copy = a.equals("zero") || b.equals("zero");
                            self.remove(dst[0].toString());
                            (copy ? self : indexed).add(dst[0].toString());
                            continue;
                        }
                    }
                    if (dst.length == 1) indexed.remove(dst[0].toString());
                    if (m.equals("move") && i.getNumOperands() == 2) {
                        Object[] src = i.getOpObjects(1);
                        if (dst.length == 1 && src.length == 1 && self.contains(src[0].toString())) {
                            self.add(dst[0].toString());
                            continue;
                        }
                    }
                    boolean writes = !m.startsWith("s") && !m.startsWith("b") && !m.startsWith("j");
                    if (writes && dst.length == 1 && dst[0] instanceof Register r) self.remove(r.getName());
                }
            }
        }
        println("Exported " + rows + " field accesses to " + args[0]);
    }
}
