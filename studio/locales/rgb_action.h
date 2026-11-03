#pragma once
#include <stdint.h>
// Full-intensity linear RGB; global brightness remains bounded by the board.
static inline void ksl_rgb_color(const uint16_t hsv[3],uint8_t out[3]){
 unsigned h=hsv[0]%360,s=hsv[1],sector=h/60,f=h%60;
 unsigned p=255*(100-s)/100,q=255*(6000-s*f)/6000,t=255*(6000-s*(60-f))/6000;
 switch(sector){case 0:out[0]=255;out[1]=t;out[2]=p;break;case 1:out[0]=q;out[1]=255;out[2]=p;break;case 2:out[0]=p;out[1]=255;out[2]=t;break;case 3:out[0]=p;out[1]=q;out[2]=255;break;case 4:out[0]=t;out[1]=p;out[2]=255;break;default:out[0]=255;out[1]=p;out[2]=q;break;}
}
