/**********************************************************************************************
Copyright         2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:         menu.c
Author:           fangjiancai
Version :         1.0
Date:             08-01-11
Description:      一些基本的图形函数
Version:    	  1.0
Function List:    DrawFrame(),FillArea(),DrawBMP(),DrawMenu()
History:      	
    <author>      <time>   <version >   <desc>
    fangjiancai  08-01-11  1.0          
    fangjiancai  08-02-18  1.0          添加DrawBMP和DrawMenu两个函数
 ***********************************************************************************************/
 #include "include_all.h"

extern short* osd_buffer;
extern short* attr_buffer;

/********************************************************************************
Function:         DrawFrame()
Description:      画指定颜色和透明度的边框
Input:            目标缓冲区指针-DrawBuf
                  屏幕的水平像素数-Stride
                  边框的起始像素点坐标-x,y.
                  边框的宽度和高度w,h.
                  边框的厚度width,
                  边框的颜色-FrameRGBcolor,边框的Alpha值-AlphaValue
Output:           无
Return:           -1 error,0 ok
********************************************************************************/
#ifdef DVR_DEMO
int  DrawFrame(UINT16 *DestBuf,UINT32 Stride,UINT32 x,UINT32 y,UINT32 w,UINT32 h,UINT32 width,UINT16 FrameRGBcolor,UINT8 AlphaValue)
{
    int      row,col;
    UINT16  *DestAddr,*DestAddr_base;
    /*添加对指针的判断，防止对非法指针进行操作*/
    if(DestBuf == NULL)
    {
         return -1;
    }
	  
    //top line
    DestAddr_base= DestBuf + y*Stride+x;
    //printf("begin to draw top line\n");
    for(row=0;row<width;row++)
    {
         DestAddr = DestAddr_base + row*Stride;
         for(col=0;col<w;col++)
         {
              DestAddr[col]= FrameRGBcolor;
         }
	 //DestAddr +=Stride;
    }
    //printf("draw top line successful\n");
    /*设置透明度*/
    setOsdTransparency(AlphaValue,FALSE,x,y,w,width);

    //bottom line
    DestAddr_base= DestBuf + Stride*(y+h-width)+x;
    //printf("begin to draw bottom line\n");
    for(row=0;row<width;row++)
    {
         DestAddr = DestAddr_base + row*Stride;
         for(col=0;col<w;col++)
         {
              DestAddr[col]=FrameRGBcolor;
         }
	 //DestAddr +=Stride;
    }
    //printf("draw bottom line successful\n");
    /*设置透明度*/
    setOsdTransparency(AlphaValue,FALSE,x,y+h-width,w,width);
    //printf("begin to draw left and right line\n");
    //left and right
    DestAddr_base= DestBuf +y*Stride+x;
    for(row=0;row<h;row++)
    {
         
         DestAddr = DestAddr_base +row*Stride;
         for(col=0;col<width;col++)
         {
              /*left*/
              DestAddr[col] = FrameRGBcolor;
	      /*right*/
              DestAddr[col+w-width]=FrameRGBcolor;
         }
         //DestAddr +=Stride;
    }
    //printf("draw left and right line successful\n");
    /*设置左边透明度*/
    setOsdTransparency(AlphaValue,FALSE,x,y,width,h);
    /*设置右边透明度*/
    setOsdTransparency(AlphaValue,FALSE,x+w-width,y,width,h);
    return 0;
      
}


/********************************************************************************
Function:         FillArea()
Description:      将指定区域设置成指定颜色和透明度以及边框的透明度和颜色
Input:            目标缓冲区指针-DrawBuf,
                  屏幕的水平像素数-Stride
                  区域的起始像素点坐标-x,y.
                  区域的宽度和高度w,h.
                  区域的要填充颜色-FillRGBcolor
                  是否带边框-bFrame
                  边框的颜色-FramRGBcolor
                  边框的厚度-Framewidth,
                  区域的Alpha值-AlphaValue
Output:           无
Return:           -1:error,0 ok
********************************************************************************/
int  FillArea(UINT16 *DestBuf,UINT32 Stride,UINT32 x,UINT32 y,UINT32 w,UINT32 h,UINT16 FillRGBcolor,
              BOOL bFrame,UINT8 AreAlphaValue,UINT8 FrameAlphaValue,UINT16 FrameRGBcolor,UINT32 Framewidth)
{
    int      row,col;
    UINT16  *DestAddr;
    if(DestBuf == NULL)
    {
         return -1;
    }

    DestAddr = DestBuf + y*Stride +x;
    /*将包括边框在内的区域颜色都设置成区域的颜色和透明度*/
    for(row=0;row<h;row++)
    {
         for(col=0;col<w;col++)
         {
              DestAddr[col]= FillRGBcolor;
         }
	 DestAddr +=Stride;
    }
    setOsdTransparency(AreAlphaValue,FALSE,x,y,w,h);
    /*最后在单独设置边框的颜色和透明度*/
    if(bFrame)
    {
         DrawFrame(DestBuf, Stride,x,y,w,h,Framewidth,FrameRGBcolor,FrameAlphaValue);
    }
    //sleep(5);
    //setOsdTransparency(0x77,FALSE,x,y,w,h);
    //sleep(5);
    //setOsdTransparency(0x55,FALSE,x,y,w,h);
    return 0;
    
}

/*********************************************************************************
Function:         DrawBMP()
Description:      将一个rgb888的图片的数据部分转换为rgb565的存储格式，并显示
Input:            目标缓冲区指针-DrawBuf,
                  屏幕的水平像素数-Stride
                  显示图片区域的起始像素点坐标-x,y.
                  图片是否带边框-bFrame
                  边框的颜色-FramRGBcolor
                  边框的厚度-Framewidth,
                  区域的Alpha值-AlphaValue
		  边框的Alpha值-FrameAlphaValue
Output:           无
Return:           -1:error,0 ok
*********************************************************************************/
int  DrawBMP(char * picture_name,UINT16 *DestBuf,UINT32 Stride,UINT32 x,UINT32 y,BOOL bFrame,UINT8 AreAlphaValue,
                          UINT8 FrameAlphaValue,UINT16 FrameRGBcolor,UINT32 Framewidth)
{
    char      picture_path[32];
    UINT8    *rgb888_data,*temp;
    UINT32  h, w, pic_size,r, g, b,size,offset;
    int fd,HeadLen,InfoLen,i,j,row,col;
    UINT16  *rgb565_data,*p,*DestAddr,*DestAddrBase,*p_base;

    /*位图文件头*/	
    struct
    {
         UINT16 bfType;//位图文件类型
         UINT32 bfSize;//位图文件大小，以字节为单位
         UINT16 bfReserve1;//保留
         UINT16 bfReserve2;//保留	
         UINT32 bfoffbits;//位图数据的起始位置，以向对应位图文件头的相对偏移量表示，以字节为单位
    }BITMAP_FILEHEADER;//该数据结果共14个字节
    /*位图信息头*/		
    struct
    {
         UINT32 bitSize;//本结构所占用的字节数		
         UINT32 biWidth;//位图文件的宽度，以像素为单位
         UINT32 biHeight;//位图文件的高度，以像素为单位
         UINT16 biPlanes;	
         UINT16 biBitCount;	
         UINT32 biCompression;	
         UINT32 biSizeImage;
         UINT32 biXPlosPerMeter;
         UINT32 biYPlosPerMeter;	
         UINT32 biClrUsed;		
         UINT32 biClrImportant;	
    }BITMAP_INFOHEADER;//该结构共占用40个字节

    /*判断指针是否为空*/
    if(DestBuf==NULL)
    {
         printf("the pointer of DestBuf was NULL\n");
         return -1;
    }
    if(picture_name==NULL)
    {
	 printf("the filename was NULL\n");
         return -1;
    }
    /*打开位图文件，并读取位图数据到rgb888_data缓冲区中*/
    sprintf(picture_path,"/davinci/%s",picture_name);
    printf("picture_path=%s\n",picture_path);
    fd = open(picture_name,O_RDONLY);
    if(fd<=0)
    {
         printf("open the picture file fail\n");
         return ERROR;
    }
    size = lseek(fd, 0, SEEK_END);
    printf("size = %d\n",size);
    rgb888_data = (UINT8 *)malloc(size);
    if(rgb888_data ==NULL)
    {
         printf("malloc() fail 1\n");
	 return ERROR;
    }
    else
    {
         printf("malloc() successful 1\n");
    }
    lseek(fd,0,SEEK_SET);
    size=read(fd,rgb888_data,size);
    printf("size =%d\n",size);
    
    /*读取位图文件信息，宽和高*/
    HeadLen = /*sizeof(BITMAP_FILEHEADER)*/14;
    InfoLen = sizeof(BITMAP_INFOHEADER);
    printf("HeadLen =%d,InfoLen =%d\n",HeadLen,InfoLen);
    memcpy(&BITMAP_FILEHEADER,rgb888_data,HeadLen);
    memcpy(&BITMAP_INFOHEADER,rgb888_data + HeadLen,InfoLen);
    offset = BITMAP_FILEHEADER.bfoffbits;
	
    w = BITMAP_INFOHEADER.biWidth;
    h = BITMAP_INFOHEADER.biHeight;
    pic_size =BITMAP_INFOHEADER.biSizeImage;
    printf("w=%d,h=%d,Imagesize=%d,offset=%d\n",w,h,pic_size,offset);
    printf("bixplosperMeter=%d,biyplosperMeter=%d\n",BITMAP_INFOHEADER.biXPlosPerMeter,BITMAP_INFOHEADER.biYPlosPerMeter);
    rgb565_data = (UINT16 *)malloc(w*h*2);
    if(rgb565_data == NULL)
    {
         printf("malloc() for rgb565_data fail\n");
	 return ERROR;
    }
    /*将位图数据部分转换为rgb565存储格式，rgb888是按照从下到上，从左到右存储的，
     转换后按照从上到下，从左到右的16位存储*/
    temp = rgb888_data+54;
    p_base= rgb565_data+w*(h-1);
    for(i=0;i<h;i++)
    {
         p=p_base -w*i;
         for(j=0;j<w;j++)
         {
              /*依次取出红，绿，蓝各个颜色分量，并组装成16为的565格式*/
              b = (*temp) & 0xf8;
              g = (*(temp+1)) &0xfc;
              r = (*(temp +2)) &0xf8;
              p[j] =  ( ((r >> 3) << 11) | ((g >> 2)  << 5) | (b >> 3) );
              temp +=3;
         }
    }
    printf("copy successful\n");

    /*将图片的565数据拷贝到framebuffer*/
    DestAddrBase= DestBuf+ y*Stride +x;
    p_base=rgb565_data;
    /*将包括边框在内的区域颜色都设置成区域的颜色和透明度*/
    for(row=0;row<h;row++)
    {
         DestAddr =DestAddrBase+Stride*row;
         p =p_base+row*w;
         for(col=0;col<w;col++)
         {
              DestAddr[col]= p[col];
         }
         //printf("row=%d    DestAddr=%x    p=%x\n",row,DestAddr,p);
    }
    printf("draw pic successful\n");
    setOsdTransparency(AreAlphaValue,FALSE,x,y,w,h);
    printf("setosdTransparency() finally\n");
    /*最后在单独设置边框的颜色和透明度*/
    if(bFrame)
    {
         DrawFrame(DestBuf, Stride,x,y,w,h,Framewidth,FrameRGBcolor,FrameAlphaValue);
    }
    printf("DrawFrame() finally\n");
    free(rgb565_data);
    free(rgb888_data);
    printf("free memory\n");
    //sleep(5);
    //setOsdTransparency(0x77,FALSE,x,y,w,h);
    //sleep(5);
    //setOsdTransparency(0x55,FALSE,x,y,w,h);
    return 0;
}
/*******************************************************************************
Function:         DrawMenu()
Description:      在一个边框内画几幅图片
Input:            目标缓冲区指针-DrawBuf
Output:           无
Return:           -1:error,0 ok
 ******************************************************************************/
int DrawMenu(UINT16 *DestBuf)
{
	char filename[32];

	if(DestBuf==NULL)
	{
             printf("the pointer of DestBuf was NULL\n");
	     return -1;
	}
	/*画最外面的一个边框*/
	DrawFrame(DestBuf,720,150,50,400,400,3,0xffff,0x77);

	/*画各个菜单图片，（包括边框在内）*/
	sprintf(filename,"record.bmp");
	DrawBMP(filename,DestBuf,720,180,80,TRUE,0x77,0x33,0xFFFF,3);
	sprintf(filename,"preview.bmp");
	DrawBMP(filename,DestBuf,720,400,80,TRUE,0x77,0x33,0xFFFF,3);
	sprintf(filename,"alarm.bmp");
	DrawBMP(filename,DestBuf,720,180,300,TRUE,0x77,0x33,0xFFFF,3);
	sprintf(filename,"playback.bmp");
	DrawBMP(filename,DestBuf,720,400,300,TRUE,0x77,0x33,0xFFFF,3);
	return 0;
}
#endif
