/* Disable the hardware prefetcher on:					   */
/*	Core2					  			   */
/*	Nehalem, Westmere, SandyBridge, IvyBridge, Haswell and Broadwell   */
/* See: https://software.intel.com/en-us/articles/disclosure-of-hw-prefetcher-control-on-some-intel-processors */
/*									   */
/* The key is MSR 0x1a4							   */
/* bit 0: L2 HW prefetcher						   */
/* bit 1: L2 adjacent line prefetcher					   */
/* bit 2: DCU (L1 Data Cache) next line prefetcher			   */
/* bit 3: DCU IP prefetcher (L1 Data Cache prefetch based on insn address) */
/*									   */
/* This code uses the /dev/msr interface, and you'll need to be root.      */
/*									   */
/* by Vince Weaver, vincent.weaver _at_ maine.edu -- 26 February 2016	   */

#define CORE2_PREFETCH_MSR    0x1a0
#define NHM_PREFETCH_MSR    0x1a4
#define ALL_CORES -1

#include <bits/stdc++.h>
using namespace std;


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <sys/syscall.h>


static int open_msr(int core) {

    char msr_filename[BUFSIZ];
    int fd;

    sprintf(msr_filename, "/dev/cpu/%d/msr", core);
    fd = open(msr_filename, O_RDWR);
    if (fd < 0) {
        if (errno == ENXIO) {
            fprintf(stderr, "rdmsr: No CPU %d\n", core);
            exit(2);
        } else if (errno == EIO) {
            fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n", core);
            exit(3);
        } else {
            perror("rdmsr:open");
            fprintf(stderr, "Trying to open %s\n", msr_filename);
            //exit(127);
        }
    }

    return fd;
}

static long long read_msr(int fd, int which) {

    uint64_t data;

    if (pread(fd, &data, sizeof data, which) != sizeof data) {
        perror("rdmsr:pread");
        exit(127);
    }

    return (long long) data;
}

static int write_msr(int fd, uint64_t which, uint64_t value) {

    if (pwrite(fd, &value, sizeof(value), which) != sizeof value) {
        perror("rdmsr:pwrite");
        exit(127);
    }

    return 0;
}

/* FIXME: should really error out if not an Intel CPU */
static int detect_cpu(void) {

    FILE *fff;

    int family, model = -1;
    char buffer[BUFSIZ], *result;
    char vendor[BUFSIZ];
    int is_core2 = -1;

    fff = fopen("/proc/cpuinfo", "r");
    if (fff == NULL) return -1;

    while (1) {
        result = fgets(buffer, BUFSIZ, fff);
        if (result == NULL) break;

        if (!strncmp(result, "vendor_id", 8)) {
            sscanf(result, "%*s%*s%s", vendor);

            if (strncmp(vendor, "GenuineIntel", 12)) {
                fprintf(stderr, "%s not an Intel chip\n", vendor);
                return -1;
            }
        }

        if (!strncmp(result, "cpu family", 10)) {
            sscanf(result, "%*s%*s%*s%d", &family);
            if (family != 6) {
                fprintf(stderr, "Wrong CPU family %d\n", family);
                return -1;
            }
        }

        if (!strncmp(result, "model", 5)) {
            sscanf(result, "%*s%*s%d", &model);
        }

    }

    fclose(fff);

    switch (model) {
        case 26:
        case 30:
        case 31: /* nhm */
            fprintf(stderr, "Found Nehalem CPU\n");
            is_core2 = 0;
            break;

        case 46: /* nhm-ex */
            fprintf(stderr, "Found Nehalem-EX CPU\n");
            is_core2 = 0;
            break;

        case 37:
        case 44: /* wsm */
            fprintf(stderr, "Found Westmere CPU\n");
            is_core2 = 0;
            break;

        case 47: /* wsm-ex */
            fprintf(stderr, "Found Westmere-EX CPU\n");
            is_core2 = 0;
            break;

        case 42: /* snb */
            fprintf(stderr, "Found Sandybridge CPU\n");
            is_core2 = 0;
            break;

        case 45: /* snb-ep */
            fprintf(stderr, "Found Sandybridge-EP CPU\n");
            is_core2 = 0;
            break;

        case 58: /* ivb */
            fprintf(stderr, "Found Ivybridge CPU\n");
            is_core2 = 0;
            break;


        case 62: /* ivb-ep */
            fprintf(stderr, "Found Ivybridge-EP CPU\n");
            is_core2 = 0;
            break;

        case 60:
        case 69:
        case 70: /* hsw */
            fprintf(stderr, "Found Haswell CPU\n");
            is_core2 = 0;
            break;

        case 63: /* hsw-ep */
            fprintf(stderr, "Found Haswell-EP CPU\n");
            is_core2 = 0;
            break;

        case 61:
        case 71: /* bdw */
            fprintf(stderr, "Found Broadwell CPU\n");
            is_core2 = 0;
            break;

        case 86:
        case 79: /* bdw-? */
            fprintf(stderr, "Found Broadwell-?? CPU\n");
            is_core2 = 0;
            break;

        case 15:
        case 22:
        case 23:
        case 29: /* core2 */
            fprintf(stderr, "Found Core2 CPU\n");
            is_core2 = 1;
            break;
        default:
            fprintf(stderr, "Unsupported model %d\n", model);
            is_core2 = -1;
            break;
    }

    return is_core2;
}


/* Enable prefetch on nehalem and newer */
static int set_prefetch_nhm(int core, bool enable) {

    int begin, end;

    fprintf(stderr, "%s all prefetch\n", enable ? "enable" : "disable");

    if (core == ALL_CORES) {
        begin = 0;
        end = 1024;
    } else {
        begin = core;
        end = core;
    }

    for (int core = begin; core <= end; core++) {

        int fd = open_msr(core);
        if (fd < 0) break;

        /* Read original results */
        int resultBefore = read_msr(fd, NHM_PREFETCH_MSR);

        write_msr(fd, NHM_PREFETCH_MSR, enable ? 0x0: 0xf);

        /* Verify change */
        int resultAfter = read_msr(fd, NHM_PREFETCH_MSR);

        cerr << "\tMSR for core " << dec << core << ": " << hex << resultBefore << " to " << resultAfter << endl;

        /*
         fprintf(stderr, "\tCore %d old : L2HW=%c L2ADJ=%c DCU=%c DCUIP=%c\n",
               core,
               resultBefore&0x1?'N':'Y',
               resultBefore&0x2?'N':'Y',
               resultBefore&0x4?'N':'Y',
               resultBefore&0x8?'N':'Y'
        );

        fprintf(stderr, "\tCore %d new : L2HW=%c L2ADJ=%c DCU=%c DCUIP=%c\n",
               core,
               resultAfter&0x1?'N':'Y',
               resultAfter&0x2?'N':'Y',
               resultAfter&0x4?'N':'Y',
               resultAfter&0x8?'N':'Y'
        );*/

        close(fd);

    }

    return 0;
}


int main(int argc, char* argv[]) {

    int c;
    int core = -1;
    int result = -1;
    bool enable = true;

    fprintf(stderr, "\n");

    opterr = 0;

    while ((c = getopt(argc, argv, "c:deh")) != -1) {
        switch (c) {
            case 'c':
                core = atoi(optarg);
                break;
            case 'd':
                enable = false;
                break;
            case 'e':
                enable = true;
                break;

            case 'h':
                fprintf(stderr, "Usage: %s [-c core] [-d] [-e] [-h]\n\n", argv[0]);
                exit(0);
            default:
                exit(-1);
        }
    }

    int coreType = detect_cpu();
    if (coreType < 0 || coreType == 1) {
        fprintf(stderr, "Unsupported CPU type\n");
        return -1;
    }

    result = set_prefetch_nhm(core, enable);


    if (result < 0) {

        fprintf(stderr, "Unable to access prefetch MSR.\n");
        fprintf(stderr, "* Verify you have an Intel Nehalem or newer processor\n");
        fprintf(stderr, "* You will probably need to run as root\n");
        fprintf(stderr, "* Make sure the msr module is installed\n");
        fprintf(stderr, "\n");

        return -1;

    }

    return 0;
}