/**
 * @brief		FRA measurement command for LC898123 F40
 *
 * @author		Copyright (C) 2016, ON Semiconductor, all right reserved.
 *
 * @file		OisFRA.c
 * @date		svn:$Date:: 2016-07-29 18:11:58 +0900#$
 * @version	svn:$Revision: 64 $
 * @attention
 **/

#define		__OISFRA__

#include	<math.h>
#include	"Ois.h"


#define		RAM_XSIN	HALL_FRA_XSININ
#define		RAM_XIN1	HALL_FRA_XHOUTB	
#define		RAM_XIN2	HALL_FRA_XHOUTA	

#define		RAM_YSIN	HALL_FRA_YSININ
#define		RAM_YIN1	HALL_FRA_YHOUTB	
#define		RAM_YIN2	HALL_FRA_YHOUTA	

#define		RAM_ZSIN	CLAF_RAMA_AFSINE
#define		RAM_ZIN1	CLAF_RAMA_AFDEV
#define		RAM_ZIN2	CLAF_DELAY_AFDZ0

							


extern	void RamWrite32A( INT32, INT32 );
extern 	void RamRead32A( UINT16, void * );
extern void	WitTim( UINT16 );

void	FRA_Avg( void ) ;
void	FRA_Calc( StFRAMes_t *, UINT8 ) ;
UINT32	Freq_Convert( float ) ;

extern void	SetSineWave(   UINT8 , UINT8 );
extern void	SetSinWavGenInt( void );
extern void	SetTransDataAdr( UINT16, UINT32  ) ;
extern void	MeasureWait( void ) ;
extern void	ClrMesFil( void ) ;
extern void	SetWaitTime( UINT16 ) ;

void	FRA_Avg( void )
{
	UINT8	i, j ;
	float	SfTempGain, SfTempPhase ;

	
	for( i = 0; i < StFRAParam.StHostCom.UcAvgCycl - 1; i++ ) {
		for( j = i + 1; j < StFRAParam.StHostCom.UcAvgCycl; j++ ) {
			if( StFRAParam.SfGain[ j ]	< StFRAParam.SfGain[ i ] ) {
				SfTempGain				= StFRAParam.SfGain[ i ] ;
				StFRAParam.SfGain[ i ]	= StFRAParam.SfGain[ j ] ;
				StFRAParam.SfGain[ j ]	= SfTempGain ;
			}

			if( StFRAParam.SfPhase[ j ] < StFRAParam.SfPhase[ i ] ) {
				SfTempPhase				= StFRAParam.SfPhase[ i ] ;
				StFRAParam.SfPhase[ i ]	= StFRAParam.SfPhase[ j ] ;
				StFRAParam.SfPhase[ j ]	= SfTempPhase ;
			}
		}
	}

	
	if( StFRAParam.StHostCom.UcAvgCycl & 0x01 ) {
		i = StFRAParam.StHostCom.UcAvgCycl / 2 ;
	} else {
		i= ( StFRAParam.StHostCom.UcAvgCycl / 2 ) - 1 ;
	}

	StFRAParam.StMesRslt.SfGainAvg	= StFRAParam.SfGain[ i ] ;
	StFRAParam.StMesRslt.SfPhaseAvg	= StFRAParam.SfPhase[ i ] ;

	return ;
}



void	FRA_Calc( StFRAMes_t *PtFRAMes, UINT8 UcMesCnt )
{
	float	SfTempGain, SfTempPhase ;

	SfTempGain	= ( float )PtFRAMes->UllCumulAdd1 / ( float )PtFRAMes->UllCumulAdd2 ;
	StFRAParam.SfGain[ UcMesCnt ]	= 20.0F * log10f( SfTempGain ) ;

	SfTempPhase	= StFRAParam.StHostCom.SfFrqCom.SfFltVal / FS_FREQ ;
	StFRAParam.SfPhase[ UcMesCnt ]	= SfTempPhase * (( float )PtFRAMes->UsFsCount * 360.0F) ;

	return ;
}



UINT32	Freq_Convert( float SfFreq )
{
	UINT32	UlPhsStep ;

	UlPhsStep	= ( UINT32 )( ( SfFreq * ( float )0x100000000 / FS_FREQ + 0.5F ) / 2.0F ) ;

	return( UlPhsStep ) ;
}



void	MesStart_FRA_Single( UINT8	UcSingleSweepSel, UINT8	UcDirSel )
{
	UnllnVal		StMeasValueA, StMeasValueB ;
	INT32			SlMeasureParameterA, SlMeasureParameterB ;
	float			SfTmp ;
	UINT8	i ;
	UINT32	UlSampleNum, UlReadVal ;

	if( UcDirSel == X_DIR ) {																		
		SlMeasureParameterA		=	RAM_XIN1 ;														
		SlMeasureParameterB		=	RAM_XIN2 ;														
	} else if( UcDirSel == Y_DIR ) {																
		SlMeasureParameterA		=	RAM_YIN1 ;														
		SlMeasureParameterB		=	RAM_YIN2 ;														
#ifdef	SEL_CLOSED_AF
	} else {																						
		SlMeasureParameterA		=	RAM_ZIN1 ;														
		SlMeasureParameterB		=	RAM_ZIN2 ;														
#endif
	}

	SetSinWavGenInt() ;

	RamWrite32A( SinWave_Offset,	Freq_Convert( StFRAParam.StHostCom.SfFrqCom.SfFltVal ) ) ;		

	SfTmp	= StFRAParam.StHostCom.SfAmpCom.SfFltVal / 1400.0F ;									
	RamWrite32A( SinWave_Gain,		( UINT32 )( ( float )0x7FFFFFFF * SfTmp ) ) ;					

	if( UcDirSel == X_DIR ) {
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)RAM_XSIN ) ;								
	}else if( UcDirSel == Y_DIR ){
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)RAM_YSIN ) ;								
#ifdef	SEL_CLOSED_AF
	}else{
		SetTransDataAdr( SinWave_OutAddr	,	(UINT32)RAM_ZSIN ) ;								
#endif
	}

	RamWrite32A( SinWaveC_Regsiter	,	0x00000001 ) ;												

	
#if LPF_ENA
	
	UlReadVal	= ( UINT32 )( ( float )0x7FFFFFFF * ( FS_FREQ / ( FS_FREQ + M_PI * StFRAParam.StHostCom.SfFrqCom.SfFltVal ) ) ) ;

	
	UlSampleNum	= 2 * UlReadVal - 0x7FFFFFFF ;
	RamWrite32A ( MeasureFilterA_Coeff_c1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_c1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Coeff_c2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_c2, UlSampleNum ) ;

	
	UlSampleNum	= 0x7FFFFFFF - UlReadVal ;
	RamWrite32A ( MeasureFilterA_Coeff_a1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_a1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Coeff_b1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_b1, UlSampleNum ) ;

	
	UlSampleNum	= 2 * ( 0x7FFFFFFF - UlReadVal ) ;
	RamWrite32A ( MeasureFilterA_Coeff_a2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_a2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Coeff_b2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_b2, UlSampleNum ) ;

	ClrMesFil() ;																					
	SetWaitTime( 500 ) ;

#else	
	
	UlReadVal	= ( UINT32 )( ( float )0x7FFFFFFF * ( StFRAParam.StHostCom.SfFrqCom.SfFltVal / (StFRAParam.StHostCom.SfFrqCom.SfFltVal + FS_FREQ /  M_PI ) ) ) ;

	
	UlSampleNum	= 0x7FFFFFFF - ( 2 * UlReadVal ) ;
	RamWrite32A ( MeasureFilterA_Coeff_c1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_c1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Coeff_c2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_c2, UlSampleNum ) ;

	
	UlSampleNum	= UlReadVal ;
	RamWrite32A ( MeasureFilterA_Coeff_a1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_a1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Coeff_b1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_b1, UlSampleNum ) ;

	
	UlSampleNum	= ( 0x7FFFFFFF - UlReadVal ) ;
	RamWrite32A ( MeasureFilterA_Coeff_a2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_a2, UlSampleNum ) ;
	UlSampleNum = ~UlSampleNum + 1;
	RamWrite32A ( MeasureFilterA_Coeff_b2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Coeff_b2, UlSampleNum ) ;

	ClrMesFil() ;																					
	SetWaitTime( (int)(8000 / StFRAParam.StHostCom.SfFrqCom.SfFltVal) ) ;
#endif	


	if( UcSingleSweepSel ) {
		RamRead32A( StMeasFunc_PMC_UcPhaseMesMode, &UlReadVal ) ;
		UlReadVal	|= 0x00000300 ;																	
		RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, UlReadVal ) ;									
	}

	for( i = 0 ; i < StFRAParam.StHostCom.UcAvgCycl ; i++ ) {
		RamRead32A( StMeasFunc_PMC_UcPhaseMesMode, &UlReadVal ) ;
		UlReadVal	|= 0x00000001 ;																	
		RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, UlReadVal ) ;									
		RamWrite32A( StMeasFunc_MFA_LLiAbsInteg1, 0x00000000 ) ;									
		RamWrite32A( StMeasFunc_MFA_LLiAbsInteg1 + 4, 0x00000000 ) ;								
		RamWrite32A( StMeasFunc_MFB_LLiAbsInteg2, 0x00000000 ) ;									
		RamWrite32A( StMeasFunc_MFB_LLiAbsInteg2 + 4, 0x00000000 ) ;								
		RamWrite32A( StMeasFunc_PMC_SiFsCountF, 0x00000000 ) ;										
		RamWrite32A( StMeasFunc_PMC_SiFsCountR, 0x00000000 ) ;										
		RamWrite32A( StMeasFunc_PMC_UcCrossDetectA, 0x00000000 ) ;									

		SetTransDataAdr( StMeasFunc_MFA_PiMeasureRam1, ( UINT32 )SlMeasureParameterA ) ;			
		SetTransDataAdr( StMeasFunc_MFB_PiMeasureRam2, ( UINT32 )SlMeasureParameterB ) ;			
		RamWrite32A( StMeasFunc_MFA_SiSampleNumA, 0 ) ;												
		RamWrite32A( StMeasFunc_MFB_SiSampleNumB, 0 ) ;												

		UlSampleNum	= ( UINT32 )( FS_FREQ / StFRAParam.StHostCom.SfFrqCom.SfFltVal ) + 1 ;

		if( UcSingleSweepSel ) {
			RamRead32A( StMeasFunc_PMC_UcPhaseMesMode, &UlReadVal ) ;
			UlReadVal	&= 0xFFFFFDFF ;																
			RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, UlReadVal ) ;
		}

		
		RamWrite32A( StMeasFunc_MFB_SiSampleMaxB, UlSampleNum ) ;									
		RamWrite32A( StMeasFunc_MFA_SiSampleMaxA, UlSampleNum ) ;									

#if !LPF_ENA
		if( i == 0)
			WitTim( 8000 / StFRAParam.StHostCom.SfFrqCom.SfFltVal ) ;
#endif
		MeasureWait() ;

		RamRead32A( StMeasFunc_MFA_LLiAbsInteg1 	, &StMeasValueA.StUllnVal.UlLowVal ) ;			
		RamRead32A( StMeasFunc_MFA_LLiAbsInteg1 + 4 , &StMeasValueA.StUllnVal.UlHigVal ) ;
		RamRead32A( StMeasFunc_MFB_LLiAbsInteg2 	, &StMeasValueB.StUllnVal.UlLowVal ) ;			
		RamRead32A( StMeasFunc_MFB_LLiAbsInteg2 + 4	, &StMeasValueB.StUllnVal.UlHigVal ) ;

		StFRAMes.UllCumulAdd1	= StMeasValueA.UllnValue ;
		StFRAMes.UllCumulAdd2	= StMeasValueB.UllnValue ;
		RamRead32A( StMeasFunc_PMC_SiFsCountF, &UlReadVal ) ;
		StFRAMes.UsFsCount		= UlReadVal ;
		if( StFRAMes.UllCumulAdd1 == 0 || StFRAMes.UllCumulAdd2 == 0 ) {
			i--;
			continue;
		}

		FRA_Calc( &StFRAMes, i ) ;
		RamWrite32A( StMeasFunc_MFA_LLiAbsInteg1, 0x00000000 ) ;									
		RamWrite32A( StMeasFunc_MFA_LLiAbsInteg1 + 4, 0x00000000 ) ;								
		RamWrite32A( StMeasFunc_MFB_LLiAbsInteg2, 0x00000000 ) ;									
		RamWrite32A( StMeasFunc_MFB_LLiAbsInteg2 + 4, 0x00000000 ) ;								
		if( !UcSingleSweepSel ) {
			RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, 0x00000000 ) ;								
		} else {
			RamRead32A( StMeasFunc_PMC_UcPhaseMesMode, &UlReadVal ) ;
			UlReadVal	&= 0x00000300 ;																
			RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, UlReadVal ) ;								
		}
		RamWrite32A( StMeasFunc_PMC_UcCrossDetectA, 0x00000000 ) ;									
	}

	if( !UcSingleSweepSel ) {																		
		RamWrite32A( SinWaveC_Regsiter,		0x00000000 ) ;											
		SetTransDataAdr( SinWave_OutAddr,	( UINT32 )0x00000000 ) ;								


		if( UcDirSel == X_DIR ) {
			RamWrite32A( RAM_XSIN,	0x00000000 ) ;													
		} else if( UcDirSel == Y_DIR ) {
			RamWrite32A( RAM_YSIN,	0x00000000 ) ;													
#ifdef	SEL_CLOSED_AF
		} else {
			RamWrite32A( RAM_ZSIN,	0x00000000 ) ;													
#endif
		}
	}

	FRA_Avg() ;

	if( StFRAParam.StMesRslt.SfPhaseAvg > 180.0F ) {
		StFRAParam.StMesRslt.SfPhaseAvg	-= 360.0F ;
	} else if( StFRAParam.StMesRslt.SfPhaseAvg < -180.0F ) {
		StFRAParam.StMesRslt.SfPhaseAvg	+= 360.0F ;
	}
	StFRAParam.StMesRslt.SfPhaseAvg	= fmod(StFRAParam.StMesRslt.SfPhaseAvg, 180.0) ;
}



void	MesStart_FRA_Continue( void )
{
	UnllnVal	StMeasValueA, StMeasValueB ;
	UINT8		i ;
	UINT32		UlSampleNum, UlReadVal ;

	RamWrite32A( SinWave_Offset,	Freq_Convert( StFRAParam.StHostCom.SfFrqCom.SfFltVal ) ) ;

	
#if LPF_ENA
	
	UlReadVal	= ( UINT32 )( ( float )0x7FFFFFFF * ( FS_FREQ / ( FS_FREQ + M_PI * StFRAParam.StHostCom.SfFrqCom.SfFltVal ) ) ) ;

	
	UlSampleNum	= 2 * UlReadVal - 0x7FFFFFFF ;
	RamWrite32A ( MeasureFilterA_Temp_c1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_c1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Temp_c2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_c2, UlSampleNum ) ;

	
	UlSampleNum	= 0x7FFFFFFF - UlReadVal ;
	RamWrite32A ( MeasureFilterA_Temp_a1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_a1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Temp_b1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_b1, UlSampleNum ) ;

	UlSampleNum	= 2 * ( 0x7FFFFFFF - UlReadVal ) ;
	RamWrite32A ( MeasureFilterA_Temp_a2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_a2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Temp_b2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_b2, UlSampleNum ) ;

	ClrMesFil() ;																					
	SetWaitTime( 500 ) ;

#else	
	
	UlReadVal	= ( UINT32 )( ( float )0x7FFFFFFF * ( StFRAParam.StHostCom.SfFrqCom.SfFltVal / (StFRAParam.StHostCom.SfFrqCom.SfFltVal + FS_FREQ /  M_PI ) ) ) ;

	
	UlSampleNum	= 0x7FFFFFFF - ( 2 * UlReadVal ) ;
	RamWrite32A ( MeasureFilterA_Temp_c1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_c1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Temp_c2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_c2, UlSampleNum ) ;

	
	UlSampleNum	= UlReadVal ;
	UlSampleNum *= 2 ;			
	RamWrite32A ( MeasureFilterA_Temp_a1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_a1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterA_Temp_b1, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_b1, UlSampleNum ) ;

	
	UlSampleNum	= ( 0x7FFFFFFF - UlReadVal ) ;
	RamWrite32A ( MeasureFilterA_Temp_a2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_a2, UlSampleNum ) ;

	UlSampleNum = ~UlSampleNum + 1;
	RamWrite32A ( MeasureFilterA_Temp_b2, UlSampleNum ) ;
	RamWrite32A ( MeasureFilterB_Temp_b2, UlSampleNum ) ;

	ClrMesFil() ;																					
	SetWaitTime( 8000 / StFRAParam.StHostCom.SfFrqCom.SfFltVal ) ;
#endif	

	RamRead32A( StMeasFunc_PMC_UcPhaseMesMode, &UlReadVal ) ;
	UlReadVal	|= 0x00000400 ;																		
	RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, UlReadVal ) ;										

	for( i = 0 ; i < StFRAParam.StHostCom.UcAvgCycl ; i++ ) {
		RamRead32A( StMeasFunc_PMC_UcPhaseMesMode, &UlReadVal ) ;
		UlReadVal	|= 0x00000001 ;																	
		RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, UlReadVal ) ;									
		RamWrite32A( StMeasFunc_MFA_LLiAbsInteg1, 0x00000000 ) ;									
		RamWrite32A( StMeasFunc_MFA_LLiAbsInteg1 + 4, 0x00000000 ) ;								
		RamWrite32A( StMeasFunc_MFB_LLiAbsInteg2, 0x00000000 ) ;									
		RamWrite32A( StMeasFunc_MFB_LLiAbsInteg2 + 4, 0x00000000 ) ;								
		RamWrite32A( StMeasFunc_PMC_SiFsCountF, 0x00000000 ) ;										
		RamWrite32A( StMeasFunc_PMC_SiFsCountR, 0x00000000 ) ;										
		RamWrite32A( StMeasFunc_PMC_UcCrossDetectA, 0x00000000 ) ;									

		RamWrite32A( StMeasFunc_MFA_SiSampleNumA, 0 ) ;												
		RamWrite32A( StMeasFunc_MFB_SiSampleNumB, 0 ) ;												

		UlSampleNum	= ( UINT32 )( FS_FREQ / StFRAParam.StHostCom.SfFrqCom.SfFltVal ) + 1 ;

		RamRead32A( StMeasFunc_PMC_UcPhaseMesMode, &UlReadVal ) ;
		UlReadVal	&= 0xFFFFFDFF ;																	
		RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, UlReadVal ) ;									

		
		RamWrite32A( StMeasFunc_MFB_SiSampleMaxB, UlSampleNum ) ;									
		RamWrite32A( StMeasFunc_MFA_SiSampleMaxA, UlSampleNum ) ;									
#if !LPF_ENA
		if( i == 0 ) {
			WitTim( (int)(8000 / StFRAParam.StHostCom.SfFrqCom.SfFltVal) ) ;
		}
#endif	

		MeasureWait() ;

		RamRead32A( StMeasFunc_MFA_LLiAbsInteg1 	, &StMeasValueA.StUllnVal.UlLowVal ) ;			
		RamRead32A( StMeasFunc_MFA_LLiAbsInteg1 + 4 , &StMeasValueA.StUllnVal.UlHigVal ) ;
		RamRead32A( StMeasFunc_MFB_LLiAbsInteg2 	, &StMeasValueB.StUllnVal.UlLowVal ) ;			
		RamRead32A( StMeasFunc_MFB_LLiAbsInteg2 + 4	, &StMeasValueB.StUllnVal.UlHigVal ) ;

		StFRAMes.UllCumulAdd1	= StMeasValueA.UllnValue ;
		StFRAMes.UllCumulAdd2	= StMeasValueB.UllnValue ;
		RamRead32A( StMeasFunc_PMC_SiFsCountF, &UlReadVal ) ;
		StFRAMes.UsFsCount		= UlReadVal ;
		if( StFRAMes.UllCumulAdd1 == 0 || StFRAMes.UllCumulAdd2 == 0 ) {
			i--;
			continue;
		}

		FRA_Calc( &StFRAMes, i ) ;
		RamWrite32A( StMeasFunc_MFA_LLiAbsInteg1, 0x00000000 ) ;									
		RamWrite32A( StMeasFunc_MFA_LLiAbsInteg1 + 4, 0x00000000 ) ;								
		RamWrite32A( StMeasFunc_MFB_LLiAbsInteg2, 0x00000000 ) ;									
		RamWrite32A( StMeasFunc_MFB_LLiAbsInteg2 + 4, 0x00000000 ) ;								
		RamRead32A( StMeasFunc_PMC_UcPhaseMesMode, &UlReadVal ) ;
		UlReadVal	&= 0x00000300 ;
		RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, UlReadVal ) ;									
		RamWrite32A( StMeasFunc_PMC_UcCrossDetectA, 0x00000000 ) ;									
	}

	FRA_Avg() ;

	if( StFRAParam.StMesRslt.SfPhaseAvg > 180.0F ) {
		StFRAParam.StMesRslt.SfPhaseAvg	-= 360.0F ;
	} else if( StFRAParam.StMesRslt.SfPhaseAvg < -180.0F ) {
		StFRAParam.StMesRslt.SfPhaseAvg	+= 360.0F ;
	}
	StFRAParam.StMesRslt.SfPhaseAvg	= fmod( StFRAParam.StMesRslt.SfPhaseAvg, 180.0);
}



void	MesEnd_FRA_Sweep( void )
{
	
	RamWrite32A( StMeasFunc_PMC_UcPhaseMesMode, 0 ) ;
	RamWrite32A( StMeasFunc_PMC_UcCrossDetectA, 0x00000000 ) ;										
	RamWrite32A( StMeasFunc_MFA_SiSampleMaxA, 0 ) ;
	RamWrite32A( StMeasFunc_MFB_SiSampleMaxB, 0 ) ;
	
	RamWrite32A( SinWaveC_Regsiter,		0x00000000 ) ;												
	SetTransDataAdr( SinWave_OutAddr,	( UINT32 )0x00000000 ) ;									


	RamWrite32A( RAM_XSIN,	0x00000000 ) ;															
	RamWrite32A( RAM_YSIN,	0x00000000 ) ;															
#ifdef	SEL_CLOSED_AF
	RamWrite32A( RAM_ZSIN,	0x00000000 ) ;															
#endif

	return ;

}
