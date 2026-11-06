// Disassembles switch cases that auto-analysis left as raw bytes.
// A switch compiles to a computed jump (jr through a table), which Ghidra can't follow by flow.
// The decompiler can resolve the table, so ask it for the targets, add them as references,
// disassemble them, and regrow the function body. Repeats because a case can hold another switch.
//@category Castaway

import java.util.*;

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.JumpTable;
import ghidra.program.model.symbol.*;

public class FixSwitchTables extends GhidraScript {
    private boolean hasOpenJump(Function f) {
        for (Instruction i : currentProgram.getListing().getInstructions(f.getBody(), true)) {
            String m = i.getMnemonicString();
            if (!m.equals("jr") && !m.equals("_jr")) continue;
            if (i.toString().contains("ra")) continue;
            if (i.getReferencesFrom().length == 0) return true;
        }
        return false;
    }

    @Override
    protected void run() throws Exception {
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        Listing listing = currentProgram.getListing();
        int fixedFuncs = 0, targets = 0, failed = 0;
        Set<Address> done = new HashSet<>();
        for (int pass = 0; pass < 4; pass++) {
            List<Function> todo = new ArrayList<>();
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (!done.contains(f.getEntryPoint()) && hasOpenJump(f)) todo.add(f);
            }
            println("pass " + pass + ": functions with unresolved jumps = " + todo.size());
            if (todo.isEmpty()) break;
            for (Function f : todo) {
                monitor.checkCancelled();
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                HighFunction h = r.getHighFunction();
                JumpTable[] tables = h == null ? null : h.getJumpTables();
                if (tables == null || tables.length == 0) {
                    failed++;
                    done.add(f.getEntryPoint());
                    continue;
                }
                boolean any = false;
                for (JumpTable t : tables) {
                    Instruction jump = listing.getInstructionAt(t.getSwitchAddress());
                    for (Address dest : t.getCases()) {
                        if (jump != null) {
                            jump.addOperandReference(0, dest, RefType.COMPUTED_JUMP, SourceType.ANALYSIS);
                        }
                        if (listing.getInstructionAt(dest) == null) {
                            disassemble(dest);
                            targets++;
                            any = true;
                        }
                    }
                }
                CreateFunctionCmd.fixupFunctionBody(currentProgram, f, monitor);
                if (any) fixedFuncs++;
                else done.add(f.getEntryPoint());
            }
        }
        d.dispose();
        println("Switch fix: functions extended " + fixedFuncs + ", case targets disassembled " + targets
            + ", unresolved " + failed);
    }
}
