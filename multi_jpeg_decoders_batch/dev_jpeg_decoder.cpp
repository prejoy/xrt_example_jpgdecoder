
#include <unistd.h>
#include <sys/select.h>


#include "dev_jpeg_decoder.hpp"



enum argno_of_jpeg_decoder{
    karg_jpeg_ptr=0,
    karg_jpeg_size,
    karg_jpeg_yuv_ptr,
    karg_jpeg_infos_ptr,
};


int gb_deviceID;
std::string gb_xclbin_file;

xrt::device gb_device;
xrt::uuid gb_xclbin_uuid;

xrt::kernel gb_krnl_jpeg[JPEG_DECODER_INSTANCES];
xrt::run gb_run_jpeg[JPEG_DECODER_INSTANCES];
int gb_krnl_num_jpeg;

xrt::bo gb_jpeg_buffer[JPEG_DECODER_INSTANCES];
xrt::bo gb_yuv_buffer[JPEG_DECODER_INSTANCES];
xrt::bo gb_infos_buffer[JPEG_DECODER_INSTANCES];

xrt::queue main_queue;
xrt::queue task_queue[JPEG_DECODER_INSTANCES];

//=====
int gb_dev_state;
int gb_krnl_state[JPEG_DECODER_INSTANCES];


void msleep(unsigned int n_ms){
    struct timeval tval;
    tval.tv_sec = n_ms/1000;
    tval.tv_usec = (n_ms*1000)%1000000;
    select(0,NULL,NULL,NULL,&tval);
}


int write_kernel(int kid,jpeg_struct_forhw &jpeg){
    gb_run_jpeg[kid].set_arg(karg_jpeg_ptr         ,gb_jpeg_buffer[kid]);
    gb_run_jpeg[kid].set_arg(karg_jpeg_size        ,jpeg.file_size);
    gb_run_jpeg[kid].set_arg(karg_jpeg_yuv_ptr     ,gb_yuv_buffer[kid]);
    gb_run_jpeg[kid].set_arg(karg_jpeg_infos_ptr   ,gb_infos_buffer[kid]);

#if DEBUG_RWBUFF==1
    gb_jpeg_buffer[kid].write(jpeg.jpeg_data,jpeg.file_size,0);
#endif
    gb_jpeg_buffer[kid].sync(XCL_BO_SYNC_BO_TO_DEVICE,jpeg.file_size,0);

    return 0;
}

int run_kernel(int kid){
#if DEBUG_RUNKERENL==1
    gb_run_jpeg[kid].start();
#endif
    return 0;
}

int read_kernel(int kid,jpeg_struct_forhw &jpeg){
    ert_cmd_state rv;
    int to_s=5;
#if DEBUG_RUNKERENL==1
    rv = gb_run_jpeg[kid].wait(1000);
    // do{
    //     sleep(1);
    //     rv = gb_run_jpeg[kid].state();
    //     std::cout<<"gb_run_jpeg[kid].state: "<< rv << std::endl;
    //     if(rv == ERT_CMD_STATE_COMPLETED){
    //         gb_run_jpeg[kid].wait();
    //         break;
    //     }
    // }while(--to_s > 0);

    if(rv != ERT_CMD_STATE_COMPLETED){
        std::cerr<<"gb_run_jpeg[kid].wait(1000) return "<< rv << std::endl;
        gb_run_jpeg[kid].abort();
        return -1;
    }
#endif

    gb_infos_buffer[kid].sync(XCL_BO_SYNC_BO_FROM_DEVICE,MAX_INFO_SIZE,0);
#if DEBUG_RWBUFF==1
    gb_infos_buffer[kid].read(jpeg.infos_data,MAX_INFO_SIZE,0);
#endif

    uint32_t *infodata;
#if DEBUG_RWBUFF==1
    infodata = (uint32_t *)jpeg.infos_data;
#else
    infodata = gb_infos_buffer[kid].map<uint32_t*>();
#endif

    int format = *(infodata+24);
    unsigned int block_height = *(infodata+11);
    unsigned int block_width = *(infodata+21);
    int total_yuv_size=0;

#if DEBUG_INPROVED_KERNEL==1
    total_yuv_size = (block_height*block_width*8*8)*4;
#else
    if (format == 3){   //yuv444
        total_yuv_size = (block_height*block_width*8*8)*3;
    }else if (format == 1){   //yuv420
        total_yuv_size = (block_height*block_width*8*8)*3/2;
    }else{
        total_yuv_size = (block_height*block_width*8*8)*3;
        std::cerr<<"not support other yuv format now!!"<<std::endl;
    }
#endif
    gb_yuv_buffer[kid].sync(XCL_BO_SYNC_BO_FROM_DEVICE,total_yuv_size,0);
#if DEBUG_RWBUFF==1
    gb_yuv_buffer[kid].read(jpeg.yuv_data,total_yuv_size,0);
#endif

    return 0;
}


int jpeg_decoders_init(int devID,std::string &xclbinfile){
    gb_dev_state = -1;
    try{
        std::cout<<"deviceID: " << gb_deviceID << std::endl;
        gb_device = xrt::device(gb_deviceID);

        gb_xclbin_file = xclbinfile;
        std::cout<<"loading xclbin :"<<gb_xclbin_file<<std::endl;
        gb_xclbin_uuid = gb_device.load_xclbin(gb_xclbin_file);

        std::string kernel_name;
        std::cout<<"create kernel : "<< KRNL_BASENAME << std::endl;
        for (int i = 0; i < JPEG_DECODER_INSTANCES; i++)
        {
            //kernel 名称和硬件对应，这里的序号是硬件修改过的。
            kernel_name = std::string(KRNL_BASENAME) + std::string(":{") + std::string(KRNL_BASENAME) + "_" + std::to_string(i+1) + std::string("}");
            std::cout<<"config instance:"<<kernel_name<<std::endl;

            gb_krnl_jpeg[i]    = xrt::kernel(gb_device,gb_xclbin_uuid,kernel_name);

            gb_jpeg_buffer[i]  = xrt::bo (gb_device, MAX_JPEG_FILE_SIZE, xrt::bo::flags::normal, (gb_krnl_jpeg[i]).group_id (karg_jpeg_ptr));
            gb_yuv_buffer[i]   = xrt::bo (gb_device, MAX_YUV_BUFF_SIZE,  xrt::bo::flags::normal, (gb_krnl_jpeg[i]).group_id (karg_jpeg_yuv_ptr));
            gb_infos_buffer[i] = xrt::bo (gb_device, MAX_INFO_SIZE,      xrt::bo::flags::normal, (gb_krnl_jpeg[i]).group_id (karg_jpeg_infos_ptr));

            gb_run_jpeg[i] = xrt::run (gb_krnl_jpeg[i]);
            gb_krnl_state[i] = 0;
        }
    }
    catch(const std::exception& e){
        std::cerr << e.what() << '\n';
        gb_dev_state = -1;
        return -1;
    }

    gb_dev_state = 0;
    return 0;

}


int jpeg_decoders_run_batch(jpeg_struct_forhw jpegs[],size_t num){
    int offset=0;
    int rv;

    while(num){
        for (size_t i = 0; i < (JPEG_DECODER_INSTANCES > num ?num : JPEG_DECODER_INSTANCES); i++)
        {
            write_kernel(i,jpegs[offset+i]);
            run_kernel(i);
        }

        for (size_t i = 0; i < (JPEG_DECODER_INSTANCES > num ?num : JPEG_DECODER_INSTANCES) ; i++)
        {
            rv = read_kernel(i,jpegs[offset+i]);
            if(rv !=0){
                return -1;
            }
        }

        offset+=(JPEG_DECODER_INSTANCES > num ?num : JPEG_DECODER_INSTANCES);
        num-=(JPEG_DECODER_INSTANCES > num ?num : JPEG_DECODER_INSTANCES);
    }
  
    return 0;
}


int jpeg_decoders_run_async(jpeg_struct_forhw jpegs[],size_t num){
    int offset=0;
    int rv;
    size_t i;
    int all_num=num;
    xrt::queue::event sync_event[JPEG_DECODER_INSTANCES];
    int inst=0;

    //预分配方式或动态方式，预分配方案按动态最优，动态可能需要配合callback，较难实现。
    // sync的size 还没有调整，需要改动！！！
    for ( i = 0; i < num; i++)
    {
        inst = i%JPEG_DECODER_INSTANCES;
        task_queue[inst].enqueue([inst]  {gb_run_jpeg[inst].set_arg(karg_jpeg_ptr         ,gb_jpeg_buffer[inst]); });
        task_queue[inst].enqueue([inst,i,&jpegs]  {gb_run_jpeg[inst].set_arg(karg_jpeg_size        ,jpegs[i].file_size); });
        task_queue[inst].enqueue([inst]  {gb_run_jpeg[inst].set_arg(karg_jpeg_yuv_ptr     ,gb_yuv_buffer[inst]); });
        task_queue[inst].enqueue([inst]  {gb_run_jpeg[inst].set_arg(karg_jpeg_infos_ptr   ,gb_infos_buffer[inst]); });
    #if DEBUG_RWBUFF==1
        task_queue[inst].enqueue([inst,i,&jpegs]  {gb_jpeg_buffer[inst].write(jpegs[i].jpeg_data); });
    #endif
        sync_event[inst] = task_queue[inst].enqueue([inst]  {gb_jpeg_buffer[inst].sync(XCL_BO_SYNC_BO_TO_DEVICE); });
    #if DEBUG_RUNKERENL==1
        sync_event[inst] = task_queue[inst].enqueue([inst,i]  {gb_run_jpeg[inst].start(); gb_run_jpeg[inst].wait(1000); });
        // sync_event[inst] = task_queue[inst].enqueue([inst,i]  {std::cout<<"i and inst is "<<i<<","<<inst<<std::endl ; gb_run_jpeg[inst].start(); gb_run_jpeg[inst].wait(1000); });
    #endif
        task_queue[inst].enqueue([inst]  {gb_yuv_buffer[inst].sync(XCL_BO_SYNC_BO_FROM_DEVICE); });
        sync_event[inst] = task_queue[inst].enqueue([inst]  {gb_infos_buffer[inst].sync(XCL_BO_SYNC_BO_FROM_DEVICE); });
    #if DEBUG_RWBUFF==1
        task_queue[inst].enqueue([inst,i,&jpegs]  {gb_yuv_buffer[inst].read(jpegs[i].yuv_data); });
        sync_event[inst] = task_queue[inst].enqueue([inst,i,&jpegs]  {gb_infos_buffer[inst].read(jpegs[i].infos_data); });
    #endif
    }

    for (i = 0; i < (JPEG_DECODER_INSTANCES > all_num ?all_num : JPEG_DECODER_INSTANCES); i++){
        sync_event[i].wait();
    }
    
    return 0;
}

