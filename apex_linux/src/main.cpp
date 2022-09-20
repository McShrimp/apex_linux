#include "main.h"
#include <float.h>
#include <array>
#include "timer.h"
#include <thread>


float last_vis_time_flt[72]{};
float nearDist{};
//Color glowColor{15.0f, 0.2f, 0.2f};
bool is_vis[72];
int ff{};
int z{};
bool low[72]{};
bool locked{0};
float bestFov{360};
Timer<std::chrono::milliseconds> start;
Timer<std::chrono::milliseconds> aim;
Timer<std::chrono::milliseconds> startaim;
int bestEnt{};




void skinchanger(QWORD weaponAddy, QWORD playerAddy, rx_handle apexProc);

void lockTarget(rx_handle apexProc, QWORD input);


int main(void)
{
	//std::chrono::system_clock::time_point t1;
	//std::chrono::system_clock::time_point t2;

	int pid = GetApexProcessId();

	if (pid == 0)
	{
		printf("[-] r5apex.exe was not found\n");
		return 0;
	}

	rx_handle r5apex = rx_open_process(pid, RX_ALL_ACCESS);
	if (r5apex == 0)
	{
		printf("[-] unable to attach r5apex.exe\n");
		return 0;
	}

	printf("[+] r5apex.exe pid [%d]\n", pid);

	//
	// get base address
	//
	QWORD base_module = GetApexBaseAddress(pid);
	if (base_module == 0)
	{
		base_module = 0x140000000;
	}

	printf("[+] r5apex.exe base [0x%lx]\n", base_module);

	DWORD dwBulletSpeed = 0, dwBulletGravity = 0, dwMuzzle = 0, dwVisibleTime = 0;

	QWORD base_module_dump = rx_dump_module(r5apex, base_module);

	if (base_module_dump == 0)
	{
		printf("[-] failed to dump r5apex.exe\n");
		rx_close_handle(r5apex);
		return 0;
	}

	QWORD IClientEntityList = 0;
	{
		char pattern[] = "\x4C\x8B\x15\x00\x00\x00\x00\x33\xF6";
		char mask[] = "xxx????xx";

		// IClientEntityList = 0x1a203b8 + base_module + 0x280050;
		IClientEntityList = rx_scan_pattern(base_module_dump, pattern, mask, 10);
		if (IClientEntityList)
		{
			IClientEntityList = ResolveRelativeAddressEx(r5apex, IClientEntityList, 3, 7);
			IClientEntityList = IClientEntityList + 0x08;
		}
	}

	QWORD dwLocalPlayer = 0;
	{

		// 89 41 28 48 8B 05 ? ? ? ?
		char pattern[] = "\x89\x41\x28\x48\x8B\x05\x00\x00\x00\x00";
		char mask[] = "xxxxxx????";
		dwLocalPlayer = rx_scan_pattern(base_module_dump, pattern, mask, 11);
		if (dwLocalPlayer)
		{
			dwLocalPlayer = dwLocalPlayer + 0x03;
			dwLocalPlayer = ResolveRelativeAddressEx(r5apex, dwLocalPlayer, 3, 7);
		}
	}
	/*char levelName[50];
	QWORD dwLevelName = ResolveRelativeAddressEx(r5apex, 0x13a17b8, 3, 7);
		// levelName = rx_read_process(r5apex, dwLevelName, levelName, 50);
	
	char levelName[260];
		{
			QWORD name_ptr = rx_read_i32(r5apex, 0x13a17b8 - base_module);
			rx_read_process(r5apex, name_ptr, levelName, 260);
		}*/
	
	QWORD IInputSystem = 0;
	{
		// 48 8B 05 ? ? ? ? 48 8D 4C  24 20 BA 01 00 00 00 C7
		char pattern[] = "\x48\x8B\x05\x00\x00\x00\x00\x48\x8D\x4C\x24\x20\xBA\x01\x00\x00\x00\xC7";
		char mask[] = "xxx????xxxxxxxxxxx";

		IInputSystem = rx_scan_pattern(base_module_dump, pattern, mask, 19);
		IInputSystem = ResolveRelativeAddressEx(r5apex, IInputSystem, 3, 7);
		IInputSystem = IInputSystem - 0x10;
	}

	QWORD GetAllClasses = 0;
	{
		// 48 8B 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 89 74 24 20
		char pattern[] = "\x48\x8B\x05\x00\x00\x00\x00\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x48\x89\x74\x24\x20";
		char mask[] = "xxx????xxxxxxxxxxxxxx";
		GetAllClasses = rx_scan_pattern(base_module_dump, pattern, mask, 22);
		GetAllClasses = ResolveRelativeAddressEx(r5apex, GetAllClasses, 3, 7);
		GetAllClasses = rx_read_i64(r5apex, GetAllClasses);
	}

	QWORD sensitivity = 0;
	{
		// sensitivity
		// 48 8B 05 ? ? ? ? F3 0F 10 3D ? ? ? ? F3 0F 10 70 68
		char pattern[] = "\x48\x8B\x05\x00\x00\x00\x00\xF3\x0F\x10\x3D\x00\x00\x00\x00\xF3\x0F\x10\x70\x68";
		char mask[] = "xxx????xxxx????xxxxx";
		sensitivity = rx_scan_pattern(base_module_dump, pattern, mask, 21);

		if (sensitivity)
		{
			sensitivity = ResolveRelativeAddressEx(r5apex, sensitivity, 3, 7);
			sensitivity = rx_read_i64(r5apex, sensitivity);
		}
	}

	{

		char pattern[] = "\x75\x0F\xF3\x44\x0F\x10\xBF\x00\x00\x00\x00";
		char mask[] = "xxxxxxx????";
		QWORD temp_address = rx_scan_pattern(base_module_dump, pattern, mask, 12);
		if (temp_address)
		{

			QWORD bullet_gravity = temp_address + 0x02;
			bullet_gravity = bullet_gravity + 0x05;

			QWORD bullet_speed = temp_address - 0x6D;
			bullet_speed = bullet_speed + 0x04;

			dwBulletSpeed = rx_read_i32(r5apex, bullet_speed);
			dwBulletGravity = rx_read_i32(r5apex, bullet_gravity);
		}
	}

	{
		char pattern[] = "\xF3\x0F\x10\x91\x00\x00\x00\x00\x48\x8D\x04\x40";
		char mask[] = "xxxx????xxxx";

		QWORD temp_address = rx_scan_pattern(base_module_dump, pattern, mask, 13);
		if (temp_address)
		{
			temp_address = temp_address + 0x04;
			dwMuzzle = rx_read_i32(r5apex, temp_address);
		}
	}

	{
		// 48 8B CE  ? ? ? ? ? 84 C0 0F 84 BA 00 00 00
		char pattern[] = "\x48\x8B\xCE\x00\x00\x00\x00\x00\x84\xC0\x0F\x84\xBA\x00\x00\x00";
		char mask[] = "xxx?????xxxxxxxx";
		QWORD temp_address = rx_scan_pattern(base_module_dump, pattern, mask, 17);
		if (temp_address)
		{
			temp_address = temp_address + 0x10;
			dwVisibleTime = rx_read_i32(r5apex, temp_address + 0x4);
		}
	}

	rx_free_module(base_module_dump);

	while (GetAllClasses)
	{

		QWORD recv_table = rx_read_i64(r5apex, GetAllClasses + 0x18);
		QWORD recv_name = rx_read_i64(r5apex, recv_table + 0x4C8);

		char name[260];
		rx_read_process(r5apex, recv_name, name, 260);

		if (!strcmp(name, "DT_Player"))
		{
			m_iHealth = dump_table(r5apex, recv_table, "m_iHealth");
			m_iViewAngles = dump_table(r5apex, recv_table, "m_ammoPoolCapacity") - 0x14;
			m_bZooming = dump_table(r5apex, recv_table, "m_bZooming");
			m_lifeState = dump_table(r5apex, recv_table, "m_lifeState");
			m_iCameraAngles = dump_table(r5apex, recv_table, "m_zoomFullStartTime") + 0x2EC;
		}

		if (!strcmp(name, "DT_BaseEntity"))
		{
			m_iTeamNum = dump_table(r5apex, recv_table, "m_iTeamNum");
			m_vecAbsOrigin = 0x014c;
		}

		if (!strcmp(name, "DT_BaseCombatCharacter"))
		{
			m_iWeapon = dump_table(r5apex, recv_table, "m_latestPrimaryWeapons");
		}

		if (!strcmp(name, "DT_BaseAnimating"))
		{
			m_iBoneMatrix = dump_table(r5apex, recv_table, "m_nForceBone") + 0x50 - 0x8;
		}

		if (!strcmp(name, "DT_WeaponX"))
		{
			m_playerData = dump_table(r5apex, recv_table, "m_playerData");
		}

		GetAllClasses = rx_read_i64(r5apex, GetAllClasses + 0x20);
	}

	DWORD previous_tick = 0;
	float lastvis_aim[70];
	memset(lastvis_aim, 0, sizeof(lastvis_aim));

	if (IClientEntityList == 0)
	{
		printf("[-] IClientEntityList not found\n");
		goto ON_EXIT;
	}

	if (dwLocalPlayer == 0)
	{
		printf("[-] dwLocalPlayer not found\n");
		goto ON_EXIT;
	}
	/*if (dwLevelName == 0)
	{
		printf("[-] dwLevelName not found\n");
		goto ON_EXIT;
	}*/

	if (IInputSystem == 0)
	{
		printf("[-] IInputSystem not found\n");
		goto ON_EXIT;
	}

	if (sensitivity == 0)
	{
		printf("[-] sensitivity not found\n");
		goto ON_EXIT;
	}

	if (dwBulletSpeed == 0)
	{
		printf("[-] dwBulletSpeed not found\n");
		goto ON_EXIT;
	}

	if (dwBulletGravity == 0)
	{
		printf("[-] dwBulletGravity not found\n");
		goto ON_EXIT;
	}

	if (dwMuzzle == 0)
	{
		printf("[-] dwMuzzle not found\n");
		goto ON_EXIT;
	}

	if (dwVisibleTime == 0)
	{
		printf("[-] dwVisibleTime not found\n");
		goto ON_EXIT;
	}

	dwMuzzle = dwMuzzle - 0x04;

	printf("[+] IClientEntityList: %lx\n", IClientEntityList - base_module);
	printf("[+] dwLocalPlayer: %lx\n", dwLocalPlayer - base_module);
	//printf("[+] LevelName1: %.260s\n", levelName);
	printf("[+] IInputSystem: %lx\n", IInputSystem - base_module);
	printf("[+] sensitivity: %lx\n", sensitivity - base_module);
	printf("[+] dwBulletSpeed: %x\n", dwBulletSpeed);
	printf("[+] dwBulletGravity: %x\n", dwBulletGravity);
	printf("[+] dwMuzzle: %x\n", dwMuzzle);
	printf("[+] dwVisibleTime: %x\n", dwMuzzle);
	printf("[+] m_iHealth: %x\n", m_iHealth);
	printf("[+] m_iViewAngles: %x\n", m_iViewAngles);
	printf("[+] m_bZooming: %x\n", m_bZooming);
	printf("[+] m_iCameraAngles: %x\n", m_iCameraAngles);
	printf("[+] m_lifeState: %x\n", m_lifeState);
	printf("[+] m_iTeamNum: %x\n", m_iTeamNum);
	printf("[+] m_vecAbsOrigin: %x\n", m_vecAbsOrigin);
	printf("[+] m_iWeapon: %x\n", m_iWeapon);
	printf("[+] m_iBoneMatrix: %x\n", m_iBoneMatrix);
	printf("[+] m_playerData: %x\n", m_playerData);

	printf (R"("
  _______ _______         _______ _______         
(  ____ (  ___  |\     /(  ____ (  ____ |\     /|
| (    \| (   ) ( \   / | (    )| (    \( \   / )
| |     | (___) |\ (_) /| (____)| (__    \ (_) / 
| | ____|  ___  | \   / |  _____|  __)    ) _ (  
| | \_  | (   ) |  ) (  | (     | (      / ( ) \ 
| (___) | )   ( |  | |  | )     | (____/( /   \ )
(_______|/     \|  \_/  |/      (_______|/     \|
                                                 

")");  

	while (1)
	{
		if (!rx_process_exists(r5apex))
		{
			break;
		}
		

		/*if(IsButtonDown(r5apex, IInputSystem, 87)&& AIMFOV < 360)
		AIMFOV += 1;
		if(IsButtonDown(r5apex, IInputSystem, 89)&& AIMFOV >= 4)
		AIMFOV -= 1;
		if(IsButtonDown(r5apex, IInputSystem, 88) && AIMSMOOTH>= 0)
		AIMSMOOTH -= 1;
		if(IsButtonDown(r5apex, IInputSystem, 90)&& AIMSMOOTH < 20)
		AIMSMOOTH += 1;*/
		//if (IsButtonDown(r5apex, IInputSystem, 80))
		
/*for (int i = 50; i <= 150; i++) {
if(IsButtonDown(r5apex, IInputSystem, i))
	printf("Key: %x", i);
}*/
		
		
		
		
			/*clear();
			printf("Current Aimsmooth: %f", AIMSMOOTH);
			printf("\tCurrent AimFOV: %f", AIMFOV);
			printf("\tVis Bypass: %i", (int)visbypass);
			printf("\tDistance to nearest target: %i", nearDist);

			printf (R"(
  _______ _______         _______ _______         
(  ____ (  ___  |\     /(  ____ (  ____ |\     /|
| (    \| (   ) ( \   / | (    )| (    \( \   / )
| |     | (___) |\ (_) /| (____)| (__    \ (_) / 
| | ____|  ___  | \   / |  _____|  __)    ) _ (  
| | \_  | (   ) |  ) (  | (     | (      / ( ) \ 
| (___) | )   ( |  | |  | )     | (____/( /   \ )
(_______|/     \|  \_/  |/      (_______|/     \|
                                                 

")");  */
		
		
		QWORD localplayer = rx_read_i64(r5apex, dwLocalPlayer);

		if (localplayer == 0)
		{
			previous_tick = 0;
			memset(lastvis_aim, 0, sizeof(lastvis_aim));
			continue;
		}

		DWORD local_team = rx_read_i32(r5apex, localplayer + m_iTeamNum);

		float fl_sensitivity = rx_read_float(r5apex, sensitivity + 0x68);
		DWORD weapon_id = rx_read_i32(r5apex, localplayer + m_iWeapon) & 0xFFFF;
		QWORD weapon = GetClientEntity(r5apex, IClientEntityList, weapon_id - 1);

		float bulletSpeed = rx_read_float(r5apex, weapon + dwBulletSpeed);
		float bulletGravity = rx_read_float(r5apex, weapon + dwBulletGravity);

		vec3 muzzle;
		rx_read_process(r5apex, localplayer + dwMuzzle, &muzzle, sizeof(vec3));

		float target_fov = 360.0f;
		QWORD target_entity = 0;

		vec3 local_position;
		rx_read_process(r5apex, localplayer + m_vecAbsOrigin, &local_position, sizeof(vec3));

		
		for (int i = 0; i < 70; i++)
		{

			QWORD entity = GetClientEntity(r5apex, IClientEntityList, i);

			if (entity == 0)
				continue;

			if (entity == localplayer)
				continue;

			if (rx_read_i32(r5apex, entity + m_iHealth) == 0)
			{
				lastvis_aim[i] = 0;
				continue;
			}

			if (rx_read_i32(r5apex, entity + m_iTeamNum) == local_team)
			{
				continue;
			}
//
			if (rx_read_i32(r5apex, entity + m_lifeState) != 0)
			{
				lastvis_aim[i] = 0;
				continue;
			}

			if (rx_read_i32(r5apex, entity + 0x2688) != 0) //bleedoutstate
			{
				lastvis_aim[i] = 0;
				continue;
			}

			if(IsButtonDown(r5apex, IInputSystem, 87)){
				srand(time(NULL));
			ff = rand() % 27 + 1;

			skinchanger(weapon, localplayer, r5apex);
			}

			vec3 head = GetBonePosition(r5apex, entity, 2);

			vec3 velocity;
			rx_read_process(r5apex, entity + m_vecAbsOrigin - 0xC, &velocity, sizeof(vec3));
			
			
			//nearDist = vec_distance(local_position, enmPos);

			float fl_time = vec_distance(head, muzzle) / bulletSpeed;
			
			head.z += (700.0f * bulletGravity * 0.5f) * (fl_time * fl_time);

			//if (neardist)
			velocity.x = velocity.x * fl_time;
			velocity.y = velocity.y * fl_time;
			velocity.z = velocity.z * fl_time;

			head.x += velocity.x;
			head.y += velocity.y;
			head.z += velocity.z;

			vec3 target_angle = CalcAngle(muzzle, head);
			vec3 breath_angles;

			rx_read_process(r5apex, localplayer + m_iViewAngles - 0x10, &breath_angles, sizeof(vec3));

			float last_visible = rx_read_float(r5apex, entity + dwVisibleTime);
			 
  		// If the player was never visible the value is -1
  			is_vis[i] = last_visible > last_vis_time_flt[i] || last_visible < 0.f && last_vis_time_flt[i] > 0.f;
 
  			last_vis_time_flt[i] = last_visible;

				std::array<float, 3> glowColorRed{61.f, 2.f, 2.f};
				std::array<float, 3> glowColorGreen{2.f, 61.f, 2.f};
				std::array<float, 3> glowColorBlue{2.f, 2.f, 61.f};
				std::array<float, 3> glowColorPurple{61.f, 15.f, 53.f};

		
				if (rx_read_i32(r5apex, entity + m_iHealth)+rx_read_i32(r5apex, entity + 0x0170) < 70) // health + shield
				low[i] = true;
				else
				low[i] = false;	
				


			if (last_visible != 0.00f)
			{
				
			
				
				float fov = get_fov(breath_angles, target_angle);


				if (last_visible > lastvis_aim[i])
				{
					if (low[i])
				rx_write_process(r5apex, entity + 0x1d0, &glowColorPurple, sizeof(Color)) == sizeof(Color);
				else
				rx_write_process(r5apex, entity + 0x1d0, &glowColorGreen, sizeof(Color)) == sizeof(Color);

					//printf("Distance, %f", nearDist/100);
					if ((!locked || aim.diff() > 25) && fov < target_fov){
					z = i;
					target_fov = fov;
					bestFov = fov;
					target_entity = entity;
					lastvis_aim[i] = last_visible;
					//printf("fov: %f", fov);
					aim.reset();
					}
					
				}
				else
				if (start.diff() >= 1){	
				start.reset();
				
			if (!is_vis[i])
			if (low[i])
			rx_write_process(r5apex, entity + 0x1d0, &glowColorBlue, sizeof(Color)) == sizeof(Color);
			else
			rx_write_process(r5apex, entity + 0x1d0, &glowColorRed, sizeof(Color)) == sizeof(Color);
			}			

				
			}

			//printf("Time: %i", start.diff());
			//rx_write_i32(r5apex, entity + 0x262, 16256);
			//rx_write_i32(r5apex, entity + 0x2d0, 1193322764);
			std::array<int8_t, 4> glow;
			
 glow = {101, 102, 50, 100 };
  // If the player was never visible the value is -1
				
	
			
		

			//for (int i = 0; i < 4; ++i)
			//rx_write_i32(r5apex, entity + 0x2c4+(0x4*(i+1)), glow[i]);  //Setting the of the Glow at all necessary spots
			
			rx_write_i32(r5apex, entity + 0x3c8, 1); 
			rx_write_i32(r5apex, entity + 0x3d0, 2);
			//rx_write_i32(r5apex, entity + 0x3d8, 1); // glow thru walls test
			
			
					if(IsButtonDown(r5apex, IInputSystem, 80))
rx_write_process(r5apex, entity + 0x1d0, &glowColorBlue, sizeof(Color)) == sizeof(Color);			

			
			rx_write_process(r5apex, entity + 0x2c4, &glow, sizeof(GlowMode)) == sizeof(GlowMode);
			
			//rx_write_i32(r5apex, entity + 0x1D0, 2.f);
			//rx_write_i32(r5apex, entity + 0x1D4, 61.f);
			//rx_write_i32(r5apex, entity + 0x1D8, 2.f);
	
		}

	
		if (target_entity && IsButtonDown(r5apex, IInputSystem, AIMKEY))
		{
			aim.reset();
			bestEnt = z;
			
			if (rx_read_i32(r5apex, target_entity + m_iHealth) == 0){
				locked = false;
				continue;
			}
			
			
			vec3 target_angle = {0, 0, 0};
			float fov = 360.0f;
			int bone_list[] = {2, 3, 5, 8};

			vec3 breath_angles;
			rx_read_process(r5apex, localplayer + m_iViewAngles - 0x10, &breath_angles, sizeof(vec3));

			

				srand(time(NULL));
				vec3 head;
				int v2 = rand() % 4 + 1; 
				int i{};
				if (v2 == 4)
				i = 7;
				else 
				i = 3;

				head = GetBonePosition(r5apex, target_entity, i);
				
				//if (rand() % 1 + 3)
				//head = GetBonePosition(r5apex, target_entity, bone_list[1]);

				

				//if (rand() % 5 + 1)
				//continue;

				//if (rand() % 5 + 1)
				//head = GetBonePosition(r5apex, target_entity, bone_list[3]);

	
				

				vec3 velocity;
				rx_read_process(r5apex, target_entity + m_vecAbsOrigin - 0xC, &velocity, sizeof(vec3));
				
				vec3 enmPos;
				rx_read_process(r5apex, target_entity + 0x158, &enmPos, sizeof(vec3));

				bool close = (calcDistance(local_position, enmPos) < 400);
				//float close_fov = AIMFOV;
				//if (close)
				//float close_fov = AIMFOV + 12; 	
			
				float fl_time = vec_distance(head, muzzle) / bulletSpeed;
				
				head.z += (700.0f * bulletGravity * 0.5f) * (fl_time * fl_time);
				
				velocity.x = velocity.x * fl_time;
				velocity.y = velocity.y * fl_time;
				velocity.z = velocity.z * fl_time;

				
				
				head.x += velocity.x;
				head.y += velocity.y;
				head.z += velocity.z;
				
				
				//printf("distance %f", calcDistance(local_position, enmPos));
				vec3 angle = CalcAngle(muzzle, head);
				float temp_fov = get_fov(breath_angles, angle);

				if (temp_fov < 2.5)
				locked = true;
				else
				locked = false;

				if (!is_vis[z])
				locked = false;

				if (temp_fov < fov)
				{
					fov = temp_fov;
					target_angle = angle;
				}
			

			DWORD weapon_id = rx_read_i32(r5apex, localplayer + m_iWeapon) & 0xFFFF;
			QWORD weapon = GetClientEntity(r5apex, IClientEntityList, weapon_id - 1);
			float zoom_fov = rx_read_float(r5apex, weapon + m_playerData + 0xb8);

			if (rx_read_i8(r5apex, localplayer + m_bZooming))
			{
				fl_sensitivity = (zoom_fov / 90.0f) * fl_sensitivity;
			}
			
			if ((fov <= AIMFOV)/* || (fov <= close_fov)*/)
			{

				vec3 angles;
				angles.x = breath_angles.x - target_angle.x;
				angles.y = breath_angles.y - target_angle.y;
				angles.z = 0;
				vec_clamp(&angles);

				float x = angles.y;
				float y = angles.x;
				x = (x / fl_sensitivity) / 0.022f;
				y = (y / fl_sensitivity) / -0.022f;

				float sx = 0.0f, sy = 0.0f;

				float smooth = AIMSMOOTH;

				DWORD aim_ticks = 0;

				if (smooth >= 1.0f)
				{
					if (sx < x)
						sx = sx + 1.0f + (x / smooth);
					else if (sx > x)
						sx = sx - 1.0f + (x / smooth);
					else
						sx = x;

					if (sy < y)
						sy = sy + 1.0f + (y / smooth);
					else if (sy > y)
						sy = sy - 1.0f + (y / smooth);
					else
						sy = y;
					aim_ticks = (DWORD)(smooth / 100.0f);
				}
				else
				{
					sx = x;
					sy = y;
				}

				

				if (abs((int)sx) > 100)
					continue;

				if (abs((int)sy) > 100)
					continue;

				DWORD current_tick = rx_read_i32(r5apex, IInputSystem + 0xcd8);

				//printf("Time: %i", (int)(current_tick-previous_tick));
				
				if (current_tick - previous_tick > aim_ticks)
				{
					
					
					previous_tick = current_tick;
					typedef struct
					{
						int x, y;
					} mouse_data;
					mouse_data data;

					data.x = (int)sx;
					data.y = (int)sy;
					
					
					

					
					rx_write_process(r5apex, IInputSystem + 0x1DB0, &data, sizeof(data));
					//}
			
				}
			}
		}
	
	
	}

ON_EXIT:
	rx_close_handle(r5apex);
}

QWORD rx_read_i64(rx_handle process, QWORD address)
{
	QWORD buffer = 0;
	rx_read_process(process, address, &buffer, sizeof(buffer));
	return buffer;
}

DWORD rx_read_i32(rx_handle process, QWORD address)
{
	DWORD buffer = 0;
	rx_read_process(process, address, &buffer, sizeof(buffer));
	return buffer;
}

WORD rx_read_i16(rx_handle process, QWORD address)
{
	WORD buffer = 0;
	rx_read_process(process, address, &buffer, sizeof(buffer));
	return buffer;
}

BYTE rx_read_i8(rx_handle process, QWORD address)
{
	BYTE buffer = 0;
	rx_read_process(process, address, &buffer, sizeof(buffer));
	return buffer;
}

float rx_read_float(rx_handle process, QWORD address)
{
	float buffer = 0;
	rx_read_process(process, address, &buffer, sizeof(buffer));
	return buffer;
}

BOOL rx_write_i32(rx_handle process, QWORD address, DWORD value)
{
	return rx_write_process(process, address, &value, sizeof(value)) == sizeof(value);
}

QWORD ResolveRelativeAddressEx(
    rx_handle process,
    QWORD Instruction,
    DWORD OffsetOffset,
    DWORD InstructionSize)
{

	QWORD Instr = (QWORD)Instruction;
	DWORD RipOffset = rx_read_i32(process, Instr + OffsetOffset);
	QWORD ResolvedAddr = (QWORD)(Instr + InstructionSize + RipOffset);
	return ResolvedAddr;
}

QWORD rx_dump_module(rx_handle process, QWORD base)
{
	QWORD a0, a1, a2, a3 = 0;
	char *a4;

	a0 = base;
	if (a0 == 0)
		return 0;

	a1 = rx_read_i32(process, a0 + 0x03C) + a0;
	if (a1 == a0)
	{
		return 0;
	}

	a2 = rx_read_i32(process, a1 + 0x050);
	if (a2 < 8)
		return 0;

	a4 = (char *)malloc(a2 + 24);

	*(QWORD *)(a4) = base;
	*(QWORD *)(a4 + 8) = a2;
	*(QWORD *)(a4 + 16) = a3;

	a4 += 24;

	QWORD image_dos_header = base;
	QWORD image_nt_header = rx_read_i32(process, image_dos_header + 0x03C) + image_dos_header;

	DWORD headers_size = rx_read_i32(process, image_nt_header + 0x54);
	rx_read_process(process, image_dos_header, a4, headers_size);

	unsigned short machine = rx_read_i16(process, image_nt_header + 0x4);

	QWORD section_header = machine == 0x8664 ? image_nt_header + 0x0108 : image_nt_header + 0x00F8;

	for (WORD i = 0; i < rx_read_i16(process, image_nt_header + 0x06); i++)
	{

		QWORD section = section_header + (i * 40);
		QWORD local_virtual_address = base + rx_read_i32(process, section + 0x0c);
		DWORD local_virtual_size = rx_read_i32(process, section + 0x8);
		QWORD target_virtual_address = (QWORD)a4 + rx_read_i32(process, section + 0xc);
		rx_read_process(process, local_virtual_address, (PVOID)target_virtual_address, local_virtual_size);
	}

	return (QWORD)a4;
}

void rx_free_module(QWORD dumped_module)
{
	dumped_module -= 24;
	free((void *)dumped_module);
}

BOOL bDataCompare(const BYTE *pData, const BYTE *bMask, const char *szMask)
{

	for (; *szMask; ++szMask, ++pData, ++bMask)
		if (*szMask == 'x' && *pData != *bMask)
			return 0;

	return (*szMask) == 0;
}

QWORD FindPatternEx(QWORD dwAddress, QWORD dwLen, BYTE *bMask, char *szMask)
{

	if (dwLen <= 0)
		return 0;
	for (QWORD i = 0; i < dwLen; i++)
		if (bDataCompare((BYTE *)(dwAddress + i), bMask, szMask))
			return (QWORD)(dwAddress + i);

	return 0;
}

QWORD rx_scan_pattern(QWORD dumped_module, PCSTR pattern, PCSTR mask, QWORD length)
{
	QWORD ret = 0;
	QWORD a0;

	if (dumped_module == 0)
		return 0;

	dumped_module -= 24;
	a0 = *(QWORD *)(dumped_module);
	dumped_module += 24;

	QWORD image_dos_header = dumped_module;
	QWORD image_nt_header = *(DWORD *)(image_dos_header + 0x03C) + image_dos_header;

	unsigned short machine = *(unsigned short *)(image_nt_header + 0x4);

	QWORD section_header = machine == 0x8664 ? image_nt_header + 0x0108 : image_nt_header + 0x00F8;

	for (WORD i = 0; i < *(unsigned short *)(image_nt_header + 0x06); i++)
	{

		QWORD section = section_header + (i * 40);
		QWORD section_address = image_dos_header + *(DWORD *)(section + 0x14);
		DWORD section_size = *(DWORD *)(section + 0x10);
		DWORD section_characteristics = *(DWORD *)(section + 0x24);

		if (section_characteristics & 0x00000020)
		{
			QWORD address = FindPatternEx(section_address, section_size - length, (BYTE *)pattern, (char *)mask);
			if (address)
			{
				ret = (address - dumped_module) + a0;
				break;
			}
		}
	}
	return ret;
}

void skinchanger(QWORD weaponAddy, QWORD playerAddy, rx_handle apexProc){
	srand(time(NULL));
	
			printf("Weapon: %i", rx_read_i32(apexProc, weaponAddy + 0x0e48));
			printf("Skin: %i", rx_read_i32(apexProc, playerAddy + 0x0e48));
			rx_write_i32(apexProc, playerAddy + 0x0e48, ff); 
			rx_write_i32(apexProc, weaponAddy + 0x0e48, ff); 
}


