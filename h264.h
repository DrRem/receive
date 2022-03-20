//
// Created by Administrator on 2022/3/7.
//

#ifndef RECEIVE_H264_H
#define RECEIVE_H264_H

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <netdb.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PACKET_BUFFER_END            (unsigned int)0x00000000

#define MAX_RTP_PKT_LENGTH     1400

#define DEST_IP                "192.168.240.129"
#define DEST_PORT            12348

#define H264                    96
#define PRTP					101
#define  MAXDATASIZE 1500

#define DELTA                  10000          //us
#define Measure_message_length 600         //bytes
#define Timestamp_parameter    1640966400000000
typedef int SOCKET;


typedef struct
{
    /**//* byte 0 */
    unsigned char csrc_len : 4;        /**//* expect 0 */
    unsigned char extension : 1;        /**//* expect 1, see RTP_OP below */
    unsigned char padding : 1;        /**//* expect 0 */
    unsigned char version : 2;        /**//* expect 2 */
    /**//* byte 1 */
    unsigned char payload : 7;        /**//* RTP_PAYLOAD_RTSP */
    unsigned char marker : 1;        /**//* expect 1 */
    /**//* bytes 2, 3 */
    unsigned short seq_no;
    /**//* bytes 4-7 */
    unsigned  int timestamp;
    /**//* bytes 8-11 */
    unsigned int ssrc;            /**//* stream number is used here. */
} RTP_FIXED_HEADER;


typedef struct
{
    unsigned char v;               //!< Version, 2 bits, MUST be 0x2
    unsigned char p;			   //!< Padding bit, Padding MUST NOT be used
    unsigned char x;			   //!< Extension, MUST be zero
    unsigned char cc;       	   //!< CSRC count, normally 0 in the absence of RTP mixers
    unsigned char m;			   //!< Marker bit
    unsigned char pt;			   //!< 7 bits, Payload Type, dynamically established
    unsigned short seq;			   //!< RTP sequence number, incremented by one for each sent packet
    unsigned int timestamp;	       //!< timestamp, 27 MHz for H.264
    unsigned int ssrc;			   //!< Synchronization Source, chosen randomly
    unsigned char * payload;      //!< the payload including payload headers
    unsigned int paylen;		   //!< length of payload in bytes
} RTPpacket_t;



typedef struct {
    //byte 0
    unsigned char TYPE : 5;
    unsigned char NRI : 2;
    unsigned char F : 1;

} NALU_HEADER; /**//* 1 BYTES */

typedef struct {
    //byte 0
    unsigned char TYPE : 5;
    unsigned char NRI : 2;
    unsigned char F : 1;


} FU_INDICATOR; /**//* 1 BYTES */

typedef struct {
    //byte 0
    unsigned char TYPE : 5;
    unsigned char R : 1;
    unsigned char E : 1;
    unsigned char S : 1;
} FU_HEADER; /**//* 1 BYTES */


typedef struct
{
    /**//* byte 0-3 */
    unsigned int send_Index;
    /**//* bytes 4-7 */
    unsigned int send_Count;
    /**//* bytes 8-11 */
    unsigned int send_Time;   /* stream number is used here. */
    /**//* bytes 12-15 */
} PRTPDATA;

#endif //RECEIVE_H264_H
