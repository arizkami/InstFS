import json
import re
import sys
from pathlib import Path


# -----------------------------
# Utility
# -----------------------------

def parse_value(value):
    value = value.strip()

    # remove quotes
    if value.startswith('"') and value.endswith('"'):
        value = value[1:-1]

    try:
        if "." in value:
            return float(value)
        return int(value)
    except:
        return value


def extract_opcodes(line):
    """
    Robust opcode extractor.
    Handles values with spaces.
    Example:
    sample=Mf B-1.$EXT
    """
    tokens = []
    i = 0
    length = len(line)

    while i < length:
        eq = line.find("=", i)
        if eq == -1:
            break

        # find key
        key_start = line.rfind(" ", 0, eq)
        key = line[key_start+1:eq].strip()

        # find next key=
        next_match = re.search(r"\s+\w+=", line[eq+1:])
        if next_match:
            value_end = eq + 1 + next_match.start()
            value = line[eq+1:value_end]
            i = value_end
        else:
            value = line[eq+1:]
            i = length

        tokens.append((key, value.strip()))

    return tokens


# -----------------------------
# SFZ Parser
# -----------------------------

class SFZParser:

    def __init__(self, verbose=False):
        self.data = {
            "control": {},
            "global": {},
            "groups": []
        }

        self.defines = {}
        self.current_section = None
        self.current_group = None
        self.current_region = None
        self.verbose = verbose
        self.processed_files = set()  # Track processed files to avoid circular includes

    # -----------------------------
    # Recursive file parsing
    # -----------------------------

    def parse_file(self, filepath):
        filepath = Path(filepath).resolve()
        
        # Avoid circular includes
        if str(filepath) in self.processed_files:
            if self.verbose:
                print(f"[INFO] Skipping already processed file: {filepath}")
            return
        
        if not filepath.exists():
            print(f"[ERROR] File not found: {filepath}")
            return
        
        self.processed_files.add(str(filepath))
        
        if self.verbose:
            print(f"[INFO] Parsing: {filepath}")

        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            for raw_line in f:
                self.process_line(raw_line.strip(), filepath.parent)

    # -----------------------------
    # Line processor
    # -----------------------------

    def process_line(self, line, base_path):

        if not line:
            return

        if line.startswith("//"):
            return

        # remove inline comment
        if "//" in line:
            line = line.split("//")[0].strip()

        # -------------------------
        # #define
        # -------------------------
        if line.startswith("#define"):
            match = re.match(r'#define\s+\$(\w+)\s+(.+)', line)
            if match:
                key, value = match.groups()
                self.defines[key] = value.strip()
                if self.verbose:
                    print(f"[DEFINE] ${key} = {value.strip()}")
            return

        # -------------------------
        # #include
        # -------------------------
        if line.startswith("#include"):
            match = re.search(r'"([^"]+)"', line)
            if match:
                include_path = match.group(1)

                # macro replacement
                for macro, val in self.defines.items():
                    include_path = include_path.replace(f"${macro}", val)

                include_path = include_path.replace("\\", "/")
                include_file = (base_path / include_path).resolve()

                if include_file.exists():
                    if self.verbose:
                        print(f"[INCLUDE] {include_path}")
                    self.parse_file(include_file)
                else:
                    print(f"[ERROR] Include file not found: {include_file}")
            return

        # -------------------------
        # Section switching
        # -------------------------
        if line.startswith("<control>"):
            self.current_section = "control"
            return

        if line.startswith("<global>"):
            self.current_section = "global"
            return

        if line.startswith("<master>"):
            # Master is like a group
            self.current_section = "group"
            self.current_group = {
                "opcodes": {},
                "regions": []
            }
            self.data["groups"].append(self.current_group)
            self.current_region = None
            return

        if line.startswith("<group>"):
            self.current_section = "group"
            self.current_group = {
                "opcodes": {},
                "regions": []
            }
            self.data["groups"].append(self.current_group)
            self.current_region = None
            return

        if line.startswith("<region>"):
            self.current_section = "region"

            if self.current_group is None:
                self.current_group = {
                    "opcodes": {},
                    "regions": []
                }
                self.data["groups"].append(self.current_group)

            region = {}
            region.update(self.data["global"])
            region.update(self.current_group["opcodes"])

            self.current_group["regions"].append(region)
            self.current_region = region
            
            # Parse opcodes on the same line as <region>
            rest_of_line = line[8:].strip()  # Remove "<region>" prefix
            if rest_of_line and "=" in rest_of_line:
                opcodes = extract_opcodes(rest_of_line)
                for key, value in opcodes:
                    # macro replace inside value
                    for macro, val in self.defines.items():
                        value = value.replace(f"${macro}", val)
                    value = parse_value(value)
                    self.current_region[key] = value
            
            return

        # -------------------------
        # Opcode parsing
        # -------------------------
        if "=" in line:
            opcodes = extract_opcodes(line)

            for key, value in opcodes:

                # macro replace inside value
                for macro, val in self.defines.items():
                    value = value.replace(f"${macro}", val)

                value = parse_value(value)

                if self.current_section == "control":
                    self.data["control"][key] = value

                elif self.current_section == "global":
                    self.data["global"][key] = value

                elif self.current_section == "group" and self.current_group:
                    self.current_group["opcodes"][key] = value

                elif self.current_section == "region" and self.current_region:
                    self.current_region[key] = value

    # -----------------------------
    # Normalize
    # -----------------------------

    def normalize(self):

        normalized = {
            "control": self.data["control"],
            "regions": []
        }

        default_path = self.data["control"].get("default_path", "")

        region_count = 0
        for group in self.data["groups"]:
            for region in group["regions"]:

                sample = region.get("sample", None)

                # prepend default_path if exists
                if sample and default_path:
                    sample = str(Path(default_path) / sample).replace("\\", "/")
                elif sample:
                    sample = sample.replace("\\", "/")

                norm = {
                    "sample": sample,
                    "key_range": [
                        region.get("lokey", 0),
                        region.get("hikey", 127)
                    ],
                    "vel_range": [
                        region.get("lovel", 0),
                        region.get("hivel", 127)
                    ],
                    "root_key": region.get(
                        "pitch_keycenter",
                        region.get("lokey", 60)
                    ),
                    "tune": region.get("tune", 0),
                    "volume": region.get("volume", 0),
                    "loop_mode": region.get("loop_mode", "one_shot"),
                    "amp_env": {
                        "attack": region.get("ampeg_attack", 0.0),
                        "decay": region.get("ampeg_decay", 0.0),
                        "sustain": region.get("ampeg_sustain", 1.0),
                        "release": region.get("ampeg_release", 0.0),
                    }
                }

                normalized["regions"].append(norm)
                region_count += 1

        if self.verbose:
            print(f"\n[INFO] Total regions parsed: {region_count}")
            print(f"[INFO] Total files processed: {len(self.processed_files)}")

        return normalized


# -----------------------------
# Main
# -----------------------------

def main():

    if len(sys.argv) < 2:
        print("Usage: python sfz_to_json.py <file.sfz> [--verbose]")
        print("\nConverts SFZ instrument files to JSON format.")
        print("Supports nested #include directives and <master> sections.")
        return

    sfz_file = sys.argv[1]
    verbose = "--verbose" in sys.argv or "-v" in sys.argv

    parser = SFZParser(verbose=verbose)
    parser.parse_file(sfz_file)

    normalized = parser.normalize()

    output_file = Path(sfz_file).with_suffix(".json")

    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(normalized, f, indent=2)

    print(f"\n✔ Converted to {output_file}")
    print(f"Total regions: {len(normalized['regions'])}")
    print(f"Files processed: {len(parser.processed_files)}")
    
    if verbose:
        print(f"\nProcessed files:")
        for filepath in sorted(parser.processed_files):
            print(f"  - {filepath}")


if __name__ == "__main__":
    main()
