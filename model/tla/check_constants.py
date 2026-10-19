import pathlib
import re
import sys

tree = pathlib.Path(sys.argv[1])


def constants(module):
    model = (tree / module).read_text()
    block = model[model.index("\nCONSTANTS") : model.index("\nVARIABLES")]
    return set(re.findall(r"^  ([A-Za-z]\w*),?$", block, re.M))


expected = constants("SequencePaxos.tla")


def declared(cfg):
    text = cfg.read_text()
    if cfg.name.startswith("APA_"):
        wrapper = cfg.with_suffix(".tla").read_text()
        return set(re.findall(r"^OVERRIDE_(\w+) ==", wrapper, re.M))
    body = re.search(r"^CONSTANTS\n((?:  .*\n)*)", text, re.M).group(1)
    return set(re.findall(r"^  (\w+) =", body, re.M))


failed = False
for cfg in sorted(tree.glob("*.cfg")):
    names = declared(cfg)
    model = expected
    if cfg.name.endswith(".base.cfg"):
        model = constants("SequencePaxos.base.tla")
    missing = sorted(model - names)
    unknown = sorted(names - model)
    if missing or unknown:
        failed = True
        print(f"{cfg.name}: missing {missing} unknown {unknown}")
print(f"{len(expected)} constants in the model")
sys.exit(1 if failed else 0)
