/**
 * @brief		OIS system command for LC898123 F40
 *
 * @author		Copyright (C) 2016, ON Semiconductor, all right reserved.
 *
 * @file		OisCmd.c
 * @date		svn:$Date:: 2016-09-13 13:11:32 +0900#$
 * @version	svn:$Revision: 73 $
 * @attention
 **/

#define		__OISCMD__

#if 0
#include	<stdlib.h>	
#include	<math.h>	
#else
#include <linux/kernel.h>
#endif
#include	"Ois.h"


#if 0
extern	void RamWrite32A(INT32 addr, INT32 data);
#else
extern	void RamWrite32A(UINT16 addr, UINT32 data);
#endif
extern 	void RamRead32A( UINT16 addr, void * data );
extern void	WitTim( UINT16 );

UINT32	UlBufDat[ 64 ] ;							

void	IniCmd( void ) ;							
void	IniPtAve( void ) ;							
void	MesFil( UINT8 ) ;							
void	MeasureStart( INT32 , INT32 , INT32 ) ;		
void	MeasureWait( void ) ;						
void	MemoryClear( UINT16 , UINT16 ) ;			
void	SetWaitTime( UINT16 ) ; 					

UINT32	TneOff( UnDwdVal, UINT8 ) ;					
UINT32	TneOff_3Bit( UnDwdVal, UINT8 ) ;			
UINT32	TneBia( UnDwdVal, UINT8 ) ;					
UINT32	TnePtp ( UINT8	UcDirSel, UINT8	UcBfrAft );
UINT32	TneCen( UINT8	UcTneAxs, UnDwdVal	StTneVal, UINT8 UcDacSel);
UINT32	LopGan( UINT8	UcDirSel );
UINT32	TneGvc( void );
UINT8	TneHvc( void );
void	DacControl( UINT8 UcMode, UINT32 UiChannel, UINT32 PuiData );

#ifdef	NEUTRAL_CENTER_FINE
void	TneFin( void ) ;							
#endif	

void	RdHallCalData( void ) ;
void	SetSinWavePara( UINT8 , UINT8 ) ;			
void	SetSineWave( UINT8 , UINT8 );
void	SetSinWavGenInt( void );
void	SetTransDataAdr( UINT16  , UINT32  ) ;		


#define 	MES_XG1			0						
#define 	MES_XG2			1						

#define 	HALL_ADJ		0
#define 	LOOPGAIN		1
#define 	THROUGH			2
#define 	NOISE			3
#define		OSCCHK			4
#define		GAINCURV		5
#define		SELFTEST		6


 #define 	TNE 			80						

 

 #define 	OFFSET_DIV		2						
 #define 	OFFSET_DIV_3BIT	0x2000					
 #define 	TIME_OUT		40						

 #define	BIAS_HLMT		(UINT32)0xBF000000
 #define	BIAS_LLMT		(UINT32)0x20000000

 
 #define 	MARGIN			0x0300					

 #define 	BIAS_ADJ_RANGE_XY	0x9999				

 #define 	HALL_MAX_RANGE_XY	BIAS_ADJ_RANGE_XY + MARGIN
 #define 	HALL_MIN_RANGE_XY	BIAS_ADJ_RANGE_XY - MARGIN


#ifdef	SEL_CLOSED_AF
 #define	BIAS_ADJ_RANGE_Z	0xAFDE				
 #define 	HALL_MAX_RANGE_Z	BIAS_ADJ_RANGE_Z + MARGIN
 #define 	HALL_MIN_RANGE_Z	BIAS_ADJ_RANGE_Z - MARGIN
#endif

 #define 	DECRE_CAL		0x0100					



#define		SLT_OFFSET		(0x0AB0)
#define		LENS_MARGIN		(0x0800)
#define		PIXEL_SIZE		(1.00f)							
#define		SPEC_RANGE		(81.6f)							
#define		SPEC_PIXEL		(PIXEL_SIZE / SPEC_RANGE)		
#define ULTHDVAL	0x01000000								

INT16		SsNvcX = 1 ;									
INT16		SsNvcY = 1 ;									

const UINT8	UcDacArray[ 7 ]	= {
	0x06,
	0x05,
	0x04,
	0x03,
	0x02,
	0x01,
	0x00
} ;

void	MemClr( UINT8	*NcTgtPtr, UINT16	UsClrSiz )
{
	UINT16	UsClrIdx ;

	for ( UsClrIdx = 0 ; UsClrIdx < UsClrSiz ; UsClrIdx++ )
	{
		*NcTgtPtr	= 0 ;
		NcTgtPtr++ ;
	}
}


UINT32	TneRun( void )
{
	UINT32	UlHlySts, UlHlxSts, UlAtxSts, UlAtySts, UlGvcSts ;
	UnDwdVal		StTneVal ;
	UINT32	UlFinSts, UlReadVal ;
	UINT32	UlCurDac ;

	RtnCen( BOTH_OFF ) ;		
	WitTim( TNE ) ;

	RamWrite32A( HALL_RAM_HXOFF,  0x00000000 ) ;			
	RamWrite32A( HALL_RAM_HYOFF,  0x00000000 ) ;			
	RamWrite32A( HallFilterCoeffX_hxgain0 , SXGAIN_LOP ) ;
	RamWrite32A( HallFilterCoeffY_hygain0 , SYGAIN_LOP ) ;
	DacControl( 0 , HLXBO , XY_BIAS ) ;
	RamWrite32A( StCaliData_UiHallBias_X , XY_BIAS ) ;
	DacControl( 0 , HLYBO , XY_BIAS ) ;
	RamWrite32A( StCaliData_UiHallBias_Y , XY_BIAS ) ;

	RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
	RamRead32A(  CMD_IO_DAT_ACCESS , &UlCurDac ) ;			
	UlCurDac = (UlCurDac & 0x00070000) | XY_OFST;
	RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS , UlCurDac ) ;			

	
	StTneVal.UlDwdVal	= TnePtp( Y_DIR , PTP_BEFORE ) ;
	UlHlySts	= TneCen( Y_DIR, StTneVal, OFFDAC_3BIT ) ;
	RtnCen( YONLY_ON ) ;		
	WitTim( TNE ) ;

	
	StTneVal.UlDwdVal	= TnePtp( X_DIR , PTP_BEFORE ) ;
	UlHlxSts	= TneCen( X_DIR, StTneVal, OFFDAC_3BIT ) ;
	RtnCen( XONLY_ON ) ;		
	WitTim( TNE ) ;

	
	StTneVal.UlDwdVal	= TnePtp( Y_DIR , PTP_AFTER ) ;
	UlHlySts	= TneCen( Y_DIR, StTneVal, OFFDAC_3BIT ) ;
	RtnCen( YONLY_ON ) ;		
	WitTim( TNE ) ;

	
	StTneVal.UlDwdVal	= TnePtp( X_DIR , PTP_AFTER ) ;
	UlHlxSts	= TneCen( X_DIR, StTneVal, OFFDAC_3BIT ) ;
	RtnCen( BOTH_OFF ) ;		

#ifdef	NEUTRAL_CENTER
	TneHvc();
#endif	

	WitTim( TNE ) ;

	StAdjPar.StHalAdj.UsAdxOff = StAdjPar.StHalAdj.UsHlxCna  ;
	StAdjPar.StHalAdj.UsAdyOff = StAdjPar.StHalAdj.UsHlyCna  ;

	RamWrite32A( HALL_RAM_HXOFF,  (UINT32)((StAdjPar.StHalAdj.UsAdxOff << 16 ) & 0xFFFF0000 )) ;
	RamWrite32A( HALL_RAM_HYOFF,  (UINT32)((StAdjPar.StHalAdj.UsAdyOff << 16 ) & 0xFFFF0000 )) ;

	RamRead32A( StCaliData_UiHallOffset_X , &UlReadVal ) ;
	StAdjPar.StHalAdj.UsHlxOff = (UINT16)( UlReadVal ) ;

	RamRead32A( StCaliData_UiHallBias_X , &UlReadVal ) ;
	StAdjPar.StHalAdj.UsHlxGan = (UINT16)( UlReadVal >> 16 ) ;

	RamRead32A( StCaliData_UiHallOffset_Y , &UlReadVal ) ;
	StAdjPar.StHalAdj.UsHlyOff = (UINT16)( UlReadVal ) ;

	RamRead32A( StCaliData_UiHallBias_Y , &UlReadVal ) ;
	StAdjPar.StHalAdj.UsHlyGan = (UINT16)( UlReadVal >> 16 ) ;

#ifdef	NEUTRAL_CENTER_FINE
		TneFin();

		RamWrite32A( HALL_RAM_HXOFF,  (UINT32)((StAdjPar.StHalAdj.UsAdxOff << 16 ) & 0xFFFF0000 )) ;
		RamWrite32A( HALL_RAM_HYOFF,  (UINT32)((StAdjPar.StHalAdj.UsAdyOff << 16 ) & 0xFFFF0000 )) ;
#endif	

	RtnCen( BOTH_ON ) ;					
	WitTim( TNE ) ;

	UlAtxSts	= LopGan( X_DIR ) ;		
	UlAtySts	= LopGan( Y_DIR ) ;		


#if 0
	UlGvcSts = 0 ;
	UlFinSts = (UINT32)GyrSlf() ;
	if( UlFinSts & 0xF0 )				
		UlGvcSts |= EXE_GXADJ ;
	if( UlFinSts & 0x0F )				
		UlGvcSts |= EXE_GYADJ ;

	UlGvcSts |= TneGvc() ;
#else
	UlGvcSts = TneGvc() ;
#endif

	UlFinSts	= ( UlHlxSts - EXE_END ) + ( UlHlySts - EXE_END ) + ( UlAtxSts - EXE_END ) + ( UlAtySts - EXE_END )  + ( UlGvcSts - EXE_END ) + EXE_END ;
	StAdjPar.StHalAdj.UlAdjPhs = UlFinSts ;

	return( UlFinSts ) ;
}


#ifdef	SEL_CLOSED_AF
UINT32	TneRunZ( void )
{
	UINT32	UlHlzSts, UlAtzSts ;
	UnDwdVal		StTneVal ;
	UINT32	UlFinSts , UlReadVal ;
	UINT32	UlCurDac ;

	RtnCen( ZONLY_OFF ) ;
	WitTim( TNE ) ;


	RamWrite32A( CLAF_RAMA_AFOFFSET,  0x00000000 ) ;		
	RamWrite32A( CLAF_Gain_afloop2 , SZGAIN_LOP ) ;
	DacControl( 0 , HLAFBO , Z_BIAS ) ;
	RamWrite32A( StCaliData_UiHallBias_AF , Z_BIAS) ;
	DacControl( 0 , HLAFO, Z_OFST ) ;
	RamWrite32A( StCaliData_UiHallOffset_AF , Z_OFST ) ;

	RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
	RamRead32A(  CMD_IO_DAT_ACCESS , &UlCurDac ) ;			
	UlCurDac = (UlCurDac & 0x00000707) | Z_OFST;
	RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS , UlCurDac ) ;			

	
	StTneVal.UlDwdVal	= TnePtp( Z_DIR , PTP_BEFORE ) ;
	UlHlzSts	= TneCen( Z_DIR, StTneVal, OFFDAC_8BIT ) ;

	WitTim( TNE ) ;

	UlReadVal = 0x00010000 - (UINT32)StAdjPar.StHalAdj.UsHlzCna ;
	StAdjPar.StHalAdj.UsAdzOff = (UINT16)UlReadVal ;

	RamWrite32A( CLAF_RAMA_AFOFFSET,  (UINT32)((StAdjPar.StHalAdj.UsAdzOff << 16 ) & 0xFFFF0000 )) ;

	RamRead32A( StCaliData_UiHallOffset_AF , &UlReadVal ) ;
	StAdjPar.StHalAdj.UsHlzOff = (UINT16)( UlReadVal >> 16 ) ;

	RamRead32A( StCaliData_UiHallBias_AF , &UlReadVal ) ;
	StAdjPar.StHalAdj.UsHlzGan = (UINT16)( UlReadVal >> 16 ) ;

	RtnCen( ZONLY_ON ) ;				

	WitTim( TNE ) ;

	UlAtzSts	= LopGan( Z_DIR ) ;		

	UlFinSts	= ( UlHlzSts - EXE_END ) + ( UlAtzSts - EXE_END ) + EXE_END ;
	StAdjPar.StHalAdj.UlAdjPhs = UlFinSts ;

	return( UlFinSts ) ;
}

UINT32	TneRunA( void )
{
	UINT32	UlFinSts ;

	UlFinSts = TneRunZ();
	UlFinSts |= TneRun();

	StAdjPar.StHalAdj.UlAdjPhs = UlFinSts ;
	return( UlFinSts ) ;
}

#endif


UINT32	TnePtp( UINT8	UcDirSel, UINT8	UcBfrAft )
{
	UnDwdVal		StTneVal ;
	INT32			SlMeasureParameterA = 0, SlMeasureParameterB = 0;
	INT32			SlMeasureParameterNum = 0;
	INT32			SlMeasureMaxValue, SlMeasureMinValue ;

	SlMeasureParameterNum	=	2000 ;		

	if( UcDirSel == X_DIR ) {								
		SlMeasureParameterA		=	HALL_RAM_HXIDAT ;		
		SlMeasureParameterB		=	HALL_RAM_HYIDAT ;		
	} else if( UcDirSel == Y_DIR ) {						
		SlMeasureParameterA		=	HALL_RAM_HYIDAT ;		
		SlMeasureParameterB		=	HALL_RAM_HXIDAT ;		
#ifdef	SEL_CLOSED_AF
	} else {												
		SlMeasureParameterA		=	CLAF_RAMA_AFADIN ;		
		SlMeasureParameterB		=	CLAF_RAMA_AFADIN ;		
#endif
	}
	SetSinWavGenInt();

	RamWrite32A( SinWave_Offset		,	0x105E36 ) ;									
	RamWrite32A( SinWave_Gain		,	0x7FFFFFFF ) ;									
	RamWrite32A( SinWaveC_Regsiter	,	0x00000001 ) ;									
	if( UcDirSel == X_DIR ) {
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)HALL_RAM_SINDX1 ) ;		
	}else if( UcDirSel == Y_DIR ){
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)HALL_RAM_SINDY1 ) ;		
#ifdef	SEL_CLOSED_AF
	}else{
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)CLAF_RAMA_AFOUT ) ;		
#endif
	}

	MesFil( THROUGH ) ;					

	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;	

	MeasureWait() ;						

	RamWrite32A( SinWaveC_Regsiter	,	0x00000000 ) ;									

	if( UcDirSel == X_DIR ) {
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)0x00000000 ) ;			
		RamWrite32A( HALL_RAM_SINDX1		,	0x00000000 ) ;							
	}else if( UcDirSel == Y_DIR ){
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)0x00000000 ) ;			
		RamWrite32A( HALL_RAM_SINDY1		,	0x00000000 ) ;							
#ifdef	SEL_CLOSED_AF
	}else{
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)0x00000000 ) ;			
		RamWrite32A( CLAF_RAMA_AFOUT		,	0x00000000 ) ;							
#endif
	}
	RamRead32A( StMeasFunc_MFA_SiMax1 , ( UINT32 * )&SlMeasureMaxValue ) ;		
	RamRead32A( StMeasFunc_MFA_SiMin1 , ( UINT32 * )&SlMeasureMinValue ) ;		

	StTneVal.StDwdVal.UsHigVal = (UINT16)((SlMeasureMaxValue >> 16) & 0x0000FFFF );
	StTneVal.StDwdVal.UsLowVal = (UINT16)((SlMeasureMinValue >> 16) & 0x0000FFFF );

	if( UcBfrAft == 0 ) {
		if( UcDirSel == X_DIR ) {
			StAdjPar.StHalAdj.UsHlxCen	= ( ( INT16 )StTneVal.StDwdVal.UsHigVal + ( INT16 )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlxMax	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlxMin	= StTneVal.StDwdVal.UsLowVal ;
		} else if( UcDirSel == Y_DIR ){
			StAdjPar.StHalAdj.UsHlyCen	= ( ( INT16 )StTneVal.StDwdVal.UsHigVal + ( INT16 )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlyMax	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlyMin	= StTneVal.StDwdVal.UsLowVal ;
#ifdef	SEL_CLOSED_AF
		} else {
			StAdjPar.StHalAdj.UsHlzCen	= ( ( INT16 )StTneVal.StDwdVal.UsHigVal + ( INT16 )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlzMax	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlzMin	= StTneVal.StDwdVal.UsLowVal ;
#endif
		}
	} else {
		if( UcDirSel == X_DIR ){
			StAdjPar.StHalAdj.UsHlxCna	= ( ( INT16 )StTneVal.StDwdVal.UsHigVal + ( INT16 )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlxMxa	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlxMna	= StTneVal.StDwdVal.UsLowVal ;
		} else if( UcDirSel == Y_DIR ){
			StAdjPar.StHalAdj.UsHlyCna	= ( ( INT16 )StTneVal.StDwdVal.UsHigVal + ( INT16 )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlyMxa	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlyMna	= StTneVal.StDwdVal.UsLowVal ;
#ifdef	SEL_CLOSED_AF
		} else {
			StAdjPar.StHalAdj.UsHlzCna	= ( ( INT16 )StTneVal.StDwdVal.UsHigVal + ( INT16 )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlzMxa	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlzMna	= StTneVal.StDwdVal.UsLowVal ;
#endif
		}
	}

	StTneVal.StDwdVal.UsHigVal	= 0x7fff - StTneVal.StDwdVal.UsHigVal ;		
	StTneVal.StDwdVal.UsLowVal	= StTneVal.StDwdVal.UsLowVal - 0x7fff ; 	


	return( StTneVal.UlDwdVal ) ;
}

UINT16	UsValBef,UsValNow ;
UINT32	TneCen( UINT8	UcTneAxs, UnDwdVal	StTneVal, UINT8 UcDacSel)
{
	UINT8 	UcTmeOut, UcTofRst ;
	UINT16	UsBiasVal = 0;
	UINT32	UlTneRst, UlBiasVal , UlValNow ;
	UINT16	UsHalMaxRange , UsHalMinRange ;

	UcTmeOut	= 1 ;
	UlTneRst	= FAILURE ;
	UcTofRst	= FAILURE ;

#ifdef	SEL_CLOSED_AF
	if(UcTneAxs == Z_DIR){
		UsHalMaxRange = HALL_MAX_RANGE_Z ;
		UsHalMinRange = HALL_MIN_RANGE_Z ;
	}else{
		UsHalMaxRange = HALL_MAX_RANGE_XY;
		UsHalMinRange = HALL_MIN_RANGE_XY;
	}
#else
	UsHalMaxRange = HALL_MAX_RANGE_XY ;
	UsHalMinRange = HALL_MIN_RANGE_XY ;
#endif

	if( UcDacSel == 1){				
		StTneVal.UlDwdVal	= TneOff_3Bit( StTneVal, UcTneAxs ) ;		
	}

	while ( UlTneRst && (UINT32)UcTmeOut )
	{
		if( UcTofRst == FAILURE ) {
			StTneVal.UlDwdVal	= TneBia( StTneVal, UcTneAxs ) ;	
		} else {
			StTneVal.UlDwdVal	= TneBia( StTneVal, UcTneAxs ) ;		
			UcTofRst	= FAILURE ;
		}

		if( (StTneVal.StDwdVal.UsHigVal > MARGIN ) && (StTneVal.StDwdVal.UsLowVal > MARGIN ) )	
		{
			UcTofRst	= SUCCESS ;
			UsValBef = UsValNow = 0x0000 ;
		}else if( (StTneVal.StDwdVal.UsHigVal <= MARGIN ) && (StTneVal.StDwdVal.UsLowVal <= MARGIN ) ){
			UcTofRst	= SUCCESS ;
			UlTneRst	= (UINT32)FAILURE ;
		}else{
			UcTofRst	= FAILURE ;

			UsValBef = UsValNow ;

			if( UcTneAxs == X_DIR  ) {
				RamRead32A( StCaliData_UiHallOffset_X , &UlValNow ) ;
				UsValNow = (UINT16)( UlValNow >> 16 ) ;
			}else if( UcTneAxs == Y_DIR ){
				RamRead32A( StCaliData_UiHallOffset_Y , &UlValNow ) ;
				UsValNow = (UINT16)( UlValNow >> 16 ) ;
#ifdef	SEL_CLOSED_AF
			}else{
				RamRead32A( StCaliData_UiHallOffset_AF , &UlValNow ) ;
				UsValNow = (UINT16)( UlValNow >> 16 ) ;
#endif
			}
			if( ((( UsValBef & 0xFF00 ) == 0x1000 ) && ( UsValNow & 0xFF00 ) == 0x1000 )
			 || ((( UsValBef & 0xFF00 ) == 0xEF00 ) && ( UsValNow & 0xFF00 ) == 0xEF00 ) )
			{
				if( UcTneAxs == X_DIR ) {
					RamRead32A( StCaliData_UiHallBias_X , &UlBiasVal ) ;
					UsBiasVal = (UINT16)( UlBiasVal >> 16 ) ;

				}else if( UcTneAxs == Y_DIR ){
					RamRead32A( StCaliData_UiHallBias_Y , &UlBiasVal ) ;
					UsBiasVal = (UINT16)( UlBiasVal >> 16 ) ;
#ifdef	SEL_CLOSED_AF
				}else{
					RamRead32A( StCaliData_UiHallBias_AF , &UlBiasVal ) ;
					UsBiasVal = (UINT16)( UlBiasVal >> 16 ) ;
#endif
				}

				if( UsBiasVal > DECRE_CAL )
				{
					UsBiasVal -= DECRE_CAL ;
				}

				if( UcTneAxs == X_DIR ) {
					UlBiasVal = ( UINT32 )( UsBiasVal << 16 ) ;
					DacControl( 0 , HLXBO , UlBiasVal ) ;
					RamWrite32A( StCaliData_UiHallBias_X , UlBiasVal ) ;
				}else if( UcTneAxs == Y_DIR ){
					UlBiasVal = ( UINT32 )( UsBiasVal << 16 ) ;
					DacControl( 0 , HLYBO , UlBiasVal ) ;
					RamWrite32A( StCaliData_UiHallBias_Y , UlBiasVal ) ;
#ifdef	SEL_CLOSED_AF
				}else{
					UlBiasVal = ( UINT32 )( UsBiasVal << 16 ) ;
					DacControl( 0 , HLAFBO , UlBiasVal ) ;
					RamWrite32A( StCaliData_UiHallBias_AF , UlBiasVal ) ;
#endif
				}
			}
		}

		if((( (UINT16)0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) < UsHalMaxRange )
		&& (( (UINT16)0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) > UsHalMinRange ) ) {
			if(UcTofRst	== SUCCESS)
			{
				UlTneRst	= (UINT32)SUCCESS ;
				break ;
			}
		}
		UlTneRst	= (UINT32)FAILURE ;
		UcTmeOut++ ;

		if ( ( UcTmeOut / 2 ) == TIME_OUT ) {		
			UcTmeOut	= 0 ;
		}
	}

	SetSinWavGenInt() ;		

	if( UlTneRst == (UINT32)FAILURE ) {
		if( UcTneAxs == X_DIR ) {
			UlTneRst					= EXE_HXADJ ;
			StAdjPar.StHalAdj.UsHlxGan	= 0xFFFF ;
			StAdjPar.StHalAdj.UsHlxOff	= 0xFFFF ;
		}else if( UcTneAxs == Y_DIR ) {
			UlTneRst					= EXE_HYADJ ;
			StAdjPar.StHalAdj.UsHlyGan	= 0xFFFF ;
			StAdjPar.StHalAdj.UsHlyOff	= 0xFFFF ;
#ifdef	SEL_CLOSED_AF
		} else {
			UlTneRst					= EXE_HZADJ ;
			StAdjPar.StHalAdj.UsHlzGan	= 0xFFFF ;
			StAdjPar.StHalAdj.UsHlzOff	= 0xFFFF ;
#endif
		}
	} else {
		UlTneRst	= EXE_END ;
	}

	return( UlTneRst ) ;
}

UINT32	TneBia( UnDwdVal	StTneVal, UINT8	UcTneAxs )
{
	UINT32			UlSetBia ;
	UINT16			UsHalAdjRange;
#ifdef	SEL_CLOSED_AF
	if(UcTneAxs == Z_DIR){
		UsHalAdjRange = BIAS_ADJ_RANGE_Z ;
	}else{
		UsHalAdjRange = BIAS_ADJ_RANGE_XY ;
	}
#else
		UsHalAdjRange = BIAS_ADJ_RANGE_XY ;
#endif

	if( UcTneAxs == X_DIR ) {
		RamRead32A( StCaliData_UiHallBias_X , &UlSetBia ) ;
	} else if( UcTneAxs == Y_DIR ) {
		RamRead32A( StCaliData_UiHallBias_Y , &UlSetBia ) ;
#ifdef	SEL_CLOSED_AF
	} else {
		RamRead32A( StCaliData_UiHallBias_AF , &UlSetBia ) ;
#endif
	}

	if( UlSetBia == 0x00000000 )	UlSetBia = 0x01000000 ;
	UlSetBia = (( UlSetBia >> 16 ) & (UINT32)0x0000FF00 ) ;
	UlSetBia *= (UINT32)UsHalAdjRange ;
	UlSetBia /= (UINT32)( 0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) ;
	if( UlSetBia > (UINT32)0x0000FFFF )		UlSetBia = 0x0000FFFF ;
	UlSetBia = ( UlSetBia << 16 ) ;
	if( UlSetBia > BIAS_HLMT )		UlSetBia = BIAS_HLMT ;
	if( UlSetBia < BIAS_LLMT )		UlSetBia = BIAS_LLMT ;

	if( UcTneAxs == X_DIR ) {
		DacControl( 0 , HLXBO , UlSetBia ) ;
		RamWrite32A( StCaliData_UiHallBias_X , UlSetBia) ;
	} else if( UcTneAxs == Y_DIR ){
		DacControl( 0 , HLYBO , UlSetBia ) ;
		RamWrite32A( StCaliData_UiHallBias_Y , UlSetBia) ;
#ifdef	SEL_CLOSED_AF
	} else {
		DacControl( 0 , HLAFBO , UlSetBia ) ;
		RamWrite32A( StCaliData_UiHallBias_AF , UlSetBia) ;
#endif
	}

	StTneVal.UlDwdVal	= TnePtp( UcTneAxs , PTP_AFTER ) ;

	return( StTneVal.UlDwdVal ) ;
}


UINT32	TneOff_3Bit( UnDwdVal	StTneVal, UINT8	UcTneAxs )
{
	INT16	SsCenVal ;											
	UINT32	UlDacDat, UlCurDac ;						
	UINT8	i, index ;

	if( UcTneAxs == X_DIR ) {
		RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , &UlCurDac ) ;			
		UlCurDac	&= 0x0000000F ;
	} else {
		RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , &UlCurDac ) ;			
		UlCurDac	= ( UlCurDac & 0x00000F00 ) >> 8 ;
	}

	for( i = 0 ; i < 7 ; i++ ) {
		if( ( UINT8 )UlCurDac == UcDacArray[ i ] ) {
			break ;
		}
	}

	SsCenVal	= ( INT16 )( ( StTneVal.StDwdVal.UsHigVal - StTneVal.StDwdVal.UsLowVal ) / OFFSET_DIV_3BIT / OFFSET_DIV ) ;	

	index	= ( INT8 )i - ( INT8 )SsCenVal ;

	if( index > 6 ) {
		index	= 6 ;
	} else if( ( INT8 )index < 0 ) {
		index	= 0 ;
	}

	UlDacDat	= ( UINT32 )UcDacArray[ index ] ;

	if( UcTneAxs == X_DIR ) {									
		RamWrite32A( StCaliData_UiHallOffset_X , UlDacDat ) ;	

		RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , &UlCurDac ) ;			
		UlDacDat	= ( UlCurDac & 0x000F0F00) | UlDacDat ;		

		RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS , UlDacDat ) ;			

	} else if( UcTneAxs == Y_DIR ){								
		RamWrite32A( StCaliData_UiHallOffset_Y , UlDacDat ) ;	

		RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , &UlCurDac ) ;			
		UlDacDat = ( UlCurDac & 0x000F000F ) | ( UlDacDat << 8 ) ;	

		RamWrite32A( CMD_IO_ADR_ACCESS , VGAVREF ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS , UlDacDat ) ;			
	}



	return( StTneVal.UlDwdVal ) ;
}

void	MesFil( UINT8	UcMesMod )		
{
	UINT32	UlMeasFilaA = 0, UlMeasFilaB = 0, UlMeasFilaC = 0;
	UINT32	UlMeasFilbA = 0, UlMeasFilbB = 0, UlMeasFilbC = 0;

	if( !UcMesMod ) {								

		UlMeasFilaA	=	0x02F19B01 ;	
		UlMeasFilaB	=	0x02F19B01 ;
		UlMeasFilaC	=	0x7A1CC9FF ;
		UlMeasFilbA	=	0x7FFFFFFF ;	
		UlMeasFilbB	=	0x00000000 ;
		UlMeasFilbC	=	0x00000000 ;

	} else if( UcMesMod == LOOPGAIN ) {				

		UlMeasFilaA	=	0x115CC757 ;	
		UlMeasFilaB	=	0x115CC757 ;
		UlMeasFilaC	=	0x5D467153 ;
		UlMeasFilbA	=	0x7F667431 ;	
		UlMeasFilbB	=	0x80998BCF ;
		UlMeasFilbC	=	0x7ECCE863 ;

	} else if( UcMesMod == THROUGH ) {				

		UlMeasFilaA	=	0x7FFFFFFF ;	
		UlMeasFilaB	=	0x00000000 ;
		UlMeasFilaC	=	0x00000000 ;
		UlMeasFilbA	=	0x7FFFFFFF ;	
		UlMeasFilbB	=	0x00000000 ;
		UlMeasFilbC	=	0x00000000 ;

	} else if( UcMesMod == NOISE ) {				

		UlMeasFilaA	=	0x02F19B01 ;	
		UlMeasFilaB	=	0x02F19B01 ;
		UlMeasFilaC	=	0x7A1CC9FF ;
		UlMeasFilbA	=	0x02F19B01 ;	
		UlMeasFilbB	=	0x02F19B01 ;
		UlMeasFilbC	=	0x7A1CC9FF ;

	} else if(UcMesMod == OSCCHK) {
		UlMeasFilaA	=	0x05C141BB ;	
		UlMeasFilaB	=	0x05C141BB ;
		UlMeasFilaC	=	0x747D7C88 ;
		UlMeasFilbA	=	0x05C141BB ;	
		UlMeasFilbB	=	0x05C141BB ;
		UlMeasFilbC	=	0x747D7C88 ;

	} else if( UcMesMod == SELFTEST ) {				

		UlMeasFilaA	=	0x115CC757 ;	
		UlMeasFilaB	=	0x115CC757 ;
		UlMeasFilaC	=	0x5D467153 ;
		UlMeasFilbA	=	0x7FFFFFFF ;	
		UlMeasFilbB	=	0x00000000 ;
		UlMeasFilbC	=	0x00000000 ;

	}

	RamWrite32A ( MeasureFilterA_Coeff_a1	, UlMeasFilaA ) ;
	RamWrite32A ( MeasureFilterA_Coeff_b1	, UlMeasFilaB ) ;
	RamWrite32A ( MeasureFilterA_Coeff_c1	, UlMeasFilaC ) ;

	RamWrite32A ( MeasureFilterA_Coeff_a2	, UlMeasFilbA ) ;
	RamWrite32A ( MeasureFilterA_Coeff_b2	, UlMeasFilbB ) ;
	RamWrite32A ( MeasureFilterA_Coeff_c2	, UlMeasFilbC ) ;

	RamWrite32A ( MeasureFilterB_Coeff_a1	, UlMeasFilaA ) ;
	RamWrite32A ( MeasureFilterB_Coeff_b1	, UlMeasFilaB ) ;
	RamWrite32A ( MeasureFilterB_Coeff_c1	, UlMeasFilaC ) ;

	RamWrite32A ( MeasureFilterB_Coeff_a2	, UlMeasFilbA ) ;
	RamWrite32A ( MeasureFilterB_Coeff_b2	, UlMeasFilbB ) ;
	RamWrite32A ( MeasureFilterB_Coeff_c2	, UlMeasFilbC ) ;
}

void	ClrMesFil( void )
{
	RamWrite32A ( MeasureFilterA_Delay_z11	, 0 ) ;
	RamWrite32A ( MeasureFilterA_Delay_z12	, 0 ) ;

	RamWrite32A ( MeasureFilterA_Delay_z21	, 0 ) ;
	RamWrite32A ( MeasureFilterA_Delay_z22	, 0 ) ;

	RamWrite32A ( MeasureFilterB_Delay_z11	, 0 ) ;
	RamWrite32A ( MeasureFilterB_Delay_z12	, 0 ) ;

	RamWrite32A ( MeasureFilterB_Delay_z21	, 0 ) ;
	RamWrite32A ( MeasureFilterB_Delay_z22	, 0 ) ;
}

void	MeasureStart( INT32 SlMeasureParameterNum , INT32 SlMeasureParameterA , INT32 SlMeasureParameterB )
{
	MemoryClear( StMeasFunc_SiSampleNum , sizeof( MeasureFunction_Type ) ) ;
	RamWrite32A( StMeasFunc_MFA_SiMax1	 , 0x80000000 ) ;					
	RamWrite32A( StMeasFunc_MFB_SiMax2	 , 0x80000000 ) ;					
	RamWrite32A( StMeasFunc_MFA_SiMin1	 , 0x7FFFFFFF ) ;					
	RamWrite32A( StMeasFunc_MFB_SiMin2	 , 0x7FFFFFFF ) ;					

	SetTransDataAdr( StMeasFunc_MFA_PiMeasureRam1	, ( UINT32 )SlMeasureParameterA ) ;		
	SetTransDataAdr( StMeasFunc_MFB_PiMeasureRam2	, ( UINT32 )SlMeasureParameterB ) ;		
	RamWrite32A( StMeasFunc_MFA_SiSampleNumA	 	, 0 ) ;									
	RamWrite32A( StMeasFunc_MFB_SiSampleNumB	 	, 0 ) ;									
	RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode		, 0 ) ;									
	ClrMesFil() ;																			
	SetWaitTime(50) ;
	RamWrite32A( StMeasFunc_MFB_SiSampleMaxB		, SlMeasureParameterNum ) ;				
	RamWrite32A( StMeasFunc_MFA_SiSampleMaxA		, SlMeasureParameterNum ) ;				
}

void	MeasureWait( void )
{
	UINT32	SlWaitTimerStA, SlWaitTimerStB ;
	UINT16	UsTimeOut = 2000;

	do {
		RamRead32A( StMeasFunc_MFA_SiSampleMaxA, &SlWaitTimerStA ) ;
		RamRead32A( StMeasFunc_MFB_SiSampleMaxB, &SlWaitTimerStB ) ;
		UsTimeOut--;
	} while ( (SlWaitTimerStA || SlWaitTimerStB) && UsTimeOut );

}

void	MemoryClear( UINT16 UsSourceAddress, UINT16 UsClearSize )
{
	UINT16	UsLoopIndex ;

	for ( UsLoopIndex = 0 ; UsLoopIndex < UsClearSize ;  ) {
		RamWrite32A( UsSourceAddress	, 	0x00000000 ) ;				
		UsSourceAddress += 4;
		UsLoopIndex += 4 ;
	}
}

#define 	ONE_MSEC_COUNT	20			
void	SetWaitTime( UINT16 UsWaitTime )
{
	RamWrite32A( WaitTimerData_UiWaitCounter	, 0 ) ;
	RamWrite32A( WaitTimerData_UiTargetCount	, (UINT32)(ONE_MSEC_COUNT * UsWaitTime)) ;
}


#define 	LOOP_NUM		2136			
#define 	LOOP_FREQ		0x00F586D9		
#define 	LOOP_GAIN		0x040C3713		

#define 	LOOP_MAX_X		SXGAIN_LOP << 1	
#define 	LOOP_MIN_X		SXGAIN_LOP >> 1	
#define 	LOOP_MAX_Y		SYGAIN_LOP << 1	
#define 	LOOP_MIN_Y		SYGAIN_LOP >> 1	

#ifdef	SEL_CLOSED_AF
#define 	LOOP_NUM_Z		1885			
#define 	LOOP_FREQ_Z		0x0116437F		
#define 	LOOP_GAIN_Z		0x0207567A		
#define 	LOOP_MAX_Z		SZGAIN_LOP << 1	
#define 	LOOP_MIN_Z		SZGAIN_LOP >> 1	
#endif

UINT32	LopGan( UINT8	UcDirSel )
{
	UnllnVal		StMeasValueA , StMeasValueB ;
	INT32			SlMeasureParameterA = 0, SlMeasureParameterB = 0;
	INT32			SlMeasureParameterNum = 0;
	UINT64	UllCalculateVal ;
	UINT32	UlReturnState = 0;
	UINT16	UsSinAdr = 0;
	UINT32	UlLopFreq , UlLopGain;
#ifdef	SEL_CLOSED_AF
	UINT32	UlSwitchBk ;
#endif

	SlMeasureParameterNum	=	(INT32)LOOP_NUM ;

	if( UcDirSel == X_DIR ) {		
		SlMeasureParameterA		=	HALL_RAM_HXOUT1 ;		
		SlMeasureParameterB		=	HALL_RAM_HXLOP ;		
		RamWrite32A( HallFilterCoeffX_hxgain0 , SXGAIN_LOP ) ;
		UsSinAdr = HALL_RAM_SINDX0;
		UlLopFreq = LOOP_FREQ;
		UlLopGain = LOOP_GAIN;
	} else if( UcDirSel == Y_DIR ){						
		SlMeasureParameterA		=	HALL_RAM_HYOUT1 ;		
		SlMeasureParameterB		=	HALL_RAM_HYLOP ;		
		RamWrite32A( HallFilterCoeffY_hygain0 , SYGAIN_LOP ) ;
		UsSinAdr = HALL_RAM_SINDY0;
		UlLopFreq = LOOP_FREQ;
		UlLopGain = LOOP_GAIN;
#ifdef	SEL_CLOSED_AF
	} else {						
		SlMeasureParameterNum	=	(signed INT32)LOOP_NUM_Z ;
		SlMeasureParameterA		=	CLAF_RAMA_AFLOP2 ;		
		SlMeasureParameterB		=	CLAF_DELAY_AFPZ0 ;		
		RamWrite32A( CLAF_Gain_afloop2 , SZGAIN_LOP ) ;
		RamRead32A( CLAF_RAMA_AFCNT , &UlSwitchBk ) ;
		RamWrite32A( CLAF_RAMA_AFCNT , UlSwitchBk & 0xffffff0f ) ;
		UsSinAdr = CLAF_RAMA_AFCNTO;
		UlLopFreq = LOOP_FREQ_Z;
		UlLopGain = LOOP_GAIN_Z;
#endif
	}

	SetSinWavGenInt();

	RamWrite32A( SinWave_Offset		,	UlLopFreq ) ;								
	RamWrite32A( SinWave_Gain		,	UlLopGain ) ;								

	RamWrite32A( SinWaveC_Regsiter	,	0x00000001 ) ;								

	SetTransDataAdr( SinWave_OutAddr	,	( UINT32 )UsSinAdr ) ;	

	MesFil( LOOPGAIN ) ;					

#if 0
	
	
	RamRead32A ( MeasureFilterB_Coeff_a1, &UlReadVal ) ;
	UlReadVal = UlReadVal * 0.7;
	RamWrite32A ( MeasureFilterB_Coeff_a1	, UlReadVal ) ;
	RamWrite32A ( MeasureFilterB_Coeff_b1	, UlReadVal ) ;
#endif

	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;					

	MeasureWait() ;						

	RamRead32A( StMeasFunc_MFA_LLiAbsInteg1 		, &StMeasValueA.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFA_LLiAbsInteg1 + 4 	, &StMeasValueA.StUllnVal.UlHigVal ) ;
	RamRead32A( StMeasFunc_MFB_LLiAbsInteg2 		, &StMeasValueB.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFB_LLiAbsInteg2 + 4		, &StMeasValueB.StUllnVal.UlHigVal ) ;

	SetSinWavGenInt();		

	SetTransDataAdr( SinWave_OutAddr	,	(UINT32)0x00000000 ) ;	
	RamWrite32A( UsSinAdr		,	0x00000000 ) ;				

	if( UcDirSel == X_DIR ) {		
		UllCalculateVal = ( StMeasValueB.UllnValue * 1000 / StMeasValueA.UllnValue ) * SXGAIN_LOP / 1000 ;
		if( UllCalculateVal > (UINT64)0x000000007FFFFFFF )		UllCalculateVal = (UINT64)0x000000007FFFFFFF ;
		StAdjPar.StLopGan.UlLxgVal = (UINT32)UllCalculateVal ;
		RamWrite32A( HallFilterCoeffX_hxgain0 , StAdjPar.StLopGan.UlLxgVal ) ;
		if( UllCalculateVal > LOOP_MAX_X ){
			UlReturnState = EXE_LXADJ ;
		}else if( UllCalculateVal < LOOP_MIN_X ){
			UlReturnState = EXE_LXADJ ;
		}else{
			UlReturnState = EXE_END ;
		}

	}else if( UcDirSel == Y_DIR ){							
		UllCalculateVal = ( StMeasValueB.UllnValue * 1000 / StMeasValueA.UllnValue ) * SYGAIN_LOP / 1000 ;
		if( UllCalculateVal > (UINT64)0x000000007FFFFFFF )		UllCalculateVal = (UINT64)0x000000007FFFFFFF ;
		StAdjPar.StLopGan.UlLygVal = (UINT32)UllCalculateVal ;
		RamWrite32A( HallFilterCoeffY_hygain0 , StAdjPar.StLopGan.UlLygVal ) ;
		if( UllCalculateVal > LOOP_MAX_Y ){
			UlReturnState = EXE_LYADJ ;
		}else if( UllCalculateVal < LOOP_MIN_Y ){
			UlReturnState = EXE_LYADJ ;
		}else{
			UlReturnState = EXE_END ;
		}
#ifdef	SEL_CLOSED_AF
	}else{							
		UllCalculateVal = ( StMeasValueB.UllnValue * 1000 / StMeasValueA.UllnValue ) * SZGAIN_LOP / 1000 ;
		if( UllCalculateVal > (UINT64)0x000000007FFFFFFF )		UllCalculateVal = (UINT64)0x000000007FFFFFFF ;
		StAdjPar.StLopGan.UlLzgVal = (UINT32)UllCalculateVal ;
		RamWrite32A( CLAF_Gain_afloop2 , StAdjPar.StLopGan.UlLzgVal ) ;
		if( UllCalculateVal > LOOP_MAX_Z ){
			UlReturnState = EXE_LZADJ ;
		}else if( UllCalculateVal < LOOP_MIN_Z ){
			UlReturnState = EXE_LZADJ ;
		}else{
			UlReturnState = EXE_END ;
		}
		RamWrite32A( CLAF_RAMA_AFCNT , UlSwitchBk ) ;
#endif
	}

	return( UlReturnState ) ;

}




#define 	GYROF_NUM		2048			
#define 	GYROF_UPPER		0x1000			
#define 	GYROF_LOWER		0xF000			
UINT32	TneGvc( void )
{
	UINT32	UlRsltSts;
	INT32			SlMeasureParameterA , SlMeasureParameterB ;
	INT32			SlMeasureParameterNum ;
	UnllnVal		StMeasValueA , StMeasValueB ;
	INT32			SlMeasureAveValueA , SlMeasureAveValueB ;


	

	MesFil( THROUGH ) ;					

	SlMeasureParameterNum	=	GYROF_NUM ;					
	SlMeasureParameterA		=	GYRO_RAM_GX_ADIDAT ;		
	SlMeasureParameterB		=	GYRO_RAM_GY_ADIDAT ;		

	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;					


	MeasureWait() ;					

	RamRead32A( StMeasFunc_MFA_LLiIntegral1 		, &StMeasValueA.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFA_LLiIntegral1 + 4		, &StMeasValueA.StUllnVal.UlHigVal ) ;
	RamRead32A( StMeasFunc_MFB_LLiIntegral2 		, &StMeasValueB.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFB_LLiIntegral2 + 4		, &StMeasValueB.StUllnVal.UlHigVal ) ;

	SlMeasureAveValueA = (INT32)( (INT64)StMeasValueA.UllnValue / SlMeasureParameterNum ) ;
	SlMeasureAveValueB = (INT32)( (INT64)StMeasValueB.UllnValue / SlMeasureParameterNum ) ;

	SlMeasureAveValueA = ( SlMeasureAveValueA >> 16 ) & 0x0000FFFF ;
	SlMeasureAveValueB = ( SlMeasureAveValueB >> 16 ) & 0x0000FFFF ;

	SlMeasureAveValueA = 0x00010000 - SlMeasureAveValueA ;
	SlMeasureAveValueB = 0x00010000 - SlMeasureAveValueB ;

	UlRsltSts = EXE_END ;
	StAdjPar.StGvcOff.UsGxoVal = ( UINT16 )( SlMeasureAveValueA & 0x0000FFFF );		
	if(( (INT16)StAdjPar.StGvcOff.UsGxoVal > (INT16)GYROF_UPPER ) || ( (INT16)StAdjPar.StGvcOff.UsGxoVal < (INT16)GYROF_LOWER )){
		UlRsltSts |= EXE_GXADJ ;
	}
	RamWrite32A( GYRO_RAM_GXOFFZ , (( SlMeasureAveValueA << 16 ) & 0xFFFF0000 ) ) ;		

	StAdjPar.StGvcOff.UsGyoVal = ( UINT16 )( SlMeasureAveValueB & 0x0000FFFF );		
	if(( (INT16)StAdjPar.StGvcOff.UsGyoVal > (INT16)GYROF_UPPER ) || ( (INT16)StAdjPar.StGvcOff.UsGyoVal < (INT16)GYROF_LOWER )){
		UlRsltSts |= EXE_GYADJ ;
	}
	RamWrite32A( GYRO_RAM_GYOFFZ , (( SlMeasureAveValueB << 16 ) & 0xFFFF0000 ) ) ;		


	RamWrite32A( GYRO_RAM_GYROX_OFFSET , 0x00000000 ) ;			
	RamWrite32A( GYRO_RAM_GYROY_OFFSET , 0x00000000 ) ;			
	RamWrite32A( GyroFilterDelayX_GXH1Z2 , 0x00000000 ) ;		
	RamWrite32A( GyroFilterDelayY_GYH1Z2 , 0x00000000 ) ;		

	return( UlRsltSts );


}



UINT8	RtnCen( UINT8	UcCmdPar )
{
	UINT8	UcSndDat ;

	if( !UcCmdPar ){								
		RamWrite32A( CMD_RETURN_TO_CENTER , BOTH_SRV_ON ) ;
	}else if( UcCmdPar == XONLY_ON ){				
		RamWrite32A( CMD_RETURN_TO_CENTER , XAXS_SRV_ON ) ;
	}else if( UcCmdPar == YONLY_ON ){				
		RamWrite32A( CMD_RETURN_TO_CENTER , YAXS_SRV_ON ) ;
#ifdef	SEL_CLOSED_AF
	}else if( UcCmdPar == ZONLY_OFF ){				
		RamWrite32A( CMD_RETURN_TO_CENTER , ZAXS_SRV_OFF ) ;
	}else if( UcCmdPar == ZONLY_ON ){				
		RamWrite32A( CMD_RETURN_TO_CENTER , ZAXS_SRV_ON ) ;
#endif
	}else{											
		RamWrite32A( CMD_RETURN_TO_CENTER , BOTH_SRV_OFF ) ;
	}

	while( UcSndDat ) {
		UcSndDat = RdStatus(1);
	}
#ifdef	SEL_CLOSED_AF
	if( UcCmdPar == ZONLY_OFF ){				
		RamWrite32A( CLAF_RAMA_AFOUT		,	0x00000000 ) ;				
	}
#endif
	return( UcSndDat );
}



void	OisEna( void )
{
	UINT8	UcStRd = 1;

	RamWrite32A( CMD_OIS_ENABLE , OIS_ENABLE ) ;
	while( UcStRd ) {
		UcStRd = RdStatus(1);
	}
}

void	OisEnaNCL( void )
{
	UINT8	UcStRd = 1;

	RamWrite32A( CMD_OIS_ENABLE , OIS_ENA_NCL | OIS_ENABLE ) ;
	while( UcStRd ) {
		UcStRd = RdStatus(1);
	}
}

void	OisEnaDrCl( void )
{
	UINT8	UcStRd = 1;

	RamWrite32A( CMD_OIS_ENABLE , OIS_ENA_DOF | OIS_ENABLE ) ;
	while( UcStRd ) {
		UcStRd = RdStatus(1);
	}
}

void	OisEnaDrNcl( void )
{
	UINT8	UcStRd = 1;

	RamWrite32A( CMD_OIS_ENABLE , OIS_ENA_DOF | OIS_ENA_NCL | OIS_ENABLE ) ;
	while( UcStRd ) {
		UcStRd = RdStatus(1);
	}
}
void	OisDis( void )
{
	UINT8	UcStRd = 1;

	RamWrite32A( CMD_OIS_ENABLE , OIS_DISABLE ) ;
	while( UcStRd ) {
		UcStRd = RdStatus(1);
	}
}


void	SetRec( void )
{
	UINT8	UcStRd = 1;

	RamWrite32A( CMD_MOVE_STILL_MODE ,	MOVIE_MODE ) ;
	while( UcStRd ) {
		UcStRd = RdStatus(1);
	}
}


void	SetStill( void )
{
	UINT8	UcStRd = 1;

	RamWrite32A( CMD_MOVE_STILL_MODE ,	STILL_MODE ) ;
	while( UcStRd ) {
		UcStRd = RdStatus(1);
	}
}


	
	
	
	
const UINT32	CucFreqVal[ 17 ]	= {
		0xFFFFFFFF,				
		0x0001A306,				
		0x0003460B,				
		0x0004E911,				
		0x00068C16,				
		0x00082F1C,				
		0x0009D222,				
		0x000B7527,				
		0x000D182D,				
		0x000EBB32,				
		0x00105E38,				
		0x0012013E,				
		0x0013A443,				
		0x00154749,				
		0x0016EA4E,				
		0x00188D54,				
		0x001A305A				
	} ;

void	SetSinWavePara( UINT8 UcTableVal ,  UINT8 UcMethodVal )
{
	UINT32	UlFreqDat ;


	if(UcTableVal > 0x10 )
		UcTableVal = 0x10 ;			
	UlFreqDat = CucFreqVal[ UcTableVal ] ;

	if( UcMethodVal == CIRCWAVE) {
		RamWrite32A( SinWave_Phase	,	0x00000000 ) ;		
		RamWrite32A( CosWave_Phase 	,	0x20000000 );		
	}else{
		RamWrite32A( SinWave_Phase	,	0x00000000 ) ;		
		RamWrite32A( CosWave_Phase 	,	0x00000000 );		
	}


	if( UlFreqDat == 0xFFFFFFFF )			
	{
		RamWrite32A( SinWave_Offset		,	0x00000000 ) ;									
		RamWrite32A( SinWave_Phase		,	0x00000000 ) ;									

		RamWrite32A( CosWave_Offset		,	0x00000000 );									
		RamWrite32A( CosWave_Phase 		,	0x00000000 );									

		RamWrite32A( SinWaveC_Regsiter	,	0x00000000 ) ;									
		SetTransDataAdr( SinWave_OutAddr	,	0x00000000 ) ;		
		SetTransDataAdr( CosWave_OutAddr	,	0x00000000 );		
		RamWrite32A( HALL_RAM_HXOFF1		,	0x00000000 ) ;				
		RamWrite32A( HALL_RAM_HYOFF1		,	0x00000000 ) ;				
	}
	else
	{
		RamWrite32A( SinWave_Offset		,	UlFreqDat ) ;									
		RamWrite32A( CosWave_Offset		,	UlFreqDat );									

		RamWrite32A( SinWaveC_Regsiter	,	0x00000001 ) ;									
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)HALL_RAM_HXOFF1 ) ;		
		SetTransDataAdr( CosWave_OutAddr	,	 (UINT32)HALL_RAM_HYOFF1 );		

	}


}




void	SetPanTiltMode( UINT8 UcPnTmod )
{
	UINT8	UcStRd = 1;

	switch ( UcPnTmod ) {
		case OFF :
			RamWrite32A( CMD_PAN_TILT ,	PAN_TILT_OFF ) ;
			break ;
		case ON :
			RamWrite32A( CMD_PAN_TILT ,	PAN_TILT_ON ) ;
			break ;
	}

	while( UcStRd ) {
		UcStRd = RdStatus(1);
	}
}

 #ifdef	NEUTRAL_CENTER
UINT8	TneHvc( void )
{
	UINT8	UcRsltSts;
	INT32			SlMeasureParameterA , SlMeasureParameterB ;
	INT32			SlMeasureParameterNum ;
	UnllnVal		StMeasValueA , StMeasValueB ;
	INT32			SlMeasureAveValueA , SlMeasureAveValueB ;

	RtnCen( BOTH_OFF ) ;		

	WitTim( 500 ) ;

	

	MesFil( THROUGH ) ;					

	SlMeasureParameterNum	=	64 ;		
	SlMeasureParameterA		=	(UINT32)HALL_RAM_HXIDAT ;		
	SlMeasureParameterB		=	(UINT32)HALL_RAM_HYIDAT ;		

	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;					

	ClrMesFil();					
	SetWaitTime(50) ;

	MeasureWait() ;					

	RamRead32A( StMeasFunc_MFA_LLiIntegral1 		, &StMeasValueA.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFA_LLiIntegral1 + 4 	, &StMeasValueA.StUllnVal.UlHigVal ) ;
	RamRead32A( StMeasFunc_MFB_LLiIntegral2 		, &StMeasValueB.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFB_LLiIntegral2 + 4	, &StMeasValueB.StUllnVal.UlHigVal ) ;

	SlMeasureAveValueA = (INT32)((( (INT64)StMeasValueA.UllnValue * 100 ) / SlMeasureParameterNum ) / 100 ) ;
	SlMeasureAveValueB = (INT32)((( (INT64)StMeasValueB.UllnValue * 100 ) / SlMeasureParameterNum ) / 100 ) ;

	StAdjPar.StHalAdj.UsHlxCna = ( UINT16 )(( SlMeasureAveValueA >> 16 ) & 0x0000FFFF );		
	StAdjPar.StHalAdj.UsHlxCen = StAdjPar.StHalAdj.UsHlxCna;											

	StAdjPar.StHalAdj.UsHlyCna = ( UINT16 )(( SlMeasureAveValueB >> 16 ) & 0x0000FFFF );		
	StAdjPar.StHalAdj.UsHlyCen = StAdjPar.StHalAdj.UsHlyCna;											

	UcRsltSts = EXE_END ;				

	return( UcRsltSts );
}
 #endif	

 #ifdef	NEUTRAL_CENTER_FINE
void	TneFin( void )
{
	UINT32	UlReadVal ;
	UINT16	UsAdxOff, UsAdyOff ;
	INT32			SlMeasureParameterNum ;
	INT32			SlMeasureAveValueA , SlMeasureAveValueB ;
	UnllnVal		StMeasValueA , StMeasValueB ;
	UINT32	UlMinimumValueA, UlMinimumValueB ;
	UINT16	UsAdxMin, UsAdyMin ;
	UINT8	UcFin ;

	
	RamRead32A( HALL_RAM_HXOFF,  &UlReadVal ) ;
	UsAdxOff = UsAdxMin = (UINT16)( UlReadVal >> 16 ) ;

	RamRead32A( HALL_RAM_HYOFF,  &UlReadVal ) ;
	UsAdyOff = UsAdyMin = (UINT16)( UlReadVal >> 16 ) ;

	
	RtnCen( BOTH_ON ) ;
	WitTim( TNE ) ;

	MesFil( THROUGH ) ;					

	SlMeasureParameterNum = 2000 ;

	MeasureStart( SlMeasureParameterNum , HALL_RAM_HALL_X_OUT , HALL_RAM_HALL_Y_OUT ) ;					

	MeasureWait() ;						

	RamRead32A( StMeasFunc_MFA_LLiIntegral1 	, &StMeasValueA.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFA_LLiIntegral1 + 4 , &StMeasValueA.StUllnVal.UlHigVal ) ;
	SlMeasureAveValueA = (INT32)((( (INT64)StMeasValueA.UllnValue * 100 ) / SlMeasureParameterNum ) / 100 ) ;

	RamRead32A( StMeasFunc_MFB_LLiIntegral2 	, &StMeasValueB.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFB_LLiIntegral2 + 4	, &StMeasValueB.StUllnVal.UlHigVal ) ;
	SlMeasureAveValueB = (INT32)((( (INT64)StMeasValueB.UllnValue * 100 ) / SlMeasureParameterNum ) / 100 ) ;




	UlMinimumValueA = abs(SlMeasureAveValueA) ;
	UlMinimumValueB = abs(SlMeasureAveValueB) ;
	UcFin = 0x11 ;

	while( UcFin ) {
		if( UcFin & 0x01 ) {
			if( UlMinimumValueA >= abs(SlMeasureAveValueA) ) {
				UlMinimumValueA = abs(SlMeasureAveValueA) ;
				UsAdxMin = UsAdxOff ;
				
				if( SlMeasureAveValueA > 0 )
					UsAdxOff = (INT16)UsAdxOff + (SlMeasureAveValueA >> 17) + 1 ;
				else
					UsAdxOff = (INT16)UsAdxOff + (SlMeasureAveValueA >> 17) - 1 ;

				RamWrite32A( HALL_RAM_HXOFF,  (UINT32)((UsAdxOff << 16 ) & 0xFFFF0000 )) ;
			} else {
				UcFin &= 0xFE ;		
			}
		}

		if( UcFin & 0x10 ) {
			if( UlMinimumValueB >= abs(SlMeasureAveValueB) ) {
				UlMinimumValueB = abs(SlMeasureAveValueB) ;
				UsAdyMin = UsAdyOff ;
				
				if( SlMeasureAveValueB > 0 )
					UsAdyOff = (INT16)UsAdyOff + (SlMeasureAveValueB >> 17) + 1 ;
				else
					UsAdyOff = (INT16)UsAdyOff + (SlMeasureAveValueB >> 17) - 1 ;

				RamWrite32A( HALL_RAM_HYOFF,  (UINT32)((UsAdyOff << 16 ) & 0xFFFF0000 )) ;
			} else {
				UcFin &= 0xEF ;		
			}
		}

		MeasureStart( SlMeasureParameterNum , HALL_RAM_HALL_X_OUT , HALL_RAM_HALL_Y_OUT ) ;					
		MeasureWait() ;						

		RamRead32A( StMeasFunc_MFA_LLiIntegral1 	, &StMeasValueA.StUllnVal.UlLowVal ) ;	
		RamRead32A( StMeasFunc_MFA_LLiIntegral1 + 4 , &StMeasValueA.StUllnVal.UlHigVal ) ;
		SlMeasureAveValueA = (INT32)((( (INT64)StMeasValueA.UllnValue * 100 ) / SlMeasureParameterNum ) / 100 ) ;

		RamRead32A( StMeasFunc_MFB_LLiIntegral2 	, &StMeasValueB.StUllnVal.UlLowVal ) ;	
		RamRead32A( StMeasFunc_MFB_LLiIntegral2 + 4	, &StMeasValueB.StUllnVal.UlHigVal ) ;
		SlMeasureAveValueB = (INT32)((( (INT64)StMeasValueB.UllnValue * 100 ) / SlMeasureParameterNum ) / 100 ) ;



	}	




	StAdjPar.StHalAdj.UsHlxCna = UsAdxMin;								
	StAdjPar.StHalAdj.UsHlxCen = StAdjPar.StHalAdj.UsHlxCna;			

	StAdjPar.StHalAdj.UsHlyCna = UsAdyMin;								
	StAdjPar.StHalAdj.UsHlyCen = StAdjPar.StHalAdj.UsHlyCna;			

	StAdjPar.StHalAdj.UsAdxOff = StAdjPar.StHalAdj.UsHlxCna  ;
	StAdjPar.StHalAdj.UsAdyOff = StAdjPar.StHalAdj.UsHlyCna  ;

	
	RtnCen( BOTH_OFF ) ;		

}
 #endif	


void	TneSltPos( UINT8 UcPos )
{
	INT16 SsOff = 0x0000 ;
	INT32	SlX, SlY;


	UcPos &= 0x07 ;

	if ( UcPos ) {
		SsOff = SLT_OFFSET * (UcPos - 4);
#if 0
		SsOff = (INT16)(SsOff / sqrt(2));		
#else
		SsOff = (INT16)(SsOff *10 / 14);		
#endif
	}

#if 0
	if ( outX & 0x01 )	SsNvcX = -1;
	else				SsNvcX = 1;

	if ( outY & 0x01 )	SsNvcY = -1;
	else				SsNvcY = 1;

	SlX = (INT32)((SsOff * SsNvcX) << 16);
	SlY = (INT32)((SsOff * SsNvcY) << 16);

	if ( outX & 0x02 ) {
		
		if ( outY & 0x01 )	SsNvcX = -1;
		else				SsNvcX = 1;

		if ( outX & 0x01 )	SsNvcY = -1;
		else				SsNvcY = 1;

		SlY = (INT32)((SsOff * SsNvcX) << 16);
		SlX = (INT32)((SsOff * SsNvcY) << 16);
	} else {
		
		if ( outX & 0x01 )	SsNvcX = -1;
		else				SsNvcX = 1;

		if ( outY & 0x01 )	SsNvcY = -1;
		else				SsNvcY = 1;

		SlX = (INT32)((SsOff * SsNvcX) << 16);
		SlY = (INT32)((SsOff * SsNvcY) << 16);
	}
#else
	SsNvcX = -1;
	SsNvcY = 1;

	SlX = (INT32)((SsOff * SsNvcY) << 16);
	SlY = (INT32)((SsOff * SsNvcX) << 16);
#endif

	RamWrite32A( HALL_RAM_GYROX_OUT, SlX ) ;
	RamWrite32A( HALL_RAM_GYROY_OUT, SlY ) ;

}

void	TneVrtPos( UINT8 UcPos )
{
	INT16 SsOff = 0x0000 ;
	INT32	SlX, SlY;


	UcPos &= 0x07 ;

	if ( UcPos ) {
		SsOff = SLT_OFFSET * (UcPos - 4);
	}

#if 0
	if ( outX & 0x02 ) {
		
		if ( outX & 0x01 )	SsNvcY = -1;
		else				SsNvcY = 1;

		SlX = (INT32)((SsOff * SsNvcY) << 16);
		SlY = 0x00000000;
	} else {
		
		if ( outY & 0x01 )	SsNvcY = -1;
		else				SsNvcY = 1;

		SlX = 0x00000000;
		SlY = (INT32)((SsOff * SsNvcY) << 16);
	}
#else
		SsNvcY = 1;
		SlX = (INT32)((SsOff * SsNvcY) << 16);
		SlY = 0x00000000;
#endif

	RamWrite32A( HALL_RAM_GYROX_OUT, SlX ) ;
	RamWrite32A( HALL_RAM_GYROY_OUT, SlY ) ;
}

void	TneHrzPos( UINT8 UcPos )
{
	INT16 SsOff = 0x0000 ;
	INT32	SlX, SlY;


	UcPos &= 0x07 ;

	if ( UcPos ) {
		SsOff = SLT_OFFSET * (UcPos - 4);
	}

#if 0
	if ( outX & 0x02 ) {
		
		if ( outY & 0x01 )	SsNvcX = -1;
		else				SsNvcX = 1;

		SlX = 0x00000000;
		SlY = (INT32)((SsOff * SsNvcX) << 16);
	} else {
		
		if ( outX & 0x01 )	SsNvcX = -1;
		else				SsNvcX = 1;

		SlX = (INT32)((SsOff * SsNvcX) << 16);
		SlY = 0x00000000;
	}
#else
		SsNvcX = -1;
		SlX = 0x00000000;
		SlY = (INT32)((SsOff * SsNvcX) << 16);
#endif

	RamWrite32A( HALL_RAM_GYROX_OUT, SlX ) ;
	RamWrite32A( HALL_RAM_GYROY_OUT, SlY ) ;
}

void	SetSinWavGenInt( void )
{

	RamWrite32A( SinWave_Offset		,	0x00000000 ) ;		
	RamWrite32A( SinWave_Phase		,	0x00000000 ) ;		
	RamWrite32A( SinWave_Gain		,	0x00000000 ) ;		

	RamWrite32A( CosWave_Offset		,	0x00000000 );		
	RamWrite32A( CosWave_Phase 		,	0x20000000 );		
	RamWrite32A( CosWave_Gain 		,	0x00000000 );		

	RamWrite32A( SinWaveC_Regsiter	,	0x00000000 ) ;								

}


void	SetTransDataAdr( UINT16 UsLowAddress , UINT32 UlLowAdrBeforeTrans )
{
	UnDwdVal	StTrsVal ;

	if( UlLowAdrBeforeTrans < 0x00009000 ){
		StTrsVal.StDwdVal.UsHigVal = (UINT16)(( UlLowAdrBeforeTrans & 0x0000F000 ) >> 8 ) ;
		StTrsVal.StDwdVal.UsLowVal = (UINT16)( UlLowAdrBeforeTrans & 0x00000FFF ) ;
	}else{
		StTrsVal.UlDwdVal = UlLowAdrBeforeTrans ;
	}
	RamWrite32A( UsLowAddress	,	StTrsVal.UlDwdVal );

}


UINT8	RdStatus( UINT8 UcStBitChk )
{
	UINT32	UlReadVal ;

	RamRead32A( CMD_READ_STATUS , &UlReadVal );
	if( UcStBitChk ){
		UlReadVal &= READ_STATUS_INI ;
	}
	if( !UlReadVal ){
		return( SUCCESS );
	}else{
		return( FAILURE );
	}
}


void	DacControl( UINT8 UcMode, UINT32 UiChannel, UINT32 PuiData )
{
	UINT32	UlAddaInt ;
	if( !UcMode ) {
		RamWrite32A( CMD_IO_ADR_ACCESS , ADDA_DASEL ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS , UiChannel ) ;
		RamWrite32A( CMD_IO_ADR_ACCESS , ADDA_DAO ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS , PuiData ) ;
		;
		;
		UlAddaInt = 0x00000040 ;
		while ( (UlAddaInt & 0x00000040) != 0 ) {
			RamWrite32A( CMD_IO_ADR_ACCESS , ADDA_ADDAINT ) ;
			RamRead32A(  CMD_IO_DAT_ACCESS , &UlAddaInt ) ;
			;
		}
	} else {
		RamWrite32A( CMD_IO_ADR_ACCESS , ADDA_DASEL ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS , UiChannel ) ;
		RamWrite32A( CMD_IO_ADR_ACCESS , ADDA_DAO ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , &PuiData ) ;
		;
		;
		UlAddaInt = 0x00000040 ;
		while ( (UlAddaInt & 0x00000040) != 0 ) {
			RamWrite32A( CMD_IO_ADR_ACCESS , ADDA_ADDAINT ) ;
			RamRead32A(  CMD_IO_DAT_ACCESS , &UlAddaInt ) ;
			;
		}
	}

	return ;
}

UINT8	WrHallCalData( void )
{
	UINT32	UiChkSum1,	UiChkSum2 ;
	UINT32	UlSrvStat,	UlOisStat ;
	UINT8 ans;
	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;		
	ReadCalDataF40(UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
#ifdef	SEL_CLOSED_AF
		UlBufDat[0] &= ~( HALL_CALB_FLG | CLAF_CALB_FLG | HALL_CALB_BIT );
#else
		UlBufDat[0] &= ~( HALL_CALB_FLG | HALL_CALB_BIT );
#endif
		UlBufDat[0] |= StAdjPar.StHalAdj.UlAdjPhs ;							
		UlBufDat[ HALL_MAX_BEFORE_X	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlxMax << 16) ;		
		UlBufDat[ HALL_MIN_BEFORE_X	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlxMin << 16) ;		
		UlBufDat[ HALL_MAX_AFTER_X	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlxMxa << 16) ;		
		UlBufDat[ HALL_MIN_AFTER_X	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlxMna << 16) ;		
		UlBufDat[ HALL_MAX_BEFORE_Y	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlyMax << 16) ;		
		UlBufDat[ HALL_MIN_BEFORE_Y	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlyMin << 16) ;		
		UlBufDat[ HALL_MAX_AFTER_Y	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlyMxa << 16) ;		
		UlBufDat[ HALL_MIN_AFTER_Y	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlyMna << 16) ;		
		UlBufDat[ HALL_BIAS_DAC_X	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlxGan << 16) ;		
		UlBufDat[ HALL_OFFSET_DAC_X	 ] 	= (UINT32)(StAdjPar.StHalAdj.UsHlxOff) ;			
		UlBufDat[ HALL_BIAS_DAC_Y	 ] 	= (UINT32)(StAdjPar.StHalAdj.UsHlyGan << 16) ;		
		UlBufDat[ HALL_OFFSET_DAC_Y	 ] 	= (UINT32)(StAdjPar.StHalAdj.UsHlyOff) ;			
		UlBufDat[ LOOP_GAIN_X		 ] 	= StAdjPar.StLopGan.UlLxgVal ;						
		UlBufDat[ LOOP_GAIN_Y		 ]	= StAdjPar.StLopGan.UlLygVal ;						
		UlBufDat[ MECHA_CENTER_X	 ]	= (UINT32)(StAdjPar.StHalAdj.UsAdxOff << 16) ;		
		UlBufDat[ MECHA_CENTER_Y	 ]	= (UINT32)(StAdjPar.StHalAdj.UsAdyOff << 16) ;		
		UlBufDat[ OPT_CENTER_X		 ]	= 0L;												
		UlBufDat[ OPT_CENTER_Y		 ]	= 0L;												
		UlBufDat[ GYRO_OFFSET_X		 ]	= (UINT32)(StAdjPar.StGvcOff.UsGxoVal << 16) ;		
		UlBufDat[ GYRO_OFFSET_Y		 ]	= (UINT32)(StAdjPar.StGvcOff.UsGyoVal << 16) ;		
#ifdef	SEL_CLOSED_AF
		UlBufDat[ AF_HALL_BIAS_DAC	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzGan << 16) ;		
		UlBufDat[ AF_HALL_OFFSET_DAC ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzOff << 16) ;      
		UlBufDat[ AF_LOOP_GAIN		 ]	= StAdjPar.StLopGan.UlLzgVal ) ;					
		UlBufDat[ AF_MECHA_CENTER	 ]	= (UINT32)(StAdjPar.StHalAdj.UsAdzOff << 16) ;		
		UlBufDat[ AF_HALL_AMP_MAG	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzAmp << 16);		
		UlBufDat[ AF_HALL_MAX_BEFORE ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzMax << 16) ;		
		UlBufDat[ AF_HALL_MIN_BEFORE ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzMin << 16) ;      
		UlBufDat[ AF_HALL_MAX_AFTER	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzMxa << 16) ;      
		UlBufDat[ AF_HALL_MIN_AFTER	 ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzMna << 16) ;      
#endif

		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );															
}


UINT8	WrGyroGainData( void )
{
	UINT32	UlReadVal ;
	UINT32	UiChkSum1,	UiChkSum2 ;
	UINT32	UlSrvStat,	UlOisStat ;
	UINT8 ans;
	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;		
	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
		UlBufDat[0] &= ~GYRO_GAIN_FLG;										
		RamRead32A(  GyroFilterTableX_gxzoom , &UlReadVal ) ;
		UlBufDat[ GYRO_GAIN_X ] 	= UlReadVal;							
		RamRead32A(  GyroFilterTableY_gyzoom , &UlReadVal ) ;
		UlBufDat[ GYRO_GAIN_Y ] 	= UlReadVal;							
		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );															
}


UINT8	WrGyroAngleData( void )
{
	UINT32	UlReadVal ;
	UINT32	UiChkSum1,	UiChkSum2 ;
	UINT32	UlSrvStat,	UlOisStat ;
	UINT8	ans;
	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;													
	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
		UlReadVal = UlBufDat[0];											
		UlReadVal &= ~ANGL_CORR_FLG;
		UlBufDat[ 0 ] 	= UlReadVal;										
		RamRead32A(  GyroFilterTableX_gx45x , &UlReadVal ) ;				
		UlBufDat[ MIXING_GX45X ] 	= UlReadVal;
		RamRead32A(  GyroFilterTableX_gx45y , &UlReadVal ) ;				
		UlBufDat[ MIXING_GX45Y ] 	= UlReadVal;
		RamRead32A(  GyroFilterTableY_gy45y , &UlReadVal ) ;				
		UlBufDat[ MIXING_GY45Y ] 	= UlReadVal;
		RamRead32A(  GyroFilterTableY_gy45x , &UlReadVal ) ;				
		UlBufDat[ MIXING_GY45X ] 	= UlReadVal;
		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );															
}


UINT8	WrGyroOffsetData( void )
{
	UINT32	UlFctryX, UlFctryY;
	UINT32	UlCurrX, UlCurrY;
	UINT32	UlGofX, UlGofY;
	UINT32	UiChkSum1,	UiChkSum2 ;
	UINT32	UlSrvStat,	UlOisStat ;
	UINT8	ans;
	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;													
	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
		RamRead32A(  GYRO_RAM_GXOFFZ , &UlGofX ) ;
		RamWrite32A( StCaliData_SiGyroOffset_X ,	UlGofX ) ;

		RamRead32A(  GYRO_RAM_GYOFFZ , &UlGofY ) ;
		RamWrite32A( StCaliData_SiGyroOffset_Y ,	UlGofY ) ;

		UlCurrX		= UlBufDat[ GYRO_OFFSET_X ] ;
		UlCurrY		= UlBufDat[ GYRO_OFFSET_Y ] ;
		UlFctryX	= UlBufDat[ GYRO_FCTRY_OFST_X ] ;
		UlFctryY	= UlBufDat[ GYRO_FCTRY_OFST_Y ] ;

		if( UlFctryX == 0xFFFFFFFF )
			UlBufDat[ GYRO_FCTRY_OFST_X ] = UlCurrX ;

		if( UlFctryY == 0xFFFFFFFF )
			UlBufDat[ GYRO_FCTRY_OFST_Y ] = UlCurrY ;

		UlBufDat[ GYRO_OFFSET_X ] = UlGofX ;
		UlBufDat[ GYRO_OFFSET_Y ] = UlGofY ;

		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );															

}

#ifdef	SEL_CLOSED_AF
UINT8	WrCLAFData( void )
{
	UINT32	UlReadVal ;
	UINT32	UiChkSum1,	UiChkSum2 ;
	UINT32	UlSrvStat,	UlOisStat ;
	UINT8	ans;
	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;													
	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
		UlReadVal = UlBufDat[0];											
		UlReadVal &= ~CLAF_CALB_FLG;
		UlBufDat[  0 ]	= UlReadVal;										
		UlBufDat[ AF_HALL_BIAS_DAC ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzGan << 16);		
		UlBufDat[ AF_HALL_OFFSET_DAC ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzOff << 16);		
		UlBufDat[ AF_LOOP_GAIN ]		= StAdjPar.StLopGan.UlLzgVal;						
		UlBufDat[ AF_MECHA_CENTER ]		= (UINT32)(StAdjPar.StHalAdj.UsAdzOff << 16);		
		UlBufDat[ AF_HALL_AMP_MAG ]		= (UINT32)(StAdjPar.StHalAdj.UsHlzAmp << 16);		
		UlBufDat[ AF_HALL_MAX_BEFORE ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzMax << 16);		
		UlBufDat[ AF_HALL_MIN_BEFORE ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzMin << 16);		
		UlBufDat[ AF_HALL_MAX_AFTER ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzMxa << 16);		
		UlBufDat[ AF_HALL_MIN_AFTER ]	= (UINT32)(StAdjPar.StHalAdj.UsHlzMna << 16);		
		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );															
}
#endif


UINT8	WrMixingData( void )
{
	UINT32		UlReadVal ;
	UINT32		UiChkSum1,	UiChkSum2 ;
	UINT32		UlSrvStat,	UlOisStat ;
	UnDwdVal	StShift ;
	UINT8		ans;
	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;													
	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
		UlReadVal = UlBufDat[0];											
		UlReadVal &= ~CROS_TALK_FLG;
		UlBufDat[ 0 ] 	= UlReadVal;										
		RamRead32A(  HF_hx45x , &UlReadVal ) ;
		UlBufDat[ MIXING_HX45X	 ] 	= UlReadVal;
		RamRead32A(  HF_hx45y , &UlReadVal ) ;
		UlBufDat[ MIXING_HX45Y	 ] 	= UlReadVal;
		RamRead32A(  HF_hy45y , &UlReadVal ) ;
		UlBufDat[ MIXING_HY45Y	 ] 	= UlReadVal;
		RamRead32A(  HF_hy45x , &UlReadVal ) ;
		UlBufDat[ MIXING_HY45X	 ] 	= UlReadVal;
		RamRead32A(  HF_ShiftX , &StShift.UlDwdVal ) ;
		UlBufDat[ MIXING_HXSX	 ] 	= ((UINT32)StShift.StCdwVal.UcRamVa0 << 24) | ((UINT32)StShift.StCdwVal.UcRamVa1 << 8);
		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );															
}


UINT8	WrFstData( void )
{
	UINT32	UlReadVal ;
	UINT32	UiChkSum1,	UiChkSum2 ;
	UINT32	UlSrvStat,	UlOisStat ;
	UINT8	ans;
	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;													
	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
		UlReadVal = UlBufDat[0];											
		UlReadVal &= ~OPAF_FST_FLG;
		UlBufDat[ 0 ] 	= UlReadVal;										

		RamRead32A(  OLAF_Long_M_RRMD1 , &UlReadVal ) ;						
		UlBufDat[ AF_LONG_M_RRMD1 ] 	= UlReadVal;
		RamRead32A(  OLAF_Long_I_RRMD1 , &UlReadVal ) ;						
		UlBufDat[ AF_LONG_I_RRMD1 ] 	= UlReadVal;

		RamRead32A(  OLAF_Short_IIM_RRMD1 , &UlReadVal ) ;					
		UlBufDat[ AF_SHORT_IIM_RRMD1 ] 	= UlReadVal;
		RamRead32A(  OLAF_Short_IMI_RRMD1 , &UlReadVal ) ;					
		UlBufDat[ AF_SHORT_IMI_RRMD1 ] 	= UlReadVal;

		RamRead32A(  OLAF_Short_MIM_RRMD1 , &UlReadVal ) ;					
		UlBufDat[ AF_SHORT_MIM_RRMD1 ] 	= UlReadVal;
		RamRead32A(  OLAF_Short_MMI_RRMD1 , &UlReadVal ) ;					
		UlBufDat[ AF_SHORT_MMI_RRMD1 ] 	= UlReadVal;

		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );															
}

UINT8	WrMixCalData( UINT8 UcMode, mlMixingValue *mixval )
{
	UINT32	UlReadVal ;
	UINT32	UiChkSum1,	UiChkSum2 ;
	UINT32	UlSrvStat,	UlOisStat ;
	UINT8	ans;


	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;													
	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
		UlReadVal = UlBufDat[0];											
		if( UcMode )
			UlReadVal &= ~CROS_TALK_FLG;
		else
			UlReadVal |= CROS_TALK_FLG;

		UlBufDat[ 0 ] 	= UlReadVal;										

#if 0
		if ( outX & 0x02 ) {
			
			UlBufDat[ MIXING_HXSX ] = mixval->hxsx | (((UINT32)mixval->hysx) << 16) ;

			UlBufDat[ MIXING_HX45X ] = mixval->hy45yL * (-1) ;
			UlBufDat[ MIXING_HX45Y ] = mixval->hy45xL * (-1) ;
			UlBufDat[ MIXING_HY45Y ] = mixval->hx45xL ;
			UlBufDat[ MIXING_HY45X ] = mixval->hx45yL ;
		} else {
			
			UlBufDat[ MIXING_HXSX ] = mixval->hysx | (((UINT32)mixval->hxsx) << 16) ;

			UlBufDat[ MIXING_HX45X ] = mixval->hx45xL ;
			UlBufDat[ MIXING_HX45Y ] = mixval->hx45yL ;
			UlBufDat[ MIXING_HY45Y ] = mixval->hy45yL * (-1) ;
			UlBufDat[ MIXING_HY45X ] = mixval->hy45xL * (-1) ;
		}
#else
		
		UlBufDat[ MIXING_HXSX ] = mixval->hxsx | (((UINT32)mixval->hysx) << 16) ;

		UlBufDat[ MIXING_HX45X ] = mixval->hy45yL * (-1) ;
		UlBufDat[ MIXING_HX45Y ] = mixval->hy45xL ;
		UlBufDat[ MIXING_HY45Y ] = mixval->hx45xL ;
		UlBufDat[ MIXING_HY45X ] = mixval->hx45yL * (-1) ;
#endif

		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );
}


UINT8	ErCalData( UINT16 UsFlag )
{
	UINT32	UlReadVal ;
	UINT32	UiChkSum1,	UiChkSum2 ;
	UINT32	UlSrvStat,	UlOisStat ;
	UINT8	ans;
	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;													
	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
		UlReadVal = UlBufDat[0];											
		
		UlReadVal |= (UINT32)UsFlag ;

		UlBufDat[ 0 ] 	= UlReadVal ;										

		
		if ( UsFlag & HALL_CALB_FLG ) {
			UlBufDat[ HALL_MAX_BEFORE_X	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_MIN_BEFORE_X	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_MAX_AFTER_X	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_MIN_AFTER_X	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_MAX_BEFORE_Y	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_MIN_BEFORE_Y	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_MAX_AFTER_Y	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_MIN_AFTER_Y	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_BIAS_DAC_X	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_OFFSET_DAC_X	 ] 	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_BIAS_DAC_Y	 ] 	= 0xFFFFFFFF ;					
			UlBufDat[ HALL_OFFSET_DAC_Y	 ] 	= 0xFFFFFFFF ;					
			UlBufDat[ LOOP_GAIN_X		 ] 	= 0xFFFFFFFF ;					
			UlBufDat[ LOOP_GAIN_Y		 ]	= 0xFFFFFFFF ;					
			UlBufDat[ MECHA_CENTER_X	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ MECHA_CENTER_Y	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ OPT_CENTER_X		 ]	= 0xFFFFFFFF ;					
			UlBufDat[ OPT_CENTER_Y		 ]	= 0xFFFFFFFF ;					
			UlBufDat[ GYRO_OFFSET_X		 ]	= 0xFFFFFFFF ;					
			UlBufDat[ GYRO_OFFSET_Y		 ]	= 0xFFFFFFFF ;					
#ifdef	SEL_CLOSED_AF
			UlBufDat[ AF_HALL_BIAS_DAC	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ AF_HALL_OFFSET_DAC ]	= 0xFFFFFFFF ;					
			UlBufDat[ AF_LOOP_GAIN		 ]	= 0xFFFFFFFF ;					
			UlBufDat[ AF_MECHA_CENTER	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ AF_HALL_AMP_MAG	 ]	= 0xFFFFFFFF ;					
			UlBufDat[ AF_HALL_MAX_BEFORE ]	= 0xFFFFFFFF ;					
			UlBufDat[ AF_HALL_MIN_BEFORE ]	= 0xFFFFFFFF ;     				
			UlBufDat[ AF_HALL_MAX_AFTER	 ]	= 0xFFFFFFFF ;     				
			UlBufDat[ AF_HALL_MIN_AFTER	 ]	= 0xFFFFFFFF ;      			
#endif
		}

		
		if ( UsFlag & GYRO_GAIN_FLG ) {
			UlBufDat[ GYRO_GAIN_X		 ]	= 0xFFFFFFFF ;					
			UlBufDat[ GYRO_GAIN_Y		 ]	= 0xFFFFFFFF ;					
		}

		
		if ( UsFlag & ANGL_CORR_FLG ) {
			UlBufDat[ MIXING_GX45X		 ] 	= 0xFFFFFFFF ;
			UlBufDat[ MIXING_GX45Y		 ] 	= 0xFFFFFFFF ;
			UlBufDat[ MIXING_GY45Y		 ] 	= 0xFFFFFFFF ;
			UlBufDat[ MIXING_GY45X		 ] 	= 0xFFFFFFFF ;
		}

	#ifdef	SEL_CLOSED_AF
		
		if ( UsFlag & CLAF_CALB_FLG ) {
			UlBufDat[ AF_HALL_BIAS_DAC	 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_HALL_OFFSET_DAC ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_LOOP_GAIN		 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_MECHA_CENTER	 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_HALL_AMP_MAG	 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_HALL_MAX_BEFORE ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_HALL_MIN_BEFORE ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_HALL_MAX_AFTER	 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_HALL_MIN_AFTER	 ] 	= 0xFFFFFFFF ;
		}
	#else
		
		if ( UsFlag & OPAF_FST_FLG ) {
			UlBufDat[ AF_LONG_M_RRMD1	 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_LONG_I_RRMD1	 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_SHORT_IIM_RRMD1 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_SHORT_IMI_RRMD1 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_SHORT_MIM_RRMD1 ] 	= 0xFFFFFFFF ;
			UlBufDat[ AF_SHORT_MMI_RRMD1 ] 	= 0xFFFFFFFF ;
		}
	#endif

		
		if ( UsFlag & HLLN_CALB_FLG ) {
			UlBufDat[ LN_POS1			 ]	= 0xFFFFFFFF ;		
			UlBufDat[ LN_POS2			 ]	= 0xFFFFFFFF ;		
			UlBufDat[ LN_POS3			 ]	= 0xFFFFFFFF ;		
			UlBufDat[ LN_POS4			 ]	= 0xFFFFFFFF ;		
			UlBufDat[ LN_POS5			 ]	= 0xFFFFFFFF ;		
			UlBufDat[ LN_POS6			 ]	= 0xFFFFFFFF ;		
			UlBufDat[ LN_POS7			 ]	= 0xFFFFFFFF ;		
			UlBufDat[ LN_STEP			 ]	= 0xFFFFFFFF ;		
		}

		
		if ( UsFlag & CROS_TALK_FLG ) {
			UlBufDat[ MIXING_HX45X		 ] 	= 0xFFFFFFFF ;
			UlBufDat[ MIXING_HX45Y		 ] 	= 0xFFFFFFFF ;
			UlBufDat[ MIXING_HY45Y		 ] 	= 0xFFFFFFFF ;
			UlBufDat[ MIXING_HY45X		 ] 	= 0xFFFFFFFF ;
			UlBufDat[ MIXING_HXSX		 ] 	= 0xFFFFFFFF ;
		}

		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );															
}

void	RdHallCalData( void )
{
	UnDwdVal		StReadVal ;

	RamRead32A(  StCaliData_UsCalibrationStatus, &StAdjPar.StHalAdj.UlAdjPhs ) ;

	RamRead32A( StCaliData_SiHallMax_Before_X,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlxMax = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_SiHallMin_Before_X, &StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlxMin = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_SiHallMax_After_X,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlxMxa = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_SiHallMin_After_X,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlxMna = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_SiHallMax_Before_Y, &StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlyMax = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_SiHallMin_Before_Y, &StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlyMin = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_SiHallMax_After_Y,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlyMxa = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_SiHallMin_After_Y,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlyMna = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_UiHallBias_X,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlxGan = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_UiHallOffset_X,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlxOff = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_UiHallBias_Y,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlyGan = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_UiHallOffset_Y,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsHlyOff = StReadVal.StDwdVal.UsHigVal ;

	RamRead32A( StCaliData_SiLoopGain_X,	&StAdjPar.StLopGan.UlLxgVal ) ;
	RamRead32A( StCaliData_SiLoopGain_Y,	&StAdjPar.StLopGan.UlLygVal ) ;

	RamRead32A( StCaliData_SiLensCen_Offset_X,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsAdxOff = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_SiLensCen_Offset_Y,	&StReadVal.UlDwdVal ) ;
	StAdjPar.StHalAdj.UsAdyOff = StReadVal.StDwdVal.UsHigVal ;

	RamRead32A( StCaliData_SiGyroOffset_X,		&StReadVal.UlDwdVal ) ;
	StAdjPar.StGvcOff.UsGxoVal = StReadVal.StDwdVal.UsHigVal ;
	RamRead32A( StCaliData_SiGyroOffset_Y,		&StReadVal.UlDwdVal ) ;
	StAdjPar.StGvcOff.UsGyoVal = StReadVal.StDwdVal.UsHigVal ;

}

UINT16	TneADO( )
{
#if 0
	INT16 iRetVal;
	UINT16	UsSts = 0 ;
	UnDwdVal rg ;
	INT32 limit ;
	INT32 gxgain ;
	INT32 gygain ;
	INT16 gout_x_marginp ;
	INT16 gout_x_marginm ;
	INT16 gout_y_marginp ;
	INT16 gout_y_marginm ;

	INT16 x_max ;
	INT16 x_min ;
	INT16 x_off ;
	INT16 y_max ;
	INT16 y_min ;
	INT16 y_off ;
	INT16 x_max_after ;
	INT16 x_min_after ;
	INT16 y_max_after ;
	INT16 y_min_after ;
	INT16 gout_x ;
	INT16 gout_y ;

	UINT32	UiChkSum1,	UiChkSum2 ;
	UINT32	UlSrvStat,	UlOisStat ;

	ReadCalDataF40( UlBufDat, &UiChkSum2 );

	
	RdHallCalData();

	x_max = (INT16)StAdjPar.StHalAdj.UsHlxMxa ;
	x_min = (INT16)StAdjPar.StHalAdj.UsHlxMna ;
	x_off = (INT16)StAdjPar.StHalAdj.UsAdxOff ;
	y_max = (INT16)StAdjPar.StHalAdj.UsHlyMxa ;
	y_min = (INT16)StAdjPar.StHalAdj.UsHlyMna ;
	y_off = (INT16)StAdjPar.StHalAdj.UsAdyOff ;

	RamRead32A( GF_LimitX_HLIMT,	&limit ) ;
	RamRead32A( StCaliData_SiGyroGain_X,	&gxgain ) ;
	RamRead32A( StCaliData_SiGyroGain_Y,	&gygain ) ;
	RamRead32A( GyroFilterShiftX,	&rg.UlDwdVal ) ;

	x_max_after = (x_max - x_off) ;
	if (x_off < 0)
	{
	    if ((0x7FFF - abs(x_max)) < abs(x_off)) x_max_after = 0x7FFF ;
	}

	x_min_after = (x_min - x_off) ;
	if (x_off > 0)
	{
	    if ((0x7FFF - abs(x_min)) < abs(x_off)) x_min_after = 0x8001 ;
	}

	y_max_after = (y_max - y_off) ;
	if (y_off < 0)
	{
	    if ((0x7FFF - abs(y_max)) < abs(y_off)) y_max_after = 0x7FFF ;
	}

	y_min_after = (y_min - y_off);
	if (y_off > 0)
	{
	    if ((0x7FFF - abs(y_min)) < abs(y_off)) y_min_after = 0x8001 ;
	}

	gout_x = (INT16)((INT32)(((float)gxgain / 0x7FFFFFFF) * limit * (2^rg.StCdwVal.UcRamVa1)) >> 16);
	gout_y = (INT16)((INT32)(((float)gygain / 0x7FFFFFFF) * limit * (2^rg.StCdwVal.UcRamVa1)) >> 16);


	gout_x_marginp = (INT16)(gout_x + LENS_MARGIN);			
	gout_x_marginm = (INT16)((gout_x + LENS_MARGIN) * -1);	
	gout_y_marginp = (INT16)(gout_y + LENS_MARGIN);			
	gout_y_marginm = (INT16)((gout_y + LENS_MARGIN) * -1);	



	
	if (x_max_after < gout_x) {
		UsSts = 1 ;
	}
	else if (y_max_after < gout_y) {
		UsSts = 2 ;
	}
	else if (x_min_after > (gout_x * -1)) {
		UsSts = 3 ;
	}
	else if (y_min_after > (gout_y * -1)) {
		UsSts = 4 ;
	}
	else {
		
		if (x_max_after < gout_x_marginp) {
			x_off -= (gout_x_marginp - x_max_after);
		}
		if (x_min_after > gout_x_marginm) {
			x_off += abs(x_min_after - gout_x_marginm);
		}
		if (y_max_after < gout_y_marginp) {
			y_off -= (gout_y_marginp - y_max_after);
		}
		if (y_min_after > gout_y_marginm) {
			y_off += abs(y_min_after - gout_y_marginm);
		}
		
		if ( (StAdjPar.StHalAdj.UsAdxOff != (UINT16)x_off) || (StAdjPar.StHalAdj.UsAdyOff != (UINT16)y_off) ) {
			StAdjPar.StHalAdj.UsAdxOff = x_off ;
			StAdjPar.StHalAdj.UsAdyOff = y_off ;

			RamWrite32A( StCaliData_SiLensCen_Offset_X ,	(UINT32)(StAdjPar.StHalAdj.UsAdxOff << 16) ) ;
			RamWrite32A( StCaliData_SiLensCen_Offset_Y ,	(UINT32)(StAdjPar.StHalAdj.UsAdyOff << 16) ) ;

			
			RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
			RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
			RtnCen( BOTH_OFF ) ;												
			iRetVal = EraseCalDataF40();
			if ( iRetVal != 0 ) return( iRetVal );

			UlBufDat[ MECHA_CENTER_X ] = (UINT32)(StAdjPar.StHalAdj.UsAdxOff << 16) ;
			UlBufDat[ MECHA_CENTER_Y ] = (UINT32)(StAdjPar.StHalAdj.UsAdyOff << 16) ;

			WriteCalDataF40( UlBufDat, &UiChkSum1 );							
			ReadCalDataF40( UlBufDat, &UiChkSum2 );								

			if(UiChkSum1 != UiChkSum2 ){
				iRetVal = 0x10;													
				return( iRetVal );
			}

			if( !UlSrvStat ) {
				RtnCen( BOTH_OFF ) ;
			} else if( UlSrvStat == 3 ) {
				RtnCen( BOTH_ON ) ;
			} else {
				RtnCen( UlSrvStat ) ;
			}

			if( UlOisStat != 0)               OisEna() ;

			
			x_max_after = (x_max - x_off) ;
			if (x_off < 0)
			{
			    if ((0x7FFF - abs(x_max)) < abs(x_off)) x_max_after = 0x7FFF ;
			}

			x_min_after = (x_min - x_off) ;
			if (x_off > 0)
			{
			    if ((0x7FFF - abs(x_min)) < abs(x_off)) x_min_after = 0x8001 ;
			}

			y_max_after = (y_max - y_off) ;
			if (y_off < 0)
			{
			    if ((0x7FFF - abs(y_max)) < abs(y_off)) y_max_after = 0x7FFF ;
			}

			y_min_after = (y_min - y_off);
			if (y_off > 0)
			{
			    if ((0x7FFF - abs(y_min)) < abs(y_off)) y_min_after = 0x8001 ;
			}
		}
	}

	
	
	
	if (UsSts == 0) {
		UINT16 UsReadVal ;
		float flDistanceX, flDistanceY ;
		float flDistanceAD = SLT_OFFSET * 6 ;

		UsReadVal = abs((UlBufDat[ LN_POS7 ] >> 16) - (UlBufDat[ LN_POS1 ] >> 16)) ;
		flDistanceX = ((float)UsReadVal) / 10.0f ;

		

		UsReadVal = abs((UlBufDat[ LN_POS7 ] & 0xFFFF) - (UlBufDat[ LN_POS1 ] & 0xFFFF)) ;
		flDistanceY = ((float)UsReadVal) / 10.0f ;

		if ( (x_max_after * (flDistanceX / flDistanceAD)) < SPEC_PIXEL ) {
			
			UsSts |= 0x0100 ;
		}
		else if ( (y_max_after * (flDistanceY / flDistanceAD)) < SPEC_PIXEL ) {
			
			UsSts |= 0x0200 ;
		}
		else if ( (abs(x_min_after) * (flDistanceX / flDistanceAD)) < SPEC_PIXEL ) {
			
			UsSts |= 0x0300 ;
		}
		else if ( (abs(y_min_after) * (flDistanceY / flDistanceAD)) < SPEC_PIXEL ) {
			
			UsSts |= 0x0400 ;
		}
	}
#else
	UINT16	UsSts = 0 ;
#endif

	return( UsSts ) ;
}


#if 0
void	SetHalLnData( UINT16 *UsPara )
{
	UINT8	i;
	UINT16	UsRdAdr;

	UsRdAdr = HAL_LN_COEFAX;

	for( i=0; i<17 ; i++ ){
		RamWrite32A( UsRdAdr + (i * 4) , (UINT32)UsPara[i*2+1] << 16 | (UINT32)UsPara[i*2] );
	}
}
#endif

#if 0
UINT8	WrHalLnData( UINT8 UcMode )
{
	UINT32		UlReadVal ;
	UINT32		UiChkSum1,	UiChkSum2 ;
	UINT32		UlSrvStat,	UlOisStat ;
	UINT16		UsRdAdr;
	UnDwdVal	StRdDat[17];
	UnDwdVal	StWrDat;
	UINT8		i;
	UnDwdVal	StShift ;
	UINT8		ans;
	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;													
	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ){
	UlReadVal = UlBufDat[0];												
	if( UcMode ){
		UlReadVal &= ~HLLN_CALB_FLG;
	}else{
		UlReadVal |= HLLN_CALB_FLG;
	}
	UlBufDat[ 0 ] 	= UlReadVal;											

	UsRdAdr = HAL_LN_COEFAX;
	for( i=0; i<17 ; i++ ){
		RamRead32A( UsRdAdr + (i * 4) , &(StRdDat[i].UlDwdVal));
	}

	for( i=0; i<9 ; i++ ){
							
		StWrDat.UlDwdVal = (INT32)StRdDat[i+8].StDwdVal.UsHigVal << 16 | StRdDat[i].StDwdVal.UsLowVal ;
		
		UlBufDat[ 44 + i*2 ] 	= StWrDat.UlDwdVal;
	}

	for( i=0; i<8 ; i++ ){
							
		StWrDat.UlDwdVal = (INT32)StRdDat[i+9].StDwdVal.UsLowVal << 16 | StRdDat[i].StDwdVal.UsHigVal ;
		
		UlBufDat[ 45 + i*2 ] 	= StWrDat.UlDwdVal;
	}
		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;

	return( ans );															
}

#endif


UINT8	WrLinCalData( UINT8 UcMode, mlLinearityValue *linval )
{
#if 0
	UINT32		UlReadVal ;
	UINT32		UiChkSum1,	UiChkSum2 ;
	UINT32		UlSrvStat,	UlOisStat ;
	UINT8		ans;
	double		*pPosX, *pPosY;

	RamRead32A( CMD_RETURN_TO_CENTER , &UlSrvStat ) ;
	RamRead32A( CMD_OIS_ENABLE , &UlOisStat ) ;
	RtnCen( BOTH_OFF ) ;													

	ReadCalDataF40( UlBufDat, &UiChkSum2 );
	ans = EraseCalDataF40();
	if ( ans == 0 ) {
		UlReadVal = UlBufDat[0];											
		if( UcMode ){
			UlReadVal &= ~HLLN_CALB_FLG;
		}else{
			UlReadVal |= HLLN_CALB_FLG;
		}
		UlBufDat[ 0 ] 	= UlReadVal;										

#if 0
		pPosX = linval->positionX;
		pPosY = linval->positionY;
#else
		
		pPosX = linval->positionY;
		pPosY = linval->positionX;
#endif

#ifdef	_BIG_ENDIAN_
		UlBufDat[ LN_POS1 ]	= (UINT32)(*pPosX * 10) | ((UINT32)(*pPosY *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS2 ]	= (UINT32)(*pPosX * 10) | ((UINT32)(*pPosY *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS3 ]	= (UINT32)(*pPosX * 10) | ((UINT32)(*pPosY *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS4 ]	= (UINT32)(*pPosX * 10) | ((UINT32)(*pPosY *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS5 ]	= (UINT32)(*pPosX * 10) | ((UINT32)(*pPosY *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS6 ]	= (UINT32)(*pPosX * 10) | ((UINT32)(*pPosY *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS7 ]	= (UINT32)(*pPosX * 10) | ((UINT32)(*pPosY *10) << 16); pPosX++; pPosY++;		
#else
		UlBufDat[ LN_POS1 ]	= (UINT32)(*pPosY * 10) | ((UINT32)(*pPosX *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS2 ]	= (UINT32)(*pPosY * 10) | ((UINT32)(*pPosX *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS3 ]	= (UINT32)(*pPosY * 10) | ((UINT32)(*pPosX *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS4 ]	= (UINT32)(*pPosY * 10) | ((UINT32)(*pPosX *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS5 ]	= (UINT32)(*pPosY * 10) | ((UINT32)(*pPosX *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS6 ]	= (UINT32)(*pPosY * 10) | ((UINT32)(*pPosX *10) << 16); pPosX++; pPosY++;		
		UlBufDat[ LN_POS7 ]	= (UINT32)(*pPosY * 10) | ((UINT32)(*pPosX *10) << 16); pPosX++; pPosY++;		
#endif
		UlBufDat[ LN_STEP ]	= ((linval->dacY[1] - linval->dacY[0]) >> 16) | ((linval->dacX[1] - linval->dacX[0]) & 0xFFFF0000);					

		WriteCalDataF40( UlBufDat, &UiChkSum1 );							
		ReadCalDataF40( UlBufDat, &UiChkSum2 );								

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;														
		}
	}
	if( !UlSrvStat ) {
		RtnCen( BOTH_OFF ) ;
	} else if( UlSrvStat == 3 ) {
		RtnCen( BOTH_ON ) ;
	} else {
		RtnCen( UlSrvStat ) ;
	}

	if( UlOisStat != 0)               OisEna() ;
#else
	UINT8		ans = 0;
#endif
    
	return( ans );															
}

void	OscStb( void )
{
	RamWrite32A( CMD_IO_ADR_ACCESS , STBOSCPLL ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS , OSC_STB ) ;
}

#if 0
UINT8	GyrSlf( void )
{
	UINT8		UcFinSts = 0 ;
	float		flGyrRltX ;
	float		flGyrRltY ;
	float		flMeasureAveValueA , flMeasureAveValueB ;
	INT32		SlMeasureParameterNum ;
	INT32		SlMeasureParameterA, SlMeasureParameterB ;
	UnllnVal	StMeasValueA , StMeasValueB ;
	UINT32		UlReadVal;

	
	RamWrite32A( 0xF01D, 0x75000000 );	
	while( RdStatus( 0 ) ) ;

	RamRead32A ( 0xF01D, &UlReadVal );

	if( (UlReadVal >> 24) != 0x85 )
	{
		return	(0xFF);
	}

	
	RamWrite32A( 0xF01E, 0x1B100000 );	
	while( RdStatus( 0 ) ) ;

	MesFil( SELFTEST ) ;

	SlMeasureParameterNum	=	20 * 4;									
	SlMeasureParameterA		=	(UINT32)GYRO_RAM_GX_ADIDAT ;			
	SlMeasureParameterB		=	(UINT32)GYRO_RAM_GY_ADIDAT ;			

	ClrMesFil() ;														
	WitTim( 300 ) ;

	RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, 0x00000000 ) ;			

	
	MeasureStart( SlMeasureParameterNum, SlMeasureParameterA , SlMeasureParameterB ) ;
	MeasureWait() ;														


	RamRead32A( StMeasFunc_MFA_LLiIntegral1 	, &StMeasValueA.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFA_LLiIntegral1 + 4 , &StMeasValueA.StUllnVal.UlHigVal ) ;

	RamRead32A( StMeasFunc_MFB_LLiIntegral2 	, &StMeasValueB.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFB_LLiIntegral2 + 4	, &StMeasValueB.StUllnVal.UlHigVal ) ;

	flMeasureAveValueA = (float)((( (INT64)StMeasValueA.UllnValue >> 16 ) / (float)SlMeasureParameterNum ) ) ;
	flMeasureAveValueB = (float)((( (INT64)StMeasValueB.UllnValue >> 16 ) / (float)SlMeasureParameterNum ) ) ;

	flGyrRltX = flMeasureAveValueA / 175.0 ;	
	flGyrRltY = flMeasureAveValueB / 175.0 ;	


	if( fabs(flGyrRltX) >= 25 ){
		UcFinSts |= 0x10;
	}

	if( fabs(flGyrRltY) >= 25 ){
		UcFinSts |= 0x01;
	}

	
	RamWrite32A( 0xF01E, 0x1BDB0000 );	
	while( RdStatus( 0 ) ) ;

	ClrMesFil() ;														
	WitTim( 300 ) ;

	RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, 0x00000000 ) ;			

	
	MeasureStart( SlMeasureParameterNum, SlMeasureParameterA , SlMeasureParameterB ) ;
	MeasureWait() ;														


	RamRead32A( StMeasFunc_MFA_LLiIntegral1 	, &StMeasValueA.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFA_LLiIntegral1 + 4 , &StMeasValueA.StUllnVal.UlHigVal ) ;

	RamRead32A( StMeasFunc_MFB_LLiIntegral2 	, &StMeasValueB.StUllnVal.UlLowVal ) ;	
	RamRead32A( StMeasFunc_MFB_LLiIntegral2 + 4	, &StMeasValueB.StUllnVal.UlHigVal ) ;

	flMeasureAveValueA = (float)((( (INT64)StMeasValueA.UllnValue >> 16 ) / (float)SlMeasureParameterNum ) ) ;
	flMeasureAveValueB = (float)((( (INT64)StMeasValueB.UllnValue >> 16 ) / (float)SlMeasureParameterNum ) ) ;

	flGyrRltX = flMeasureAveValueA / GYRO_SENSITIVITY ;
	flGyrRltY = flMeasureAveValueB / GYRO_SENSITIVITY ;



	if( UcFinSts != 0 )	{
		
		if( fabs(flGyrRltX) >= 60){
			UcFinSts |= 0x20;
		}

		if( fabs(flGyrRltY) >= 60){
			UcFinSts |= 0x02;
		}
	} else {
		
		if( fabs(flGyrRltX) < 60){
			UcFinSts |= 0x20;
		}

		if( fabs(flGyrRltY) < 60){
			UcFinSts |= 0x02;
		}
	}

	
	RamWrite32A( 0xF01E, 0x1B180000 );	
	while( RdStatus( 0 ) ) ;


	return( UcFinSts ) ;
}
#endif

#ifdef	SEL_CLOSED_AF
UINT32 AFHallAmp( void )
{
	INT32		SlMeasureParameterA , SlMeasureParameterB ;
	INT32		SlMeasureParameterNum ;
	UINT32 		UlDatVal;
	UnllnVal	StMeasValueA ;

	SlMeasureParameterNum = 1024;													

	RtnCen( BOTH_OFF ) ;													

	SlMeasureParameterA		=	CLAF_RAMA_AFADIN ;									
	SlMeasureParameterB		=	CLAF_RAMA_AFADIN ;									

	RamWrite32A( 0x03FC ,	0x7FFFFFFF) ;											
	WitTim(500);
	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;		
	MeasureWait() ;						

	RamRead32A( StMeasFunc_MFA_LLiIntegral1, &StMeasValueA.StUllnVal.UlLowVal ) ;		
	RamRead32A( StMeasFunc_MFA_LLiIntegral1+4, &StMeasValueA.StUllnVal.UlHigVal) ;		

	 UlDatVal = (UINT32)( (INT64)StMeasValueA.UllnValue / SlMeasureParameterNum ) ;

	RamWrite32A( MESHGH,UlDatVal) ;													
	SlMeasureParameterNum = 1024;														

	RamWrite32A( 0x03FC ,	0x80000000) ;												
	WitTim(500);
	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;	
	MeasureWait() ;						

	RamRead32A( StMeasFunc_MFA_LLiIntegral1, &StMeasValueA.StUllnVal.UlLowVal ) ;		
	RamRead32A( StMeasFunc_MFA_LLiIntegral1+4, &StMeasValueA.StUllnVal.UlHigVal) ;		

	UlDatVal = (UINT32)( (INT64)StMeasValueA.UllnValue / SlMeasureParameterNum ) ;

	RamWrite32A( MESLOW	,UlDatVal) ;													
	RamWrite32A( 0x03FC ,	0x0) ;														

	return(( UINT32 )0x0202);
}
#endif

#if 0
UINT32 AFTmp25(void)
{
	INT32		SlMeasureParameterA , SlMeasureParameterB ;
	INT32		SlMeasureParameterNum ;
	UINT32 		UlDatVal;
	UnllnVal	StMeasValueA ;

	SlMeasureParameterNum = 1024;													
	RtnCen( BOTH_OFF ) ;															
	SlMeasureParameterA		=	0x0620;												
	SlMeasureParameterB		=	0x0620;												

	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;		
	MeasureWait() ;						

	RamRead32A( StMeasFunc_MFA_LLiIntegral1, &StMeasValueA.StUllnVal.UlLowVal ) ;		
	RamRead32A( StMeasFunc_MFA_LLiIntegral1+4, &StMeasValueA.StUllnVal.UlHigVal) ;	

	 UlDatVal = (UINT32)( (INT64)StMeasValueA.UllnValue / SlMeasureParameterNum ) ;


	RamWrite32A( 0x0624		,UlDatVal) ;										



	return(UlDatVal);
}

void AFMesUpDn(void)
{
	INT32		SlMeasureParameterA , SlMeasureParameterB ;
	INT32		SlMeasureParameterNum ;
	UINT32 		UlDatVal;
	UnllnVal	StMeasValueA ;

	SlMeasureParameterNum = 1024;														

	RamRead32A( 0x03C0		,	( UINT32 * )&SlMeasureParameterA ) ;					
	RamWrite32A( 0x03C0		,	SlMeasureParameterA & 0xFE) ;							

	SlMeasureParameterA		=	0x0620;													
	SlMeasureParameterB		=	0x0620;													

	RamWrite32A( 0x03FC ,	0x7FFFFFFF) ;												
	WitTim(500);
	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;	
	MeasureWait() ;						

	RamRead32A( StMeasFunc_MFA_LLiIntegral1, &StMeasValueA.StUllnVal.UlLowVal ) ;		
	RamRead32A( StMeasFunc_MFA_LLiIntegral1+4, &StMeasValueA.StUllnVal.UlHigVal) ;		

	 UlDatVal = (UINT32)( (INT64)StMeasValueA.UllnValue / SlMeasureParameterNum ) ;

	RamWrite32A( MESHGH,UlDatVal) ;														
	SlMeasureParameterNum = 1024;														

	RamWrite32A( 0x03FC ,	0x80000000) ;												
	WitTim(500);
	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;	
	MeasureWait() ;						

	RamRead32A( StMeasFunc_MFA_LLiIntegral1, &StMeasValueA.StUllnVal.UlLowVal ) ;		
	RamRead32A( StMeasFunc_MFA_LLiIntegral1+4, &StMeasValueA.StUllnVal.UlHigVal) ;		

	UlDatVal = (UINT32)( (INT64)StMeasValueA.UllnValue / SlMeasureParameterNum ) ;

	RamWrite32A( MESLOW	,UlDatVal) ;													
	RamWrite32A( 0x03FC ,	0x0) ;														


}
#endif

UINT8	GyroReCalib( stReCalib * pReCalib )
{
	UINT8	UcSndDat ;
	UINT32	UlRcvDat ;
	UINT32	UlGofX, UlGofY ;
	UINT32	UiChkSum ;

	ReadCalDataF40( UlBufDat, &UiChkSum );

	
	RamWrite32A( CMD_CALIBRATION , 0x00000000 ) ;

	do {
		UcSndDat = RdStatus(1);
	} while (UcSndDat != 0);

	RamRead32A( CMD_CALIBRATION , &UlRcvDat ) ;
	UcSndDat = (unsigned char)(UlRcvDat >> 24);								

	
	if( UlBufDat[ GYRO_FCTRY_OFST_X ] == 0xFFFFFFFF )
		pReCalib->SsFctryOffX = (UlBufDat[ GYRO_OFFSET_X ] >> 16) ;
	else
		pReCalib->SsFctryOffX = (UlBufDat[ GYRO_FCTRY_OFST_X ] >> 16) ;

	if( UlBufDat[ GYRO_FCTRY_OFST_Y ] == 0xFFFFFFFF )
		pReCalib->SsFctryOffY = (UlBufDat[ GYRO_OFFSET_Y ] >> 16) ;
	else
		pReCalib->SsFctryOffY = (UlBufDat[ GYRO_FCTRY_OFST_Y ] >> 16) ;

	
	RamRead32A(  GYRO_RAM_GXOFFZ , &UlGofX ) ;
	RamRead32A(  GYRO_RAM_GYOFFZ , &UlGofY ) ;

	pReCalib->SsRecalOffX = (UlGofX >> 16) ;
	pReCalib->SsRecalOffY = (UlGofY >> 16) ;
	pReCalib->SsDiffX = pReCalib->SsFctryOffX - pReCalib->SsRecalOffX ;
	pReCalib->SsDiffY = pReCalib->SsFctryOffY - pReCalib->SsRecalOffY ;


	return( UcSndDat );
}

#if 0
void GetDir( UINT8 *outX, UINT8 *outY )
{
	UINT32	UlReadVal;

	RamWrite32A( CMD_IO_ADR_ACCESS, DRVCH1SEL ) ;
	RamRead32A ( CMD_IO_DAT_ACCESS, &UlReadVal ) ;
	*outX = (UINT8)(UlReadVal & 0xFF);

	RamWrite32A( CMD_IO_ADR_ACCESS, DRVCH2SEL ) ;
	RamRead32A ( CMD_IO_DAT_ACCESS, &UlReadVal ) ;
	*outY = (UINT8)(UlReadVal & 0xFF);
}
#endif

UINT32	ReadCalibID( void )
{
	UINT32	UlCalibId;

	
	RamRead32A( SiCalID, &UlCalibId );

	return( UlCalibId );
}


UINT8 FrqDet( void )
{
	INT32 SlMeasureParameterA , SlMeasureParameterB ;
	INT32 SlMeasureParameterNum ;
	UINT32 UlXasP_P , UlYasP_P ;
#ifdef	SEL_CLOSED_AF
	UINT32 UlAasP_P ;
#endif	

	UINT8 UcRtnVal;

	UcRtnVal = 0;

	
	MesFil( OSCCHK ) ;													

	SlMeasureParameterNum	=	1000 ;									
	SlMeasureParameterA		=	(UINT32)HALL_RAM_HXOUT0 ;				
	SlMeasureParameterB		=	(UINT32)HALL_RAM_HYOUT0 ;				

	ClrMesFil() ;														
	WitTim( 300 ) ;

	RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, 0x00000000 ) ;			

	
	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;
	SetWaitTime(1) ;
	MeasureWait() ;														
	RamRead32A( StMeasFunc_MFA_UiAmp1, &UlXasP_P ) ;					
	RamRead32A( StMeasFunc_MFB_UiAmp2, &UlYasP_P ) ;					

	WitTim( 50 ) ;

	
	if(  UlXasP_P > ULTHDVAL ){
		UcRtnVal = 1;
	}
	
	if(  UlYasP_P > ULTHDVAL ){
		UcRtnVal |= 2;
	}

#ifdef	SEL_CLOSED_AF
	
	SlMeasureParameterA		=	(UINT32)CLAF_RAMA_AFDEV ;		
	SlMeasureParameterB		=	(UINT32)CLAF_RAMA_AFDEV ;		

	

	WitTim( 300 ) ;

	
	MeasureStart( SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB ) ;
	SetWaitTime(1) ;
	MeasureWait() ;														
	RamRead32A( StMeasFunc_MFA_UiAmp1, &UlAasP_P ) ;					

	WitTim( 50 ) ;

	
	if(  UlAasP_P > ULTHDVAL ){
		UcRtnVal |= 4;
	}

#endif	

	return(UcRtnVal);													
}
