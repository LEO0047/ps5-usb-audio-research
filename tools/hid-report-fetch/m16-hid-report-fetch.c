#include <libusb-1.0/libusb.h>

#include <stdint.h>
#include <stdio.h>

#define TARGET_VID 0x2d99
#define TARGET_PID 0xa020
#define HID_INTERFACE 0
#define HID_REPORT_LENGTH 317

int main(void) {
    libusb_context *ctx = NULL;
    libusb_device_handle *handle = NULL;
    unsigned char report[HID_REPORT_LENGTH];

    int rc = libusb_init(&ctx);
    if (rc != 0) {
        fprintf(stderr, "libusb_init: %s\n", libusb_error_name(rc));
        return 1;
    }

    handle = libusb_open_device_with_vid_pid(ctx, TARGET_VID, TARGET_PID);
    if (!handle) {
        fprintf(stderr, "device %04x:%04x not found or could not be opened\n", TARGET_VID, TARGET_PID);
        libusb_exit(ctx);
        return 2;
    }

    fprintf(stderr, "request: standard IN GET_DESCRIPTOR(HID_REPORT), interface=%d, requested=%d bytes\n",
            HID_INTERFACE, HID_REPORT_LENGTH);
    fprintf(stderr, "safety: no interface claim, no driver detach, no vendor request, no OUT transfer\n");

    rc = libusb_control_transfer(
        handle,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,
        LIBUSB_REQUEST_GET_DESCRIPTOR,
        (uint16_t)(LIBUSB_DT_REPORT << 8),
        HID_INTERFACE,
        report,
        sizeof(report),
        2000
    );

    if (rc < 0) {
        fprintf(stderr, "GET_DESCRIPTOR(HID_REPORT) failed: %s\n", libusb_error_name(rc));
        libusb_close(handle);
        libusb_exit(ctx);
        return 3;
    }

    fprintf(stderr, "received: %d bytes\n", rc);
    if (rc != HID_REPORT_LENGTH) {
        fprintf(stderr, "unexpected report length: expected %d, received %d\n", HID_REPORT_LENGTH, rc);
        libusb_close(handle);
        libusb_exit(ctx);
        return 4;
    }

    if (fwrite(report, 1, (size_t)rc, stdout) != (size_t)rc) {
        fprintf(stderr, "failed to write report bytes to stdout\n");
        libusb_close(handle);
        libusb_exit(ctx);
        return 5;
    }

    libusb_close(handle);
    libusb_exit(ctx);
    return 0;
}
