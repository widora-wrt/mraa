/*
 * Author: Thomas Ingleby <thomas.c.ingleby@intel.com>
 * Contributions: Jon Trulson <jtrulson@ics.com>
 *                Brendan le Foll <brendan.le.foll@intel.com>
 * Copyright (c) 2014 - 2015 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "lcd.h"
#include "mraa_internal.h"

static mraa_lcd_context
mraa_lcd_init_internal(mraa_adv_func_t* func_table)
{
    mraa_lcd_context dev = (mraa_lcd_context) calloc(1, sizeof(struct _lcd));
    if (dev == NULL) {
        syslog(LOG_CRIT, "uart: Failed to allocate memory for context");
        return NULL;
    }
    dev->index = -1;
    dev->fd = -1;
    dev->advance_func = func_table;
    return dev;
}

mraa_lcd_context
mraa_lcd_init(int index)
{
     mraa_lcd_context dev = mraa_lcd_init_raw((char*)plat->lcd_dev[index].device_path);
    return dev;
}

mraa_lcd_context
mraa_lcd_init_raw(const char* path)
{
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize = 0;
    mraa_lcd_context dev = mraa_lcd_init_internal(plat == NULL ? NULL : plat->adv_func);
    if (dev == NULL) {
        syslog(LOG_ERR, "uart: Failed to allocate memory for context");
        return NULL;
    }
    dev->path = path;
    if (!dev->path) {
        syslog(LOG_ERR, "lcd: device path undefined, open failed");
        free(dev);
        return NULL;
    }
    // now open the device
    if ((dev->fd = open(dev->path, O_RDWR)) == -1) {
        syslog(LOG_ERR, "lcd: open() failed");
        free(dev);
        return NULL;
    }
    if (ioctl(dev->fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        syslog(LOG_ERR,"Error reading fixed information");
        exit(2);
    }
    if (ioctl(dev->fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        syslog(LOG_ERR,"Error reading variable information");
        free(dev);
        return NULL;
    }
    dev->xres= vinfo.xres;
    dev->yres= vinfo.yres;
    dev->bits_per_pixel= vinfo.bits_per_pixel;
    dev->xoffset= vinfo.xoffset;
    dev->line_length=finfo.line_length;
	dev->fphzk = NULL;
	dev->fphzk= fopen(plat->font16_lib_path, "rb");
    if(dev->fphzk == NULL){
        syslog(LOG_ERR,"Error: not found HZK16");
        free(dev);
        return NULL;
    }
	screensize = dev->xres * dev->yres * dev->bits_per_pixel / 8;
    dev->fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED,dev->fd, 0);
    if ((int)dev->fbp == -1) {
        syslog(LOG_ERR,"Error: failed to map framebuffer device to memory");
        free(dev);
        return NULL;
    }
    return dev;
}
unsigned short mraa_lcd_rgb2tft(int c)
{
    unsigned char r,g,b;
    unsigned int t;
    b=(c&0xff)/8;c>>=8;
    g=(c&0xff)/4;c>>=8;
    r=(c&0xff)/8;
    t=r;t<<=6;t|=g;t<<=5;t|=b;
    return t;
}
mraa_result_t
mraa_lcd_drawdot(mraa_lcd_context dev,unsigned int x,unsigned int y,unsigned short color)
{
	if(x>=dev->xres)return;
	if(y>=dev->yres)return;
    long int location = (x+dev->xoffset) * (dev->bits_per_pixel/8) +(y+dev->yoffset) *  dev->line_length;
    *((unsigned short int*)(dev->fbp + location)) =color;
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_drawline(mraa_lcd_context dev,unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2,unsigned short color)
{
    unsigned int t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 
	delta_x=x2-x1; //计算坐标增量 
	delta_y=y2-y1; 
	uRow=x1; 
	uCol=y1; 
	if(delta_x>0)incx=1; //设置单步方向 
	else if(delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;//水平线 
	else{incy=-1;delta_y=-delta_y;} 
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y; 
	for(t=0;t<=distance+1;t++ )//画线输出 
	{  
		mraa_lcd_drawdot(dev,uRow,uCol,color);//画点 
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) 
		{ 
			xerr-=distance; 
			uRow+=incx; 
		} 
		if(yerr>distance) 
		{ 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_drawrect(mraa_lcd_context dev,unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2,unsigned short Color)
{
	mraa_lcd_drawline(dev,x1,y1,x1,y2,Color);	
	mraa_lcd_drawline(dev,x1,y1,x2,y1,Color);	
	mraa_lcd_drawline(dev,x2,y2,x1,y2,Color);	
	mraa_lcd_drawline(dev,x2,y1,x2,y2,Color);	
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_drawrectfill(mraa_lcd_context dev,unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2,unsigned short color)
{
     int x = 0, y = 0;
     for(x=x1;x<x2;x++)
     for(y=y1;y<y2;y++)
     {
        mraa_lcd_drawdot(dev,x,y,color);
     }
     return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_circle_point(mraa_lcd_context dev,int x,int y,int x0,int y0,int color)
{
	mraa_lcd_drawdot(dev,x+x0,y+y0,color);
	mraa_lcd_drawdot(dev,y+x0,x+y0,color);
	mraa_lcd_drawdot(dev,y+x0,-x+y0,color);
	mraa_lcd_drawdot(dev,x+x0,-y+y0,color);
	mraa_lcd_drawdot(dev,-x+x0,-y+y0,color);
	mraa_lcd_drawdot(dev,-y+x0,-x+y0,color);
	mraa_lcd_drawdot(dev,-y+x0,x+y0,color);
	mraa_lcd_drawdot(dev,-x+x0,y+y0,color);
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_drawcircle(mraa_lcd_context dev,int x0,int y0,int r,int color)
{
	int x,y,d;
	x=0;
	y=r;
	d=1-r;
	mraa_lcd_circle_point(dev,x,y,x0,y0,color);
	while(x<=y)
	{
		if(d<0){d+=2*x+3;x++;}
		else{d+=2*(x-y)+5;x++;y--;}
		mraa_lcd_circle_point(dev,x,y,x0,y0,color);
	}
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_circle_line(mraa_lcd_context dev,int x,int y,int x0,int y0,int color)
{
    mraa_lcd_drawline(dev,x+x0,y+y0,-x+x0,y+y0,color);
    mraa_lcd_drawline(dev,y+x0,-x+y0,-y+x0,-x+y0,color);
    mraa_lcd_drawline(dev,-x+x0,-y+y0,x+x0,-y+y0,color);
    mraa_lcd_drawline(dev,-y+x0,x+y0,y+x0,x+y0,color);
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_drawcirclefill(mraa_lcd_context dev,unsigned int x0,unsigned int y0,unsigned int r,unsigned short color)
{
	int x,y,d;
	x=0;
	y=r;
	d=1-r;
	mraa_lcd_circle_point(dev,x,y,x0,y0,color);
	while(x<=y)
	{
		if(d<0){d+=2*x+3;x++;}
		else{d+=2*(x-y)+5;x++;y--;}
		mraa_lcd_circle_line(dev,x,y,x0,y0,color);
	}
    mraa_lcd_drawline(dev,x0-r,y0,x0+r,y0,color);
    return MRAA_SUCCESS; 
}
mraa_result_t
mraa_lcd_drawclear(mraa_lcd_context dev,unsigned short c)
{
    mraa_lcd_drawrectfill(dev,0,0,dev->xres,dev->yres,c);
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_draw_x_8bit(mraa_lcd_context dev,unsigned char Data,unsigned short X,unsigned short Y,unsigned short F_Color,unsigned short B_Color,unsigned short A_Color)
{
	char i;
	for(i=0;i<8;i++)
	{
		if(BIT(i)&Data)mraa_lcd_drawdot(dev,X,Y,F_Color);
		else if(B_Color!=A_Color)mraa_lcd_drawdot(dev,X,Y,B_Color);
		X++;
	}
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_draw_x_8bit_(mraa_lcd_context dev,unsigned char Data,unsigned short X,unsigned short Y,unsigned short F_Color,unsigned short B_Color,unsigned short A_Color)
{
	char i;
	for(i=7;i>-1;i--)
	{
		if(BIT(i)&Data)mraa_lcd_drawdot(dev,X,Y,F_Color);
		else if(B_Color!=A_Color)mraa_lcd_drawdot(dev,X,Y,B_Color);
		X++;
	}
    return MRAA_SUCCESS;
}
/****************************************************************************
Date:2013/8/14
Vision:V1.0
Func:垂直打印一字节显示像素
Note:(Data) 一字节控制像素亮灭 (X,Y) 引用全局地址但不改变全局地址
****************************************************************************/
mraa_result_t
mraa_lcd_draw_y_8bit(mraa_lcd_context dev,unsigned char Data,unsigned short X,unsigned short Y,unsigned short F_Color,unsigned short B_Color,unsigned short A_Color)
{
	char i;
	for(i=0;i<8;i++)
	{
		if(BIT(i)&Data)mraa_lcd_drawdot(dev,X,Y,F_Color);
		else if(B_Color!=A_Color)mraa_lcd_drawdot(dev,X,Y,B_Color);
		Y++;
	}
    return MRAA_SUCCESS;
}

mraa_result_t
mraa_lcd_draw_full_list(mraa_lcd_context dev,void *Data,unsigned short Data_Length,unsigned short X,unsigned short Y,unsigned short F_Color,unsigned short B_Color,unsigned short A_Color)
{
	unsigned short i;
	unsigned char *p;
	p=(unsigned char *)Data;
	for(i=0;i<Data_Length;i++)
	{
		mraa_lcd_draw_x_8bit(dev,*p++,X,Y,F_Color,B_Color,A_Color);
		Y++;
	}
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_draw_full_lists(mraa_lcd_context dev,void *Data,unsigned short w,unsigned short Data_Length,unsigned short X,unsigned short Y,unsigned short F_Color,unsigned short B_Color,unsigned short A_Color)
{
	unsigned short i,n;
	unsigned char *p;
	p=(unsigned char *)Data;
    w/=8;
	for(i=0;i<Data_Length;i++)
	{
        for(n=0;n<w;n++)
        {
		    mraa_lcd_draw_x_8bit_(dev,*p++,X+n*8,Y,F_Color,B_Color,A_Color);
        }
		Y++;
	}
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_stop(mraa_lcd_context dev)
{
	unsigned long screensize;
    if (!dev) {
        syslog(LOG_ERR, "lcd: stop: context is NULL");
        return MRAA_ERROR_INVALID_HANDLE;
    }
	if(dev->fphzk!=NULL)
	{
        fclose(dev->fphzk);
    	dev->fphzk = NULL;
	}
    if (dev->fd >= 0) {
		screensize = dev->xres * dev->yres * dev->bits_per_pixel / 8;
	    munmap(dev->fbp, screensize);
        close(dev->fd);
    }
    free(dev);
    return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_drawfont_ascii(mraa_lcd_context dev,unsigned short f,unsigned short X,unsigned short Y,unsigned short Char,unsigned short f_color,unsigned short b_color,unsigned short a_color)
{            
    FontTypeStruct Font;  
    unsigned int Addr;
	unsigned char i,w;
    Font=FontGetType(f);
    Addr=(unsigned int)Font.ELib;
	w=(Font.Witdh+4)/8;//+7是为了照顾宽度为6/12的字符
    Addr+=(Char-' ')*w*Font.High; 
	for(i=0;i<w;i++)
	{
		mraa_lcd_draw_full_list(dev,(unsigned char *)(Addr+i*Font.High),Font.High,X,Y,f_color,b_color,a_color);
		X+=8;
	}
	return MRAA_SUCCESS;
}
mraa_result_t
mraa_lcd_drawfont_word(mraa_lcd_context dev,unsigned short f,unsigned short X,unsigned short Y,const unsigned char *word,unsigned short f_color,unsigned short b_color,unsigned short a_color)
{            
    
    int  k=0, offset,utf8word;
    FontTypeStruct Font; 
    unsigned char buffer[32];
    unsigned char buf[3] = "啊";
    Font=FontGetType(f);
    if((word[0]&0xf0)==0xe0)
    {
        utf8word=word[0];utf8word<<=8;utf8word|=word[1];utf8word<<=8;utf8word|=word[2];
    }else{
        utf8word=word[0];utf8word<<=8;utf8word|=word[1];
    }
    printf("utf8word=%x\n",utf8word);
    buf[0]=0xB0;
    buf[1]=0xA1;
    while(gb2312_utf8_code[k][1])
    {
        if(gb2312_utf8_code[k][1]==utf8word)
        {
            buf[0]=gb2312_utf8_code[k][0]>>8;
            buf[1]=gb2312_utf8_code[k][0]&0xff;
            break;
        }
        k++;
    }
    printf("buf[0]=%xbuf[0]=%x\n",buf[0],buf[1]);
    offset = (94*(unsigned int)(buf[0]-0xa0-1)+(buf[1]-0xa0-1))*32;
    fseek(dev->fphzk, offset, SEEK_SET);
    fread(buffer, 1, 32, dev->fphzk);
    printf("buffer[0]=%xbuffer[0]=%x\n",buffer[0],buffer[1]);
    mraa_lcd_raw_full_lists(dev,(unsigned char *)(&buffer[0]),16,Font.High,X,Y,f_color,b_color,a_color);
    printf("dev->fphzk=%x\n",dev->fphzk);
	return MRAA_SUCCESS;
}

mraa_result_t
mraa_lcd_drawfont_string(mraa_lcd_context dev,unsigned short f,unsigned short X,unsigned short Y,const unsigned char *Str,unsigned short f_color,unsigned short b_color,unsigned short a_color)
{            
    unsigned short XX,i;
    FontTypeStruct Font;  
    int offset=0;
	unsigned char *P=Str,w;
    Font=FontGetType(f);
    XX=X;
	w=(Font.Witdh+4)/8;//+7是为了照顾宽度为6/12的字符
    int l=strlen(P);
    for(i=0;i<l;i)
    {
        if(P[i]<0x80)
        {
            if((dev->xres-X)<Font.Witdh)
            {
                X=XX;Y+=Font.High;
            } 
            mraa_lcd_drawfont_ascii(dev,f,X,Y,P[i],f_color,b_color,a_color);
            i++;
            X+=Font.Witdh;
        }
        else 
        {
            if((dev->xres-X)<(8*2))
            {
                X=XX;Y+=Font.High;
            } 
            offset=0;
            if(Font.High>16)offset=(((int)Font.High-16)/2+2);
            mraa_lcd_drawfont_word(dev,1616,X,(int)Y+offset,&P[i],f_color,b_color,a_color);
            if((P[i]&0xf0)==0xe0)i+=3;
             else i+=2;
            X+=8*2;
        }
    }
	return MRAA_SUCCESS;
}
int
mraa_lcd_read(mraa_lcd_context dev, char* buf, size_t len)
{
   return 0;
}

int
mraa_lcd_write(mraa_lcd_context dev, const char* buf, size_t len)
{
    return 0;
}

