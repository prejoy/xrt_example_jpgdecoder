#pragma once

#include <iostream>
#include <string>
#include <queue>

#include "xrt.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_uuid.h"
#include "experimental/xrt_queue.h"

#include "utils_XAcc_jpeg.hpp"

#include "config.hpp"

struct jpeg_struct_forhw
{
    uint8_t *jpeg_data;    // host buffer for input JPEG file
    int     file_size;
    uint8_t *yuv_data;     // host buffer for decoded YUV planner image data
    uint8_t *infos_data;   // host buffer for JPEG file information packet
};


extern xrt::bo gb_jpeg_buffer[JPEG_DECODER_INSTANCES];
extern xrt::bo gb_yuv_buffer[JPEG_DECODER_INSTANCES];
extern xrt::bo gb_infos_buffer[JPEG_DECODER_INSTANCES];

int jpeg_decoders_init(int devID,std::string &xclbinfile);
int jpeg_decoders_run_batch(jpeg_struct_forhw jpegs[],size_t num);
int jpeg_decoders_run_async(jpeg_struct_forhw jpegs[],size_t num);


