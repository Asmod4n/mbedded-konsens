import re,sys,collections
f=sys.argv[1]
names={'ob':{},'fn':{}}
for l in open(f):
    m=re.match(r'c?(ob|fn)=\((\d+)\) (.*)',l.rstrip('\n'))
    if m: names[m[1]][m[2]]=m[3]
def nm(k,l):
    m=re.match(r'c?'+k+r'=\((\d+)\)',l); return names[k].get(m[1]) if m else l.split('=',1)[1].strip()
ob=fn=cob=cfn=None; calls=False; tot=0; into=collections.Counter(); fninc=collections.Counter()
for l in open(f):
    if l.startswith("ob="): ob=nm('ob',l); cob=None; continue
    if l.startswith("fn="): fn=nm('fn',l); cob=None; continue
    if l.startswith("cob="): cob=nm('ob',l); continue
    if l.startswith("cfn="): cfn=nm('fn',l); continue
    if l.startswith("calls="): calls=True; continue
    if l[:1].isdigit() or l[:1] in "+-*":
        p=l.split()
        if len(p)<2: continue
        v=int(p[1])
        if calls:
            calls=False
            callee_ob=cob or ob
            if 'konsensd' in (ob or '') and 'sqlite' in (callee_ob or ''): into['sqlite (inclusive, called from konsensd)']+=v
            fninc[cfn]+=v
            cob=None
        else: tot+=v
print(f"total {tot/1e6:.0f} M Ir")
for k,v in into.items(): print(f"{100*v/tot:5.1f} % {k}")
pats={'decode (cbor generic/struct_read, decode_*)':r'^(konsens::decode|std::expected<void, cbor::error> cbor::generic::struct_read<konsens::(accept_decide|entry)|konsens::\(anonymous namespace\)::decode_bytes)',
 'crc32c::Extend':r'^crc32c::Extend$', 'encode_result':r'^konsens::encode_result', 'results_agree':r'^konsens::results_agree',
 'commit_phase_one':r'commit_phase_one', 'commit_phase_two':r'commit_phase_two', 'handle_incoming':r'handle_incoming', 'take_the_frames_of':r'take_the_frames_of','handle_a_receive':r'handle_a_receive','sqlite3_step':r'^sqlite3_step$','apply_the_done_records':r'apply_the_done_records','check_the_done_records':r'check_the_done_records','write_the_result':r'write_the_result','end_the_step':r'end_the_step','io_uring_submit':r'io_uring_submit','malloc':r'^malloc$','free':r'^free$'}
for k,pat in pats.items():
    v=max([x for n,x in fninc.items() if n and re.search(pat,n)] or [0])
    print(f"{100*v/tot:5.1f} % max inclusive of {k}")
