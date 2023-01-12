# multi jpeg-decoders

输入jpeg图片，demo 当前支持YUV420的jpeg图片，解码后转为RGB，并存为位图。  
硬件IP实现了jpeg 转 YUV 原始数据，并转为bgrx格式，支持YUV420格式  


```
$ ./multi_jpeg_decoder -h
arguments:
-x [input xclbin file]
-i [input jpeg file]

# ex
$ ./multi_jpeg_decoder -x DPU_6pe_misc_add_6_jepg_100Mhz.xclbin -i ./feishu.jpg

# 输出 ./decoded.bmp 图片
```

## 编译 
source XRT 环境  和 Vitis2202环境，然后编译，ex:
```bash
source /opt/xilinx/xrt/setup.sh
source /data/tool/xilinx/Vitis/2022.2/settings64.sh
make 
```
