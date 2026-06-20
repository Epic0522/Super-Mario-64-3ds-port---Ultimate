#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <stdbool.h>

extern bool         configFullscreen;
extern unsigned int configKeyA;
extern unsigned int configKeyB;
extern unsigned int configKeyStart;
extern unsigned int configKeyL;
extern unsigned int configKeyR;
extern unsigned int configKeyZ;
extern unsigned int configKeyCUp;
extern unsigned int configKeyCDown;
extern unsigned int configKeyCLeft;
extern unsigned int configKeyCRight;
extern unsigned int configKeyStickUp;
extern unsigned int configKeyStickDown;
extern unsigned int configKeyStickLeft;
extern unsigned int configKeyStickRight;

extern unsigned int puppycam_sensitivityX;
extern unsigned int puppycam_sensitivityY;
extern unsigned int puppycam_invertX;
extern unsigned int puppycam_invertY;
extern unsigned int puppycam_degrade;
extern unsigned int puppycam_aggression;
extern unsigned int puppycam_panlevel;
extern unsigned int dynamic_shadows_enabled;
#ifdef TARGET_N3DS
extern unsigned int config3dsAntiAliasing;
extern unsigned int config3dsWideMode;
#endif

void configfile_load(const char *filename);
void configfile_save(const char *filename);

#endif
