#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <string.h>

int main(int argc, const char** argv){
    char ebuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle; // pcap handler
    struct bpf_program fcode;
    int counter=1;

    const char *filter = "";

    if(argc==2) // no filter string
    {
        handle = pcap_open_offline(argv[1], ebuf);
    }
    else if(argc==3)
    {
        handle = pcap_open_offline(argv[2], ebuf);
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
            printf("No more packet from file\n");
            break;
        }//end if read no more packet
    }
    pcap_close(handle);
    return 0;
}