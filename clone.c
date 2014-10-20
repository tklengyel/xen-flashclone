/*
 * Tamas K Lengyel (C) 2013
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/utsname.h> /* for utsname in xl info */
#include <xentoollog.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#include <limits.h>

#include <xenctrl.h>
#include <xen/hvm/params.h>

int hvm_params[] = {
        HVM_PARAM_ACPI_IOPORTS_LOCATION,
        HVM_PARAM_IDENT_PT,
        HVM_PARAM_PAGING_RING_PFN,
        HVM_PARAM_ACCESS_RING_PFN,
        HVM_PARAM_SHARING_RING_PFN,
        //HVM_PARAM_VM86_TSS,
        HVM_PARAM_CONSOLE_PFN,
        HVM_PARAM_ACPI_IOPORTS_LOCATION,
        HVM_PARAM_VIRIDIAN,
        HVM_PARAM_IOREQ_PFN,
        HVM_PARAM_BUFIOREQ_PFN,
        HVM_PARAM_STORE_PFN,
        HVM_PARAM_STORE_EVTCHN,
        HVM_PARAM_PAE_ENABLED
 };

#define HVM_PARAM_COUNT sizeof(hvm_params)/sizeof(int)

int main(int argc, char **argv) {

    if (argc < 3) {
        printf("Usage: %s origin-domID clone-domID\n", argv[0]);
        return 1;
    }

    domid_t origin = atoi(argv[1]), clone = atoi(argv[2]);

    xc_interface *xc = xc_interface_open(0, 0, 0);
    vcpu_guest_context_any_t vcpu_context;

    uint32_t hvm_context_size;
    uint8_t *hvm_context;

    if (xc == NULL) {
        fprintf(stderr, "xc_interface_open() failed!\n");
        return 0;
    }

    if (xc_vcpu_getcontext(xc, origin, 0, &vcpu_context)) {
        printf("Failed to get the VCPU context of domain %u\n", origin);
        return 1;
    }

    printf("Setting VCPU context of clone\n");

    if (xc_vcpu_setcontext(xc, clone, 0, &vcpu_context)) {
        printf("Failed to set the VCPU context of domain %u\n", clone);
    }

    /*printf("Setting HVM parameters of clone\n");

    int hvm_param_copy = 0;
    while (hvm_param_copy < HVM_PARAM_COUNT) {
        unsigned long value = 0;
        xc_get_hvm_param(xc, origin, hvm_params[hvm_param_copy], &value);
        if (value) {
            switch (hvm_params[hvm_param_copy]) {
            case HVM_PARAM_CONSOLE_PFN:
            case HVM_PARAM_IOREQ_PFN:
            case HVM_PARAM_BUFIOREQ_PFN:
            case HVM_PARAM_STORE_PFN:
                break;
            default:
                printf("Setting HVM param %i with value %lu\n",
                    hvm_params[hvm_param_copy], value);

                xc_set_hvm_param(xc, clone, hvm_params[hvm_param_copy], value);
            break;
            }
        }
        hvm_param_copy++;
    }*/

    hvm_context_size = xc_domain_hvm_getcontext(xc, origin, NULL, 0);
    if (hvm_context_size <= 0) {
        printf("HVM context size <= 0. Not an HVM domain?\n");
        return 1;
    }

    hvm_context = malloc(hvm_context_size * sizeof(uint8_t));

    if (xc_domain_hvm_getcontext(xc, origin, hvm_context, hvm_context_size)
            <= 0) {
        printf("Failed to get HVM context.\n");
        return 1;
    }

    xc_dominfo_t info;
    xc_domain_getinfo(xc, origin, 1, &info);
    int page = xc_domain_maximum_gpfn(xc, origin) + 1;
    printf("Sharing memory.. Origin domain has %lu kb ram and %i pages.\n",
            info.max_memkb, page);

    xc_memshr_control(xc, origin, 1);
    xc_memshr_control(xc, clone, 1);

    uint64_t shandle, chandle;
    int shared = 0;
    while (page >= 0) {
        page--;
        if (xc_memshr_nominate_gfn(xc, origin, page, &shandle)) {
            continue;
        }
        if (xc_memshr_nominate_gfn(xc, clone, page, &chandle)) {
            continue;
        }

        if (xc_memshr_share_gfns(xc, origin, page, shandle, clone, page,
                chandle))
            continue;

        shared++;
    }

    printf("Shared %i pages\n", shared);

    /*hvm_param_copy = 0;
    while (hvm_param_copy < HVM_PARAM_COUNT) {
        unsigned long value = 0;
        switch(hvm_params[hvm_param_copy]) {
            case HVM_PARAM_CONSOLE_PFN:
            case HVM_PARAM_IOREQ_PFN:
            case HVM_PARAM_BUFIOREQ_PFN:
            case HVM_PARAM_STORE_PFN:
                xc_get_hvm_param(xc, origin, hvm_params[hvm_param_copy], &value);
                if (value) {
                    printf("Setting HVM param %i with value %lu\n",
                        hvm_params[hvm_param_copy], value);

                    xc_clear_domain_page(xc, clone, hvm_params[hvm_param_copy]);
                    xc_set_hvm_param(xc, clone, hvm_params[hvm_param_copy], value);
                }
            break;
        }

        hvm_param_copy++;
    }*/

    /*printf("Setting HVM context of clone\n");

    if (xc_domain_hvm_setcontext(xc, clone, hvm_context, hvm_context_size)) {
        printf("Failed to set HVM context.\n");
        return 1;
    }*/

    xc_interface_close(xc);
    return 0;
}

