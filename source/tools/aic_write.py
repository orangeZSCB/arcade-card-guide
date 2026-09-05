#!/usr/bin/env python3
"""Amusement IC writer over a Sega 837-15396 / TN32MSEC003S style reader
(the "kobato" board on /dev/cu.usbmodem*).

Reader quirk: CMD_POLL (0x42) only reports a card at the moment it is presented.
So every action here waits for that single event, then immediately runs the
official FeliCa activation chain (Polling -> ReqSysCode -> Active2) before
touching any block.

Uses CMD_FELICA_THROUGH (0x71), service 0x000B, factory SPAD0 cipher.
Never touches block 0x80 (MC / write-lock).

  dump                 wait for card -> activate -> back up every readable block
  plan   [--code N]    show the SPAD0 payload that would be written
  write  [--code N]    backup -> RW probe -> write SPAD0 -> verify -> RW proof
  rwtest               reversible write test on a free user block
"""
import argparse
import os
import sys
import time
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aime_proto import (Reader, FEL_POLL, FEL_REQ_SYSCODE, FEL_ACTIVE2,   # noqa: E402
                        FEL_REQ_RESPONSE)
from spad0_crypto import spad0_encrypt, spad0_decrypt                     # noqa: E402

PORT = os.environ.get('AIME_PORT', '/dev/cu.usbmodem206F317847411')
SVC_RW = 0x000B          # AIC user + system service (read/write when not issuer-locked)
SVC_ALT = 0x0009         # fallback read/write service seen in official captures
USER_BLOCKS = list(range(0x00, 0x0F))
SYS_BLOCKS = [0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87]
LOCK_BLOCK = 0x80        # MC block: writing here can permanently lock the card
BACKUP_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'backups')
VERBOSE = False


def say(*a):
    if VERBOSE:
        print(*a, flush=True)


# ---------------------------------------------------------------- card access
def rearm(r):
    """Radio off/on to make the reader poll again."""
    r.cmd(0x41, retries=0)
    time.sleep(0.05)
    r.cmd(0x40, b'\x03', retries=0)


def detect(r, tries=240, gap=0.3, rearm_every=20):
    """Wait for a card. Two independent paths are tried each round:

    * CMD_POLL 0x42 - the reader's own latch, only fires when a card is presented
    * 0x71 FeliCa Polling - a direct RF command, works even if the latch already
      fired, and leaves the card activated
    """
    r.cmd(0x40, b'\x03', retries=0)
    for i in range(tries):
        d = r.detect()
        if d and d.get('idm'):
            print('  (通过 0x42 POLL 捕获)', flush=True)
            return d
        if d and not d.get('idm'):
            print('  检测到非 FeliCa 卡: %s' % d, flush=True)
            return d
        card, raw = r.felica_poll()
        if card:
            print('  (通过 0x71 FeliCa Polling 捕获, syscode=%04X)' % card['syscode'], flush=True)
            return card
        if rearm_every and i and i % rearm_every == 0:
            rearm(r)
        if i and i % 16 == 0:
            print('  ... 还在等卡 (%d 轮，请现在把卡贴上去)' % i, flush=True)
        time.sleep(gap)
    return None


def activate(r, idm):
    """Official FeliCa activation chain the game runs before reading blocks."""
    steps = []
    for name, res in (
        ('polling', r.felica(idm, FEL_POLL, b'\xff\xff\x01\x0f')),
        ('reqSysCode', r.felica(idm, FEL_REQ_SYSCODE, idm)),
        ('active2', r.felica(idm, FEL_ACTIVE2, idm + b'\x00')),
    ):
        st = res['status'] if res else None
        steps.append((name, st, res['payload'].hex(' ') if res else '-'))
        say('  activate %-11s status=%s payload=%s' % (name, st, steps[-1][2]))
    return steps


def fel_read(r, idm, svc, blk):
    """-> (state, data16, detail); state: ok | protected | error | noreply."""
    d, raw = r.felica_read(idm, svc, [blk])
    if raw is None:
        return 'noreply', None, 'no frame'
    if raw['status'] != 0:
        return 'error', None, 'frame_status=%02X' % raw['status']
    p = d['data'] if d else b''
    if len(p) < 11:
        return 'error', None, 'short payload %s' % p.hex(' ')
    st1, st2, n = p[8], p[9], p[10]
    if (st1, st2) != (0, 0):
        return 'protected', None, 'flags=%02X%02X' % (st1, st2)
    data = p[11:11 + 16 * n]
    if n != 1 or len(data) < 16:
        return 'error', None, 'numBlock=%d len=%d' % (n, len(data))
    return 'ok', data[:16], ''


def fel_write(r, idm, svc, blk, data16):
    d, raw = r.felica_write(idm, svc, blk, data16)
    if raw is None:
        return 'noreply', 'no frame'
    if raw['status'] != 0:
        return 'error', 'frame_status=%02X' % raw['status']
    p = d['data'] if d else b''
    if len(p) < 10:
        return 'error', 'short payload %s' % p.hex(' ')
    st1, st2 = p[8], p[9]
    return ('ok', '') if (st1, st2) == (0, 0) else ('denied', 'flags=%02X%02X' % (st1, st2))


def read_block(r, idm, svc, blk, retries=2):
    """Read with re-activation retries, since the reader drops the card."""
    last = None
    for attempt in range(retries + 1):
        st, data, detail = fel_read(r, idm, svc, blk)
        last = (st, data, detail)
        if st in ('ok', 'protected'):
            return last
        say('  blk %02X attempt %d -> %s %s' % (blk, attempt, st, detail))
        activate(r, idm)
    return last


def write_block(r, idm, svc, blk, data16, retries=2):
    last = None
    for attempt in range(retries + 1):
        st, detail = fel_write(r, idm, svc, blk, data16)
        last = (st, detail)
        if st in ('ok', 'denied'):
            return last
        say('  write blk %02X attempt %d -> %s %s' % (blk, attempt, st, detail))
        activate(r, idm)
    return last


def dump_card(r, idm, pmm=None):
    out = {}
    fails = 0
    for blk in USER_BLOCKS + SYS_BLOCKS:
        st, data, detail = read_block(r, idm, SVC_RW, blk)
        out[blk] = (st, data)
        if st == 'ok':
            print('  blk %02X: %s' % (blk, data.hex()))
        else:
            fails += 1
            print('  blk %02X: %s%s' % (blk, st.upper(), (' (%s)' % detail) if detail else ''))
        if fails and fails % 4 == 0:
            rearm(r)
            activate(r, idm)
    return out


def save_backup(idm, pmm, dump, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write('# kobato backup %s\n' % datetime.now().isoformat(timespec='seconds'))
        f.write('# IDm %s\n' % idm.hex())
        if pmm:
            f.write('# PMm %s\n' % pmm.hex())
        for blk, (st, data) in sorted(dump.items()):
            f.write('%02x %s\n' % (blk, data.hex() if data else st))
    print('  backup -> %s' % path)


# --------------------------------------------------------------- AIC payload
def code_from_idm(idm):
    """501 + zero-padded decimal IDm (17 digits) = 20 digits, SEGA-issued shape."""
    return '501' + str(int.from_bytes(idm, 'big')).zfill(17)


def spad0_payload(code):
    digits = code.replace('-', '').replace(' ', '')
    if len(digits) != 20 or not digits.isdigit():
        raise SystemExit('access code must be 20 digits, got %r' % code)
    plain = b'\x00' * 6 + bytes.fromhex(digits)
    if plain[6] & 0xF0 != 0x50:
        raise SystemExit('access code must start with 5x to be an Amusement IC code')
    cipher = spad0_encrypt(plain)
    assert spad0_decrypt(cipher) == plain, 'cipher round-trip failed'
    return plain, cipher


def card_string(idm):
    try:
        from bemani_card import CardCipher
        return CardCipher.encode(idm.hex().upper())
    except Exception as exc:                                  # pragma: no cover
        return '(CardCipher unavailable: %s)' % exc


def fmt_code(code):
    return '-'.join(code[i:i + 4] for i in range(0, 20, 4))


# ---------------------------------------------------------------------- main
def open_reader():
    r = Reader(PORT, baud=115200, timeout=0.2, verbose=False)
    info = r.boot()
    print('reader  fw=%r hw=%r' % (info['fw'], info['hw']))
    if info['hw'] is None:
        raise SystemExit('reader did not answer, check cable/port')
    return r


def acquire(r, args):
    print('=' * 60)
    print('现在把 FeliCa 卡贴到读卡器上（卡已经在上面的话，拿起来重新贴一次）')
    print('=' * 60, flush=True)
    d = detect(r, tries=args.tries)
    if not d:
        raise SystemExit('no card detected')
    idm, pmm = d.get('idm'), d.get('pmm')
    print('card    type=%02X IDm=%s PMm=%s' % (d['type'], idm.hex() if idm else '-',
                                               pmm.hex() if pmm else '-'))
    if not idm:
        raise SystemExit('not a FeliCa card (type %02X)' % d['type'])
    print('激活链路:')
    for name, st, pl in activate(r, idm):
        print('  %-11s status=%s %s' % (name, st, pl))
    return d, idm, pmm


def cmd_dump(args):
    r = open_reader()
    d, idm, pmm = acquire(r, args)
    print('dumping service %04X:' % SVC_RW)
    dump = dump_card(r, idm, pmm)
    path = os.path.join(BACKUP_DIR, 'kobato_%s_%s.txt'
                        % (idm.hex(), datetime.now().strftime('%Y%m%d_%H%M%S')))
    save_backup(idm, pmm, dump, path)
    st, data = dump.get(0x00, ('error', None))
    if data:
        dec = spad0_decrypt(data)
        print('SPAD0   plaintext %s' % dec.hex())
        if dec[5] == 0 and dec[6] & 0xF0 == 0x50:
            print('        access code %s' % fmt_code(dec[6:].hex()))
            print('        e-amusement card no. %s' % card_string(idm))
        else:
            print('        (not an Amusement IC block)')
    ok = sum(1 for v in dump.values() if v[0] == 'ok')
    print('可读块 %d/%d' % (ok, len(dump)))
    r.close()


def cmd_plan(args):
    r = open_reader()
    d, idm, pmm = acquire(r, args)
    code = args.code or code_from_idm(idm)
    plain, cipher = spad0_payload(code)
    cur_st, cur, _ = read_block(r, idm, SVC_RW, 0x00)
    print('current blk 00 %s (%s)' % (cur.hex() if cur else '-', cur_st))
    print('access code    %s' % fmt_code(code))
    print('SPAD0 plain    %s' % plain.hex())
    print('SPAD0 cipher   %s' % cipher.hex())
    print('e-amusement    %s' % card_string(idm))
    r.close()


def probe_free_block(dump):
    for blk in (0x0D, 0x0C, 0x0B, 0x05):
        st, data = dump.get(blk, ('error', None))
        if st == 'ok' and data is not None:
            return blk, data
    return None, None


def rw_probe(r, idm, blk, original):
    pattern = bytes((0x5A if i & 1 else 0xA5) for i in range(16))
    if pattern == original:
        pattern = bytes(16)
    st, detail = write_block(r, idm, SVC_RW, blk, pattern)
    if st != 'ok':
        print('  RW probe blk %02X: %s %s' % (blk, st, detail))
        return False
    rst, back, _ = read_block(r, idm, SVC_RW, blk)
    ok = rst == 'ok' and back == pattern
    st2, d2 = write_block(r, idm, SVC_RW, blk, original)
    rst2, back2, _ = read_block(r, idm, SVC_RW, blk)
    restored = back2 == original
    print('  RW probe blk %02X: write %s, restore %s' % (blk, 'OK' if ok else 'FAIL',
                                                         'OK' if restored else 'FAIL'))
    return ok and restored


def cmd_write(args):
    r = open_reader()
    d, idm, pmm = acquire(r, args)
    code = args.code or code_from_idm(idm)
    plain, cipher = spad0_payload(code)
    print('target  access code %s' % fmt_code(code))

    print('step 1  full backup')
    dump = dump_card(r, idm, pmm)
    path = os.path.join(BACKUP_DIR, 'kobato_%s_%s.txt'
                        % (idm.hex(), datetime.now().strftime('%Y%m%d_%H%M%S')))
    save_backup(idm, pmm, dump, path)

    print('step 2  reversible RW probe (proves the card is not issuer-locked)')
    blk, original = probe_free_block(dump)
    if blk is None:
        raise SystemExit('no readable free user block for the safety probe, aborting')
    if not rw_probe(r, idm, blk, original):
        raise SystemExit('RW probe failed, card is locked or unstable, aborting')

    if args.dry_run:
        print('dry run, nothing written')
        r.close()
        return

    print('step 3  write SPAD0 (block 0x00, service %04X)' % SVC_RW)
    st, detail = write_block(r, idm, SVC_RW, 0x00, cipher)
    if st != 'ok':
        print('  service %04X refused (%s %s), retrying with %04X' % (SVC_RW, st, detail, SVC_ALT))
        st, detail = write_block(r, idm, SVC_ALT, 0x00, cipher)
        if st != 'ok':
            raise SystemExit('write failed: %s %s' % (st, detail))
    print('  write accepted')

    print('step 4  verify')
    vst, vdata, vd = read_block(r, idm, SVC_RW, 0x00)
    if vst != 'ok' or vdata != cipher:
        raise SystemExit('verify failed: state=%s detail=%s data=%s'
                         % (vst, vd, vdata.hex() if vdata else '-'))
    dec = spad0_decrypt(vdata)
    print('  on-card cipher    %s' % vdata.hex())
    print('  decrypted         %s' % dec.hex())
    print('  access code       %s' % fmt_code(dec[6:].hex()))
    print('  e-amusement card  %s' % card_string(idm))

    print('step 5  post-write RW proof (card must still be rewritable)')
    dump2 = dump_card(r, idm, pmm)
    if not rw_probe(r, idm, blk, dump2[blk][1]):
        raise SystemExit('card is no longer writable!')

    changed = [b for b in USER_BLOCKS + SYS_BLOCKS
               if dump[b][0] == 'ok' and dump2[b][0] == 'ok' and dump[b][1] != dump2[b][1]]
    print('step 6  diff vs backup: changed blocks %s' % (['%02X' % b for b in changed] or 'none'))
    print('done. lock block %02X untouched, card still read/write' % LOCK_BLOCK)
    r.close()


def cmd_rwtest(args):
    r = open_reader()
    d, idm, pmm = acquire(r, args)
    dump = dump_card(r, idm, pmm)
    blk, original = probe_free_block(dump)
    if blk is None:
        raise SystemExit('no free block')
    rw_probe(r, idm, blk, original)
    r.close()


def main():
    global VERBOSE
    ap = argparse.ArgumentParser()
    ap.add_argument('action', choices=['dump', 'plan', 'write', 'rwtest'])
    ap.add_argument('--code', help='explicit 20 digit access code (default 501+decimal IDm)')
    ap.add_argument('--tries', type=int, default=240)
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('-v', '--verbose', action='store_true')
    args = ap.parse_args()
    VERBOSE = args.verbose
    {'dump': cmd_dump, 'plan': cmd_plan, 'write': cmd_write, 'rwtest': cmd_rwtest}[args.action](args)


if __name__ == '__main__':
    main()
