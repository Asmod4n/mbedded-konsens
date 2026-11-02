import json
import sys

profile = json.load(open(sys.argv[1]))
profile["syscalls"].append({"names": ["io_uring_setup", "io_uring_enter", "io_uring_register"],
                            "action": "SCMP_ACT_ALLOW"})
json.dump(profile, open(sys.argv[2], "w"), indent=2)
