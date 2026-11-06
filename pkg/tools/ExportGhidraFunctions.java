// Exports Ghidra's function boundaries for tools/ps2_refs.py.
// Output CSV columns: start,end,size,ranges,name  (hex addresses; end is exclusive;
// size is the body's byte count, which differs from end-start when ranges > 1).
// Headless: pass the output path as a script arg. GUI: asks for a file.
//@category Castaway

import java.io.*;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;

public class ExportGhidraFunctions extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File out = args.length >= 1 ? new File(args[0]) : askFile("Save function list", "Export");
        MemoryBlock text = currentProgram.getMemory().getBlock(".text");
        int n = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
            w.println("start,end,size,ranges,name");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (!text.contains(f.getEntryPoint())) continue;
                AddressSetView body = f.getBody();
                w.printf("%08X,%08X,%d,%d,\"%s\"%n",
                    f.getEntryPoint().getOffset(), body.getMaxAddress().getOffset() + 1,
                    body.getNumAddresses(), body.getNumAddressRanges(),
                    f.getName(true).replace("\"", "\"\""));
                n++;
            }
        }
        println("Exported " + n + " functions to " + out);
    }
}
