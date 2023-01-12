#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

#include "xrt/xrt_kernel.h"
#include "xrt/xrt_uuid.h"
#include "experimental/xrt_queue.h"

#include "utils_XAcc_jpeg.hpp"


#include "dev_jpeg_decoder.hpp"



void print_help(void);
void read_file(const char *file_name, int file_size, char *read_buffer);
void rebuild_infos(xf::codec::img_info& img_info,
                   xf::codec::cmp_info cmp_info[MAX_NUM_COLOR],
                   xf::codec::bas_info& bas_info,
                   int& rtn,
                   int& rtn2,
                   uint32_t infos[1024]);
void rebuild_image( xf::codec::bas_info* bas_info,
                     uint8_t* yuv_mcu_pointer,
                     std::string savefile_name);
void rebuild_image_improved_bgrxbgrx(xf::codec::bas_info* bas_info,uint8_t* bgrx_pointer,std::string savefile_name);
void rebuild_image_improved_bNgNrN(xf::codec::bas_info* bas_info,uint8_t* bgrx_pointer,std::string savefile_name);
void write_bmp(uint8_t *rgb_array, std::string file_name, int image_width, int image_height);




int main(int argc, char * const *argv)
{
    const char *optstring = "i:x:h";
    int file_size;      // input JPEG file size (bytes)
    int opt;
    int rv;
    std::string file_name("lantern.jpg"),xclbin_file("./mult_jpeg_decoder_only_100Mhz");

    while ((opt = getopt(argc, argv, optstring)) != -1) {
        if ((opt == 'i') && optarg) {
            file_name = std::string(optarg);
        }
        else if ((opt == 'x') && optarg) {
           xclbin_file = std::string(optarg);
        }
        else if ((opt == 'h') ) {
            print_help();
            exit(0);
        }
    }

    struct stat statbuff;
    if (stat(file_name.c_str(), &statbuff)) {
        std::cerr << "Cannot open file " << file_name << std::endl;
        return EXIT_FAILURE;
    }
    file_size = statbuff.st_size;
    std::cout << "Input JPEG file size = " << file_size << std::endl;

    if (stat(xclbin_file.c_str(), &statbuff)) {
        std::cerr << "Cannot open file " << xclbin_file << std::endl;
        return EXIT_FAILURE;
    }

    rv = jpeg_decoders_init(0,xclbin_file);
    if(rv!=0){
        std::cerr<<"jpeg_decoders_init failed."<<std::endl;
        exit(1);
    }


    jpeg_struct_forhw jpeg_test_cases[TEST_PIC_NUM];   //测试TEST_PIC_NUM张 ，3+2
    for (size_t i = 0; i < TEST_PIC_NUM; i++)
    {
#if DEBUG_RWBUFF == 1
        jpeg_test_cases[i].jpeg_data  = new uint8_t [file_size];
        jpeg_test_cases[i].file_size = file_size;
        jpeg_test_cases[i].yuv_data   = new uint8_t [MAX_YUV_BUFF_SIZE];
        jpeg_test_cases[i].infos_data = new uint8_t [MAX_INFO_SIZE];
#else
        jpeg_test_cases[i].jpeg_data = gb_jpeg_buffer[i%JPEG_DECODER_INSTANCES].map<uint8_t*>();
        jpeg_test_cases[i].file_size = file_size;
        jpeg_test_cases[i].yuv_data   = gb_yuv_buffer[i%JPEG_DECODER_INSTANCES].map<uint8_t*>();
        jpeg_test_cases[i].infos_data = gb_infos_buffer[i%JPEG_DECODER_INSTANCES].map<uint8_t*>(); 
#endif
        read_file(file_name.c_str(), file_size, (char*)jpeg_test_cases[i].jpeg_data);
    }


    struct timespec tpstart;
    struct timespec tpend;
    long timedif;
    std::cout << "test--Run krnl_jpeg 100 pics ... " ;

    clock_gettime(CLOCK_MONOTONIC, &tpstart);
#if EXEC_MODEL==1
    rv = jpeg_decoders_run_async(jpeg_test_cases,TEST_PIC_NUM);
#else
    rv = jpeg_decoders_run_batch(jpeg_test_cases,TEST_PIC_NUM);
#endif
    if(rv != 0){
        std::cerr<<"jpeg_decoders_run_batch failed"<<std::endl;
        exit(1);
    }
    clock_gettime(CLOCK_MONOTONIC, &tpend);
    timedif = 1000000 * (tpend.tv_sec-tpstart.tv_sec) + (tpend.tv_nsec-tpstart.tv_nsec)/1000;
    std::cout << "it took "<< timedif << " us."<<std::endl;
    std::cout<<"all jpeg pics decoder done.host combin..."<<std::endl;

   // extract JPEG decoder return information
    xf::codec::cmp_info cmp_info[MAX_NUM_COLOR];
    xf::codec::img_info img_info;
    xf::codec::bas_info bas_info;

    // 0: decode jfif successful
    // 1: marker in jfif is not in expectation
    int rtn = 0;

    // 0: decode huffman successful
    // 1: huffman data is not in expectation
    int rtn2 = false;

    for (size_t i = 0; i < TEST_PIC_NUM; i++)
    {
        rebuild_infos(img_info, cmp_info, bas_info, rtn, rtn2, (uint32_t*)jpeg_test_cases[i].infos_data);
        #if DEBUG_SIMPLIFIED_INFO==0
        if (rtn || rtn2) {
            printf("[ERROR]: Decoding the bad case input file!\n");
        if (rtn == 1) {
                printf("[code 1] marker in jfif is not in expectation!\n");
            } else if (rtn == 2) {
                printf("[code 2] huffman table is not in expectation!\n");
            } else {
                if (rtn2) {
                    printf("[code 3] huffman data is not in expectation!\n");
                }
            }
            return 1;
        } 
        #endif

#if DEBUG_BUILD_BMP==1   //构建bmp图片
        // std::cout << "Successfully decode the JPEG file," << i << std::endl;
#if DEBUG_INPROVED_KERNEL==1
        rebuild_image_improved_bgrxbgrx(&bas_info, jpeg_test_cases[i].yuv_data,std::string("outpic")+std::to_string(i)+std::string(".bmp"));
#else
        rebuild_image(&bas_info, jpeg_test_cases[i].yuv_data,std::string("outpic")+std::to_string(i)+std::string(".bmp"));
#endif

#endif
    }
#if DEBUG_BUILD_BMP==0   //构建bmp图片
    std::cout<<"pass it."<<std::endl;
#endif
    return 0;
}


void print_help()
{
    std::cout<<"arguments:"<<std::endl;
    std::cout<<"-x [input xclbin file]"<<std::endl;
    std::cout<<"-i [input jpeg file]"<<std::endl;
}

// function to read JPEG image as binary file
void read_file(const char *file_name, int file_size, char *read_buffer)
{
    // read jpeg image as binary file to host memory
    std::ifstream input_file(file_name, std::ios::in | std::ios::binary);
    input_file.read(read_buffer, file_size);
    input_file.close();
}


// extract image information from output of JPEG decoder kernel
void rebuild_infos(xf::codec::img_info& img_info,
                   xf::codec::cmp_info cmp_info[MAX_NUM_COLOR],
                   xf::codec::bas_info& bas_info,
                   int& rtn,
                   int& rtn2,
                   uint32_t infos[1024]) {

    img_info.hls_cs_cmpc = *(infos + 0);
    img_info.hls_mcuc = *(infos + 1);
    img_info.hls_mcuh = *(infos + 2);
    img_info.hls_mcuv = *(infos + 3);

    rtn = *(infos + 4);
    rtn2 = *(infos + 5);


    bas_info.all_blocks = *(infos + 10);

    for (int i = 0; i < MAX_NUM_COLOR; i++) {
        bas_info.axi_height[i] = *(infos + 11 + i);
    }

    for (int i = 0; i < 4; i++) {
        bas_info.axi_map_row2cmp[i] = *(infos + 14 + i);
    }

    bas_info.axi_mcuv = *(infos + 18);
    bas_info.axi_num_cmp = *(infos + 19);
    bas_info.axi_num_cmp_mcu = *(infos + 20);

    for (int i = 0; i < MAX_NUM_COLOR; i++) {
        bas_info.axi_width[i] = *(infos + 21 + i);
    }

    int format = *(infos + 24);
    bas_info.format = (xf::codec::COLOR_FORMAT)format;

    for (int i = 0; i < MAX_NUM_COLOR; i++) {
        bas_info.hls_mbs[i] = *(infos + 25 + i);
    }

    bas_info.hls_mcuc = *(infos + 28);

    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                bas_info.idct_q_table_x[c][i][j] = *(infos + 29 + c * 64 + i * 8 + j);
            }
        }
    }
    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                bas_info.idct_q_table_y[c][i][j] = *(infos + 221 + c * 64 + i * 8 + j);
            }
        }
    }
    
    bas_info.mcu_cmp = *(infos + 413);

    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 64; i++) {
            bas_info.min_nois_thld_x[c][i] = *(infos + 414 + c * 64 + i);
        }
    }
    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 64; i++) {
            bas_info.min_nois_thld_y[c][i] = *(infos + 606 + c * 64 + i);
        }
    }
    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                bas_info.q_tables[c][i][j] = *(infos + 798 + c * 64 + i * 8 + j);
            }
        }
    }

    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        cmp_info[c].bc = *(infos + 990 + c * 6);
        cmp_info[c].bch = *(infos + 991 + c * 6);
        cmp_info[c].bcv = *(infos + 992 + c * 6);
        cmp_info[c].mbs = *(infos + 993 + c * 6);
        cmp_info[c].sfh = *(infos + 994 + c * 6);
        cmp_info[c].sfv = *(infos + 995 + c * 6);
    }

}


// pixel value saturation, int32 -> uint8
static uint8_t pixel_sat_32_8(int32_t in)
{
	uint8_t result;
	if (in < 0) {
		result = 0;
	} else if (in > 255) {
		result = 255;
	} else {
		result = in;
	}
	return result;
}

// BMP file head structure
static uint8_t BmpFileHead[54] = {
    0x42, 0x4d,             // bfType:              byte  0 -  1
    0x00, 0x00, 0x00, 0x00, // bfSize:              byte  2 -  5
    0x00, 0x00,             // bfReserved1:         byte  6 -  7
    0x00, 0x00,             // bfReserved2:         byte  8 -  9
    0x36, 0x00, 0x00, 0x00, // bfOffBits:           byte 10 - 13
    0x28, 0x00, 0x00, 0x00, // biSize:              byte 14 - 17
    0x00, 0x00, 0x00, 0x00, // biWidth:             byte 18 - 21
    0x00, 0x00, 0x00, 0x00, // biHeight:            byte 22 - 25
    0x01, 0x00,             // biPlanes:            byte 26 - 27
    0x20, 0x00,             // biBitCount:          byte 28 - 29
    0x00, 0x00, 0x00, 0x00, // biCompression:       byte 30 - 33
    0x00, 0x00, 0x00, 0x00, // biSizeImages:        byte 34 - 37
    0x00, 0x00, 0x00, 0x00, // biXPelsPerMeter:     byte 38 - 41
    0x00, 0x00, 0x00, 0x00, // biYPelsPerMeter:     byte 42 - 46
    0x00, 0x00, 0x00, 0x00, // biClrUsed:           byte 47 - 50
    0x00, 0x00, 0x00, 0x00  // biClrImportant:      byte 51 - 54
};

// write RGB444 data to BMP file
void write_bmp(uint8_t *rgb_array, std::string file_name, int image_width, int image_height)
{
    int file_size = image_height * image_width * 4 + 54; 

    FILE *fp;

    uint32_t temp;

    // complement BMP file head array
    temp = file_size;
    for (int i = 2; i < 6; i++)
    {
        BmpFileHead[i] = temp & 0xff;
        temp = temp >> 8;
    }

    temp = image_width;
    for (int i = 18; i < 22; i++)
    {
        BmpFileHead[i] = temp & 0xff;
        temp = temp >> 8;
    }    

    temp = image_height;
    for (int i = 22; i < 26; i++)
    {
        BmpFileHead[i] = temp & 0xff;
        temp = temp >> 8;
    }    
    
    fp = fopen(file_name.c_str(), "wb");

    // write file header
    fwrite(BmpFileHead, 1, 54, fp);
    
    // write pixel date
    for (int i = (image_height - 1); i >= 0; i--)
    {
        fwrite((rgb_array + 4 * image_width * i), 1, (image_width * 4), fp);
    }
    
    fclose(fp);

}

// reorder JPEG decoded YUV pixel, finish color space conversion and write out reference BMP file
void rebuild_image( xf::codec::bas_info* bas_info,
                     uint8_t* yuv_mcu_pointer,
                     std::string savefile_name) 
{   

    int image_width = bas_info->axi_width[0] * 8;
    int image_height = bas_info->axi_height[0] * 8;  

    // reorg yuv data to three planes 
   uint8_t* yuv_plane[3];
   for (int i = 0; i < 3; i++) {
       yuv_plane[i] = (uint8_t*)malloc(image_width * image_height);
   }

if (bas_info->format == 3){ //YUV444
    //一维 变 三维 Y,U,V
   for (int mcuv_cnt = 0; mcuv_cnt < bas_info->axi_height[0]; mcuv_cnt++)       //高的块数（行） 【MCU blocks rows】【8X8】
   {
       for (int mcuh_cnt = 0; mcuh_cnt < bas_info->axi_width[0]; mcuh_cnt++)    //宽的块数，非像素，（列） 【MCU blocks cols】【8X8】
       {
           for (int i = 0; i < 3; i++) // three components (Y,U,V)     //这里是单独处理三次，即plannar模式
           { 
               for (int col = 0; col < 8; col++)
               {
                   for (int row = 0; row < 8; row++) {
                    //将3通道的yuv444,混合数据放到独立的3个通道，每个通道各自是Y,U,V元素值。image从左上开始，(左向右)加(上向下)方向
                    // 由块变 行列像素     [每个MCU中的像素所在行]   * [ 行像素 （一行的块数乘以一块的宽像素个数]   +   块所在列 乘以一块列的像素个数8   + 块内列偏移  
                       uint64_t offset = (8 * mcuv_cnt + row)   *   (bas_info->axi_width[0] * 8)        +   (   mcuh_cnt *  8             + col);
                       yuv_plane[i][offset] = *yuv_mcu_pointer;
                       yuv_mcu_pointer++;
                   }
               }
           }
       }
   }    
   
}else if (bas_info->format == 2){   //YUV422
    std::cout << "not support YUV422 format now." << std::endl;
}
else if (bas_info->format == 1){    //YUV420
    // std::cout << "test debug YUV420 format now." << std::endl;
    int mcuv_cnt = 0;
    int mcuh_cnt = 0;
    /*
    4Y[8x8] share U1,V1[8x8]
    Y1     Y2
      U1 V1 
    Y3     Y4
    */
    uint8_t *YBlock = yuv_mcu_pointer,
            *UBlock = yuv_mcu_pointer + 4 * 8 * 8, 
            *VBlock = yuv_mcu_pointer + 5 * 8 * 8;
    for (int o_mcuv_cnt = 0; o_mcuv_cnt < bas_info->axi_height[0]>>1; o_mcuv_cnt++)       //高的块数（行） 【MCU blocks rows】【8X8】
    {
        for (int o_mcuh_cnt = 0; o_mcuh_cnt < bas_info->axi_width[0]>>1; o_mcuh_cnt++)    //宽的块数，非像素，（列） 【MCU blocks cols】【8X8】
        {
            for(int m=0;m<4;m++){
                mcuv_cnt = o_mcuv_cnt*2 + m/2;
                mcuh_cnt = o_mcuh_cnt*2 + m%2;

                for (int col = 0; col < 8; col++) {
                    for (int row = 0; row < 8; row++) {
                        uint64_t offset = (8 * mcuv_cnt + row) * (bas_info->axi_width[0] * 8)  +  (mcuh_cnt * 8  + col);
                        yuv_plane[0][offset] =  YBlock[m*8*8 + (col)  *8 + (row)];
                    }
                }
            }
            YBlock+=6*8*8;

            //once
            {
                for (int col = 0; col < 16; col++){
                    for (int row = 0; row < 16; row++) {
                        uint64_t offset = (16 * o_mcuv_cnt + row) * (bas_info->axi_width[0] * 8)  +  (o_mcuh_cnt * 16  + col);
                        yuv_plane[1][offset] =  UBlock[ (col/2) * 8 + (row/2)];
                        yuv_plane[2][offset] =  VBlock[ (col/2) * 8 + (row/2)];
                    }
                }
            }
            UBlock+=6*8*8;
            VBlock+=6*8*8;
        }
    } 
}
else if (bas_info->format == 0){    //YUV400
    std::cout << "not support YUV400 format now." << std::endl;
}

   // color space convertion
   uint8_t*  rgb_plane;
   rgb_plane = (uint8_t*)malloc(image_width * image_height * 4);   // RGBA format
   
   //for yuv444 only
   for (int row = 0; row < image_height; row++)
   {
       for (int col = 0; col < image_width; col++)
       {
            int16_t y = (uint16_t)yuv_plane[0][image_width * row + col] - 16;
            int16_t u = (uint16_t)yuv_plane[1][image_width * row + col] - 128;
            int16_t v = (uint16_t)yuv_plane[2][image_width * row + col] - 128;

            // u=v=0;      //for current debug only  

            int32_t r = (76284 * y + 104595 * v) >> 16;
            int32_t g = (76284 * y -  53281 * v - 25625 * u) >> 16;
            int32_t b = (76284 * y + 132252 * u) >> 16;

            rgb_plane[(image_width * row + col) * 4 + 0] = pixel_sat_32_8(b);
            rgb_plane[(image_width * row + col) * 4 + 1] = pixel_sat_32_8(g);
            rgb_plane[(image_width * row + col) * 4 + 2] = pixel_sat_32_8(r);
            rgb_plane[(image_width * row + col) * 4 + 3] = 0;
       }
   }
    
    // write pixel data
    write_bmp(rgb_plane, savefile_name, image_width, image_height);

   std::cout << "The JPEG file is decoded to image " << savefile_name <<" of " << image_width << " by " << image_height << std::endl;

}

void rebuild_image_improved_bgrxbgrx(xf::codec::bas_info* bas_info,uint8_t* bgrx_pointer,std::string savefile_name){
    int image_width = bas_info->axi_width[0] * 8;
    int image_height = bas_info->axi_height[0] * 8;  

    if (bas_info->format != 1){ //YUV420
        std::cout << "only support YUV420 format now." << std::endl;
        return;
    }

    write_bmp(bgrx_pointer, savefile_name, image_width, image_height);
    std::cout << "The JPEG file is decoded to image " << savefile_name <<" of " << image_width << " by " << image_height << std::endl;
}


void rebuild_image_improved_bNgNrN(xf::codec::bas_info* bas_info,uint8_t* bgrx_pointer,std::string savefile_name){
    int image_width = bas_info->axi_width[0] * 8;
    int image_height = bas_info->axi_height[0] * 8;  

    if (bas_info->format != 1){ //YUV420
        std::cout << "only support YUV420 format now." << std::endl;
        return;
    }

   uint8_t*  rgb_plane;
   rgb_plane = (uint8_t*)malloc(image_width * image_height * 4);   // RGBA format
   uint8_t*  p_b = bgrx_pointer + (image_width * image_height) * 0;
   uint8_t*  p_g = bgrx_pointer + (image_width * image_height) * 1;
   uint8_t*  p_r = bgrx_pointer + (image_width * image_height) * 2;

   for (int row = 0; row < image_height; row++)
   {
       for (int col = 0; col < image_width; col++)
       {
            rgb_plane[(image_width * row + col) * 4 + 0] = p_b[(image_width * row + col)];
            rgb_plane[(image_width * row + col) * 4 + 1] = p_g[(image_width * row + col)];
            rgb_plane[(image_width * row + col) * 4 + 2] = p_r[(image_width * row + col)];
            rgb_plane[(image_width * row + col) * 4 + 3] = 0;
       }
   }
    
    write_bmp(rgb_plane, savefile_name, image_width, image_height);
    std::cout << "The JPEG file is decoded to image " << savefile_name <<" of " << image_width << " by " << image_height << std::endl;
}