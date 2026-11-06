// Creates structs from cas_layouts.csv and types `this` on methods that use it.
// A class whose namespace exists becomes a Ghidra class, and its struct is the one Ghidra
// attaches to `this` in __thiscall methods. Other structs go in the /Castaway category.
// Re-running updates the same structs.
// Args: <layouts.csv> <ps2_fields.csv from ExportFieldAccess.java>
//@category Castaway

import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.NamespaceUtils;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Namespace;

public class ApplyClassLayouts extends GhidraScript {
    private static final Pattern TYPE = Pattern.compile("^([^*\\[]+)(\\*)?(?:\\[(\\d+)\\])?$");
    private final Map<String, Structure> structs = new HashMap<>();
    private DataTypeManager dtm;

    private DataType resolve(String spec) {
        Matcher m = TYPE.matcher(spec);
        if (!m.matches()) throw new IllegalArgumentException("bad type: " + spec);
        DataType dt = switch (m.group(1)) {
            case "byte" -> ByteDataType.dataType;
            case "ushort" -> UnsignedShortDataType.dataType;
            case "uint" -> UnsignedIntegerDataType.dataType;
            case "float" -> FloatDataType.dataType;
            case "char16" -> WideChar16DataType.dataType;
            case "ptr" -> dtm.getPointer(VoidDataType.dataType);
            default -> {
                Structure s = structs.get(m.group(1));
                if (s == null) throw new IllegalArgumentException("unknown type: " + m.group(1));
                yield s;
            }
        };
        if (m.group(2) != null) dt = dtm.getPointer(dt);
        if (m.group(3) != null) dt = new ArrayDataType(dt, Integer.parseInt(m.group(3)), dt.getLength());
        return dt;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        dtm = currentProgram.getDataTypeManager();
        List<String[]> rows = new ArrayList<>();
        for (String line : Files.readAllLines(Paths.get(args[0])).subList(1, Files.readAllLines(Paths.get(args[0])).size())) {
            if (!line.isBlank()) rows.add(line.split(",", 5));
        }

        // sizes first, so a struct can embed one defined later in the file
        Map<String, GhidraClass> classes = new HashMap<>();
        CategoryPath cat = new CategoryPath("/Castaway");
        for (String[] r : rows) {
            if (!r[1].equals("size")) continue;
            Namespace ns = currentProgram.getSymbolTable().getNamespace(r[0], currentProgram.getGlobalNamespace());
            Structure s;
            if (ns != null) {
                GhidraClass c = ns instanceof GhidraClass g ? g : NamespaceUtils.convertNamespaceToClass(ns);
                classes.put(r[0], c);
                // findOrCreateClassStruct can return an unsaved struct; resolve stores it
                s = (Structure) dtm.resolve(VariableUtilities.findOrCreateClassStruct(c, dtm),
                    DataTypeConflictHandler.DEFAULT_HANDLER);
            } else {
                DataType have = dtm.getDataType(cat, r[0]);
                s = have instanceof Structure h ? h
                    : (Structure) dtm.addDataType(new StructureDataType(cat, r[0], 0, dtm), DataTypeConflictHandler.DEFAULT_HANDLER);
            }
            s.setLength(Integer.decode(r[2]));
            structs.put(r[0], s);
        }

        int fields = 0;
        for (String[] r : rows) {
            if (r[1].equals("size")) continue;
            DataType dt = resolve(r[2]);
            structs.get(r[0]).replaceAtOffset(Integer.decode(r[1]), dt, dt.getLength(), r[3], r.length > 4 ? r[4] : null);
            fields++;
        }

        // only methods seen reading or writing through a0, so static functions stay as they are
        Set<String> typed = new HashSet<>();
        int methods = 0;
        for (String line : Files.readAllLines(Paths.get(args[1]))) {
            String[] p = line.split(",");
            if (p.length < 4 || !classes.containsKey(p[0]) || !typed.add(p[0] + p[1])) continue;
            Function f = getFunctionContaining(toAddr(Long.parseLong(p[3], 16)));
            if (f == null) continue;
            f.setCallingConvention("__thiscall");
            methods++;
        }
        println("Layouts: structs " + structs.size() + ", classes " + classes.size() + ", fields " + fields + ", methods typed " + methods);
    }
}
