#!/usr/bin/env python
"""One-shot development patch: general --flag parsing in app/main.cpp."""
p = "app/main.cpp"
src = open(p, encoding="utf-8").read()

old_struct = """struct Args {
    std::vector<std::string> positional;
    int depth = 1;
    bool json = false;
};"""
new_struct = """struct Args {
    std::vector<std::string> positional;
    std::map<std::string, std::string> flags;   // command-specific --flag value
    int depth = 1;
    bool json = false;
};"""
assert old_struct in src, "Args struct"
src = src.replace(old_struct, new_struct)

old_unknown = """        } else if (t.rfind("--", 0) == 0) {
            std::cerr << "usage: unknown option '" << t << "'\\n";
            std::exit(kExitUsage);
        } else {"""
new_unknown = """        } else if (t.rfind("--", 0) == 0) {
            // command-specific flag with a value (e.g. --rules X, --fetch id);
            // a trailing flag or bare boolean flag is tolerated
            if (i + 1 < tokens.size() && tokens[i + 1].rfind("--", 0) != 0) {
                args.flags[t] = tokens[++i];
            } else {
                args.flags[t] = "";
            }
        } else {"""
assert old_unknown in src, "unknown-option branch"
src = src.replace(old_unknown, new_unknown)

old_conflicts = """int cmdConflicts(const Args& args) {
    std::string rulesPath = "data/rules/rules.txt";
    for (std::size_t i = 0; i + 1 < args.positional.size(); i += 2) {
        if (args.positional[i] == "--rules") rulesPath = args.positional[i + 1];
    }"""
new_conflicts = """int cmdConflicts(const Args& args) {
    const std::string rulesPath = args.flags.count("--rules") != 0u
                                      ? args.flags.at("--rules")
                                      : std::string("data/rules/rules.txt");"""
assert old_conflicts in src, "cmdConflicts"
src = src.replace(old_conflicts, new_conflicts)

old_export = """int cmdExport(const Args& args) {
    std::string jsonPath, dotPath, htmlPath;
    for (std::size_t i = 0; i + 1 < args.positional.size(); i += 2) {
        if (args.positional[i] == "--json") jsonPath = args.positional[i + 1];
        else if (args.positional[i] == "--dot") dotPath = args.positional[i + 1];
        else if (args.positional[i] == "--html") htmlPath = args.positional[i + 1];
    }"""
new_export = """int cmdExport(const Args& args) {
    const std::string jsonPath = args.flags.count("--json") != 0u
                                     ? args.flags.at("--json") : std::string();
    const std::string dotPath = args.flags.count("--dot") != 0u
                                    ? args.flags.at("--dot") : std::string();
    const std::string htmlPath = args.flags.count("--html") != 0u
                                     ? args.flags.at("--html") : std::string();"""
assert old_export in src, "cmdExport"
src = src.replace(old_export, new_export)

old_fetch = """    const std::string& path = args.positional[0];
    std::string fetchId;
    for (std::size_t i = 1; i + 1 < args.positional.size(); i += 2) {
        if (args.positional[i] == "--fetch") fetchId = args.positional[i + 1];
    }"""
new_fetch = """    const std::string& path = args.positional[0];
    const std::string fetchId = args.flags.count("--fetch") != 0u
                                    ? args.flags.at("--fetch")
                                    : std::string();"""
assert old_fetch in src, "cmdLoad fetch"
src = src.replace(old_fetch, new_fetch)

if "#include <map>" not in src:
    src = src.replace("#include <cstdlib>", "#include <cstdlib>\n#include <map>")

open(p, "w", encoding="utf-8", newline="\n").write(src)
print("flags wired")
