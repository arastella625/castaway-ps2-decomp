// Applies Wii-derived function names to the PS2 build of The Sims 2: Castaway (SLUS_216.64).
// Input: castaway_ps2_names_ghidra.txt  (lines: <hexaddr> <confidence> <Namespace::Name> <wii_mangled>)
// Run after auto-analysis. Names are tagged with bookmarks by confidence so you can review them
// in Window > Bookmarks (category "Castaway names").
//@category Castaway
//@author ts2c-ps2 helper

import java.io.*;
import java.util.*;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.NamespaceUtils;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class ApplyCastawayNames extends GhidraScript {

    // split "A::B<x::y>::f" on top-level "::" only
    private List<String> splitPath(String s) {
        List<String> out = new ArrayList<>();
        int depth = 0, start = 0;
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '<') depth++;
            else if (c == '>') depth--;
            else if (depth == 0 && c == ':' && i + 1 < s.length() && s.charAt(i + 1) == ':') {
                out.add(s.substring(start, i));
                i++;
                start = i + 1;
            }
        }
        out.add(s.substring(start));
        return out;
    }

    @Override
    protected void run() throws Exception {
        // Headless: pass <names_file> <true|false> as script args. GUI: dialogs as usual.
        String[] args = getScriptArgs();
        File f;
        boolean includePropagated;
        if (args.length >= 2) {
            f = new File(args[0]);
            includePropagated = Boolean.parseBoolean(args[1]);
        } else {
            f = askFile("Select castaway_ps2_names_ghidra.txt", "Apply");
            includePropagated = askYesNo("Castaway names",
                "Also apply 'propagated' (call-graph) names?\n" +
                "They are less certain than the string-matched ones, but are bookmarked for review.");
        }

        int applied = 0, created = 0, skipped = 0;
        BookmarkManager bm = currentProgram.getBookmarkManager();
        SymbolTable st = currentProgram.getSymbolTable();
        Namespace global = currentProgram.getGlobalNamespace();
        Set<Address> listed = new HashSet<>();

        try (BufferedReader r = new BufferedReader(new FileReader(f))) {
            String line;
            while ((line = r.readLine()) != null) {
                if (monitor.isCancelled()) break;
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) continue;
                String[] p = line.split(" ", 4);
                if (p.length < 4) continue;
                String conf = p[1];
                if (conf.equals("propagated") && !includePropagated) { skipped++; continue; }

                Address addr = toAddr(Long.parseLong(p[0], 16));
                listed.add(addr);
                Function fn = getFunctionAt(addr);
                if (fn == null) {
                    disassemble(addr);
                    fn = createFunction(addr, null);
                    if (fn == null) { println("could not create function at " + addr); skipped++; continue; }
                    created++;
                }
                // don't overwrite names you've set yourself
                if (fn.getSymbol().getSource() == SourceType.USER_DEFINED) { skipped++; continue; }

                List<String> parts = splitPath(p[2]);
                String name = parts.remove(parts.size() - 1).replace(' ', '_');
                Namespace ns = global;
                if (!parts.isEmpty()) {
                    ns = NamespaceUtils.createNamespaceHierarchy(String.join("::", parts), global,
                            currentProgram, SourceType.IMPORTED);
                }
                try {
                    fn.setParentNamespace(ns);
                    fn.setName(name, SourceType.IMPORTED);
                } catch (Exception e) {
                    println("name failed at " + addr + ": " + p[2] + " (" + e.getMessage() + ")");
                    skipped++;
                    continue;
                }
                setPlateComment(addr, "Wii symbol: " + p[3] + "\nMatch confidence: " + conf);
                bm.setBookmark(addr, BookmarkType.ANALYSIS, "Castaway names", conf + ": " + p[3]);
                applied++;
            }
        }
        // Clear names from an earlier run that this file dropped. Only functions this script
        // bookmarked and that still carry an imported name; hand renames are left alone.
        int cleared = 0;
        List<Bookmark> stale = new ArrayList<>();
        Iterator<Bookmark> it = bm.getBookmarksIterator(BookmarkType.ANALYSIS);
        while (it.hasNext()) {
            Bookmark b = it.next();
            if (b.getCategory().equals("Castaway names") && !listed.contains(b.getAddress())) stale.add(b);
        }
        for (Bookmark b : stale) {
            Function fn = getFunctionAt(b.getAddress());
            if (fn == null || fn.getSymbol().getSource() != SourceType.IMPORTED) continue;
            fn.setParentNamespace(global);
            fn.setName(null, SourceType.DEFAULT);
            setPlateComment(b.getAddress(), null);
            bm.removeBookmark(b);
            println("cleared stale name at " + b.getAddress());
            cleared++;
        }
        println(String.format("Castaway names: applied %d, new functions %d, skipped %d, cleared %d",
            applied, created, skipped, cleared));
    }
}
