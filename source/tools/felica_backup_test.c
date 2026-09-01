// FeliCa AIC backup + safe write test (service 0x000B)
// usage: felica_backup_test backup <outfile>
//        felica_backup_test wtest <backupfile>
#include <nfc/nfc.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

static nfc_device *pnd;
static uint8_t idm[8];

static int felica_read(uint16_t svc, uint8_t block, uint8_t out[16]) {
    uint8_t tx[17]; int i = 0;
    tx[i++] = 0x10; tx[i++] = 0x06;
    memcpy(tx + i, idm, 8); i += 8;
    tx[i++] = 0x01; tx[i++] = svc & 0xFF; tx[i++] = (svc >> 8) & 0xFF;
    tx[i++] = 0x01; tx[i++] = 0x80; tx[i++] = block;
    uint8_t rx[64];
    int r = nfc_initiator_transceive_bytes(pnd, tx, i, rx, sizeof(rx), 1000);
    if (r < 12 || rx[1] != 0x07) return -1;
    if (rx[10] != 0 || rx[11] != 0) return -2; // status flags
    memcpy(out, rx + 13, 16);
    return 0;
}

static int felica_write(uint16_t svc, uint8_t block, const uint8_t data[16]) {
    uint8_t tx[33]; int i = 0;
    tx[i++] = 0x20; tx[i++] = 0x08; // Write Without Encryption
    memcpy(tx + i, idm, 8); i += 8;
    tx[i++] = 0x01; tx[i++] = svc & 0xFF; tx[i++] = (svc >> 8) & 0xFF;
    tx[i++] = 0x01; tx[i++] = 0x80; tx[i++] = block;
    memcpy(tx + i, data, 16); i += 16;
    uint8_t rx[64];
    int r = nfc_initiator_transceive_bytes(pnd, tx, i, rx, sizeof(rx), 1000);
    if (r < 12 || rx[1] != 0x09) return -1;
    if (rx[10] != 0 || rx[11] != 0) { printf("  write status: %02x %02x\n", rx[10], rx[11]); return -2; }
    return 0;
}

static void phex(const uint8_t *d, int n) { for (int i = 0; i < n; i++) printf("%02x", d[i]); }

static const int BLOCKS[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,
                             0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87};
#define NBLOCKS (int)(sizeof(BLOCKS)/sizeof(int))

static int open_card(void) {
    nfc_context *ctx; nfc_init(&ctx);
    pnd = nfc_open(ctx, NULL);
    if (!pnd) { printf("no device\n"); return -1; }
    nfc_initiator_init(pnd);
    nfc_target targets[1];
    nfc_modulation nms[2] = { { NMT_FELICA, NBR_212 }, { NMT_FELICA, NBR_424 } };
    if (nfc_initiator_poll_target(pnd, nms, 2, 20, 10, targets) < 1) { printf("no felica\n"); return -1; }
    memcpy(idm, targets[0].nti.nfi.abtId, 8);
    printf("IDm: "); for (int i = 0; i < 8; i++) printf("%02x ", idm[i]); printf("\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) { printf("usage: %s backup|wtest <file> | write <block> <hex32>\n", argv[0]); return 1; }
    if (open_card() < 0) return 1;


    if (strcmp(argv[1], "write") == 0) {
        if (argc < 4) { printf("usage: write <block> <32 hex chars>\n"); return 1; }
        unsigned blk = strtoul(argv[2], NULL, 16);
        if (strlen(argv[3]) != 32) { printf("need exactly 32 hex chars\n"); return 1; }
        uint8_t data[16], rb[16];
        for (int i = 0; i < 16; i++) sscanf(argv[3] + 2*i, "%2hhx", &data[i]);
        if (felica_read(0x000B, (uint8_t)blk, rb) == 0) { printf("old blk %02X: ", blk); phex(rb, 16); printf("\n"); }
        int rc = felica_write(0x000B, (uint8_t)blk, data);
        if (rc != 0) { printf("WRITE FAILED on blk %02X (rc=%d)\n", blk, rc); return 1; }
        if (felica_read(0x000B, (uint8_t)blk, rb) != 0) { printf("readback fail\n"); return 1; }
        printf("new blk %02X: ", blk); phex(rb, 16); printf("\n");
        if (memcmp(rb, data, 16) != 0) { printf("VERIFY MISMATCH!\n"); return 1; }
        printf("VERIFY OK\n");
        return 0;
    }

    if (strcmp(argv[1], "backup") == 0) {
        FILE *f = fopen(argv[2], "w");
        if (!f) { printf("cannot open out\n"); return 1; }
        fprintf(f, "# IDm "); phex(idm, 8); fprintf(f, "\n");
        printf("IDm "); phex(idm, 8); printf("\n");
        for (int b = 0; b < NBLOCKS; b++) {
            uint8_t d[16];
            int rc = felica_read(0x000B, BLOCKS[b], d);
            printf("blk %02X: ", BLOCKS[b]);
            if (rc == 0) {
                phex(d, 16); printf("\n");
                fprintf(f, "%02x ", BLOCKS[b]); 
                for (int i = 0; i < 16; i++) fprintf(f, "%02x", d[i]);
                fprintf(f, "\n");
            } else if (rc == -2) {
                printf("PROTECTED\n");
                fprintf(f, "%02x PROTECTED\n", BLOCKS[b]);
            } else {
                printf("ERROR\n");
                fprintf(f, "%02x ERROR\n", BLOCKS[b]);
            }
        }
        fclose(f);
        printf("backup saved: %s\n", argv[2]);
        return 0;
    }

    if (strcmp(argv[1], "wtest") == 0) {
        // load backup
        static uint8_t bk[256][16]; static char bkstate[256]; // 0=absent 1=data 2=protected
        FILE *f = fopen(argv[2], "r");
        if (!f) { printf("no backup file\n"); return 1; }
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            unsigned addr; char hex[64];
            if (sscanf(line, "%x %63s", &addr, hex) == 2) {
                if (strcmp(hex, "PROTECTED") == 0) bkstate[addr] = 2;
                else if (strcmp(hex, "ERROR") == 0) bkstate[addr] = 0;
                else { bkstate[addr] = 1; for (int i = 0; i < 16; i++) sscanf(hex + 2*i, "%2hhx", &bk[addr][i]); }
            }
        }
        fclose(f);

        const uint8_t TEST = 0x0D;
        uint8_t orig[16], pattern[16], tmp[16];
        printf("== step 1: read block %02X (must match backup)\n", TEST);
        if (felica_read(0x000B, TEST, orig) != 0) { printf("read fail\n"); return 1; }
        if (bkstate[TEST] != 1 || memcmp(orig, bk[TEST], 16)) { printf("backup mismatch, abort\n"); return 1; }
        printf("  matches backup\n");

        for (int i = 0; i < 16; i++) pattern[i] = (i & 1) ? 0x5A : 0xA5;
        printf("== step 2: write pattern probes on free blocks\n");
        int any_ok = 0;
        int probes[] = {0x0D, 0x05, 0x0E};
        for (int p = 0; p < 3; p++) {
            int pb = probes[p];
            int rc = felica_write(0x000B, pb, pattern);
            if (rc == 0) {
                printf("  blk %02X: WRITTEN (unexpected on issuer card)\n", pb);
                uint8_t rb[16];
                if (felica_read(0x000B, pb, rb) == 0 && memcmp(rb, pattern, 16) == 0) printf("  readback ok\n");
                if (felica_write(0x000B, pb, bk[pb]) != 0) { printf("  RESTORE FAILED on %02X!\n", pb); return 1; }
                printf("  restored\n");
                any_ok = 1;
            } else {
                printf("  blk %02X: WRITE DENIED\n", pb);
            }
        }
        printf(any_ok ? "some user blocks writable\n" : "ALL user block writes denied -> issuer write-lock\n");

        printf("== step 4: write attempt on protected block 0x01 (expect rejection)\n");
        int rc = felica_write(0x000B, 0x01, pattern);
        printf("  result: %s\n", rc == -2 ? "REJECTED (as expected)" : (rc == 0 ? "ACCEPTED ?! unexpected" : "no response"));

        printf("== step 5: full-card diff against backup\n");
        int diffs = 0;
        for (int b = 0; b < NBLOCKS; b++) {
            int addr = BLOCKS[b];
            uint8_t d[16];
            int r2 = felica_read(0x000B, addr, d);
            if (bkstate[addr] == 1) {
                if (r2 != 0) { printf("  blk %02X: now unreadable!\n", addr); diffs++; }
                else if (memcmp(d, bk[addr], 16)) { printf("  blk %02X: CHANGED!\n", addr); diffs++; }
            } else if (bkstate[addr] == 2) {
                if (r2 == 0) { printf("  blk %02X: was protected, now readable?!\n", addr); diffs++; }
            }
        }
        printf(diffs == 0 ? "all blocks identical to backup. SAFE\n" : "%d differences found!\n", diffs);
        return diffs == 0 ? 0 : 1;
    }
    printf("bad mode\n"); return 1;
}
