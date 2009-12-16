/**********************************************************************************************
Copyright         2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:         menu.c
Author:           fangjiancai
Version :         1.0
Date:             08-01-11
Description:      һЩ������ͼ�κ���
Version:    	  1.0
Function List:    DrawFrame(),FillArea(),DrawBMP(),DrawMenu()
History:      	
    <author>      <time>   <version >   <desc>
    fangjiancai  08-01-11  1.0          
    fangjiancai  08-02-18  1.0          ���DrawBMP��DrawMenu��������
 ***********************************************************************************************/
 #include "include_all.h"

extern short* osd_buffer;
extern short* attr_buffer;

/********************************************************************************
Function:         DrawFrame()
Description:      ��ָ����ɫ��͸���ȵı߿�
Input:            Ŀ�껺����ָ��-DrawBuf
                  ��Ļ��ˮƽ������-Stride
                  �߿����ʼ���ص�����-x,y.
                  �߿�Ŀ�Ⱥ͸߶�w,h.
                  �߿�ĺ��width,
                  �߿����ɫ-FrameRGBcolor,�߿��Alphaֵ-AlphaValue
Output:           ��
Return:           -1 error,0 ok
********************************************************************************/
#ifdef DVR_DEMO
int  DrawFrame(UINT16 *DestBuf,UINT32 Stride,UINT32 x,UINT32 y,UINT32 w,UINT32 h,UINT32 width,UINT16 FrameRGBcolor,UINT8 AlphaValue)
{
    int      row,col;
    UINT16  *DestAddr,*DestAddr_base;
    /*��Ӷ�ָ����жϣ���ֹ�ԷǷ�ָ����в���*/
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
    /*����͸����*/
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
    /*����͸����*/
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
    /*�������͸����*/
    setOsdTransparency(AlphaValue,FALSE,x,y,width,h);
    /*�����ұ�͸����*/
    setOsdTransparency(AlphaValue,FALSE,x+w-width,y,width,h);
    return 0;
      
}


/********************************************************************************
Function:         FillArea()
Description:      ��ָ���������ó�ָ����ɫ��͸�����Լ��߿��͸���Ⱥ���ɫ
Input:            Ŀ�껺����ָ��-DrawBuf,
                  ��Ļ��ˮƽ������-Stride
                  �������ʼ���ص�����-x,y.
                  ����Ŀ�Ⱥ͸߶�w,h.
                  �����Ҫ�����ɫ-FillRGBcolor
                  �Ƿ���߿�-bFrame
                  �߿����ɫ-FramRGBcolor
                  �߿�ĺ��-Framewidth,
                  �����Alphaֵ-AlphaValue
Output:           ��
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
    /*�������߿����ڵ�������ɫ�����ó��������ɫ��͸����*/
    for(row=0;row<h;row++)
    {
         for(col=0;col<w;col++)
         {
              DestAddr[col]= FillRGBcolor;
         }
	 DestAddr +=Stride;
    }
    setOsdTransparency(AreAlphaValue,FALSE,x,y,w,h);
    /*����ڵ������ñ߿����ɫ��͸����*/
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
Description:      ��һ��rgb888��ͼƬ�����ݲ���ת��Ϊrgb565�Ĵ洢��ʽ������ʾ
Input:            Ŀ�껺����ָ��-DrawBuf,
                  ��Ļ��ˮƽ������-Stride
                  ��ʾͼƬ�������ʼ���ص�����-x,y.
                  ͼƬ�Ƿ���߿�-bFrame
                  �߿����ɫ-FramRGBcolor
                  �߿�ĺ��-Framewidth,
                  �����Alphaֵ-AlphaValue
		  �߿��Alphaֵ-FrameAlphaValue
Output:           ��
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

    /*λͼ�ļ�ͷ*/	
    struct
    {
         UINT16 bfType;//λͼ�ļ�����
         UINT32 bfSize;//λͼ�ļ���С�����ֽ�Ϊ��λ
         UINT16 bfReserve1;//����
         UINT16 bfReserve2;//����	
         UINT32 bfoffbits;//λͼ���ݵ���ʼλ�ã������Ӧλͼ�ļ�ͷ�����ƫ������ʾ�����ֽ�Ϊ��λ
    }BITMAP_FILEHEADER;//�����ݽ����14���ֽ�
    /*λͼ��Ϣͷ*/		
    struct
    {
         UINT32 bitSize;//���ṹ��ռ�õ��ֽ���		
         UINT32 biWidth;//λͼ�ļ��Ŀ�ȣ�������Ϊ��λ
         UINT32 biHeight;//λͼ�ļ��ĸ߶ȣ�������Ϊ��λ
         UINT16 biPlanes;	
         UINT16 biBitCount;	
         UINT32 biCompression;	
         UINT32 biSizeImage;
         UINT32 biXPlosPerMeter;
         UINT32 biYPlosPerMeter;	
         UINT32 biClrUsed;		
         UINT32 biClrImportant;	
    }BITMAP_INFOHEADER;//�ýṹ��ռ��40���ֽ�

    /*�ж�ָ���Ƿ�Ϊ��*/
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
    /*��λͼ�ļ�������ȡλͼ���ݵ�rgb888_data��������*/
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
    
    /*��ȡλͼ�ļ���Ϣ����͸�*/
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
    /*��λͼ���ݲ���ת��Ϊrgb565�洢��ʽ��rgb888�ǰ��մ��µ��ϣ������Ҵ洢�ģ�
     ת�����մ��ϵ��£������ҵ�16λ�洢*/
    temp = rgb888_data+54;
    p_base= rgb565_data+w*(h-1);
    for(i=0;i<h;i++)
    {
         p=p_base -w*i;
         for(j=0;j<w;j++)
         {
              /*����ȡ���죬�̣���������ɫ����������װ��16Ϊ��565��ʽ*/
              b = (*temp) & 0xf8;
              g = (*(temp+1)) &0xfc;
              r = (*(temp +2)) &0xf8;
              p[j] =  ( ((r >> 3) << 11) | ((g >> 2)  << 5) | (b >> 3) );
              temp +=3;
         }
    }
    printf("copy successful\n");

    /*��ͼƬ��565���ݿ�����framebuffer*/
    DestAddrBase= DestBuf+ y*Stride +x;
    p_base=rgb565_data;
    /*�������߿����ڵ�������ɫ�����ó��������ɫ��͸����*/
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
    /*����ڵ������ñ߿����ɫ��͸����*/
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
Description:      ��һ���߿��ڻ�����ͼƬ
Input:            Ŀ�껺����ָ��-DrawBuf
Output:           ��
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
	/*���������һ���߿�*/
	DrawFrame(DestBuf,720,150,50,400,400,3,0xffff,0x77);

	/*�������˵�ͼƬ���������߿����ڣ�*/
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
