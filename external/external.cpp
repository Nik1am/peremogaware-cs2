
#include <thread>
#include <iostream>
#include <array>
#include <format>
#include <thread>

#include <Windows.h>
#include <mmsystem.h>

#include "buttons.hpp"
#include "client.dll.hpp"
#include "offsets.hpp"
#include "render.h"
#include "memory.h"
#include "vector.h"
#include "settings.h"
#include "sharedData.h"



bool bhop = 0;
bool autostrafe = 0;
bool aim = 0;
bool glow = 0;
bool antiflash = 0;
bool fovchanger = 0;

bool aim_ignore_if_spotted = 0;
bool aim_ignore_team = 0;
float aim_smooth_factor = 0.6;
float aim_predict = 4.f;
float flash_opacity = 0.f;
bool isrunning = 1;
DWORD64 glow_color_aim_targer = 0xFF00FFFF;
int fov = 120;
int d_boneoffset = 6;
bool d_aimalways = 0;
bool d_showconsole = 1;
bool d_showmenu = 1;
int player_team;
player plrList[64];
view_matrix_t dwViewMatrix;
char map_name[32];
Vector3 local_velocity;
float m_fAccuracyPenalty;
Vector2 test = Vector2(0, 0);
Vector3 aim_angle;
Vector3 local_origin;
Vector3 screen_center = Vector3(1920.f / 2, 1080.f / 2);
float factor;
Vector3 aimpos;
int aimpredictionmethod = 2;
bool aimsafemode = 1;
bool trigger = 0;

bool visthreadsleep = 1;
bool fakeangle = 0;
float fakeangleyaw = 0.f;

int test1;
int test2;

using namespace std;
using namespace cs2_dumper;
using namespace schemas::client_dll;

bool shoot_next_frame = 0;
DWORD64 color = 0xFFFFFFFF;

int jswitch = 0;
int old_m_ptotalHitsOnServer = 0;

auto to_degrees = [](const float radians) {
	return radians * 180.f / 3.1415926535;
};
auto to_radians = [](const float degrees) {
	return degrees * 3.1415926535/180.f;
};

constexpr const int GetWeaponPaint(const short& itemdefinition)
{

	switch (itemdefinition)
	{
		default: return 96;
	}

}

memify mem("cs2.exe");
uintptr_t client = mem.GetBase("client.dll");
uintptr_t engine = mem.GetBase("engine2.dll");

uintptr_t dwLocalPlayerPawn = mem.Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);

uintptr_t dwViewRender = mem.Read<uintptr_t>(client + offsets::client_dll::dwViewRender);
uintptr_t dwGameRules = mem.Read<uintptr_t>(client + offsets::client_dll::dwGameRules);

uintptr_t dwLocalPlayerController = mem.Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerController);
uintptr_t dwEntityList = mem.Read<uintptr_t>(client + offsets::client_dll::dwEntityList);
uintptr_t m_pCameraServices = mem.Read<uintptr_t>(dwLocalPlayerPawn + C_BasePlayerPawn::m_pCameraServices);
uintptr_t m_hActivePostProcessingVolume = mem.Read<uintptr_t>(m_pCameraServices + CPlayer_CameraServices::m_hActivePostProcessingVolume);
uintptr_t m_pMovementServices = mem.Read<uintptr_t>(dwLocalPlayerPawn + C_BasePlayerPawn::m_pMovementServices);
uintptr_t m_pViewModelServices = mem.Read<uintptr_t>(dwLocalPlayerPawn + C_CSPlayerPawnBase::m_pViewModelServices);
uintptr_t m_pBulletServices = mem.Read<uintptr_t>(dwLocalPlayerPawn + C_CSPlayerPawn::m_pBulletServices);
uintptr_t m_hViewEntity = mem.Read<uintptr_t>(m_pViewModelServices + CCSPlayer_ViewModelServices::m_hViewModel);

uintptr_t getControllerFromHandle(uintptr_t entityList, uintptr_t CHandle) {
	CHandle = CHandle & 0x7FFF;
	uintptr_t list = mem.Read<uintptr_t>(entityList + 8 * (CHandle >> 9) + 16);
	uintptr_t controller = list + 120 * (CHandle & 0x1FF);
	return mem.Read<uintptr_t>(controller);
}

void vis_thread() {
	while (isrunning) {
		if(visthreadsleep)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (fakeangle) {
			Vector3 dwViewAngles = mem.Read<Vector3>(client + offsets::client_dll::dwViewAngles);
			Vector3 v_fakeAngle = dwViewAngles;
			v_fakeAngle.y = dwViewAngles.y + fakeangleyaw;
			mem.Write(dwLocalPlayerPawn + C_BasePlayerPawn::v_angle, v_fakeAngle);
		}

		//fov
		if (fovchanger) {
			bool m_bIsScoped = mem.Read<uintptr_t>(dwLocalPlayerPawn + C_CSPlayerPawn::m_bIsScoped);
			if (!m_bIsScoped) {
				mem.Write<int>(m_pCameraServices + CCSPlayerBase_CameraServices::m_iFOV, fov);
			}
		}

		//antiflash
		if (antiflash) {
			float m_flFlashOverlayAlpha = mem.Read<float>(dwLocalPlayerPawn + C_CSPlayerPawnBase::m_flFlashOverlayAlpha);

			if (m_flFlashOverlayAlpha) {
				mem.Write<float>(dwLocalPlayerPawn + C_CSPlayerPawnBase::m_flFlashOverlayAlpha, flash_opacity);
				mem.Write<float>(dwLocalPlayerPawn + C_CSPlayerPawnBase::m_flFlashScreenshotAlpha, flash_opacity);
				//mem.Write<float>(dwLocalPlayerPawn + C_CSPlayerPawnBase::m_flLastSmokeOverlayAlpha, flash_opacity);
			}
		}
		//hs
		int m_totalHitsOnServer = mem.Read<int>(m_pBulletServices + CCSPlayer_BulletServices::m_totalHitsOnServer);
		if (m_totalHitsOnServer != old_m_ptotalHitsOnServer) {
			old_m_ptotalHitsOnServer = m_totalHitsOnServer;
			PlaySound(TEXT("hitsound.wav"), NULL, SND_FILENAME | SND_ASYNC);
		}	
	}
}

void bhop_thread() {
	while (isrunning)
	{
		if (bhop) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			//bhop
			bool m_bOnGroundLastTick = mem.Read<bool>(dwLocalPlayerPawn + C_CSPlayerPawn::m_bOnGroundLastTick);

			if (GetAsyncKeyState(VK_SPACE) && mem.Read<int32_t>(dwLocalPlayerPawn + C_BaseEntity::m_fFlags) & (1 << 0)) {
				int tick = mem.Read<int>(dwLocalPlayerController + CBasePlayerController::m_nTickBase);
				mem.Write<int>(dwLocalPlayerController + CBasePlayerController::m_nTickBase, tick + 4);
				mem.Write<std::uintptr_t>(client + buttons::jump, 65537);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				mem.Write<int>(dwLocalPlayerController + CBasePlayerController::m_nTickBase, tick + 2);
				mem.Write<std::uintptr_t>(client + buttons::jump, 256);
			}
			else {
				mem.Write<std::uintptr_t>(client + buttons::jump, 256);
			}

		}

		if (autostrafe) {// strafe
			Vector3 dwViewAngles = mem.Read<Vector3>(client + offsets::client_dll::dwViewAngles);
			Vector3 velAngle = mem.Read<Vector3>(dwLocalPlayerPawn + C_BaseEntity::m_vecAbsVelocity).ToAngle();
			int left = mem.Read<int>(client + buttons::left);
			int right = mem.Read<int>(client + buttons::right);
			if (!(mem.Read<int32_t>(dwLocalPlayerPawn + C_BaseEntity::m_fFlags) & (1 << 0))) {
				if (left == 65537) {
					dwViewAngles.y = (dwViewAngles.y * factor + velAngle.y * (1.f - factor));
				}
				else if (right == 65537) {
					dwViewAngles.y = (dwViewAngles.y * factor + velAngle.y * (1.f - factor));
				}
				mem.Write(client + offsets::client_dll::dwViewAngles, dwViewAngles);
			}
		}
	}
}

void cheat_thread()
{
	HWND hConsoleWnd = GetConsoleWindow();
	setlocale(0, ".1251");
	player_team = mem.Read<int>(dwLocalPlayerPawn + C_BaseEntity::m_iTeamNum);

	uintptr_t global_vars = mem.Read<uintptr_t>(client + offsets::client_dll::dwGlobalVars);
	

	while (isrunning)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		ShowWindow(hConsoleWnd, d_showconsole ? SW_SHOW : SW_HIDE);

		dwViewMatrix = mem.Read<view_matrix_t>(client + offsets::client_dll::dwViewMatrix);

		uintptr_t m_pWeaponServices = mem.Read<uintptr_t>(dwLocalPlayerPawn + C_BasePlayerPawn::m_pWeaponServices);
		uintptr_t m_hActiveWeapon = mem.Read<uintptr_t>(m_pWeaponServices + CPlayer_WeaponServices::m_hActiveWeapon);
		uintptr_t weapon = getControllerFromHandle(dwEntityList, m_hActiveWeapon);

		uintptr_t weaponGameSceneNode = mem.Read<uintptr_t>(weapon + C_BaseEntity::m_pGameSceneNode);

		m_fAccuracyPenalty = mem.Read<float>(weapon + C_CSWeaponBase::m_fAccuracyPenalty);

		Vector3 local_m_vecViewOffset = mem.Read<Vector3>(dwLocalPlayerPawn + C_BaseModelEntity::m_vecViewOffset);
		local_origin = mem.Read<Vector3>(dwLocalPlayerPawn + C_BasePlayerPawn::m_vOldOrigin) + local_m_vecViewOffset;
		Vector3 dwViewAngles = mem.Read<Vector3>(client + offsets::client_dll::dwViewAngles);
			    local_velocity = mem.Read<Vector3>(dwLocalPlayerPawn + C_BaseEntity::m_vecAbsVelocity);


		Vector3 dwViewAngles_pos = dwViewAngles.AngleToPos();
		aim_angle = Vector3(0,0,0);
		float min_delta = 100;

		int localPlayer_id = 0;

		//trigger
		if (trigger) {
			uintptr_t m_iIDEntIndex = mem.Read<uintptr_t>(dwLocalPlayerPawn + C_CSPlayerPawnBase::m_iIDEntIndex);
			uintptr_t trigger_playerController = getControllerFromHandle(dwEntityList, m_iIDEntIndex);
			int trigger_teamnum = mem.Read<int>(trigger_playerController + C_BaseEntity::m_iTeamNum);
			if ((GetKeyState(0x43) & 0x8000)) { // C key
				if (trigger_teamnum != 0) {
					if (aim_ignore_team || (player_team != trigger_teamnum)) {
						mouse_event(MOUSEEVENTF_LEFTDOWN, 960, 540, 0, 0);
						mouse_event(MOUSEEVENTF_LEFTUP, 960, 540, 0, 0);
					}
				}
			}
		}

		//entlist
		for (int i = 0; i < 64; i++) {
			uintptr_t playerController = getControllerFromHandle(dwEntityList, i);
			if (!playerController) continue;
			
			uintptr_t playerPawnHandle = mem.Read<uintptr_t>(playerController + CCSPlayerController::m_hPlayerPawn);

			uintptr_t pCSPlayerPawn = getControllerFromHandle(dwEntityList, playerPawnHandle);
			if (!pCSPlayerPawn) continue;

			uintptr_t pointer_to_name = mem.Read<uintptr_t>(playerController + CCSPlayerController::m_sSanitizedPlayerName);
			mem.ReadRaw(pointer_to_name, &plrList[i].name, sizeof(plrList[i].name));

			mem.ReadRaw(pCSPlayerPawn + C_CSPlayerPawn::m_szLastPlaceName, &plrList[i].m_szLastPlaceName, sizeof(plrList[i].m_szLastPlaceName));

			int m_iTeamNum = mem.Read<int>(pCSPlayerPawn + C_BaseEntity::m_iTeamNum);
			plrList[i].m_iTeamNum = m_iTeamNum;
			int m_iHealth = mem.Read<int>(pCSPlayerPawn + C_BaseEntity::m_iHealth);
			plrList[i].m_iHealth = m_iHealth;
			int m_ArmorValue = mem.Read<int>(pCSPlayerPawn + C_CSPlayerPawn::m_ArmorValue);
			plrList[i].armor = m_ArmorValue;
			bool m_bSpotted = mem.Read<bool>(pCSPlayerPawn + C_CSPlayerPawn::m_entitySpottedState + 0x8);
			
			UINT32 m_bSpottedByMask[2];
			mem.ReadRaw(pCSPlayerPawn + C_CSPlayerPawn::m_entitySpottedState + EntitySpottedState_t::m_bSpottedByMask, &m_bSpottedByMask, sizeof(m_bSpottedByMask));

			uintptr_t m_pGameSceneNode = mem.Read<uintptr_t>(pCSPlayerPawn + C_BaseEntity::m_pGameSceneNode);
			uintptr_t boneArray = mem.Read<uintptr_t>(m_pGameSceneNode + 0x1F0);

			mem.ReadRaw(boneArray, &plrList[i].bonelist, sizeof(plrList[i].bonelist));
			plrList[i].bonePos = plrList[i].bonelist[d_boneoffset].origin;

			plrList[i].bonePosHistory[2] = plrList[i].bonePosHistory[1];
			plrList[i].bonePosHistory[1] = plrList[i].bonePosHistory[0];
			plrList[i].bonePosHistory[0] = plrList[i].bonePos;

			plrList[i].bonePosPred = Vector3(0,0,0);

			switch (aimpredictionmethod)
			{
			case 0:
				plrList[i].bonePosPred = plrList[i].bonePos;
				break;
			case 1:
				if (!plrList[i].bonePosHistory[1].IsZero()) {
					plrList[i].bonePosPred = plrList[i].bonePosHistory[0] + (plrList[i].bonePosHistory[0] - plrList[i].bonePosHistory[1]);
				}
				break;
			case 2:
				if (!plrList[i].bonePosHistory[1].IsZero() && !plrList[i].bonePosHistory[2].IsZero())
				{
					plrList[i].bonePosPred = plrList[i].bonePosHistory[0] +
						(plrList[i].bonePosHistory[0] - plrList[i].bonePosHistory[1]) +
						(plrList[i].bonePosHistory[0] - plrList[i].bonePosHistory[1] + (plrList[i].bonePosHistory[1] - plrList[i].bonePosHistory[2]));
				}
				break;
			default:
				break;
			}

			




			Vector3 m_vecOrigin = mem.Read<Vector3>(m_pGameSceneNode + CGameSceneNode::m_vecOrigin);
			plrList[i].origin = m_vecOrigin;

			Vector3 m_vecViewOffset = mem.Read<Vector3>(pCSPlayerPawn + C_BaseModelEntity::m_vecViewOffset);

			if (player_team == m_iTeamNum) {
				color = 0xFF00FF00;
			}
			else {
				color = 0xFF0000FF;
			}

			plrList[i].isAimTarget = 0;
			if (pCSPlayerPawn != dwLocalPlayerPawn) {
				if (aim_ignore_team || (player_team != m_iTeamNum)) {
					if (m_bSpotted || aim_ignore_if_spotted) { //m_bSpotted
						if (m_iHealth > 0) {
							plrList[i].isAimTarget = 1;
							color = glow_color_aim_targer;
							//aim v1
							
							Vector3 dir = plrList[i].bonePosPred - local_origin;
							Vector3 angle = dir.ToAngle();

							plrList[i].deltaYaw = to_radians(dwViewAngles.y - angle.y);

							Vector3 dir_n = dir.normalize();
							float delta = dwViewAngles_pos.calculate_distance(dir_n);

							if (delta <= min_delta) {
								min_delta = delta;
								aim_angle = angle;
								aimpos = plrList[i].bonePosPred;
							}
						}
					}
				}
			}
			else {
				localPlayer_id = i;
				plrList[i].isLocalPlayer = 1;
			}

			/*
			if (dwLocalPlayerPawn != pCSPlayerPawn) {
				mem.Write<bool>(pCSPlayerPawn + C_BaseModelEntity::m_Glow + CGlowProperty::m_bEligibleForScreenHighlight, 0);
				mem.Write<bool>(pCSPlayerPawn + C_BaseModelEntity::m_Glow + CGlowProperty::m_bGlowing, 1);
			}
			*/
			/*
			mem.Write<DWORD64>(pCSPlayerPawn + C_BaseModelEntity::m_Glow + CGlowProperty::m_glowColorOverride, color);
			mem.Write<bool>(pCSPlayerPawn + C_BaseModelEntity::m_Glow + CGlowProperty::m_bFlashing, 0);
			*/

			//plrList[i].bonePosOld = mem.Read<Vector3>(boneArray + d_boneoffset * 0x20);
		}

		if (aim) {
			if (d_aimalways || (GetKeyState(0x58) & 0x8000)) { // X key
				if (!aim_angle.IsZero()) {
					if ((dwViewAngles.y - aim_angle.y) > 180) {
						aim_angle.y += 360;
					}
					if ((dwViewAngles.y - aim_angle.y) < -180) {
						aim_angle.y -= 360;
					}

					if (aimsafemode) {
						Vector3 safeaim = aimpos.WTS(dwViewMatrix, 1920.f, 1080.f);
						if (safeaim.z > 0) {
							mouse_event(MOUSEEVENTF_MOVE, ceil((safeaim.x - 960) * (1.f-aim_smooth_factor)), ceil((safeaim.y - 540) * (1.f - aim_smooth_factor)), 0, 0);
						}
					}
					else {
						mem.Write(client + offsets::client_dll::dwViewAngles, (dwViewAngles * aim_smooth_factor + aim_angle * (1 - aim_smooth_factor)));
					}

					//mem.Write(dwLocalPlayerPawn + C_BasePlayerPawn::v_angle, aim_angle);
				}
			}
		}


	}
}

int main() {
	thread t1(cheat_thread);
	thread t2(vis_thread);
	thread t3(render_menu);
	thread t4(bhop_thread);
	t1.join();
	t2.join();
	t3.join();
	t4.join();
}