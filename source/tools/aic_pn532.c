// FeliCa AIC tool over PN532 / libnfc
//   aic_pn532 dump   <outfile>            full backup, user 00-0E + system 80-91
//   aic_pn532 read   <blk> [svc]
//   aic_pn532 write  <blk> <hex32> [svc]  read old -> write -> read back -> verify
//   aic_pn532 probe  [blk] [svc]          reversible write probe, always restores
//   aic_pn532 findsvc [blk]               which service accepts a write
// blk and svc are hex, svc defaults to 000B
#include <nfc/nfc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static nfc_device *pnd;
static uint8_t idm[8], pmm[8];

static int fel_read(uint16_t svc, uint8_t block, uint8_t out[16]) {
    uint8_t tx[17]; int i = 0;
    tx[i++] = 0x10; tx[i++] = 0x06;
    memcpy(tx + i, idm, 8); i += 8;
    tx[i++] = 0x01; tx[i++] = svc & 0xFF; tx[i++] = (svc >> 8) & 0xFF;
    tx[i++] = 0x01; tx[i++] = 0x80; tx[i++] = block;
    uint8_t rx[64];
    int r = nfc_initiator_transceive_bytes(pnd, tx, i, rx, sizeof(rx), 1000);
    if (r < 13 || rx[1] != 0x07) return -1;
    if (rx[10] != 0 || rx[11] != 0) return -(0x10000 + rx[10] * 256 + rx[11]);
    if (rx[12] != 1 || r < 29) return -3;
    memcpy(out, rx + 13, 16);
    return 0;
}

static int fel_write(uint16_t svc, uint8_t block, const uint8_t data[16]) {
    uint8_t tx[33]; int i = 0;
    tx[i++] = 0x20; tx[i++] = 0x08;
    memcpy(tx + i, idm, 8); i += 8;
    tx[i++] = 0x01; tx[i++] = svc & 0xFF; tx[i++] = (svc >> 8) & 0xFF;
    tx[i++] = 0x01; tx[i++] = 0x80; tx[i++] = block;
    memcpy(tx + i, data, 16); i += 16;
    uint8_t rx[64];
    int r = nfc_initiator_transceive_bytes(pnd, tx, i, rx, sizeof(rx), 1000);
    if (r < 12 || rx[1] != 0x09) return -1;
    if (rx[10] != 0 || rx[11] != 0) return -(0x10000 + rx[10] * 256 + rx[11]);
    return 0;
}

static void phex(const uint8_t *d, int n) { for (int i = 0; i < n; i++) printf("%02x", d[i]); }

static const char *errstr(int rc) {
    static char buf[48];
    if (rc == 0) return "OK";
    if (rc == -1) return "NO_RESPONSE";
    if (rc == -3) return "BAD_LENGTH";
    if (rc <= -0x10000) { int v = -rc - 0x10000;
        snprintf(buf, sizeof buf, "CARD_STATUS_%02X%02X", (v >> 8) & 0xFF, v & 0xFF);
        return buf; }
    return "UNKNOWN";
}

static const int BLOCKS[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,
                             0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
                             0x90,0x91};
#define NBLOCKS (int)(sizeof(BLOCKS)/sizeof(int))

static int open_card(void) {
    nfc_context *ctx; nfc_init(&ctx);
    pnd = nfc_open(ctx, NULL);
    if (!pnd) { printf("no nfc device (跑 pn532_fixport.sh 了吗)\n"); return -1; }
    nfc_initiator_init(pnd);
    nfc_target t[1];
    nfc_modulation nms[2] = { { NMT_FELICA, NBR_212 }, { NMT_FELICA, NBR_424 } };
    if (nfc_initiator_poll_target(pnd, nms, 2, 20, 10, t) < 1) { printf("no felica card\n"); return -1; }
    memcpy(idm, t[0].nti.nfi.abtId, 8);
    memcpy(pmm, t[0].nti.nfi.abtPad, 8);
    printf("IDm "); phex(idm, 8); printf("\nPMm "); phex(pmm, 8); printf("\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s dump|read|write|probe|findsvc ...\n", argv[0]); return 1; }
    if (open_card() < 0) return 1;
    uint16_t svc = (argc > 3 && strstr(argv[1], "dump") == NULL) ? 0 : 0x000B;

    if (strcmp(argv[1], "dump") == 0) {
        FILE *f = argc > 2 ? fopen(argv[2], "w") : NULL;
        if (argc > 2 && !f) { printf("cannot open %s\n", argv[2]); return 1; }
        if (f) {
            fprintf(f, "# IDm "); for (int k = 0; k < 8; k++) fprintf(f, "%02x", idm[k]);
            fprintf(f, "\n# PMm "); for (int k = 0; k < 8; k++) fprintf(f, "%02x", pmm[k]);
            fprintf(f, "\n");
        }
        for (int b = 0; b < NBLOCKS; b++) {
            uint8_t d[16]; int rc = fel_read(0x000B, BLOCKS[b], d);
            printf("blk %02X: ", BLOCKS[b]);
            if (rc == 0) {
                phex(d, 16); printf("\n");
                if (f) { fprintf(f, "%02x ", BLOCKS[b]); for (int k = 0; k < 16; k++) fprintf(f, "%02x", d[k]); fprintf(f, "\n"); }
            }
            else { printf("%s\n", errstr(rc)); if (f) fprintf(f, "%02x %s\n", BLOCKS[b], errstr(rc)); }
        }
        if (f) { fclose(f); printf("backup -> %s\n", argv[2]); }
        return 0;
    }

    if (strcmp(argv[1], "read") == 0) {
        if (argc < 3) { printf("usage: read <blk> [svc]\n"); return 1; }
        unsigned blk = strtoul(argv[2], NULL, 16);
        if (argc > 3) svc = (uint16_t)strtoul(argv[3], NULL, 16);
        uint8_t d[16]; int rc = fel_read(svc, blk, d);
        printf("svc %04X blk %02X: ", svc, blk);
        if (rc == 0) phex(d, 16); else printf("%s", errstr(rc));
        printf("\n");
        return rc == 0 ? 0 : 1;
    }

    if (strcmp(argv[1], "write") == 0) {
        if (argc < 4 || strlen(argv[3]) != 32) { printf("usage: write <blk> <32 hex> [svc]\n"); return 1; }
        unsigned blk = strtoul(argv[2], NULL, 16);
        if (argc > 4) svc = (uint16_t)strtoul(argv[4], NULL, 16);
        uint8_t data[16], old[16], rb[16];
        for (int i = 0; i < 16; i++) sscanf(argv[3] + 2*i, "%2hhx", &data[i]);
        int rc0 = fel_read(svc, blk, old);
        printf("old blk %02X: ", blk);
        if (rc0 == 0) phex(old, 16); else printf("%s", errstr(rc0));
        printf("\n");
        int rc = fel_write(svc, blk, data);
        printf("write svc %04X blk %02X: %s\n", svc, blk, errstr(rc));
        if (rc != 0) return 1;
        int rc2 = fel_read(svc, blk, rb);
        printf("new blk %02X: ", blk);
        if (rc2 == 0) phex(rb, 16); else printf("%s", errstr(rc2));
        printf("\n");
        if (rc2 != 0 || memcmp(rb, data, 16)) { printf("VERIFY FAILED\n"); return 1; }
        printf("VERIFY OK\n");
        return 0;
    }

    if (strcmp(argv[1], "probe") == 0) {
        unsigned blk = argc > 2 ? strtoul(argv[2], NULL, 16) : 0x0D;
        if (argc > 3) svc = (uint16_t)strtoul(argv[3], NULL, 16);
        uint8_t orig[16], pat[16], rb[16];
        int rc = fel_read(svc, blk, orig);
        if (rc != 0) { printf("blk %02X 读不出来 (%s)，不能做探针\n", blk, errstr(rc)); return 1; }
        for (int i = 0; i < 16; i++) pat[i] = (i & 1) ? 0x5A : 0xA5;
        if (!memcmp(pat, orig, 16)) memset(pat, 0, 16);
        printf("orig blk %02X: ", blk); phex(orig, 16); printf("\n");
        rc = fel_write(svc, blk, pat);
        printf("step1 写花样 svc %04X: %s\n", svc, errstr(rc));
        if (rc != 0) return 1;
        if (fel_read(svc, blk, rb) != 0 || memcmp(rb, pat, 16)) { printf("step2 回读不符\n"); fel_write(svc, blk, orig); return 1; }
        printf("step2 回读一致\n");
        rc = fel_write(svc, blk, orig);
        printf("step3 还原: %s\n", errstr(rc));
        if (rc != 0) { printf("!!! 还原失败，请手动写回: blk %02X = ", blk); phex(orig, 16); printf("\n"); return 1; }
        if (fel_read(svc, blk, rb) != 0 || memcmp(rb, orig, 16)) { printf("!!! 还原后回读不符\n"); return 1; }
        printf("step4 还原确认，卡可反复读写\n");
        return 0;
    }

    if (strcmp(argv[1], "findsvc") == 0) {
        unsigned blk = argc > 2 ? strtoul(argv[2], NULL, 16) : 0x0D;
        uint8_t orig[16], pat[16], rb[16];
        const uint16_t svcs[] = {0x000B, 0x0009, 0x000A, 0x000D, 0x0008};
        for (size_t s = 0; s < sizeof(svcs)/sizeof(svcs[0]); s++) {
            int rc = fel_read(svcs[s], blk, orig);
            printf("svc %04X read  blk %02X: %s%s\n", svcs[s], blk, errstr(rc), rc ? "" : "");
            if (rc != 0) { if (rc == 0) {} ; printf("              "); phex(orig, 16); printf("\n"); continue; }
            printf("              "); phex(orig, 16); printf("\n");
            for (int i = 0; i < 16; i++) pat[i] = (i & 1) ? 0x5A : 0xA5;
            if (!memcmp(pat, orig, 16)) memset(pat, 0, 16);
            int wc = fel_write(svcs[s], blk, pat);
            printf("svc %04X write blk %02X: %s\n", svcs[s], blk, errstr(wc));
            if (wc == 0) {
                int rr = fel_read(svcs[s], blk, rb);
                printf("              readback: %s %s\n", errstr(rr), (rr == 0 && !memcmp(rb, pat, 16)) ? "MATCH" : "MISMATCH");
                int rs = fel_write(svcs[s], blk, orig);
                printf("              restore: %s\n", errstr(rs));
                if (rs != 0) { printf("!!! 还原失败 svc %04X blk %02X = ", svcs[s], blk); phex(orig, 16); printf("\n"); }
            }
        }
        return 0;
    }

    printf("bad mode\n");
    return 1;
}
