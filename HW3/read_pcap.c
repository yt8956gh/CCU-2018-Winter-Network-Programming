#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <time.h>




#define ETHER_ADDR_LEN 6
#define STR_BUF 16


/* Formats of IP/TCP in package refer to https://www.tcpdump.org/pcap.html */

/* Ethernet header */
struct sniff_ethernet
{
    u_char ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
    u_char ether_shost[ETHER_ADDR_LEN]; /* Source host address */
    u_short ether_type; /* IP? ARP? RARP? etc */
};

 /* IP header */
struct sniff_ip
{
    u_char ip_vhl; /* version << 4 | header length >> 2 */
    u_char ip_tos; /* type of service */
    u_short ip_len; /* total length */
    u_short ip_id; /* identification */
    u_short ip_off; /* fragment offset field */
    #define IP_RF 0x8000 /* reserved fragment flag */
    #define IP_DF 0x4000 /* dont fragment flag */
    #define IP_MF 0x2000 /* more fragments flag */
    #define IP_OFFMASK 0x1fff /* mask for fragmenting bits */
    u_char ip_ttl; /* time to live */
    u_char ip_p; /* protocol */
    u_short ip_sum; /* checksum */
    struct in_addr ip_src,ip_dst; /* source and dest address */
};

#define IP_HL(ip)((ip)->ip_vhl &0x0f)
#define IP_V(ip)(((ip)->ip_vhl)>> 4)

/* TCP header */
typedef u_int tcp_seq;

struct sniff_tcp 
{
    u_short th_sport; /*source port*/
    u_short th_dport; /*destination port */
    tcp_seq th_seq; /*sequence number*/
    tcp_seq th_ack; /*acknowledgement number */
    u_char th_offx2; /*data offset, rsvd */
    #define TH_OFF(th)(((th) -> th_offx2&0xf0)>> 4)
    u_char th_flags;
    #define TH_FIN 0x01
    #define TH_SYN 0x02
    #define TH_RST 0x04
    #define TH_PUSH 0x08
    #define TH_ACK 0x10
    #define TH_URG 0x20
    #define TH_ECE 0x40
    #define TH_CWR 0x80
    #define TH_FLAGS (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
    u_short th_win; /*window*/
    u_short th_sum; /*checksum*/
    u_short th_urp; /*urgent pointer*/
};

#define SIZE_ETHERNET 14


// Type number of protocol
#define ETHERTYPE_CPU 0x9000, 0x010c
#define ETHERTYPE_VLAN 0x8100, 0x9100, 0x9200, 0x9300
#define ETHERTYPE_MPLS 0x8847
#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_IPV6 0x86dd
#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_RARP 0x8035
#define ETHERTYPE_NSH 0x894f
#define ETHERTYPE_ETHERNET 0x6558
#define ETHERTYPE_ROCE 0x8915
#define ETHERTYPE_FCOE 0x8906

void analysis_package(const u_char *packet); // 

static const char *ip_ntoa(struct in_addr i); // ip to string

void time_transfer(time_t time_tmp); // time_t to string


int main(int argc, const char** argv)
{
    char ebuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle; // pcap handler
    struct bpf_program fcode;
    int counter=1;

    const char *filter = "";
    const char *fname = "";

    if(argc==2) // no filter string
    {
        fname = argv[1];
        handle = pcap_open_offline(fname, ebuf);
    }
    else if(argc==3)
    {
        fname = argv[2];
        handle = pcap_open_offline(fname, ebuf);
        filter = argv[1];
    }
    else
    {
        fprintf(stderr,"Syntax Error\n");
        exit(-1);
    }

    if (pcap_compile(handle, &fcode, filter, 0, PCAP_NETMASK_UNKNOWN) == -1) 
    {
        fprintf(stderr, "pcap_compile(): %s\n", pcap_geterr(handle));
        pcap_close(handle);
        exit(1);
    }

    if(strlen(filter) != 0) {
        printf("Filter: %s\n", filter);
    }


    while (1){

        struct pcap_pkthdr *header = NULL;
        const u_char *content = NULL;
        int ret = pcap_next_ex(handle, &header, &content);

        if(ret == 1) {

            if(pcap_offline_filter(&fcode, header, content) != 0) {  
                //total_amount++;
                //total_bytes += header->caplen;
                printf("\n------ Package %2d -------\n\n",counter);

                time_t time_tmp = header->ts.tv_sec;

                time_transfer(time_tmp); 
                
                analysis_package(content);  
                printf("Time out:%d\n",header->caplen);
                printf("Length: %d\n", header->len);
                //printf("--------------------------\n");   
            }//end if match
            counter++;
        }//end if success
        else if(ret == 0) {
            printf("Timeout\n");
        }//end if timeout
        else if(ret == -1) {
            fprintf(stderr, "pcap_next_ex(): %s\n", pcap_geterr(handle));
        }//end if fail
        else if(ret == -2) {
            printf("\n[Finish] No more packet from file\n");
            break;
        }//end if read no more packet
    }
    pcap_close(handle);
    return 0;
}


void time_transfer(time_t time_tmp)
{
    static char str_time[100];
    struct tm *local_time = NULL;

    local_time = localtime(&time_tmp);
    strftime(str_time, sizeof(str_time), "%Y-%m-%d  %H:%M:%S", local_time);
    printf ("[Time]\t\t%s\n", str_time);
}

static const char *ip_ntoa(struct in_addr i) {
    static char ip[STR_BUF][INET_ADDRSTRLEN];
    static int which = -1;

    which = (which + 1 == STR_BUF ? 0 : which + 1);
    
    memset(ip[which], 0, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &i, ip[which], sizeof(ip[which]));
    
    return ip[which];
}//end ip_ntoa


void analysis_package(const u_char *packet)
{

    const struct sniff_ethernet *ethernet; /*以太網頭*/
    const struct sniff_ip *ip; /*IP頭*/
    const struct sniff_tcp *tcp; /*TCP頭*/
    u_int size_ip;
    u_int size_tcp;

    ethernet =(struct sniff_ethernet *)(packet);
    ip =(struct sniff_ip *)(packet + SIZE_ETHERNET);
    size_ip = IP_HL(ip)* 4;

    u_int16_t type = ntohs(ethernet->ether_type);

    printf("[Ethernet Type]\t");

    switch (type) {

        case ETHERTYPE_IPV4:
            printf("IPv4\n");
            break;
            
        case ETHERTYPE_RARP:
            printf("RARP\n");
            return;
            
        case ETHERTYPE_IPV6:
            printf("IPv6\n");
            break;
            
        default:
            printf("Unknown %#06x\n", type);
            return;
    }//end switch


    switch(ip->ip_p)
    {
        case IPPROTO_TCP:
            printf("[Protocol]\tTCP\n\n");
            break;

        case IPPROTO_UDP:
            printf("[Protocol]\tUDP\n\n");
            break;

        case IPPROTO_ICMP:
            printf("[Protocol]\tICMP\n\n");
            break;

        case IPPROTO_IP:
            printf("[Protocol]\tIP\n\n");
            break;

        default:
            printf("[Protocol]\tunknown\n\n");
            return;
    }

    if(size_ip <20){
        //printf("[Error] Invalid IP header length: %u bytes\n",size_ip);
        //return;
    }

    tcp = (struct sniff_tcp *)(packet+ SIZE_ETHERNET + size_ip);
    size_tcp = TH_OFF(tcp)* 4;

    printf("[Source IP]\t%s\n", ip_ntoa(ip->ip_src));
    printf("[Dest. IP]\t%s\n\n", ip_ntoa(ip->ip_dst));



    if(size_tcp <20){
        //printf("[Error] Invalid TCP header length: %u bytes\n",size_tcp);
        //return;
    }

    printf("[Source Port]\t%u\n",ntohs(tcp->th_sport));
    printf("[Dest. Port]\t%u\n\n",ntohs(tcp->th_dport));

    return;
}