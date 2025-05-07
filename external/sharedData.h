#pragma once

#ifndef __data__
#define __data__

#include "settings.h"
#include <basetsd.h>
#include "vector.h"

struct bone {
	Vector3 origin;
	float pad[5];
};

struct player {
	Vector3 origin;
	Vector3 bonePos;
	Vector3 bonePosOld;
	Vector3 bonePosPred;
	Vector3 bonePosHistory[3];
	float deltaYaw;
	//Vector3 screenPos;
	bool isLocalPlayer;
	int m_iHealth;
	int armor;
	int m_iTeamNum;
	int isAimTarget;
	char name[128];
	char m_szLastPlaceName[18];
	bone bonelist[124];
};

struct vec4 {
	Vector3 vec3;
	float fl;
};

extern Vector3 aimpos;
extern Vector3 local_velocity;
extern float m_fAccuracyPenalty;
extern Vector3 aim_angle;
extern Vector3 local_origin;
extern player plrList[64];
extern char map_name[32];

extern view_matrix_t dwViewMatrix;
extern int player_team;
#endif