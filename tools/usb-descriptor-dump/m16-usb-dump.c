#include <libusb-1.0/libusb.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TARGET_VID 0x2d99
#define TARGET_PID 0xa020

static uint16_t detected_bcd_adc = 0;

static uint16_t le16(const unsigned char *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void print_hex(const char *label, const unsigned char *data, int length) {
    printf("%s (%d bytes):", label, length);
    for (int i = 0; i < length; i++) {
        if (i % 16 == 0) {
            printf("\n    ");
        }
        printf("%02x ", data[i]);
    }
    printf("\n");
}

static const char *speed_name(int speed) {
    switch (speed) {
        case LIBUSB_SPEED_LOW: return "Low Speed (1.5 Mb/s)";
        case LIBUSB_SPEED_FULL: return "Full Speed (12 Mb/s)";
        case LIBUSB_SPEED_HIGH: return "High Speed (480 Mb/s)";
        case LIBUSB_SPEED_SUPER: return "SuperSpeed (5 Gb/s)";
        case LIBUSB_SPEED_SUPER_PLUS: return "SuperSpeedPlus";
        default: return "Unknown";
    }
}

static const char *transfer_type_name(uint8_t attributes) {
    switch (attributes & LIBUSB_TRANSFER_TYPE_MASK) {
        case LIBUSB_TRANSFER_TYPE_CONTROL: return "Control";
        case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS: return "Isochronous";
        case LIBUSB_TRANSFER_TYPE_BULK: return "Bulk";
        case LIBUSB_TRANSFER_TYPE_INTERRUPT: return "Interrupt";
        default: return "Reserved";
    }
}

static const char *sync_type_name(uint8_t attributes) {
    switch ((attributes >> 2) & 0x03) {
        case 0: return "No synchronization";
        case 1: return "Asynchronous";
        case 2: return "Adaptive";
        case 3: return "Synchronous";
        default: return "Unknown";
    }
}

static const char *usage_type_name(uint8_t attributes) {
    switch ((attributes >> 4) & 0x03) {
        case 0: return "Data endpoint";
        case 1: return "Feedback endpoint";
        case 2: return "Implicit feedback data endpoint";
        case 3: return "Reserved";
        default: return "Unknown";
    }
}

static void parse_ac_descriptor(const unsigned char *d, int len) {
    if (len < 3) return;
    const uint8_t subtype = d[2];
    printf("      AC subtype: 0x%02x", subtype);
    switch (subtype) {
        case 0x01: {
            printf(" (HEADER)\n");
            if (len >= 5) {
                detected_bcd_adc = le16(d + 3);
                printf("      bcdADC: 0x%04x\n", detected_bcd_adc);
            }
            if (detected_bcd_adc >= 0x0200) {
                if (len >= 9) {
                    printf("      bCategory: 0x%02x\n", d[5]);
                    printf("      wTotalLength: %u\n", le16(d + 6));
                    printf("      bmControls: 0x%02x\n", d[8]);
                }
            } else if (len >= 8) {
                printf("      wTotalLength: %u\n", le16(d + 5));
                printf("      bInCollection: %u\n", d[7]);
                for (int i = 0; i < d[7] && 8 + i < len; i++) {
                    printf("      baInterfaceNr[%d]: %u\n", i, d[8 + i]);
                }
            }
            break;
        }
        case 0x02:
            printf(" (INPUT_TERMINAL)\n");
            if (len >= 8) {
                printf("      bTerminalID: %u\n", d[3]);
                printf("      wTerminalType: 0x%04x\n", le16(d + 4));
                printf("      bAssocTerminal: %u\n", d[6]);
                if (detected_bcd_adc >= 0x0200 && len >= 17) {
                    printf("      bCSourceID: %u\n", d[7]);
                    printf("      bNrChannels: %u\n", d[8]);
                    printf("      bmChannelConfig: 0x%08x\n", le32(d + 9));
                    printf("      iChannelNames: %u\n", d[13]);
                    printf("      bmControls: 0x%04x\n", le16(d + 14));
                    printf("      iTerminal: %u\n", d[16]);
                } else if (len >= 12) {
                    printf("      bNrChannels: %u\n", d[7]);
                    printf("      wChannelConfig: 0x%04x\n", le16(d + 8));
                    printf("      iChannelNames: %u\n", d[10]);
                    printf("      iTerminal: %u\n", d[11]);
                }
            }
            break;
        case 0x03:
            printf(" (OUTPUT_TERMINAL)\n");
            if (len >= 9) {
                printf("      bTerminalID: %u\n", d[3]);
                printf("      wTerminalType: 0x%04x\n", le16(d + 4));
                printf("      bAssocTerminal: %u\n", d[6]);
                printf("      bSourceID: %u\n", detected_bcd_adc >= 0x0200 && len >= 12 ? d[8] : d[7]);
                if (detected_bcd_adc >= 0x0200 && len >= 12) {
                    printf("      bCSourceID: %u\n", d[7]);
                    printf("      bmControls: 0x%04x\n", le16(d + 9));
                    printf("      iTerminal: %u\n", d[11]);
                } else {
                    printf("      iTerminal: %u\n", d[8]);
                }
            }
            break;
        case 0x04: printf(" (MIXER_UNIT)\n"); break;
        case 0x05: printf(" (SELECTOR_UNIT)\n"); break;
        case 0x06:
            printf(" (FEATURE_UNIT)\n");
            if (len >= 6) {
                printf("      bUnitID: %u\n", d[3]);
                printf("      bSourceID: %u\n", d[4]);
                if (detected_bcd_adc >= 0x0200) {
                    const int controls_bytes = len - 6;
                    const int entries = controls_bytes / 4;
                    for (int i = 0; i < entries; i++) {
                        printf("      bmaControls[%d]: 0x%08x\n", i, le32(d + 5 + i * 4));
                    }
                    printf("      iFeature: %u\n", d[len - 1]);
                } else {
                    const uint8_t control_size = d[5];
                    printf("      bControlSize: %u\n", control_size);
                    if (control_size > 0) {
                        const int entries = (len - 7) / control_size;
                        for (int i = 0; i < entries; i++) {
                            uint32_t value = 0;
                            for (int j = 0; j < control_size && j < 4; j++) {
                                value |= (uint32_t)d[6 + i * control_size + j] << (8 * j);
                            }
                            printf("      bmaControls[%d]: 0x%08x\n", i, value);
                        }
                        printf("      iFeature: %u\n", d[len - 1]);
                    }
                }
            }
            break;
        case 0x07: printf(" (PROCESSING_UNIT)\n"); break;
        case 0x08: printf(" (EXTENSION_UNIT)\n"); break;
        case 0x0a:
            printf(" (CLOCK_SOURCE)\n");
            if (len >= 8) {
                printf("      bClockID: %u\n", d[3]);
                printf("      bmAttributes: 0x%02x\n", d[4]);
                printf("      bmControls: 0x%02x\n", d[5]);
                printf("      bAssocTerminal: %u\n", d[6]);
                printf("      iClockSource: %u\n", d[7]);
            }
            break;
        case 0x0b:
            printf(" (CLOCK_SELECTOR)\n");
            if (len >= 7) {
                printf("      bClockID: %u\n", d[3]);
                printf("      bNrInPins: %u\n", d[4]);
                for (int i = 0; i < d[4] && 5 + i < len; i++) {
                    printf("      baCSourceID[%d]: %u\n", i, d[5 + i]);
                }
            }
            break;
        default: printf(" (UNKNOWN/OTHER)\n"); break;
    }
}

static void parse_as_descriptor(const unsigned char *d, int len) {
    if (len < 3) return;
    const uint8_t subtype = d[2];
    printf("      AS subtype: 0x%02x", subtype);
    if (subtype == 0x01) {
        printf(" (AS_GENERAL)\n");
        if (detected_bcd_adc >= 0x0200 && len >= 16) {
            printf("      bTerminalLink: %u\n", d[3]);
            printf("      bmControls: 0x%02x\n", d[4]);
            printf("      bFormatType: 0x%02x\n", d[5]);
            printf("      bmFormats: 0x%08x\n", le32(d + 6));
            printf("      bNrChannels: %u\n", d[10]);
            printf("      bmChannelConfig: 0x%08x\n", le32(d + 11));
            printf("      iChannelNames: %u\n", d[15]);
        } else if (len >= 7) {
            printf("      bTerminalLink: %u\n", d[3]);
            printf("      bDelay: %u\n", d[4]);
            printf("      wFormatTag: 0x%04x\n", le16(d + 5));
        }
    } else if (subtype == 0x02) {
        printf(" (FORMAT_TYPE)\n");
        if (len >= 4) {
            printf("      bFormatType: 0x%02x\n", d[3]);
        }
        if (detected_bcd_adc >= 0x0200) {
            if (len >= 6) {
                printf("      bSubslotSize: %u\n", d[4]);
                printf("      bBitResolution: %u\n", d[5]);
            }
        } else if (len >= 8) {
            printf("      bNrChannels: %u\n", d[4]);
            printf("      bSubframeSize: %u\n", d[5]);
            printf("      bBitResolution: %u\n", d[6]);
            printf("      bSamFreqType: %u\n", d[7]);
            if (d[7] == 0 && len >= 14) {
                uint32_t min_rate = (uint32_t)d[8] | ((uint32_t)d[9] << 8) | ((uint32_t)d[10] << 16);
                uint32_t max_rate = (uint32_t)d[11] | ((uint32_t)d[12] << 8) | ((uint32_t)d[13] << 16);
                printf("      tLowerSamFreq: %u Hz\n", min_rate);
                printf("      tUpperSamFreq: %u Hz\n", max_rate);
            } else {
                for (int i = 0; i < d[7] && 8 + i * 3 + 2 < len; i++) {
                    const int o = 8 + i * 3;
                    uint32_t rate = (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8) | ((uint32_t)d[o + 2] << 16);
                    printf("      tSamFreq[%d]: %u Hz\n", i, rate);
                }
            }
        }
    } else {
        printf(" (UNKNOWN/OTHER)\n");
    }
}

static void parse_cs_endpoint(const unsigned char *d, int len) {
    if (len < 3) return;
    printf("      CS_ENDPOINT subtype: 0x%02x", d[2]);
    if (d[2] == 0x01) {
        printf(" (EP_GENERAL)\n");
        if (len >= 4) printf("      bmAttributes: 0x%02x\n", d[3]);
        if (detected_bcd_adc >= 0x0200) {
            if (len >= 8) {
                printf("      bmControls: 0x%02x\n", d[4]);
                printf("      bLockDelayUnits: %u\n", d[5]);
                printf("      wLockDelay: %u\n", le16(d + 6));
            }
        } else if (len >= 7) {
            printf("      bLockDelayUnits: %u\n", d[4]);
            printf("      wLockDelay: %u\n", le16(d + 5));
        }
    } else {
        printf(" (UNKNOWN/OTHER)\n");
    }
}

static void parse_extra(const char *scope, const unsigned char *extra, int extra_length, uint8_t subclass) {
    if (!extra || extra_length <= 0) {
        printf("    %s extra descriptors: none\n", scope);
        return;
    }
    printf("    %s extra descriptors total: %d bytes\n", scope, extra_length);
    int offset = 0;
    while (offset + 2 <= extra_length) {
        const int len = extra[offset];
        if (len < 2 || offset + len > extra_length) {
            printf("    Invalid/truncated extra descriptor at offset %d\n", offset);
            print_hex("    Remaining raw bytes", extra + offset, extra_length - offset);
            break;
        }
        const unsigned char *d = extra + offset;
        char label[96];
        snprintf(label, sizeof(label), "    %s raw descriptor @ extra+%d, type 0x%02x", scope, offset, d[1]);
        print_hex(label, d, len);
        if (d[1] == 0x24) {
            if (subclass == 0x01) parse_ac_descriptor(d, len);
            else if (subclass == 0x02) parse_as_descriptor(d, len);
        } else if (d[1] == 0x25) {
            parse_cs_endpoint(d, len);
        } else if (d[1] == 0x0b && len >= 8) {
            printf("      IAD: firstInterface=%u interfaceCount=%u class=0x%02x subclass=0x%02x protocol=0x%02x iFunction=%u\n",
                   d[2], d[3], d[4], d[5], d[6], d[7]);
        }
        offset += len;
    }
}

static void print_string(libusb_device_handle *handle, const char *label, uint8_t index) {
    if (!index) {
        printf("  %s: <none>\n", label);
        return;
    }
    if (!handle) {
        printf("  %s: <device could not be opened; index %u>\n", label, index);
        return;
    }
    unsigned char value[256];
    const int rc = libusb_get_string_descriptor_ascii(handle, index, value, sizeof(value));
    if (rc < 0) printf("  %s: <error %s; index %u>\n", label, libusb_error_name(rc), index);
    else printf("  %s: %.*s\n", label, rc, value);
}

static void dump_raw_standard_descriptors(libusb_device_handle *handle) {
    if (!handle) {
        printf("\nRaw standard descriptor fetch skipped because device open failed.\n");
        return;
    }
    unsigned char device_raw[18];
    int rc = libusb_get_descriptor(handle, LIBUSB_DT_DEVICE, 0, device_raw, sizeof(device_raw));
    if (rc > 0) print_hex("\nRaw Device Descriptor", device_raw, rc);
    else printf("\nRaw Device Descriptor read error: %s\n", libusb_error_name(rc));

    unsigned char header[9];
    rc = libusb_get_descriptor(handle, LIBUSB_DT_CONFIG, 0, header, sizeof(header));
    if (rc < 9) {
        printf("Raw Configuration Descriptor header read error/short read: %s (%d)\n", rc < 0 ? libusb_error_name(rc) : "short read", rc);
        return;
    }
    const int total = le16(header + 2);
    unsigned char *raw = calloc((size_t)total, 1);
    if (!raw) return;
    rc = libusb_get_descriptor(handle, LIBUSB_DT_CONFIG, 0, raw, total);
    if (rc > 0) print_hex("Raw complete Configuration Descriptor stream", raw, rc);
    else printf("Raw complete Configuration Descriptor read error: %s\n", libusb_error_name(rc));
    free(raw);
}

static void dump_device(libusb_device *device, const struct libusb_device_descriptor *dd) {
    libusb_device_handle *handle = NULL;
    const int open_rc = libusb_open(device, &handle);

    printf("M16 USB descriptor dump (read-only)\n");
    printf("libusb runtime: %s\n", libusb_get_version()->describe);
    printf("Bus: %u  Address: %u  Port: %u\n", libusb_get_bus_number(device), libusb_get_device_address(device), libusb_get_port_number(device));
    printf("Negotiated speed: %s\n", speed_name(libusb_get_device_speed(device)));
    printf("libusb_open: %s\n", open_rc == 0 ? "SUCCESS" : libusb_error_name(open_rc));

    printf("\nDevice Descriptor\n");
    printf("  bLength: %u\n", dd->bLength);
    printf("  bDescriptorType: 0x%02x\n", dd->bDescriptorType);
    printf("  bcdUSB: 0x%04x\n", dd->bcdUSB);
    printf("  bDeviceClass: 0x%02x\n", dd->bDeviceClass);
    printf("  bDeviceSubClass: 0x%02x\n", dd->bDeviceSubClass);
    printf("  bDeviceProtocol: 0x%02x\n", dd->bDeviceProtocol);
    printf("  bMaxPacketSize0: %u\n", dd->bMaxPacketSize0);
    printf("  idVendor: 0x%04x (%u)\n", dd->idVendor, dd->idVendor);
    printf("  idProduct: 0x%04x (%u)\n", dd->idProduct, dd->idProduct);
    printf("  bcdDevice: 0x%04x\n", dd->bcdDevice);
    print_string(handle, "Manufacturer", dd->iManufacturer);
    print_string(handle, "Product", dd->iProduct);
    print_string(handle, "Serial Number", dd->iSerialNumber);
    printf("  bNumConfigurations: %u\n", dd->bNumConfigurations);

    dump_raw_standard_descriptors(handle);

    for (uint8_t ci = 0; ci < dd->bNumConfigurations; ci++) {
        struct libusb_config_descriptor *config = NULL;
        const int rc = libusb_get_config_descriptor(device, ci, &config);
        if (rc != 0 || !config) {
            printf("\nConfiguration %u read failed: %s\n", ci, libusb_error_name(rc));
            continue;
        }
        printf("\nConfiguration %u\n", ci);
        printf("  bLength: %u\n", config->bLength);
        printf("  bDescriptorType: 0x%02x\n", config->bDescriptorType);
        printf("  wTotalLength: %u\n", config->wTotalLength);
        printf("  bNumInterfaces: %u\n", config->bNumInterfaces);
        printf("  bConfigurationValue: %u\n", config->bConfigurationValue);
        printf("  iConfiguration: %u\n", config->iConfiguration);
        printf("  bmAttributes: 0x%02x (%s-powered%s)\n", config->bmAttributes,
               (config->bmAttributes & 0x40) ? "self" : "bus",
               (config->bmAttributes & 0x20) ? ", remote wakeup" : "");
        printf("  bMaxPower raw: 0x%02x (%u units)\n", config->MaxPower, config->MaxPower);
        printf("  MaxPower interpreted for USB 1.x/2.0: %u mA\n", config->MaxPower * 2u);
        parse_extra("Configuration", config->extra, config->extra_length, 0);

        for (int ii = 0; ii < config->bNumInterfaces; ii++) {
            const struct libusb_interface *iface = &config->interface[ii];
            printf("\n  Interface array index %d: %d alternate setting(s)\n", ii, iface->num_altsetting);
            for (int ai = 0; ai < iface->num_altsetting; ai++) {
                const struct libusb_interface_descriptor *alt = &iface->altsetting[ai];
                printf("  Interface %u, Alternate Setting %u\n", alt->bInterfaceNumber, alt->bAlternateSetting);
                printf("    bLength: %u\n", alt->bLength);
                printf("    bNumEndpoints: %u\n", alt->bNumEndpoints);
                printf("    bInterfaceClass: 0x%02x\n", alt->bInterfaceClass);
                printf("    bInterfaceSubClass: 0x%02x\n", alt->bInterfaceSubClass);
                printf("    bInterfaceProtocol: 0x%02x\n", alt->bInterfaceProtocol);
                printf("    iInterface: %u\n", alt->iInterface);
                parse_extra("Interface", alt->extra, alt->extra_length, alt->bInterfaceSubClass);

                for (int ei = 0; ei < alt->bNumEndpoints; ei++) {
                    const struct libusb_endpoint_descriptor *ep = &alt->endpoint[ei];
                    printf("    Endpoint %d\n", ei);
                    printf("      bEndpointAddress: 0x%02x (EP %u, %s)\n", ep->bEndpointAddress,
                           ep->bEndpointAddress & 0x0f,
                           (ep->bEndpointAddress & LIBUSB_ENDPOINT_IN) ? "IN/device-to-host" : "OUT/host-to-device");
                    printf("      bmAttributes: 0x%02x\n", ep->bmAttributes);
                    printf("      Transfer type: %s\n", transfer_type_name(ep->bmAttributes));
                    if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
                        printf("      Synchronization type: %s\n", sync_type_name(ep->bmAttributes));
                        printf("      Usage type: %s\n", usage_type_name(ep->bmAttributes));
                    }
                    printf("      wMaxPacketSize: 0x%04x (%u raw; %u-byte payload)\n", ep->wMaxPacketSize,
                           ep->wMaxPacketSize, ep->wMaxPacketSize & 0x07ff);
                    printf("      bInterval: %u\n", ep->bInterval);
                    printf("      bRefresh: %u\n", ep->bRefresh);
                    printf("      bSynchAddress: 0x%02x\n", ep->bSynchAddress);
                    parse_extra("Endpoint", ep->extra, ep->extra_length, alt->bInterfaceSubClass);
                }
            }
        }
        libusb_free_config_descriptor(config);
    }

    if (handle) libusb_close(handle);
}

int main(void) {
    libusb_context *ctx = NULL;
    libusb_device **devices = NULL;
    int rc = libusb_init(&ctx);
    if (rc != 0) {
        fprintf(stderr, "libusb_init failed: %s\n", libusb_error_name(rc));
        return 1;
    }
    const ssize_t count = libusb_get_device_list(ctx, &devices);
    if (count < 0) {
        fprintf(stderr, "libusb_get_device_list failed: %s\n", libusb_error_name((int)count));
        libusb_exit(ctx);
        return 1;
    }
    int found = 0;
    for (ssize_t i = 0; i < count; i++) {
        struct libusb_device_descriptor dd;
        rc = libusb_get_device_descriptor(devices[i], &dd);
        if (rc == 0 && dd.idVendor == TARGET_VID && dd.idProduct == TARGET_PID) {
            dump_device(devices[i], &dd);
            found++;
        }
    }
    libusb_free_device_list(devices, 1);
    libusb_exit(ctx);
    if (!found) {
        fprintf(stderr, "EDIFIER M16 Pro %04x:%04x not found\n", TARGET_VID, TARGET_PID);
        return 2;
    }
    return 0;
}
