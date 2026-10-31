import re,sys,collections
f=sys.argv[1]; top=int(sys.argv[2]) if len(sys.argv)>2 else 25
names={'ob':{},'fn':{}}
for l in open(f):
    m=re.match(r'(c?)(ob|fn)=\((\d+)\) (.*)',l.rstrip('\n'))
    if m: names[m[2]][m[3]]=m[4]
def nm(kind,l):
    m=re.match(r'c?'+kind+r'=\((\d+)\)',l); return names[kind].get(m[1]) if m else l.split('=',1)[1].strip()
cur_ob=cur_fn=None; calls=False; self_=collections.Counter(); incl=collections.Counter(); cfn=None; cob=None
for l in open(f):
    if l.startswith("ob="): cur_ob=nm('ob',l); continue
    if l.startswith("fn="): cur_fn=nm('fn',l); continue
    if l.startswith("cfn="): cfn=nm('fn',l); continue
    if l.startswith("calls="): calls=True; continue
    if l[:1].isdigit() or l[:1] in "+-*":
        p=l.split()
        if len(p)<2: continue
        v=int(p[1])
        if calls: calls=False; incl[cfn]+=v; continue
        self_[(cur_ob.split('/')[-1] if cur_ob else '?',cur_fn)]+=v
s=sum(self_.values())
print("self:")
for (o,fn),v in self_.most_common(top): print(f"{100*v/s:5.1f} % {o[:22]:22} {fn[:110]}")
