// Write classic Sega Aime data to a MIFARE Classic 1K card via PN532 (libnfc)
// Layout: sector 0
//   blk1 = "SBSD" + zeros
//   blk2 = 00 x6 + 20-digit access code in BCD (bytes 6..15)
//   blk3 = KeyA FFFFFFFFFFFF, ACL FF 07 80 69, KeyB "WCCFv2" (57 43 43 46 76 32)
// blk0 (manufacturer/UID) is never touched.
#include <nfc/nfc.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "mifare.h"

static nfc_device *pnd;
static uint8_t uid_last4[4];
static const uint8_t KEY_AIME[6] = {0x57,0x43,0x43,0x46,0x76,0x32};
static const uint8_t KEY_FF[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static bool auth(uint8_t block, const uint8_t key[6], bool keyB) {
    mifare_param mp; memset(&mp, 0, sizeof(mp));
    memcpy(mp.mpa.abtKey, key, 6);
    memcpy(mp.mpa.abtAuthUid, uid_last4, 4);
    return nfc_initiator_mifare_cmd(pnd, keyB ? MC_AUTH_B : MC_AUTH_A, block, &mp);
}

static bool read_block(uint8_t block, uint8_t out[16]) {
    mifare_param mp; memset(&mp, 0, sizeof(mp));
    if (!nfc_initiator_mifare_cmd(pnd, MC_READ, block, &mp)) return false;
    memcpy(out, mp.mpd.abtData, 16);
    return true;
}

static bool write_block(uint8_t block, const uint8_t data[16]) {
    mifare_param mp; memset(&mp, 0, sizeof(mp));
    memcpy(mp.mpd.abtData, data, 16);
    return nfc_initiator_mifare_cmd(pnd, MC_WRITE, block, &mp);
}

static void hexout(const uint8_t *d, int n) { for (int i = 0; i < n; i++) printf("%02x ", d[i]); printf("\n"); }

static bool reselect(nfc_target *t) {
    nfc_modulation nm = { NMT_ISO14443A, NBR_106 };
    return nfc_initiator_select_passive_target(pnd, nm, NULL, 0, t) >= 1;
}

int main(void) {
    nfc_context *ctx; nfc_init(&ctx);
    pnd = nfc_open(ctx, NULL);
    if (!pnd) { printf("no device\n"); return 1; }
    nfc_initiator_init(pnd);
    nfc_target t;
    if (!reselect(&t)) { printf("no tag on reader\n"); return 1; }
    printf("UID: "); hexout(t.nti.nai.abtUid, t.nti.nai.szUidLen);
    memcpy(uid_last4, t.nti.nai.abtUid + t.nti.nai.szUidLen - 4, 4);

    // access code = "5010000000" + decimal(UID, 10 digits) -> BCD
    uint32_t uid_dec = ((uint32_t)uid_last4[0]<<24) | ((uint32_t)uid_last4[1]<<16) |
                       ((uint32_t)uid_last4[2]<<8) | uid_last4[3];
    char code[21];
    snprintf(code, sizeof(code), "501%07u%010u", 0u, uid_dec); // placeholder, fixed below
    snprintf(code, sizeof(code), "5010000000%010u", uid_dec);
    printf("access code: %s\n", code);
    uint8_t blk2_new[16] = {0};
    for (int i = 0; i < 10; i++)
        blk2_new[6+i] = ((code[2*i]-'0') << 4) | (code[2*i+1]-'0');

    // 1) probe auth on sector 0
    bool ok = auth(2, KEY_AIME, true);
    printf("auth KeyB WCCFv2: %s\n", ok ? "OK" : "FAIL");
    if (!ok) { reselect(&t); ok = auth(2, KEY_AIME, false); printf("auth KeyA WCCFv2: %s\n", ok?"OK":"FAIL"); }
    if (!ok) { reselect(&t); ok = auth(2, KEY_FF, false); printf("auth KeyA FF: %s\n", ok?"OK":"FAIL"); }
    if (!ok) { reselect(&t); ok = auth(2, KEY_FF, true); printf("auth KeyB FF: %s\n", ok?"OK":"FAIL"); }
    if (!ok) { printf("no usable key, abort\n"); return 1; }

    uint8_t b0[16], b1[16], b2[16], b3[16];
    printf("blk0: "); if (read_block(0, b0)) hexout(b0, 16); else printf("read fail\n");
    printf("blk1: "); if (read_block(1, b1)) hexout(b1, 16); else printf("read fail\n");
    printf("blk2: "); if (read_block(2, b2)) hexout(b2, 16); else printf("read fail\n");
    printf("blk3: "); if (read_block(3, b3)) hexout(b3, 16); else printf("read fail\n");

    // 2) writability check: write back block 2 unchanged
    if (!write_block(2, b2)) { printf("WRITE DENIED on blk2, card not writable. abort\n"); return 1; }
    uint8_t chk[16];
    if (!read_block(2, chk) || memcmp(chk, b2, 16)) { printf("writeback verify failed, abort\n"); return 1; }
    printf("writability check: OK (blk2 write-back verified)\n");

    // 3) write aime data: blk1, blk2, then trailer
    uint8_t blk1_new[16] = {'S','B','S','D',0,0,0,0,0,0,0,0,0,0,0,0};
    uint8_t blk3_new[16] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0x07,0x80,0x69,
                            0x57,0x43,0x43,0x46,0x76,0x32};
    if (!write_block(1, blk1_new)) { printf("write blk1 FAIL\n"); return 1; }
    printf("blk1 written (SBSD)\n");
    if (!write_block(2, blk2_new)) { printf("write blk2 FAIL\n"); return 1; }
    printf("blk2 written (access code)\n");
    if (!write_block(3, blk3_new)) { printf("write trailer FAIL\n"); return 1; }
    printf("trailer written (KeyA=FF, ACL=FF078069, KeyB=WCCFv2)\n");

    // 4) re-select, verify with aime KeyB
    if (!reselect(&t)) { printf("reselect fail\n"); return 1; }
    if (!auth(2, KEY_AIME, true)) { printf("post-write auth with KeyB FAIL\n"); return 1; }
    uint8_t v1[16], v2[16], v3[16];
    printf("verify blk1: "); bool r1 = read_block(1, v1); if (r1) hexout(v1, 16);
    printf("verify blk2: "); bool r2 = read_block(2, v2); if (r2) hexout(v2, 16);
    printf("verify blk3: "); bool r3 = read_block(3, v3); if (r3) hexout(v3, 16);
    if (!r1 || !r2 || !r3) { printf("verify read fail\n"); return 1; }
    // KeyA/KeyB always read back as zeros (never readable); compare ACL + KeyB position via ACL only
    if (memcmp(v1, blk1_new, 16) || memcmp(v2, blk2_new, 16) || memcmp(v3 + 6, blk3_new + 6, 4)) {
        printf("verify MISMATCH\n"); return 1;
    }
    char code_rd[21];
    for (int i = 0; i < 10; i++) { code_rd[2*i] = '0' + (v2[6+i] >> 4); code_rd[2*i+1] = '0' + (v2[6+i] & 0xF); }
    code_rd[20] = 0;
    printf("readback access code: %s\n", code_rd);

    // 5) prove still writable after personalization
    if (!write_block(1, v1)) { printf("NOT writable after personalization!\n"); return 1; }
    if (!read_block(1, chk) || memcmp(chk, v1, 16)) { printf("post-write recheck fail\n"); return 1; }
    printf("still writable after personalization: OK\n");
    printf("DONE\n");
    nfc_close(pnd); nfc_exit(ctx);
    return 0;
}
