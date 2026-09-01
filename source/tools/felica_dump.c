#include <nfc/nfc.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static nfc_device *pnd;
static uint8_t idm[8];

static int felica_read(uint16_t svc, uint8_t block, uint8_t out[16]) {
    uint8_t tx[17];
    int i = 0;
    tx[i++] = 0x10; // FeliCa LEN
    tx[i++] = 0x06;
    memcpy(tx + i, idm, 8); i += 8;
    tx[i++] = 0x01;
    tx[i++] = svc & 0xFF;
    tx[i++] = (svc >> 8) & 0xFF;
    tx[i++] = 0x01;
    tx[i++] = 0x80;
    tx[i++] = block;
    uint8_t rx[64];
    int r = nfc_initiator_transceive_bytes(pnd, tx, i, rx, sizeof(rx), 1000);
    if (block <= 1 || block == 0x82) {
        printf("dbg blk %02X r=%d rx: ", block, r);
        for (int k = 0; k < (r > 0 ? (r < 40 ? r : 40) : 0); k++) printf("%02x ", rx[k]);
        printf("\n");
    }
    if (r < 13 + 16) return -1;
    if (rx[1] != 0x07) return -1;
    if (rx[10] != 0 || rx[11] != 0) return -1;
    memcpy(out, rx + 13, 16);
    return 0;
}

int main(void) {
    nfc_context *ctx;
    nfc_init(&ctx);
    pnd = nfc_open(ctx, NULL);
    if (!pnd) { printf("open fail\n"); return 1; }
    nfc_initiator_init(pnd);
    nfc_target targets[1];
    nfc_modulation nms[2] = { { NMT_FELICA, NBR_212 }, { NMT_FELICA, NBR_424 } };
    int n = nfc_initiator_poll_target(pnd, nms, 2, 20, 10, targets);
    if (n < 1) { printf("no felica\n"); nfc_close(pnd); return 1; }
    memcpy(idm, targets[0].nti.nfi.abtId, 8);
    printf("IDm: ");
    for (int i = 0; i < 8; i++) printf("%02x ", idm[i]);
    printf("\nPMm: ");
    for (int i = 0; i < 8; i++) printf("%02x ", targets[0].nti.nfi.abtPad[i]);
    printf("\nSC:  %02x%02x\n", targets[0].nti.nfi.abtSysCode[0], targets[0].nti.nfi.abtSysCode[1]);

    const int blocks[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87};
    for (size_t b = 0; b < sizeof(blocks)/sizeof(int); b++) {
        uint8_t data[16];
        if (felica_read(0x000B, blocks[b], data) == 0) {
            printf("blk %02X: ", blocks[b]);
            for (int i = 0; i < 16; i++) printf("%02x ", data[i]);
            printf("\n");
        } else {
            printf("blk %02X: --\n", blocks[b]);
        }
    }
    nfc_close(pnd);
    nfc_exit(ctx);
    return 0;
}
