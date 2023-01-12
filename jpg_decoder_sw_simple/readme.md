# jpeg decoder 

输入jpeg图片，demo 当前支持YUV444和YUV420的jpeg图片，解码后转为RGB，并存为位图。  
硬件IP实现了jpeg 转 YUV 原始数据，支持YUV420，YUV420，YUV444格式  
demo程序能将YUV420和YUV444格式 转为RGB，并存为位图。（host CPU转换）  

```
$ ./demo_jpgdecoder -h
arguments:
-x [input xclbin file]
-i [input jpeg file]

# ex
$ ./demo_jpgdecoder -x DPU_with_jpeg_and_opencv_resize_100MHz_11_15.xclbin -i ./colorful_1280_720.jpg

# 输出 ./decoded.bmp 图片
```

## 编译 
source XRT 环境  和 Vitis2102环境，然后编译，ex:
```bash
source /opt/xilinx/xrt/setup.sh
source /data/tool/xilinx/Vitis/2021.2/settings64.sh
make 
```
