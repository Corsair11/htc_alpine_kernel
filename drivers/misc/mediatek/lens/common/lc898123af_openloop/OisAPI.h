/**
 * @brief		OIS system header for LC898123
 * 				API List for customers
 *
 * @author		Copyright (C) 2015, ON Semiconductor, all right reserved.
 *
 * @file		OisAPI.h
 * @date		svn:$Date:: 2016-08-04 10:55:02 +0900#$
 * @version		svn:$Revision: 65 $
 * @attention
 **/
#ifndef OISAPI_H_
#define OISAPI_H_
#include	"MeasurementLibrary.h"

#ifdef	__OISCMD__
	#define	__OIS_CMD_HEADER__
#else
	#define	__OIS_CMD_HEADER__		extern
#endif

#ifdef	__OISFLSH__
	#define	__OIS_FLSH_HEADER__
#else
	#define	__OIS_FLSH_HEADER__		extern
#endif

#define			__OIS_MODULE_CALIBRATION__		


typedef struct STRECALIB {
	INT16	SsFctryOffX ;
	INT16	SsFctryOffY ;
	INT16	SsRecalOffX ;
	INT16	SsRecalOffY ;
	INT16	SsDiffX ;
	INT16	SsDiffY ;
} stReCalib ;

__OIS_CMD_HEADER__	UINT8	RdStatus( UINT8 ) ;						
__OIS_CMD_HEADER__	void	OisEna( void ) ;						
__OIS_CMD_HEADER__	void	OisDis( void ) ;						

__OIS_CMD_HEADER__	UINT8	RtnCen( UINT8 ) ;						
__OIS_CMD_HEADER__	void	OisEnaNCL( void ) ;						
__OIS_CMD_HEADER__	void	OisEnaDrCl( void ) ;					
__OIS_CMD_HEADER__	void	OisEnaDrNcl( void ) ;					
__OIS_CMD_HEADER__	void	SetRec( void ) ;						
__OIS_CMD_HEADER__	void	SetStill( void ) ;						

__OIS_CMD_HEADER__	void	SetPanTiltMode( UINT8 ) ;				

__OIS_CMD_HEADER__	UINT8	RunHea( void ) ;						
__OIS_CMD_HEADER__	UINT8	RunGea( void ) ;						
__OIS_CMD_HEADER__	UINT8	RunGea2( UINT8 ) ;						


__OIS_CMD_HEADER__	void	OscStb( void );							
__OIS_CMD_HEADER__	UINT8	GyroReCalib( stReCalib * ) ;			
__OIS_CMD_HEADER__	UINT32	ReadCalibID( void ) ;					

#ifdef	__OIS_MODULE_CALIBRATION__

 
 #ifdef	__OIS_CLOSED_AF__
 __OIS_CMD_HEADER__	UINT32	TneRunA( void ) ;						
 __OIS_CMD_HEADER__	UINT32	AFHallAmp( void ) ;

 #else
 __OIS_CMD_HEADER__	UINT32	TneRun( void );							
 #endif

 __OIS_CMD_HEADER__	void	TneSltPos( UINT8 ) ;					
 __OIS_CMD_HEADER__	void	TneVrtPos( UINT8 ) ;					
 __OIS_CMD_HEADER__ void	TneHrzPos( UINT8 ) ;					
 __OIS_CMD_HEADER__ UINT16	TneADO( void ) ;						
 __OIS_CMD_HEADER__	UINT8	FrqDet( void ) ;						
 

 __OIS_CMD_HEADER__	UINT8	WrHallCalData( void ) ;					
 __OIS_CMD_HEADER__	UINT8	WrGyroGainData( void ) ;				
 __OIS_CMD_HEADER__	UINT8	WrGyroAngleData( void ) ;				
 __OIS_CMD_HEADER__	UINT8	WrCLAFData( void ) ;					
 __OIS_CMD_HEADER__	UINT8	WrMixingData( void ) ;					
 __OIS_CMD_HEADER__	UINT8	WrFstData( void ) ;						
 __OIS_CMD_HEADER__	UINT8	WrMixCalData( UINT8, mlMixingValue * ) ;
 __OIS_CMD_HEADER__	UINT8	WrGyroOffsetData( void ) ;

 #ifdef	HF_LINEAR_ENA
 #endif	

 #ifdef	HF_MIXING_ENA
 __OIS_CMD_HEADER__	INT8	WrMixCalData( UINT8, mlMixingValue * ) ;
 #endif	

 __OIS_CMD_HEADER__	UINT8	WrLinCalData( UINT8, mlLinearityValue * ) ;
 __OIS_CMD_HEADER__	UINT8	ErCalData( UINT16 ) ;

 
 __OIS_FLSH_HEADER__	UINT8	ReadWPB( void ) ;						
 __OIS_FLSH_HEADER__	UINT8	UnlockCodeSet( void ) ;					
 __OIS_FLSH_HEADER__	UINT8	UnlockCodeClear(void) ;					
 __OIS_FLSH_HEADER__	void	FlashByteRead( UINT32, UINT8 *, UINT8 ) ;
 __OIS_FLSH_HEADER__	void	FlashSectorRead( UINT32, UINT8 * ) ;
 __OIS_FLSH_HEADER__	UINT8	FlashInt32Write( UINT32, UINT32 *, UINT8 ) ;

 __OIS_FLSH_HEADER__	UINT8	FlashBlockErase( UINT32 ) ;
 __OIS_FLSH_HEADER__	UINT8	FlashSectorErase( UINT32 ) ;
 __OIS_FLSH_HEADER__	UINT8	FlashSectorWrite( UINT32, UINT8 * ) ;
 __OIS_FLSH_HEADER__	UINT8	FlashProtectStatus( void ) ;

 __OIS_FLSH_HEADER__	UINT8	FlashUpdateF40( void ) ;				
 __OIS_FLSH_HEADER__	UINT8	FlashUpdateF40ex( UINT8 );				
 __OIS_FLSH_HEADER__	UINT8	EraseCalDataF40( void ) ;				
 __OIS_FLSH_HEADER__	void	ReadCalDataF40( UINT32 *, UINT32 * ) ;	
 __OIS_FLSH_HEADER__	UINT8	WriteCalDataF40( UINT32 *, UINT32 * ) ;	
 __OIS_FLSH_HEADER__	void	CalcChecksum( const UINT8 *, UINT32, UINT32 *, UINT32 * ) ;

 
 __OIS_FLSH_HEADER__	UINT8	FlashSectorRead_Burst( UINT32, UINT8 *, UINT8 ) ;
 __OIS_FLSH_HEADER__	UINT8	FlashSectorWrite_Burst( UINT32, UINT8 *, UINT8 ) ;
 
 __OIS_FLSH_HEADER__	void	FlashSectorRead_htc( UINT32, UINT8 * ) ;
 

#endif	

#endif 
