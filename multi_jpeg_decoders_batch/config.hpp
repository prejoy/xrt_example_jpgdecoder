#pragma once

#define KRNL_BASENAME               "krnl_jpeg" //names are krnl_jpeg_0,krnl_jpeg_1,....
#define JPEG_DECODER_INSTANCES      6u

#define MAX_JPEG_FILE_SIZE         1000000U        //Bytes ,not modify

#define MAX_YUV_BUFF_SIZE          MAXCMP_BC*64   //Bytes ,not modify
#define MAX_INFO_SIZE               4096                                    

#define TEST_PIC_NUM                100

#define EXEC_MODEL                  0   //0=batch，use 0 now

#define DEBUG_RUNKERENL             1   //是否运行kernel
#define DEBUG_RWBUFF                0   //是否读写buff，不读写buff对应使用map方式
#define DEBUG_BUILD_BMP             1   //主机后续构建bmp图片
#define DEBUG_SIMPLIFIED_INFO       0   //硬件简化后的 jpeg decoder info 信息 ， 0=original  1=simplied

#define DEBUG_INPROVED_KERNEL       0   //jpgdecoder + hw yuv->bgrx , 必须使用对应的xclbin