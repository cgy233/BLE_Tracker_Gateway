#ifndef __msg__header__h__
#define __msg__header__h__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#define MSG_MAX_COUNT 	12
#define MSG_MAX_BUFFER 	40

#define LS_PACK_HEADER 0x7f
#define LS_ERROR_HEADER 0xff
#define LS_COMM_SESSION 0x01//设置session 0x1
#define LS_COMM_TIME 0x02//更新时间 2
#define LS_COMM_PWD 0x03//密码操作 增加 删除 修改 3
#define LS_COMM_OPEN 0x04//开门 4
#define LS_COMM_CHIPKEY 0x05//配置密钥 5
#define LS_COMM_DEVICE 0x06//读取设备信息 6
#define LS_COMM_DEVICE_ID 0x07//设置设备ID 7
#define LS_COMM_OPENLOG 0x08//开门记录 8
#define LS_COMM_HEARBEAT 0x09//心跳 3s  9
#define LS_COMM_MSG 0x0a//获取MSG a
#define LS_COMM_OFFLINE 0x0b//设置离线密码编号+开关 b
#define LS_COMM_PINCODE 0x0c//配对 c
#define LS_COMM_BATT_VALUE 0x0d //d
#define LS_COMM_UPDATE 0x0e//升级 e
#define LS_COMM_PROJECT 0x0f//工程版本 f
#define LS_COMM_CLEAR 0x10//清空 10
#define LS_COMM_PWD_VALID 0x11//密码操作 增加 删除 修改--生效有效期 11
#define LS_COMM_USER_VALID 0x12//密码操作 增加 删除 修改--生效有效期 12
#define LS_COMM_OPENLOG_BIGDATA 0x13//开门记录 13
#define LS_COMM_MAX_GET_MCUID 0x71//获设备MCU ID

#define LS_COMM_SUCCESS 0x00
#define LS_COMM_FAILED 0xff

// LOCK OPERATE CMD
#define XV_GW_OPEN_DOOR 0 // Remote open door
#define XV_GW_ISSUE_PASSWD_1 1 // Password Issue 
#define XV_GW_ISSUE_PASSWD_2 2 // tmp password
#define XV_GW_ISSUE_CARD 3 // Issue Card numer
#define XV_GW_DELETE_PASSWD_1 4 // Password Issue 
#define XV_GW_DELETE_CARD 5 // Issue Card numer
#define XV_GW_OPEN_LOG 9


// 同步类型
 #define SYNCH_LOCK_OPEN 1
 #define SYNCH_USER_PASSWD 20010
 #define SYNCH_USER_CARD 20012
 #define SYNCH_USER_FINGER 20011
//-----------------------------------------------------------------------
#pragma pack(1)
typedef struct {
	// command
	char			command;
	// length
	char			sn[15];
	// respone
	char			respone;
	// void set(char cmd, char* s, char rs){
	// 	command = cmd;
	// 	memset(sn, 0, 15];
	// 	strcpy(sn, s);
	// 	respone = rs;
	// }
} msg_base;
typedef struct msg_open{
	char	        command;
	// length
	char			sn[15];
	// respone
	char			respone;
	// cmd id
	unsigned int	command_id;
	// void set(char* s, char rs){
	// 	msg_base::set(0, s, rs);
	// }
} msg_open;
typedef struct{
	char			command;
	// length
	char			sn[15];
	// respone
	char			respone;
	// cmd id
	unsigned int	command_id;
	char			passwd[7];
	int				start_time;
	int				end_time;
	char			pos;
	
	// void set(char * s, char rs, char* p, int&st, int &et)
	// {
	// 	msg_base::set(1, s, rs);
	// 	memset(passwd, 0, 7);
	// 	strcpy(passwd, p);
	// 	start_time = st;
	// 	end_time = et;
	// }
}msg_passwd;
typedef struct{
	char			command;
	// length
	char			sn[15];
	// respone
	char			respone;
	// cmd id
	unsigned int	command_id;
	char			passwd[7];

	
	// void set(char * s, char rs, char *p)
	// {
	// 	msg_base::set(2, s, rs);
	// 	memset(passwd, 0, 7);
	// 	strcpy(passwd, p);
	// }
}msg_temp_passwd;

typedef struct {
	char			command;
	// length
	char			sn[15];
	// respone
	char			respone;
	// cmd id
	unsigned int	command_id;
	char			number[9];
	int				start_time;
	int				end_time;
	char			pos;

	
	// void set(char * s, char rs, char *n, int&st, int &et)
	// {
	// 	msg_base::set(3, s, rs);
	// 	memset(number, 0, 9);
	// 	strcpy(number, n);
	// 	start_time = st;
	// 	end_time = et;
	// }
}msg_card;
typedef struct {
	char			command;
	// length
	char			sn[15];
	// respone
	char			respone;
	// cmd id
	unsigned int	command_id;
	char			pos;
	char			type;
	
	// void set(char * s, char rs, char p, char t)
	// {
	// 	msg_base::set(4, s, rs);
	// 	pos = p;
	// 	type = t;
	// }
}msg_delete;
#pragma pack()
// end
#endif