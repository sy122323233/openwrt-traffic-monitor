#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_IPS 256
#define SNAP_LEN 1518
#define TIMEOUT 1000
#define HISTORY_SIZE 40
#define HTTP_PORT 8080

// IP 流量统计结构
typedef struct {
    char ip[16];
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    unsigned long long history_bytes[HISTORY_SIZE];
    int history_index;
    time_t last_update;
    unsigned long long peak_rate;
} ip_stats_t;

ip_stats_t ip_table[MAX_IPS];
int ip_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int running = 1;

// 查找或添加 IP
ip_stats_t* find_or_add_ip(const char* ip) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < ip_count; i++) {
        if (strcmp(ip_table[i].ip, ip) == 0) {
            pthread_mutex_unlock(&mutex);
            return &ip_table[i];
        }
    }
    if (ip_count < MAX_IPS) {
        strcpy(ip_table[ip_count].ip, ip);
        ip_table[ip_count].rx_bytes = 0;
        ip_table[ip_count].tx_bytes = 0;
        ip_table[ip_count].peak_rate = 0;
        ip_table[ip_count].history_index = 0;
        memset(ip_table[ip_count].history_bytes, 0, sizeof(ip_table[ip_count].history_bytes));
        ip_table[ip_count].last_update = time(NULL);
        ip_count++;
        pthread_mutex_unlock(&mutex);
        return &ip_table[ip_count - 1];
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

// 更新速率
void update_rates() {
    static time_t last_calc = 0;
    time_t now = time(NULL);
    if (last_calc == 0) { last_calc = now; return; }
    if (now - last_calc >= 1) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < ip_count; i++) {
            int idx = ip_table[i].history_index;
            unsigned long long bytes_this_sec = ip_table[i].history_bytes[idx];
            if (bytes_this_sec > ip_table[i].peak_rate)
                ip_table[i].peak_rate = bytes_this_sec;
            ip_table[i].history_index = (idx + 1) % HISTORY_SIZE;
            ip_table[i].history_bytes[ip_table[i].history_index] = 0;
        }
        last_calc = now;
        pthread_mutex_unlock(&mutex);
    }
}

// 计算平均速率
unsigned long long get_avg_rate(ip_stats_t* stat, int seconds) {
    if (seconds > HISTORY_SIZE) seconds = HISTORY_SIZE;
    unsigned long long total = 0;
    int idx = stat->history_index;
    for (int i = 0; i < seconds; i++) {
        int pos = (idx - i + HISTORY_SIZE) % HISTORY_SIZE;
        total += stat->history_bytes[pos];
    }
    return total / seconds;
}

// 数据包处理
void packet_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {
    struct ether_header *eth_header = (struct ether_header *)packet;
    if (ntohs(eth_header->ether_type) != ETHERTYPE_IP) return;
    
    struct ip *ip_header = (struct ip *)(packet + sizeof(struct ether_header));
    char src_ip[16], dst_ip[16];
    inet_ntop(AF_INET, &(ip_header->ip_src), src_ip, sizeof(src_ip));
    inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, sizeof(dst_ip));
    
    int packet_size = header->len;
    ip_stats_t *src_stat = find_or_add_ip(src_ip);
    ip_stats_t *dst_stat = find_or_add_ip(dst_ip);
    
    if (src_stat) {
        src_stat->tx_bytes += packet_size;
        src_stat->history_bytes[src_stat->history_index] += packet_size;
    }
    if (dst_stat) {
        dst_stat->rx_bytes += packet_size;
        dst_stat->history_bytes[dst_stat->history_index] += packet_size;
    }
    update_rates();
}

// 生成 JSON 响应
void send_json_response(int client_fd) {
    char buffer[65536];
    int offset = snprintf(buffer, sizeof(buffer),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n"
        "{\"timestamp\":%ld,\"clients\":[",
        time(NULL));
    
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < ip_count && offset < sizeof(buffer) - 256; i++) {
        ip_stats_t *s = &ip_table[i];
        unsigned long long avg2 = get_avg_rate(s, 2);
        unsigned long long avg10 = get_avg_rate(s, 10);
        unsigned long long avg40 = get_avg_rate(s, 40);
        
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
            "%s{\"ip\":\"%s\",\"rx\":%llu,\"tx\":%llu,\"peak\":%llu,\"avg2\":%llu,\"avg10\":%llu,\"avg40\":%llu}",
            (i == 0) ? "" : ",", s->ip, s->rx_bytes, s->tx_bytes, s->peak_rate, avg2, avg10, avg40);
    }
    pthread_mutex_unlock(&mutex);
    
    snprintf(buffer + offset, sizeof(buffer) - offset, "]}");
    
    send(client_fd, buffer, strlen(buffer), 0);
    close(client_fd);
}

// HTTP 服务线程
void* http_server_thread(void* arg) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(HTTP_PORT);
    
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);
    
    printf("HTTP server started on port %d\n", HTTP_PORT);
    
    while (running) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_fd >= 0) {
            char request[1024];
            recv(client_fd, request, sizeof(request) - 1, 0);
            send_json_response(client_fd);
        }
    }
    close(server_fd);
    return NULL;
}

// 命令行打印线程
void* print_stats_thread(void* arg) {
    while (running) {
        sleep(2);
        pthread_mutex_lock(&mutex);
        printf("\n========== Traffic Statistics ==========\n");
        printf("%-16s %-12s %-12s %-8s %-8s %-8s\n", "IP", "RX", "TX", "Avg2s", "Avg10s", "Peak");
        for (int i = 0; i < ip_count; i++) {
            ip_stats_t *s = &ip_table[i];
            printf("%-16s %-12llu %-12llu %-8llu %-8llu %-8llu\n",
                   s->ip, s->rx_bytes, s->tx_bytes, get_avg_rate(s, 2), get_avg_rate(s, 10), s->peak_rate);
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    char *dev = "br-lan";
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    pthread_t http_thread, print_thread;
    
    printf("Starting traffic monitor on %s...\n", dev);
    
    handle = pcap_open_live(dev, SNAP_LEN, 1, TIMEOUT, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Error: %s\n", errbuf);
        return 1;
    }
    
    pthread_create(&http_thread, NULL, http_server_thread, NULL);
    pthread_create(&print_thread, NULL, print_stats_thread, NULL);
    
    printf("Capturing... HTTP on port %d\n", HTTP_PORT);
    pcap_loop(handle, -1, packet_handler, NULL);
    
    running = 0;
    pthread_join(http_thread, NULL);
    pthread_join(print_thread, NULL);
    pcap_close(handle);
    return 0;
}
