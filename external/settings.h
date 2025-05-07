#pragma once
#include <basetsd.h>
#include "vector.h"

#ifndef __settings__
#define __settings__
extern bool menuopened;
extern bool aim_ignore_if_spotted;
extern bool aim_ignore_team;
extern float aim_smooth_factor;
extern float aim_predict;
extern bool isrunning;
extern int fov;
extern float flash_opacity;
extern DWORD64 glow_color_sameteam;
extern DWORD64 glow_color_enemy;
extern DWORD64 glow_color_aim_targer;
extern int d_boneoffset;
extern bool d_aimalways;
extern bool d_showconsole;
extern bool d_showmenu;


extern bool bhop;
extern bool autostrafe;
extern bool aim;
extern bool glow;
extern bool antiflash;
extern bool fovchanger;
extern int aimpredictionmethod;
extern bool aimsafemode;

extern bool trigger;

extern bool visthreadsleep;
extern bool fakeangle;
extern float fakeangleyaw;

extern int test1;
extern int test2;
extern float factor;
#endif
