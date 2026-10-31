import re,sys,collections
f=sys.argv[1]; pat=sys.argv[2]
names={'fn':{}}
for l in open(f):
    m=re.match(r'c?fn=\((\d+)\) (.*)',l.rstrip('\n'))
    if m: names['fn'][m[1]]=m[2]
def nm(l):
    m=re.match(r'c?fn=\((\d+)\)',l); return names['fn'].get(m[1]) if m else l.split('=',1)[1].strip()
cur=None;cfn=None;calls=False;c=collections.Counter();tot=0
for l in open(f):
    if l.startswith("fn="): cur=nm(l); continue
    if l.startswith("cfn="): cfn=nm(l); continue
    if l.startswith("calls="): calls=int(l.split('=')[1].split()[0]); continue
    if l[:1].isdigit() or l[:1] in "+-*":
        p=l.split()
        if len(p)<2: continue
        if calls is not False:
            if re.search(pat,cfn or ''): c[cur]+=int(p[1])
            calls=False
        else: tot+=int(p[1])
for k,v in c.most_common(8): print(f"{100*v/tot:5.1f} % {k[:150]}")
