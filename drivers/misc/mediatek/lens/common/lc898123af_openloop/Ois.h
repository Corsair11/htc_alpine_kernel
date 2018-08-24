/**
 * @brief		OIS system common header for LC898123 F40
 * 				Defines, Structures, function prototypes
 *
 * @author		Copyright (C) 2016, ON Semiconductor, all right reserved.
 *
 * @file		Ois.h
 * @date		svn:$Date:: 2016-10-06 13:20:23 +0900#$
 * @version	svn:$Revision: 85 $
 * @attention
 **/
#ifndef OIS_H_
#define OIS_H_

#if 0
typedef	char				 INT8;
typedef	short				 INT16;
typedef	long                 INT32;
typedef	long long            INT64;
typedef	unsigned char       UINT8;
typedef	unsigned short      UINT16;
typedef	unsigned long       UINT32;
typedef	unsigned long long	UINT64;
#else
typedef	char				 INT8;
typedef	signed short	 INT16;
typedef	long                 INT32;
typedef	long long            INT64;
typedef	unsigned char       UINT8;
typedef	unsigned short      UINT16;
typedef	unsigned int       UINT32;
typedef	unsigned long long	UINT64;
#endif

#if 0
#define		USE_FRA			
#else
#endif

#include	"OisAPI.h"
#include	"OisLc898123F40.h"

 #define	MDL_VER			0x05
 #define	FW_VER			0x0D

#define		NEUTRAL_CENTER				
#define		NEUTRAL_CENTER_FINE			

#define	FS_FREQ			20019.53125F

#define	GYRO_SENSITIVITY	87.5		

#define		EXE_END		0x00000002L		
#define		EXE_HXADJ	0x00000006L		
#define		EXE_HYADJ	0x0000000AL		
#define		EXE_LXADJ	0x00000012L		
#define		EXE_LYADJ	0x00000022L		
#define		EXE_GXADJ	0x00000042L		
#define		EXE_GYADJ	0x00000082L		
#define		EXE_ERR		0x00000099L		
#ifdef	SEL_CLOSED_AF
 #define	EXE_HZADJ	0x00100002L		
 #define	EXE_LZADJ	0x00200002L		
#endif


#define	SUCCESS			0x00			
#define	FAILURE			0x01			

#ifndef ON
 #define	ON				0x01		
 #define	OFF				0x00		
#endif
 #define	SPC				0x02		

#define	X_DIR			0x00			
#define	Y_DIR			0x01			
#define	Z_DIR			0x02			

#define	WPB_OFF			0x01			
#define WPB_ON			0x00			

#define		SXGAIN_LOP		0x30000000
#define		SYGAIN_LOP		0x30000000
#define		XY_BIAS			0x40000000
#define		XY_OFST			0x00000303	
#ifdef	SEL_CLOSED_AF
#define		SZGAIN_LOP		0x30000000
#define		Z_BIAS			0x40000000
#define		Z_OFST			0x00030000	
#endif

struct STFILREG {						
	UINT16	UsRegAdd ;
	UINT8	UcRegDat ;
} ;

struct STFILRAM {						
	UINT16	UsRamAdd ;
	UINT32	UlRamDat ;
} ;

struct STCMDTBL {						
	UINT16 Cmd ;
	UINT32 UiCmdStf ;
	void ( *UcCmdPtr )( void ) ;
} ;

#define		CMD_IO_ADR_ACCESS				0xC000				
#define		CMD_IO_DAT_ACCESS				0xD000				
#define		CMD_RETURN_TO_CENTER			0xF010				
	#define		BOTH_SRV_OFF					0x00000000			
	#define		XAXS_SRV_ON						0x00000001			
	#define		YAXS_SRV_ON						0x00000002			
	#define		BOTH_SRV_ON						0x00000003			
	#define		ZAXS_SRV_OFF					0x00000004			
	#define		ZAXS_SRV_ON						0x00000005			
#define		CMD_PAN_TILT					0xF011				
	#define		PAN_TILT_OFF					0x00000000			
	#define		PAN_TILT_ON						0x00000001			
#define		CMD_OIS_ENABLE					0xF012				
	#define		OIS_DISABLE						0x00000000			
	#define		OIS_ENABLE						0x00000001			
	#define		OIS_ENA_NCL						0x00000002			
	#define		OIS_ENA_DOF						0x00000004			
#define		CMD_MOVE_STILL_MODE				0xF013				
	#define		MOVIE_MODE						0x00000000			
	#define		STILL_MODE						0x00000001			
#define		CMD_CALIBRATION					0xF014				
#define		CMD_CHASE_CONFIRMATION			0xF015				
#define		CMD_GYRO_SIG_CONFIRMATION		0xF016				
	
	#define		HALL_CALB_FLG					0x00008000			
	#define		HALL_CALB_BIT					0x00FF00FF			
	#define		GYRO_GAIN_FLG					0x00004000			
	#define		ANGL_CORR_FLG					0x00002000			
	#define		OPAF_FST_FLG					0x00001000			
	#define		CLAF_CALB_FLG					0x00000800			
	#define		HLLN_CALB_FLG					0x00000400			
	#define		CROS_TALK_FLG					0x00000200			
#define		CMD_READ_STATUS					0xF100				

#define		READ_STATUS_INI					0x01000000

#define		STBOSCPLL						0x00D00074			
	#define		OSC_STB							0x00000002			

#define	HLXBO				0x00000001			
#define	HLYBO				0x00000002			
#define	HLAFBO				0x00000004			
#define	HLAFO				0x00000008			

typedef struct {
	INT32				SiSampleNum ;			
	INT32				SiSampleMax ;			

	struct {
		INT32			SiMax1 ;				
		INT32			SiMin1 ;				
		UINT32	UiAmp1 ;						
		INT64		LLiIntegral1 ;				
		INT64		LLiAbsInteg1 ;				
		INT32			PiMeasureRam1 ;			
	} MeasureFilterA ;

	struct {
		INT32			SiMax2 ;				
		INT32			SiMin2 ;				
		UINT32	UiAmp2 ;						
		INT64		LLiIntegral2 ;				
		INT64		LLiAbsInteg2 ;				
		INT32			PiMeasureRam2 ;			
	} MeasureFilterB ;
} MeasureFunction_Type ;



#ifdef _BIG_ENDIAN_
union	WRDVAL{
	INT16	SsWrdVal ;
	UINT16	UsWrdVal ;
	UINT8	UcWrkVal[ 2 ] ;
	INT8	ScWrkVal[ 2 ] ;
	struct {
		UINT8	UcHigVal ;
		UINT8	UcLowVal ;
	} StWrdVal ;
} ;


union	DWDVAL {
	UINT32	UlDwdVal ;
	UINT16	UsDwdVal[ 2 ] ;
	struct {
		UINT16	UsHigVal ;
		UINT16	UsLowVal ;
	} StDwdVal ;
	struct {
		UINT8	UcRamVa3 ;
		UINT8	UcRamVa2 ;
		UINT8	UcRamVa1 ;
		UINT8	UcRamVa0 ;
	} StCdwVal ;
} ;

union	ULLNVAL {
	UINT64	UllnValue ;
	UINT32	UlnValue[ 2 ] ;
	struct {
		UINT32	UlHigVal ;
		UINT32	UlLowVal ;
	} StUllnVal ;
} ;


union	FLTVAL {
	float	SfFltVal ;
	UINT32	UlLngVal ;
	UINT16	UsDwdVal[ 2 ] ;
	struct {
		UINT16	UsHigVal ;
		UINT16	UsLowVal ;
	} StFltVal ;
} ;

#else	
union	WRDVAL{
	UINT16	UsWrdVal ;
	UINT8	UcWrkVal[ 2 ] ;
	struct {
		UINT8	UcLowVal ;
		UINT8	UcHigVal ;
	} StWrdVal ;
} ;

typedef union WRDVAL	UnWrdVal ;

union	DWDVAL {
	UINT32	UlDwdVal ;
	UINT16	UsDwdVal[ 2 ] ;
	struct {
		UINT16	UsLowVal ;
		UINT16	UsHigVal ;
	} StDwdVal ;
	struct {
		UINT8	UcRamVa0 ;
		UINT8	UcRamVa1 ;
		UINT8	UcRamVa2 ;
		UINT8	UcRamVa3 ;
	} StCdwVal ;
} ;

typedef union DWDVAL	UnDwdVal;

union	ULLNVAL {
	UINT64	UllnValue ;
	UINT32	UlnValue[ 2 ] ;
	struct {
		UINT32	UlLowVal ;
		UINT32	UlHigVal ;
	} StUllnVal ;
} ;

typedef union ULLNVAL	UnllnVal;


union	FLTVAL {
	float	SfFltVal ;
	UINT32	UlLngVal ;
	UINT16	UsDwdVal[ 2 ] ;
	struct {
		UINT16	UsLowVal ;
		UINT16	UsHigVal ;
	} StFltVal ;
} ;
#endif	

typedef union WRDVAL	UnWrdVal ;
typedef union DWDVAL	UnDwdVal;
typedef union ULLNVAL	UnllnVal;
typedef union FLTVAL	UnFltVal ;


typedef struct STADJPAR {
	struct {
		UINT32	UlAdjPhs ;				

		UINT16	UsHlxCna ;				
		UINT16	UsHlxMax ;				
		UINT16	UsHlxMxa ;				
		UINT16	UsHlxMin ;				
		UINT16	UsHlxMna ;				
		UINT16	UsHlxGan ;				
		UINT16	UsHlxOff ;				
		UINT16	UsAdxOff ;				
		UINT16	UsHlxCen ;				

		UINT16	UsHlyCna ;				
		UINT16	UsHlyMax ;				
		UINT16	UsHlyMxa ;				
		UINT16	UsHlyMin ;				
		UINT16	UsHlyMna ;				
		UINT16	UsHlyGan ;				
		UINT16	UsHlyOff ;				
		UINT16	UsAdyOff ;				
		UINT16	UsHlyCen ;				

#ifdef	SEL_CLOSED_AF
		UINT16	UsHlzCna ;				
		UINT16	UsHlzMax ;				
		UINT16	UsHlzMxa ;				
		UINT16	UsHlzMin ;				
		UINT16	UsHlzMna ;				
		UINT16	UsHlzGan ;				
		UINT16	UsHlzOff ;				
		UINT16	UsAdzOff ;				
		UINT16	UsHlzCen ;				
		UINT16	UsHlzAmp ;				
#endif
	} StHalAdj ;

	struct {
		UINT32	UlLxgVal ;				
		UINT32	UlLygVal ;				
#ifdef	SEL_CLOSED_AF
		UINT32	UlLzgVal ;				
#endif
	} StLopGan ;

	struct {
		UINT16	UsGxoVal ;				
		UINT16	UsGyoVal ;				
		UINT16	UsGxoSts ;				
		UINT16	UsGyoSts ;				
	} StGvcOff ;
} stAdjPar ;

__OIS_CMD_HEADER__	stAdjPar	StAdjPar ;		

typedef struct STHALLINEAR {
	UINT16	XCoefA[6] ;
	UINT16	XCoefB[6] ;
	UINT16	XZone[5] ;
	UINT16	YCoefA[6] ;
	UINT16	YCoefB[6] ;
	UINT16	YZone[5] ;
} stHalLinear ;

#define		BOTH_ON			0x00
#define		XONLY_ON		0x01
#define		YONLY_ON		0x02
#define		BOTH_OFF		0x03
#define		ZONLY_OFF		0x04
#define		ZONLY_ON		0x05
#define		SINEWAVE		0
#define		XHALWAVE		1
#define		YHALWAVE		2
#define		ZHALWAVE		3
#define		XACTTEST		10
#define		YACTTEST		11
#define		CIRCWAVE		255
#define		HALL_H_VAL		0x3F800000		
#define		OFFDAC_8BIT		0				
#define		OFFDAC_3BIT		1				
#define		PTP_BEFORE		0
#define		PTP_AFTER		1
#define		PTP_ACCEPT		2
#define		ACT_CHK_LVL		0x33320000		
#define		ACT_CHK_FRQ		0x00068C16		
#define		ACT_CHK_NUM		5005			
#define		ACT_THR			0x0A3D0000		
#define		GEA_DIF_HIG		0x0057			
#define		GEA_DIF_LOW		0x0001
#define		GEA_MAX_LVL		0x0A41			
#define		GEA_MIN_LVL		0x1482			
#define		GEA_MINMAX_MODE	0x00			
#define		GEA_MEAN_MODE	0x01			

#ifdef		USE_FRA
 #include	"OisFRA.h"
#endif

#endif 
