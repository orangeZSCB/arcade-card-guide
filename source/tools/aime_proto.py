"""Sega Aime reader (TN32MSEC003S / 837-15396) serial protocol client.

Frame (request):  E0 <frame_len> <addr> <seq> <cmd> <payload_len> <payload...> <sum>
                  frame_len = 5 + payload_len  (counts bytes after the len byte, incl. sum)
Frame (response): E0 <frame_len> <addr> <seq> <cmd> <status> <payload_len> <payload...> <sum>
                  frame_len = 6 + payload_len
0xD0 is the escape byte:  D0 <b-1> encodes b in {0xE0, 0xD0}.
sum = (len_byte + all data bytes) & 0xFF, computed on unescaped values.
"""
import time
import serial

STATUS = {
    0x00: "OK", 0x01: "CARD_ERROR", 0x02: "NOT_ACCEPT", 0x03: "INVALID_COMMAND",
    0x04: "INVALID_DATA", 0x05: "SUM_ERROR", 0x06: "INTERNAL_ERROR",
    0x07: "INVALID_FIRM_DATA", 0x08: "FIRM_UPDATE_SUCCESS",
    0x10: "COMP_DUMMY_2ND", 0x20: "COMP_DUMMY_3RD",
}

CMD = {
    0x30: "GET_FW_VERSION", 0x32: "GET_HW_VERSION", 0x40: "START_POLLING",
    0x41: "STOP_POLLING", 0x42: "CARD_DETECT", 0x43: "CARD_SELECT",
    0x44: "CARD_HALT", 0x50: "MIFARE_KEY_SET_A", 0x51: "MIFARE_AUTHORIZE_A",
    0x52: "MIFARE_READ", 0x54: "MIFARE_KEY_SET_B", 0x55: "MIFARE_AUTHORIZE_B",
    0x60: "TO_UPDATER_MODE", 0x61: "SEND_HEX_DATA", 0x62: "TO_NORMAL_MODE",
    0x63: "SEND_BINDATA_INIT", 0x64: "SEND_BINDATA_EXEC",
    0x71: "FELICA_THROUGH", 0x81: "EXT_BOARD_LED_RGB", 0xF0: "EXT_BOARD_INFO",
    0xF2: "EXT_FIRM_SUM",
}

# FeliCa encap sub-commands (inside 0x71)
FEL_POLL = 0x00
FEL_REQ_RESPONSE = 0x04
FEL_READ = 0x06
FEL_WRITE = 0x08
FEL_REQ_SYSCODE = 0x0C
FEL_ACTIVE2 = 0xA4


def esc(b):
    if b in (0xE0, 0xD0):
        return bytes([0xD0, (b - 1) & 0xFF])
    return bytes([b])


def build(addr, seq, cmd, payload=b""):
    body = bytes([addr, seq & 0xFF, cmd, len(payload)]) + bytes(payload)
    frame_len = 5 + len(payload)
    raw = bytes([frame_len]) + body
    s = sum(raw) & 0xFF
    return b"\xE0" + b"".join(esc(x) for x in raw + bytes([s]))


class Reader:
    def __init__(self, port, baud=115200, timeout=0.25, verbose=False):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self.ser.dtr = True
        self.ser.rts = True
        self.verbose = verbose
        self.seq = 1
        self.buf = bytearray()

    def log(self, *a):
        if self.verbose:
            print(*a)

    def _read_frame(self, timeout=2.0):
        """Parse one response frame. Returns (addr, seq, cmd, status, payload)."""
        end = time.time() + timeout
        state = "hunt"
        frame_len = 0
        data = bytearray()
        escape = False
        while time.time() < end:
            chunk = self.ser.read(64)
            if not chunk:
                continue
            for r in chunk:
                if state == "hunt":
                    if r == 0xE0:
                        state = "len"
                    continue
                if state == "len":
                    frame_len = r
                    data = bytearray()
                    state = "body"
                    continue
                if escape:
                    r = (r + 1) & 0xFF
                    escape = False
                elif r == 0xD0:
                    escape = True
                    continue
                elif r == 0xE0:
                    frame_len = r
                    data = bytearray()
                    continue
                data.append(r)
                if len(data) == frame_len:
                    body = bytes(data[:-1])
                    got = data[-1]
                    want = (frame_len + sum(body)) & 0xFF
                    ok = got == want
                    addr, seq, cmd, status, plen = body[0], body[1], body[2], body[3], body[4]
                    payload = body[5:5 + plen]
                    self.log("  <<< frame_len=%02X sum_ok=%s cmd=%02X status=%02X payload=%s"
                             % (frame_len, ok, cmd, status, payload.hex(" ")))
                    state = "hunt"
                    return dict(addr=addr, seq=seq, cmd=cmd, status=status,
                                payload=payload, sum_ok=ok, body=body)
        return None

    def cmd(self, cmd, payload=b"", addr=0x00, timeout=2.0, retries=2):
        seq = self.seq
        self.seq = (self.seq + 1) & 0xFF
        frame = build(addr, seq, cmd, payload)
        for attempt in range(retries + 1):
            self.ser.reset_input_buffer()
            self.log("  >>> %s" % frame.hex(" ").upper())
            self.ser.write(frame)
            self.ser.flush()
            res = self._read_frame(timeout)
            if res is None:
                self.log("  (no reply, attempt %d)" % (attempt + 1))
                continue
            return res
        return None

    # ---- convenience ----
    def boot(self):
        """Official amdaemon startup sequence (segatools doc/nfc.txt)."""
        out = {}
        out['reset'] = self.cmd(0x62, retries=0)
        out['fw'] = self.fw_version()
        out['hw'] = self.hw_version()
        self.cmd(0xF5, addr=0x08, retries=0)          # LED sub-board reset
        out['board'] = self.cmd(0xF0, addr=0x08, retries=0)
        self.cmd(0x54, bytes.fromhex('574343467632'), retries=0)   # MIFARE KeyA (Aime)
        self.cmd(0x50, bytes.fromhex('6090D00632F5'), retries=0)   # MIFARE KeyB (Banapassport)
        out['radio'] = self.cmd(0x40, b'\x03', retries=0)         # RADIO_ON
        return out

    def fw_version(self):
        r = self.cmd(0x30)
        return r["payload"].decode("latin1") if r and r["status"] == 0 else None

    def hw_version(self):
        r = self.cmd(0x32)
        return r["payload"].decode("latin1") if r and r["status"] == 0 else None

    def board_info(self):
        return self.cmd(0xF0, addr=0x08)

    def detect(self):
        """CMD_CARD_DETECT -> dict(type, idm, pmm) or None."""
        r = self.cmd(0x42, timeout=1.5, retries=1)
        if not r or r["status"] != 0:
            return None
        p = r["payload"]
        if not p or p[0] == 0:
            return None
        count, ctype, idlen = p[0], p[1], p[2]
        out = dict(count=count, type=ctype, id_len=idlen, raw=p)
        if ctype == 0x20 and len(p) >= 19:
            out["idm"] = bytes(p[3:11])
            out["pmm"] = bytes(p[11:19])
        elif ctype == 0x10:
            out["uid"] = bytes(p[3:3 + idlen])
        return out

    # ---- FeliCa encap through 0x71 ----
    def felica(self, idm, code, encap_payload, timeout=2.5):
        """Send CMD_FELICA_THROUGH. encap_len field = 1 + len(encap bytes)."""
        encap = bytes([code]) + bytes(encap_payload)
        payload = bytes(idm) + bytes([len(encap) + 1]) + encap
        r = self.cmd(0x71, payload, timeout=timeout)
        if not r:
            return None
        return r

    def felica_parse(self, r):
        """Return (status_word, encap_code, encap_payload) from a 0x71 response."""
        if not r or r["status"] != 0:
            return None
        p = r["payload"]
        if len(p) < 2:
            return None
        encap_len, code = p[0], p[1]
        return dict(encap_len=encap_len, code=code, data=p[2:])

    def felica_poll(self, idm_hint=b"\xFF" * 8, syscode=0xFFFF, req=0x01, t=0x0F):
        """Active FeliCa polling through 0x71. Returns (card_dict|None, raw_frame).

        Response encap: code 0x01 + IDm(8) + PMm(8) + system code(2).
        This path is not subject to the reader's report-once latch on CMD_POLL.
        """
        sc = bytes([syscode & 0xFF, syscode >> 8])
        r = self.felica(idm_hint, FEL_POLL, sc + bytes([req, t]))
        d = self.felica_parse(r)
        if d and d['code'] == 0x01 and len(d['data']) >= 18:
            data = d['data']
            return dict(type=0x20, id_len=0x10, idm=bytes(data[0:8]),
                        pmm=bytes(data[8:16]),
                        syscode=int.from_bytes(data[16:18], 'little'), raw=r), r
        return None, r

    def felica_read(self, idm, service, blocks):
        """blocks: list of ints. Block list element encoded as 2 bytes: 0x80, blk (3-byte form
        uses 0x00 prefix; 2-byte short form has high bit set => 0x80 | (blk>>8)? )
        We use the documented form: [0x80, blk] for blk < 0x100 with len byte 0x80."""
        svc = bytes([service & 0xFF, service >> 8])
        bl = b"".join(bytes([0x80, b & 0xFF]) for b in blocks)
        pl = bytes(idm) + bytes([1]) + svc + bytes([len(blocks)]) + bl
        r = self.felica(idm, FEL_READ, pl)
        d = self.felica_parse(r)
        return d, r

    def felica_write(self, idm, service, block, data16):
        svc = bytes([service & 0xFF, service >> 8])
        bl = bytes([0x80, block & 0xFF])
        pl = bytes(idm) + bytes([1]) + svc + bytes([1]) + bl + bytes(data16)
        r = self.felica(idm, FEL_WRITE, pl)
        return self.felica_parse(r), r

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass
