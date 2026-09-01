
import json, sys, os
_t = json.load(open(os.path.join(os.path.dirname(__file__), "spad0_tables.json")))
sBox, sBoxKey, hashAdd = _t["sBox"], _t["sBoxKey"], _t["hashAdd"]
nTables = len(sBox) - 1
def _rot_right(d, n, bits):
    prior = d[n-1]
    for i in range(n):
        cur = d[i]
        d[i] = ((cur >> bits) | ((prior & ((1 << bits) - 1)) << (8 - bits))) & 0xFF
        prior = cur
def _rot_left(d, n, bits):
    prior = d[0]
    for i in range(n-1, -1, -1):
        cur = d[i]
        d[i] = (((cur & ((1 << (8 - bits)) - 1)) << bits) | (prior >> (8 - bits))) & 0xFF
        prior = cur
def _inv_tables():
    inv = [[0]*256 for _ in range(nTables+1)]
    for i in range(nTables+1):
        for j in range(256):
            inv[i][sBox[i][j] ^ sBoxKey[i]] = j
    return inv
def spad0_decrypt(spad):
    spad = list(spad); inv = _inv_tables()
    spad = [inv[nTables][b] for b in spad]
    count = (spad[15] >> 4) + 7
    table = spad[15] + hashAdd * count
    for _ in range(count):
        table -= hashAdd
        _rot_right(spad, 15, 5)
        for i in range(15):
            spad[i] = inv[table % nTables][spad[i]]
    return bytes(spad)
def spad0_encrypt(spad):
    spad = list(spad)
    count = (spad[15] >> 4) + 7
    table = spad[15]
    for _ in range(count):
        t = table % nTables
        for i in range(15):
            spad[i] = sBox[t][spad[i]] ^ sBoxKey[t]
        _rot_left(spad, 15, 5)
        table += hashAdd
    return bytes((sBox[nTables][b] ^ sBoxKey[nTables]) for b in spad)
if __name__ == "__main__":
    data = bytes.fromhex(sys.argv[1])
    if sys.argv[-1] == "enc":
        print(spad0_encrypt(data).hex())
    else:
        dec = spad0_decrypt(data)
        print(dec.hex())
        print("access code:", dec[6:16].hex().upper())
