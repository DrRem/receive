#include "h264.h"


unsigned int temp[3] = {0,0,0};
// 0: index
// 1: send_timestamp
// 2: receive_timestamp

int last_index = 1;
int lost_mark = 0;
int lost_long = 0;
int lost_count = 0;
double lost_time = 0;

double jitter = 0;
double B = 0;
int late = 0;


struct timeval tv;

void measure(const PRTPDATA *prtpData);

void foo(const PRTPDATA *prtpData, unsigned int receive_timestamp);

typedef struct
{
    int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
    unsigned len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
    unsigned max_size;            //! Nal Unit Buffer size
    int forbidden_bit;            //! should be always FALSE
    int nal_reference_idc;        //! NALU_PRIORITY_xxxx
    int nal_unit_type;            //! NALU_TYPE_xxxx
    char *buf;                    //! contains the first byte followed by the EBSP
    unsigned short lost_packets;  //! true, if packet loss is detected
} NALU_t;

FILE * poutfile = NULL;
char * outputfilename = "receive.264";

long  send_count = 0,receive_count=0;

int  OpenBitstreamFile(char *fn)
{
    if (NULL == (poutfile = fopen(fn, "wb")))
    {
        printf("Error: Open input file error\n");
        getchar();
    }
    return 1;
}

NALU_t *AllocNALU(int buffersize)
{
    NALU_t *n;

    if ((n = (NALU_t*)calloc(1, sizeof(NALU_t))) == NULL)
    {
        printf("AllocNALU Error: Allocate Meory To NALU_t Failed ");
        exit(0);
    }
    return n;
}

void FreeNALU(NALU_t *n)
{
    if (n)
    {
        free(n);
    }
}

void rtp_unpackage(char *bufIn, int len)
{
    unsigned char recvbuf[1500];
    RTPpacket_t *p = NULL;
    RTP_FIXED_HEADER * rtp_hdr = NULL;
    NALU_HEADER * nalu_hdr = NULL;
    NALU_t * n = NULL;
    FU_INDICATOR	*fu_ind = NULL;
    FU_HEADER		*fu_hdr = NULL;

    PRTPDATA * prtpData = NULL;

    int total_bytes = 0;                 //当前包传出的数据
    static int total_recved = 0;         //一共传输的数据
    int fwrite_number = 0;               //存入文件的数据长度

    gettimeofday(&tv,NULL);       // get timestamp

    memcpy(recvbuf, bufIn, len);          //复制rtp包
    // printf("包长度+ rtp头：   = %d\n", len);

    //////////////////////////////////////////////////////////////////////////
    //begin rtp_payload and rtp_header

    p = (RTPpacket_t*)&recvbuf[0];
    if ((p = (RTPpacket_t*) malloc(sizeof(RTPpacket_t))) == NULL)
    {
        printf("RTPpacket_t MMEMORY ERROR\n");
    }
    if ((p->payload = (unsigned char *)malloc(MAXDATASIZE)) == NULL)
    {
        printf("RTPpacket_t payload MMEMORY ERROR\n");
    }

    if ((rtp_hdr =(RTP_FIXED_HEADER*) malloc(sizeof(RTP_FIXED_HEADER))) == NULL)
    {
        printf("RTP_HEADER MEMORY ERROR\n");
    }

    rtp_hdr = (RTP_FIXED_HEADER*)&recvbuf[0];
    // printf("版本号 : %d\n", rtp_hdr->version);
    p->v = rtp_hdr->version;
    p->p = rtp_hdr->padding;
    p->x = rtp_hdr->extension;
    p->cc = rtp_hdr->csrc_len;
    // printf("标志位 : %d\n", rtp_hdr->marker);
    p->m = rtp_hdr->marker;
    printf("负载类型:%d\n", rtp_hdr->payload);
    p->pt = rtp_hdr->payload;
    // printf("包号   : %d \n", rtp_hdr->seq_no);
    p->seq = rtp_hdr->seq_no;
    // printf("时间戳 : %d\n", rtp_hdr->timestamp);
    p->timestamp = rtp_hdr->timestamp;
    // printf("帧号   : %d\n", rtp_hdr->ssrc);
    p->ssrc = rtp_hdr->ssrc;


    if (rtp_hdr->payload == PRTP)
    {
        prtpData = (PRTPDATA*)&recvbuf[12];
        measure(prtpData);
        return;

    }

    //end rtp_payload and rtp_header
    //////////////////////////////////////////////////////////////////////////
    //begin nal_hdr
    if (!(n = AllocNALU(800000)))          //为结构体nalu_t及其成员buf分配空间。返回值为指向nalu_t存储空间的指针
    {
        printf("NALU_t MMEMORY ERROR\n");
    }
    if ((nalu_hdr = (NALU_HEADER *)malloc(sizeof(NALU_HEADER))) == NULL)
    {
        printf("NALU_HEADER MEMORY ERROR\n");
    }

    nalu_hdr = (NALU_HEADER*)&recvbuf[12];                        //网络传输过来的字节序 ，当存入内存还是和文档描述的相反，只要匹配网络字节序和文档描述即可传输正确。
    // printf("forbidden_zero_bit: %d\n", nalu_hdr->F);              //网络传输中的方式为：F->NRI->TYPE.. 内存中存储方式为 TYPE->NRI->F (和nal头匹配)。
    n->forbidden_bit = nalu_hdr->F << 7;                          //内存中的字节序。
    // printf("nal_reference_idc:  %d\n", nalu_hdr->NRI);
    n->nal_reference_idc = nalu_hdr->NRI << 5;
    // printf("nal 负载类型:       %d\n", nalu_hdr->TYPE);
    n->nal_unit_type = nalu_hdr->TYPE;

    //end nal_hdr
    //////////////////////////////////////////////////////////////////////////
    //开始解包
    if (nalu_hdr->TYPE == 0)
    {
        printf("这个包有错误，0无定义\n");
    }
    else if (nalu_hdr->TYPE > 0 && nalu_hdr->TYPE < 24)  //单包
    {
        // printf("当前包为单包\n");
        putc(0x00, poutfile);
        putc(0x00, poutfile);
        putc(0x00, poutfile);
        putc(0x01, poutfile);
        total_bytes += 4;
        memcpy(p->payload, &recvbuf[13], len - 13);
        p->paylen = len - 13;
        fwrite(nalu_hdr, 1, 1, poutfile);
        total_bytes += 1;
        fwrite_number = fwrite(p->payload, 1, p->paylen, poutfile);
        total_bytes = p->paylen;
        // printf("包长度 + nal= %d\n", total_bytes);
    }
    else if (nalu_hdr->TYPE == 24)                    //STAP-A   单一时间的组合包
    {
        // printf("当前包为STAP-A\n");
    }
    else if (nalu_hdr->TYPE == 25)                    //STAP-B   单一时间的组合包
    {
        // printf("当前包为STAP-B\n");
    }
    else if (nalu_hdr->TYPE == 26)                     //MTAP16   多个时间的组合包
    {
        // printf("当前包为MTAP16\n");
    }
    else if (nalu_hdr->TYPE == 27)                    //MTAP24   多个时间的组合包
    {
        // printf("当前包为MTAP24\n");
    }
    else if (nalu_hdr->TYPE == 28)                    //FU-A分片包，解码顺序和传输顺序相同
    {
        if ((fu_ind =(FU_INDICATOR *) malloc(sizeof(FU_INDICATOR))) == NULL)
        {
            // printf("FU_INDICATOR MEMORY ERROR\n");
        }
        if ((fu_hdr = (FU_HEADER  *)malloc(sizeof(FU_HEADER))) == NULL)
        {
            //printf("FU_HEADER MEMORY ERROR\n");
        }

        fu_ind = (FU_INDICATOR*)&recvbuf[12];
        // printf("FU_INDICATOR->F     :%d\n", fu_ind->F);
        n->forbidden_bit = fu_ind->F << 7;
        // printf("FU_INDICATOR->NRI   :%d\n", fu_ind->NRI);
        n->nal_reference_idc = fu_ind->NRI << 5;
        // printf("FU_INDICATOR->TYPE  :%d\n", fu_ind->TYPE);
        n->nal_unit_type = fu_ind->TYPE;

        fu_hdr = (FU_HEADER*)&recvbuf[13];
        // printf("FU_HEADER->S        :%d\n", fu_hdr->S);
        // printf("FU_HEADER->E        :%d\n", fu_hdr->E);
        // printf("FU_HEADER->R        :%d\n", fu_hdr->R);
        // printf("FU_HEADER->TYPE     :%d\n", fu_hdr->TYPE);
        n->nal_unit_type = fu_hdr->TYPE;               //应用的是FU_HEADER的TYPE

        if (rtp_hdr->marker == 1)                      //分片包最后一个包
        {
            // printf("当前包为FU-A分片包最后一个包\n");
            memcpy(p->payload, &recvbuf[14], len - 14);
            p->paylen = len - 14;
            fwrite_number = fwrite(p->payload, 1, p->paylen, poutfile);
            total_bytes = p->paylen;
            // printf("包长度 + FU = %d\n", total_bytes);
        }
        else if (rtp_hdr->marker == 0)                 //分片包 但不是最后一个包
        {
            if (fu_hdr->S == 1)                        //分片的第一个包
            {
                unsigned char F;
                unsigned char NRI;
                unsigned char TYPE;
                unsigned char nh;
                // printf("当前包为FU-A分片包第一个包\n");
                putc(0x00, poutfile);
                putc(0x00, poutfile);
                putc(0x00, poutfile);
                putc(0x01, poutfile);
                total_bytes += 4;

                F = fu_ind->F << 7;
                NRI = fu_ind->NRI << 5;
                TYPE = fu_hdr->TYPE;                                            //应用的是FU_HEADER的TYPE
                //nh = n->forbidden_bit|n->nal_reference_idc|n->nal_unit_type;  //二进制文件也是按 大字节序存储
                nh = F | NRI | TYPE;

                putc(nh, poutfile);

                total_bytes += 1;
                memcpy(p->payload, &recvbuf[14], len - 14);
                p->paylen = len - 14;
                fwrite_number = fwrite(p->payload, 1, p->paylen, poutfile);
                total_bytes = p->paylen;
                // printf("包长度 + FU_First = %d\n", total_bytes);
            }
            else                                      //如果不是第一个包
            {
                // printf("当前包为FU-A分片包\n");
                memcpy(p->payload, &recvbuf[14], len - 14);
                p->paylen = len - 14;
                fwrite_number = fwrite(p->payload, 1, p->paylen, poutfile);
                total_bytes = p->paylen;
                // printf("包长度 + FU = %d\n", total_bytes);
            }
        }
    }
    else if (nalu_hdr->TYPE == 29)                //FU-B分片包，解码顺序和传输顺序相同
    {
        if (rtp_hdr->marker == 1)                  //分片包最后一个包
        {
            // printf("当前包为FU-B分片包最后一个包\n");

        }
        else if (rtp_hdr->marker == 0)             //分片包 但不是最后一个包
        {
            // printf("当前包为FU-B分片包\n");
        }
    }
    else
    {
        // printf("这个包有错误，30-31 没有定义\n");
    }
    total_recved += total_bytes;
    // printf("total_recved = %d\n", total_recved);
    memset(recvbuf, 0, 1500);
    free(p->payload);
    free(p);
    FreeNALU(n);
    //结束解包
    //////////////////////////////////////////////////////////////////////////
    return ;
}

void measure(const PRTPDATA *prtpData) {
    printf("包%d: \n", prtpData->send_Index);
    send_count = (unsigned int)prtpData->send_Index;
    receive_count= receive_count+1;
    double rate = 1 - (double)receive_count / (double)send_count;
    printf("平均丢包率:  %f \n",rate);
    unsigned int receive_timestamp = tv.tv_sec*1000000 + tv.tv_usec - Timestamp_parameter;
    late = late + receive_timestamp - prtpData->send_Time;
    printf("本次测得延迟(ms):  %f \n",(double )(receive_timestamp - prtpData->send_Time)/1000);
    if (temp[0] != 0){
        if(prtpData->send_Index%2 == 0){
            if(prtpData->send_Index - temp[0] == 1) {
                foo(prtpData, receive_timestamp);
            }else{
                temp[0] = 0;
                temp[1] = 0;
                temp[2] = 0;
            }
        }else{
            temp[0] = prtpData->send_Index;
            temp[1] = prtpData->send_Time;
            temp[2] = receive_timestamp;
        }
    } else{
        if(prtpData->send_Index%2 == 1){
            temp[0] = prtpData->send_Index;
            temp[1] = prtpData->send_Time;
            temp[2] = receive_timestamp;
        }else{
            temp[0] = 0;
            temp[1] = 0;
            temp[2] = 0;
        }
    }
    if(lost_mark == 0){
        if(prtpData->send_Index - last_index >=3){
            lost_mark = 1;
            lost_count++;
            lost_long++;
        }
    }else{
        if(prtpData->send_Index - last_index == 1){
            lost_mark = 0;
        } else{
            lost_long++;
        }
    }
    last_index = prtpData->send_Index;
}

void foo(const PRTPDATA *prtpData, unsigned int receive_timestamp) {
//    printf("%d\n",temp[0]);
    int D = abs(abs(receive_timestamp - temp[2]) - abs(prtpData->send_Time - temp[1]));
    jitter = jitter + (D - jitter)/16;
    int delta = receive_timestamp - temp[2] -DELTA;
    if (delta > 0){
        double w = (double)Measure_message_length/1024/1024 / ((double)delta / 1000/1000);
        if( w > B ){
            B = w;
        }
        printf("本次测得带宽(GBps) %f \n", w);
    }

    if(lost_count != 0){
        lost_time = (double)lost_long / lost_count;
    }
    printf("本次测得抖动(ms)%f\n",(double)D/1000);
    printf("平均时延（ms）:%f\n",(double)late/(double)receive_count/1000);
    printf("平均抖动(ms): %f\n",jitter/16000);
    printf("测得最大带宽(GBps): %f\n", B);
    printf("平均连续丢包长度:%f\n", lost_time);

}



//void tempfoo(const PRTPDATA *prtpData, unsigned int receive_timestamp) {
//    if (prtpData->send_Index % 2){
//        receive_timestamp1 = receive_timestamp;
//        send_timestamp1 = prtpData->send_Time;
//        receive_count1 = 1;
//    } else{
//        receive_timestamp2 = receive_timestamp;
//        send_timestamp2 = prtpData->send_Time;
//        receive_count2 = 1;
//    }
//    A:switch (receive_lost_flag) {
//    case 0://XX
//        if(receive_count1 == 1){
//            receive_lost_flag = 1;
//            receive_count1 = 0;
//            receive_count2 = 0;
//        } else if(receive_count1 == 0) {
//            receive_lost_flag = 3;
//            receive_count1 = 0;
//            receive_count2 = 0;
//        }
//        break;
//    case 1: //1X
//        if(receive_count2 == 1){
//            receive_lost_flag = 2;
//            receive_count1 = 0;
//            receive_count2 = 0;
//        } else if(receive_count2 == 0){
//            receive_lost_flag = 5;
//            receive_count1 = 0;
//            receive_count2 = 0;
//        }
//        goto A;
//    case 3://0X
//        if(receive_count2 == 1){
//            receive_lost_flag = 4;
//            receive_count1 = 0;
//            receive_count2 = 0;
//        } else if(receive_count2 == 0){
//            receive_lost_flag = 6;
//            receive_count1 = 0;
//            receive_count2 = 0;
//        }
//        goto A;
//    case 2://11
//        receive_lost_flag = 0;
//        lose_flag = 0;
//        D = receive_timestamp2 - receive_timestamp1 + send_timestamp2 - send_timestamp1;
//        jitter = jitter + (D - jitter)/16;
//        delta = receive_timestamp2 - receive_timestamp1 - DELTA;
//        w = Measure_message_length / delta;
//        printf("本次测得带宽为(GBps) %f \n", w);
//        if(w > bandwidth){
//            bandwidth = w;
//        }
//        break;
//    case 4://01
//    case 5://10
//        receive_lost_flag = 0;
//        //printf("lose one of package!\n");
//        if(lose_flag != 0){
//            lose_flag = 1;
//            total_lost_time++;
//        }
//        break;
//    case 6://00
//        receive_lost_flag = 0;
//        lose_flag = 1;
//        total_lost_count ++;
//        break;
//    default:
//        printf("case error!\n");
//        exit(1);
//}
//    printf("测得最大带宽为(GBps): %f\n", bandwidth);
//    printf("平均抖动为(ms): %f\n", jitter/1600);
//    if(total_lost_count){
//        lost_time = total_lost_time / total_lost_count;
//    }
//    printf("平均连续丢包长度为:%f\n", lost_time);
//}


int main(int argc, char* argv[])
{

    char recvbuf[MAXDATASIZE];  //加上头最大传输数据 1500
    SOCKET socket1;
    struct sockaddr_in client;//分配一个地址结构体
    socklen_t len_client = sizeof(client);
    socklen_t	receive_bytes = 0;

    OpenBitstreamFile(outputfilename);

    //////////////////////////////////////////////////////////////////////////
    //socket 操作
    //socket1 = socket(AF_INET/*Trtp_header_t/IP协议只能是这种协议*/, SOCK_DGRAM/*UDP协议的是流式*/, 0/*自动选择协议*/);
    socket1 = socket(AF_INET, SOCK_DGRAM, 0);

    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr(DEST_IP);
    client.sin_port = htons(DEST_PORT);

    int opt = 1;
    setsockopt(socket1, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));


    if (bind(socket1, (struct sockaddr*)&client, sizeof(client)) == -1)
    {
        printf("Bind to local machine error.\n");

        return getchar();
    }
    else
    {
        printf("Bind to local machine.\n");
    }


    while ((receive_bytes = recvfrom(socket1, recvbuf, MAXDATASIZE, 0, (struct sockaddr *)&client, &len_client)) > 0)
    {
        poutfile = fopen(outputfilename, "ab+");
        rtp_unpackage(recvbuf, receive_bytes);
        fclose(poutfile);
    }
    return getchar();
}
