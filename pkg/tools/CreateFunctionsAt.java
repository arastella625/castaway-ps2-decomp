// Creates functions at the addresses in a text file (one hex address per line).
// Used for vtable slot targets: code reached only through a pointer has no call for analysis
// to follow. Addresses inside an existing function are skipped and reported, not forced.
// Args: <addresses.txt>
//@category Castaway

import java.nio.file.*;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class CreateFunctionsAt extends GhidraScript {
    @Override
    protected void run() throws Exception {
        int existing = 0, created = 0, inside = 0, failed = 0;
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            if (line.isBlank()) continue;
            Address a = toAddr(Long.parseLong(line.trim(), 16));
            if (getFunctionAt(a) != null) { existing++; continue; }
            Function owner = getFunctionContaining(a);
            if (owner != null) {
                inside++;
                if (inside <= 10) println("inside " + owner.getName(true) + " @ " + owner.getEntryPoint() + ": " + a);
                continue;
            }
            disassemble(a);
            if (createFunction(a, null) != null) created++;
            else failed++;
        }
        println("Functions at slots: existing " + existing + ", created " + created
            + ", inside another function " + inside + ", failed " + failed);
    }
}
