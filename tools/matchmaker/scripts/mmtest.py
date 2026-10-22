#!/usr/bin/env python3
# Tremor Invictus matchmaker: end-to-end check on a running service.
# Two players queue for duel, both accept, a server is reserved, the
# server reports the result, ratings move. Needs python "cryptography".
#
# usage: mmtest.py [http://127.0.0.1:27960]
import json, sys, time, urllib.request
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization

BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:27960"

class Who:
    def __init__(self):
        self.key = Ed25519PrivateKey.generate()
        self.pub = self.key.public_key().public_bytes(
            serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex()

    def call(self, method, path, body=None):
        data = json.dumps(body).encode() if body is not None else b""
        t = str(int(time.time()))
        sig = self.key.sign(f"{method}\n{path}\n{t}\n".encode() + data).hex()
        req = urllib.request.Request(BASE + path, data=data or None, method=method, headers={
            "X-Key": self.pub, "X-Time": t, "X-Signature": sig, "Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req) as r:
                return json.loads(r.read())
        except urllib.error.HTTPError as e:
            return {"http": e.code, **json.loads(e.read() or b"{}")}

a, b, server = Who(), Who(), Who()
print("register", server.call("POST", "/servers/register",
      {"address": "127.0.0.1:27961", "name": "test", "maps": {"duel": ["vale", "ruins"]}}))
print("a queue", a.call("POST", "/queue", {"modes": ["duel"]}))
print("b queue", b.call("POST", "/queue", {"modes": ["duel", "tdm"]}))
time.sleep(2)
st = a.call("POST", "/status")
print("a status", st)
assert st["state"] == "ready_check", st
print("a ready", a.call("POST", "/ready", {"accept": True}))
print("b ready", b.call("POST", "/ready", {"accept": True}))
time.sleep(2)
st = b.call("POST", "/status")
print("b status", st)
assert st["state"] == "connect" and st["token"], st
res = server.call("POST", "/servers/reservation")
print("reservation", res)
assert res["match"] == st["match"]
print("result", server.call("POST", "/servers/result", {"match": res["match"], "winner": a.pub}))
print("a status", a.call("POST", "/status"))
print("a ratings", a.call("POST", "/ratings"))
print("b ratings", b.call("POST", "/ratings"))
# an unsigned request is refused
print("unsigned", urllib.request.urlopen(BASE + "/").read())
bad = Who(); bad.pub = a.pub
print("forged", bad.call("POST", "/status"))

# a party of two and two players alone make a 2v2; the party stays together
p1, p2, c, d = Who(), Who(), Who(), Who()
code = p1.call("POST", "/party/create")["code"]
print("party", code, p2.call("POST", "/party/join", {"code": code.lower()}))
print("member queues", p2.call("POST", "/queue", {"modes": ["tdm2v2"]}))
print("leader queues", p1.call("POST", "/queue", {"modes": ["tdm2v2", "duel"]}))
st = p2.call("POST", "/status")
assert st["state"] == "searching" and st["party"]["leader"] == p1.pub, st
assert st["modes"] == ["tdm2v2"], st   # duel doesn't fit a party of two
c.call("POST", "/queue", {"modes": ["tdm2v2"]})
d.call("POST", "/queue", {"modes": ["tdm2v2"]})
time.sleep(2)
st = p1.call("POST", "/status")
assert st["state"] == "ready_check", st
team = st["red"] if p1.pub in st["red"] else st["blue"]
assert p2.pub in team and len(team) == 2, st
print("party on one team:", team == st["red"] and "red" or "blue")
# one declines: the party (all accepted) goes back, the decliner is blocked
p1.call("POST", "/ready", {"accept": True}); p2.call("POST", "/ready", {"accept": True})
c.call("POST", "/ready", {"accept": True}); d.call("POST", "/ready", {"accept": False})
time.sleep(2)
print("party after decline", p2.call("POST", "/status")["state"], "| decliner", d.call("POST", "/queue", {"modes": ["tdm2v2"]}))
print("leave", p2.call("POST", "/party/leave"), p1.call("POST", "/status")["party"]["members"] == [p1.pub])
print("ok")
