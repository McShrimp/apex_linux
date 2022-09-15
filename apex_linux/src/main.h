/*
 * tested on ubuntu 22.04.1 LTS
 */

#include "../../rx/rx.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <iostream>
#include <stdlib.h>

// keys: 107 = mouse1, 108 = mouse2, 109 = mouse3, 110 = mouse4, 111 = mouse5
#define AIMKEY 111
float AIMFOV = 12.0f; // change for FOV circle
float AIMSMOOTH = 11.0f; // change for aim snapiness
//int visbypass = 0;
#define GLOW_ESP 1
#define UPKEY 111
#define DOWNKEY 111
#define SWITCHKEY 111

#define clear() printf("\033[H\033[J")


typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned long QWORD;
typedef int BOOL;
typedef const char *PCSTR;
typedef const unsigned short *PCWSTR;
typedef void *PVOID;

//
// some windows extensions for rx library
// these are required because we are working with PE images
//


float AIMSMOOTHold{1};
float AIMFOVold{1};
float last_visibleOld {1000};
float nearDist{};
int visbypassOld{};
//Color color{}



typedef struct
{
	float x, y, z;
} vec3;

typedef struct
{
	int8_t GeneralGlowMode, BorderGlowMode, BorderSize, TransparentLevel;
}GlowMode;

typedef struct
{
	float r, b, g;
}Color;

float vec_length_sqrt(vec3 p0)
{
	return (float)sqrt(p0.x * p0.x + p0.y * p0.y + p0.z * p0.z);
}

vec3 vec_sub(vec3 p0, vec3 p1)
{
	vec3 r;

	r.x = p0.x - p1.x;
	r.y = p0.y - p1.y;
	r.z = p0.z - p1.z;
	return r;
}

void vec_clamp(vec3 *v)
{

	if (v->x > 89.0f && v->x <= 180.0f)
	{
		v->x = 89.0f;
	}
	if (v->x > 180.0f)
	{
		v->x = v->x - 360.0f;
	}
	if (v->x < -89.0f)
	{
		v->x = -89.0f;
	}
	v->y = fmodf(v->y + 180, 360) - 180;
	v->z = 0;
}

float vec_distance(vec3 p0, vec3 p1)
{
	return vec_length_sqrt(vec_sub(p0, p1));
}

 float calcDistance(vec3 p0, vec3 p1) {
    float dx = p0.x - p1.x;
    float dy = p0.y - p1.y;
    float dz = p0.z - p1.z;
    float distance = sqrtf(powf(dx, 2) + powf(dy, 2) + powf(dz, 2));
    return distance;
 }
vec3 CalcAngle(vec3 src, vec3 dst)
{
	vec3 angle;

	vec3 delta = vec_sub(src, dst);

	float hyp = sqrt(delta.x * delta.x + delta.y * delta.y);

	angle.x = atan(delta.z / hyp) * (float)(180.0 / M_PI);
	angle.y = atan(delta.y / delta.x) * (float)(180.0 / M_PI);
	angle.z = 0;

	if (delta.x >= 0.0)
		angle.y += 180.0f;

	return angle;
}

float get_fov(vec3 scrangles, vec3 aimangles)
{
	vec3 delta;

	delta.x = aimangles.x - scrangles.x;
	delta.y = aimangles.y - scrangles.y;
	delta.z = aimangles.z - scrangles.z;

	if (delta.x > 180)
		delta.x = 360 - delta.x;
	if (delta.x < 0)
		delta.x = -delta.x;

	delta.y = fmodf(delta.y + 180, 360) - 180;
	if (delta.y < 0)
		delta.y = -delta.y;

	return sqrt((float)(pow(delta.x, 2.0) + pow(delta.y, 2.0)));
}

QWORD rx_dump_module(rx_handle process, QWORD base);
void rx_free_module(QWORD dumped_module);
QWORD rx_scan_pattern(QWORD dumped_module, PCSTR pattern, PCSTR mask, QWORD length);
QWORD rx_read_i64(rx_handle process, QWORD address);
DWORD rx_read_i32(rx_handle process, QWORD address);
WORD rx_read_i16(rx_handle process, QWORD address);
BYTE rx_read_i8(rx_handle process, QWORD address);
float rx_read_float(rx_handle process, QWORD address);
BOOL rx_write_i32(rx_handle process, QWORD address, DWORD value)
{
	return rx_write_process(process, address, &value, sizeof(value)) == sizeof(value);
}

QWORD ResolveRelativeAddressEx(
    rx_handle process,
    QWORD Instruction,
    DWORD OffsetOffset,
    DWORD InstructionSize);

int GetApexProcessId(void)
{
	int pid = 0;
	rx_handle snapshot = rx_create_snapshot(RX_SNAP_TYPE_PROCESS, 0);

	RX_PROCESS_ENTRY entry;

	while (rx_next_process(snapshot, &entry))
	{
		if (!strcmp(entry.name, "wine64-preloader"))
		{
			rx_handle snapshot_2 = rx_create_snapshot(RX_SNAP_TYPE_LIBRARY, entry.pid);

			RX_LIBRARY_ENTRY library_entry;

			while (rx_next_library(snapshot_2, &library_entry))
			{
				if (!strcmp(library_entry.name, "easyanticheat_x64.dll"))
				{
					pid = entry.pid;
					break;
				}
			}
			rx_close_handle(snapshot_2);
		}
	}
	rx_close_handle(snapshot);

	return pid;
}

QWORD GetApexBaseAddress(int pid)
{
	rx_handle snapshot = rx_create_snapshot(RX_SNAP_TYPE_LIBRARY, pid);

	RX_LIBRARY_ENTRY entry;
	DWORD counter = 0;
	QWORD base = 0;

	while (rx_next_library(snapshot, &entry))
	{
		const char *sub = strstr(entry.name, "memfd:wine-mapping");

		if ((entry.end - entry.start) == 0x1000 && sub)
		{
			if (counter == 0)
				base = entry.start;
		}

		if (sub)
		{
			counter++;
		}

		else
		{
			counter = 0;
			base = 0;
		}

		if (counter >= 200)
		{
			break;
		}
	}

	rx_close_handle(snapshot);

	return base;
}

typedef struct
{
	uint8_t pad1[0xCC];
	float x;
	uint8_t pad2[0xC];
	float y;
	uint8_t pad3[0xC];
	float z;
} matrix3x4_t;

int m_iHealth;
int m_iTeamNum;
int m_iViewAngles;
int m_iCameraAngles;
int m_bZooming;
int m_iBoneMatrix;
int m_iWeapon;
int m_vecAbsOrigin;
int m_playerData;
int m_lifeState;

QWORD GetClientEntity(rx_handle game_process, QWORD entity, QWORD index)
{

	index = index + 1;
	index = index << 0x5;

	return rx_read_i64(game_process, (index + entity) - 0x280050);
}

QWORD get_interface_function(rx_handle game_process, QWORD ptr, DWORD index)
{
	return rx_read_i64(game_process, rx_read_i64(game_process, ptr) + index * 8);
}

vec3 GetBonePosition(rx_handle game_process, QWORD entity_address, int index)
{
	vec3 position;
	rx_read_process(game_process, entity_address + m_vecAbsOrigin, &position, sizeof(position));

	QWORD bonematrix = rx_read_i64(game_process, entity_address + m_iBoneMatrix);

	matrix3x4_t matrix;
	rx_read_process(game_process, bonematrix + (0x30 * index), &matrix, sizeof(matrix3x4_t));

	vec3 bonepos;
	bonepos.x = matrix.x + position.x;
	bonepos.y = matrix.y + position.y;
	bonepos.z = matrix.z + position.z;

	return bonepos;
}

BOOL IsButtonDown(rx_handle game_process, QWORD IInputSystem, int KeyCode)
{
	KeyCode = KeyCode + 1;
	DWORD a0 = rx_read_i32(game_process, IInputSystem + ((KeyCode >> 5) * 4) + 0xb0);
	return (a0 >> (KeyCode & 31)) & 1;
}

int dump_table(rx_handle game_process, QWORD table, const char *name)
{

	for (DWORD i = 0; i < rx_read_i32(game_process, table + 0x10); i++)
	{

		QWORD recv_prop = rx_read_i64(game_process, table + 0x8);
		if (!recv_prop)
		{
			continue;
		}

		recv_prop = rx_read_i64(game_process, recv_prop + 0x8 * i);
		char recv_prop_name[260];
		{
			QWORD name_ptr = rx_read_i64(game_process, recv_prop + 0x28);
			rx_read_process(game_process, name_ptr, recv_prop_name, 260);
		}

		if (!strcmp(recv_prop_name, name))
		{
			return rx_read_i32(game_process, recv_prop + 0x4);
		}
	}

	return 0;
}
