#include "onlydrop/platform.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif

uint64_t onlydrop_monotonic_ns(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0U;
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
}

int onlydrop_stdin_is_interactive(void) { return isatty(STDIN_FILENO); }

int onlydrop_available_memory_bytes(uint64_t *output) {
    if (output == NULL) return -1;
#if defined(__APPLE__)
    vm_statistics64_data_t statistics;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t page_size = 0U;
    const mach_port_t host = mach_host_self();
    uint64_t reclaimable_pages;
    if (host_page_size(host, &page_size) != KERN_SUCCESS ||
        host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&statistics, &count) != KERN_SUCCESS) {
        return -1;
    }
    reclaimable_pages = (uint64_t)statistics.free_count + (uint64_t)statistics.inactive_count;
    if (reclaimable_pages > UINT64_MAX / (uint64_t)page_size) return -1;
    *output = reclaimable_pages * (uint64_t)page_size;
    return 0;
#elif defined(__linux__)
    struct sysinfo information;
    uint64_t pages;
    if (sysinfo(&information) != 0) return -1;
    pages = (uint64_t)information.freeram + (uint64_t)information.bufferram;
    if (information.mem_unit == 0U || pages > UINT64_MAX / (uint64_t)information.mem_unit) return -1;
    *output = pages * (uint64_t)information.mem_unit;
    return 0;
#else
    return -1;
#endif
}

static bool is_private_ipv4(uint32_t address) {
    return (address & UINT32_C(0xff000000)) == UINT32_C(0x0a000000) ||
           (address & UINT32_C(0xfff00000)) == UINT32_C(0xac100000) ||
           (address & UINT32_C(0xffff0000)) == UINT32_C(0xc0a80000);
}

static bool should_skip_interface(const char *name) {
    return strncmp(name, "lo", 2U) == 0 || strncmp(name, "utun", 4U) == 0 ||
           strncmp(name, "docker", 6U) == 0 || strncmp(name, "veth", 4U) == 0 ||
           strncmp(name, "vmnet", 5U) == 0 || strncmp(name, "bridge", 6U) == 0;
}

int onlydrop_choose_public_ipv4(const char *requested_bind, char *output, size_t output_size) {
    struct in_addr requested;
    struct ifaddrs *interfaces = NULL;
    if (output_size < INET_ADDRSTRLEN) return -1;
    if (requested_bind != NULL && strcmp(requested_bind, "0.0.0.0") != 0 &&
        inet_pton(AF_INET, requested_bind, &requested) == 1) {
        return inet_ntop(AF_INET, &requested, output, (socklen_t)output_size) != NULL ? 0 : -1;
    }
    if (getifaddrs(&interfaces) == 0) {
        for (const struct ifaddrs *current = interfaces; current != NULL; current = current->ifa_next) {
            const struct sockaddr_in *address;
            uint32_t host_address;
            if (current->ifa_addr == NULL || current->ifa_addr->sa_family != AF_INET ||
                (current->ifa_flags & IFF_UP) == 0U || should_skip_interface(current->ifa_name)) continue;
            address = (const struct sockaddr_in *)current->ifa_addr;
            host_address = ntohl(address->sin_addr.s_addr);
            if (is_private_ipv4(host_address) && inet_ntop(AF_INET, &address->sin_addr, output, (socklen_t)output_size) != NULL) {
                freeifaddrs(interfaces);
                return 0;
            }
        }
        freeifaddrs(interfaces);
    }
    (void)snprintf(output, output_size, "127.0.0.1");
    return 0;
}
