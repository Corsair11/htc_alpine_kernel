#define		__OISFLSH__
#include	"Ois.h"

#include	"FromCode.h"
#include	"PmemCode.h"
#include	<linux/kernel.h>

#define	USER_RESERVE			3		
#define	ERASE_BLOCKS			(16 - USER_RESERVE)



#define BURST_LENGTH ( 8*5 ) 			

#define DMB_COEFF_ADDRESS		0x21
#define BLOCK_UNIT				0x200
#define BLOCK_BYTE				2560
#define SECTOR_SIZE				320
#define HALF_SECTOR_ADD_UNIT	0x20
#define FLASH_ACCESS_SIZE		32		

extern	void RamWrite32A( UINT16, UINT32 );
extern 	void RamRead32A( UINT16, void * );
extern 	void CntWrt( void *, UINT16) ;
extern	void CntRd3( UINT32, void *, UINT16 ) ;

extern void WPBCtrl( UINT8 );
extern void	WitTim( UINT16 );

UINT8	FlashBurstWriteF40( const UINT8 *, UINT32, UINT32 ) ;
UINT8	FlashBurstReadF40( UINT8 *, UINT32, UINT32 ) ;
void	CalcBlockChksum( UINT8, UINT32 *, UINT32 * );


void IORead32A( UINT32 IOadrs, UINT32 *IOdata )
{
	RamWrite32A( CMD_IO_ADR_ACCESS, IOadrs ) ;
	RamRead32A ( CMD_IO_DAT_ACCESS, IOdata ) ;
}

void IOWrite32A( UINT32 IOadrs, UINT32 IOdata )
{
	RamWrite32A( CMD_IO_ADR_ACCESS, IOadrs ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS, IOdata ) ;
}

UINT8 ReadWPB( void )
{
	UINT32 UlReadVal, UlCnt=0;

	do{
		IORead32A( FLASHROM_F40_WPB, &UlReadVal );
		if( (UlReadVal & 0x00000004) != 0 )	return ( 1 ) ;
		WitTim( 1 );
	}while ( UlCnt++ < 10 );
	return ( 0 );
}

UINT8 UnlockCodeSet( void )
{
	UINT32 UlReadVal;

	WPBCtrl(WPB_OFF) ;
	if ( ReadWPB() != 1 )	return ( 5 );								

	IOWrite32A( FLASHROM_F40_UNLK_CODE1, 0xAAAAAAAA ) ;					
	IOWrite32A( FLASHROM_F40_UNLK_CODE2, 0x55555555 ) ;					
	IOWrite32A( FLASHROM_F40_RSTB_FLA, 0x00000001 );					
	IOWrite32A( FLASHROM_F40_CLK_FLAON, 0x00000010 );					
	
	IOWrite32A( FLASHROM_F40_UNLK_CODE3, 0x0000ACD5 );					
	IOWrite32A( FLASHROM_F40_WPB, 1 );
	RamRead32A(  CMD_IO_DAT_ACCESS , &UlReadVal ) ;

	if ( (UlReadVal & 0x00000007) != 7 ) return(1);

	return(0);
}

UINT8 UnlockCodeClear(void)
{
	UINT32 UlReadVal;

	IOWrite32A( FLASHROM_F40_WPB, 0x00000010 );							

	RamRead32A( CMD_IO_DAT_ACCESS, &UlReadVal );
	if( (UlReadVal & 0x00000080) != 0 )	return (3);

	WPBCtrl(WPB_ON) ;

	return(0);
}


UINT8 FlashBurstWriteF40( const UINT8 *NcDataVal, UINT32 NcDataLength, UINT32 ScNvrMan)
{
	UINT32 i, j, UlCnt;
	UINT8 data[163];			
	UINT32 UlReadVal;
	UINT8 UcOddEvn;
	UINT8 Remainder;	

	data[0] = 0xF0;					
	data[1] = 0x08;					
	data[2] = BURST_LENGTH;			

	for(i = 0 ; i< (NcDataLength / BURST_LENGTH) ; i++)
	{
		UlCnt = 3;

		UcOddEvn =i % 2;					
		data[1] = 0x08 + UcOddEvn;			

		for(j = 0 ; j < BURST_LENGTH; j++)	data[UlCnt++] = *NcDataVal++;

		CntWrt( data, BURST_LENGTH + 3 );
		RamWrite32A( 0xF00A ,(UINT32) ( ( BURST_LENGTH / 5 ) * i + ScNvrMan) ) ;	
		RamWrite32A( 0xF00B ,(UINT32) (BURST_LENGTH / 5) ) ;						

		RamWrite32A( 0xF00C , 4 + 4 * UcOddEvn ) ;									
	}

	Remainder = NcDataLength % BURST_LENGTH;
	if (Remainder != 0 ){
		data[2] = Remainder;				

		UlCnt = 3;

		UcOddEvn =i % 2;					
		data[1] = 0x08 + UcOddEvn;			

		for(j = 0 ; j < Remainder; j++)	data[UlCnt++] = *NcDataVal++;

		CntWrt( data, BURST_LENGTH + 3 );
		RamWrite32A( 0xF00A ,(UINT32) ( ( BURST_LENGTH / 5 ) * i + ScNvrMan) ) ;	
		RamWrite32A( 0xF00B ,(UINT32) (Remainder /5) ) ;							
		RamWrite32A( 0xF00C , 4 + 4 * UcOddEvn ) ;									
	}

	UlCnt = 0;
	do{
		
		if(UlCnt++ > 100) return ( 1 );												
		RamRead32A( 0xF00C, &UlReadVal );
	}while ( UlReadVal != 0 );
	return( 0 );
}


UINT8	FlashBurstReadF40( UINT8 *NcDataVal, UINT32 UcDataLength, UINT32 ScNvrMan )
{
	UINT8	i ;
	UINT32	UlReadVal, UlCnt, UlCmdID ;

	for( i = 0 ; i < ( UcDataLength / BURST_LENGTH ) ; i++ )
	{
		UlCmdID		= ( ( ( UINT32 )0xF008 + ( i % 2 ) ) << 8 ) + BURST_LENGTH ;
		RamWrite32A( 0xF00A, ( UINT32 )( ( BURST_LENGTH / 5 ) * i + ScNvrMan ) ) ;		
		RamWrite32A( 0xF00B, ( BURST_LENGTH / 5 ) ) ;									
		RamWrite32A( 0xF00C, 1 + ( i % 2 ) ) ;											

		UlCnt	= 0 ;
		do {																			
			if( UlCnt++ > 100 ) {														
				return( 1 ) ;
			}
			RamRead32A( 0xF00C, &UlReadVal ) ;
		} while( UlReadVal != 0 ) ;

		CntRd3( ( UINT32 )UlCmdID, ( char * )&NcDataVal[ i * BURST_LENGTH ], BURST_LENGTH ) ;
	}

	return( 0 ) ;
}

UINT8 FlashUpdateF40(void)
{
	
	return FlashUpdateF40ex( 0 );
}

UINT8 FlashUpdateF40ex( UINT8 chiperase )
{
	UINT32 SiWrkVl0 ,SiWrkVl1;
	INT32 SiAdrVal;

	const UINT8 *NcDatVal;
	UINT32 UlReadVal, UlCnt;
	UINT8 ans, i;

	UINT16 UsChkBlocks;

	UINT8 UserMagicCode[sizeof(CcMagicCodeF40)];

	IOWrite32A( SYSDSP_REMAP, 0x00001440 ) ;				
	WitTim( 25 ) ;											

	IORead32A( SYSDSP_SOFTRES, (UINT32 *)&SiWrkVl0 ) ;		
	SiWrkVl0	&= 0xFFFFEFFF ;
	IOWrite32A( SYSDSP_SOFTRES, SiWrkVl0 ) ;

	
	RamWrite32A( 0xF006 , 0x00000000 ) ;					
	IOWrite32A( SYSDSP_DSPDIV, 1 ) ;						

	
	RamWrite32A( 0x0344, 0x00000014 ) ;						
	SiAdrVal =0x00100000;

	for(UlCnt = 0 ;UlCnt < 25 ; UlCnt++ ){
		RamWrite32A( 0x0340, SiAdrVal ) ;					
		SiAdrVal+= 0x00000008;								
		RamWrite32A( 0x0348, UlPmemCodeF40[UlCnt*5] ) ;			
		RamWrite32A( 0x034C, UlPmemCodeF40[UlCnt*5+1] ) ;		
		RamWrite32A( 0x0350, UlPmemCodeF40[UlCnt*5+2] ) ;		
		RamWrite32A( 0x0354, UlPmemCodeF40[UlCnt*5+3] ) ;		
		RamWrite32A( 0x0358, UlPmemCodeF40[UlCnt*5+4] ) ;		
		RamWrite32A( 0x033c, 0x00000001 ) ;					
	}
	
	for(UlCnt = 0 ;UlCnt < 9 ; UlCnt++ ){
		CntWrt( (INT8 *)&UpData_CommandFromTable[UlCnt*6], 6 );
	}

	if( chiperase ) {
		
		
		
		
		ans = UnlockCodeSet();
		if ( ans != 0 ) return (ans);							

		IOWrite32A( FLASHROM_F40_ADR, 0x00000000 ) ;			

		IOWrite32A( FLASHROM_F40_CMD, 5 );						

		WitTim( 13 ) ;
		UlCnt=0;
		do {
			if( UlCnt++ > 10 ){ ans=0x10; break; }

			IORead32A( FLASHROM_F40_INT, &UlReadVal );
		}while ( (UlReadVal & 0x00000080) != 0 );

	} else {
		
		
		
		
		for( i = 0 ; i < ERASE_BLOCKS ; i++ )
		{
			ans	= FlashBlockErase( i * BLOCK_UNIT );
			if( ans != 0 ) {
				return( ans ) ;
			}
		}
		ans = UnlockCodeSet();
		if ( ans != 0 ) return (ans);							
	}
	IOWrite32A( FLASHROM_F40_ADR, 0x00010000 ) ;			

	IOWrite32A( FLASHROM_F40_CMD, 4 ) ;  					

	WitTim( 5 ) ;
	UlCnt=0;
	do{
		if( UlCnt++ > 10 ) { ans=0x10; break; }

		IORead32A( FLASHROM_F40_INT, &UlReadVal );
	}while ( (UlReadVal & 0x00000080) != 0 );

	FlashBurstWriteF40( CcFromCodeF40, sizeof(CcFromCodeF40), 0 );

	ans |= UnlockCodeClear();					
	if ( ans != 0 ) return (ans);				

	UsChkBlocks = ( sizeof(CcFromCodeF40) / 160 ) + 1 ;
	RamWrite32A( 0xF00A , 0x00000000 ) ;		
#if 1
	RamWrite32A( 0xF00B , UsChkBlocks ) ;		
#else
	RamWrite32A( 0xF00B , 0x00000100 ) ;		
#endif
	RamWrite32A( 0xF00C , 0x00000100 ) ;		

	NcDatVal = CcFromCodeF40;
	SiWrkVl0 = 0;
	for( UlCnt = 0; UlCnt < sizeof(CcFromCodeF40) ;UlCnt++){
		SiWrkVl0 += *NcDatVal++;
	}
#if 1
	UsChkBlocks *= 160 ;
	for( ; UlCnt < UsChkBlocks ; UlCnt++ )
	{
		SiWrkVl0 += 0xFF ;
	}
#else
	for(; UlCnt < 40960; UlCnt++){
		SiWrkVl0 += 0xFF;
	}
#endif

	UlCnt=0;
	do{																
		if( UlCnt++ > 100 ) return ( 6 );							
		RamRead32A( 0xF00C, &UlReadVal );
	}while( UlReadVal != 0 );
	printk("[OIS_Cali] UlCnt=%u\n", UlCnt);

	RamRead32A( 0xF00D, &SiWrkVl1 );								

	if( SiWrkVl0 != SiWrkVl1 ){
		return(0x20);
	}


	
	if ( !chiperase ) {
		UINT32 sumH, sumL;
		UINT16 Idx;

		
		for( UlCnt = 0; UlCnt < sizeof(CcMagicCodeF40) ;UlCnt++) {
			UserMagicCode[UlCnt] = CcMagicCodeF40[UlCnt];
		}

		
		for( UlCnt = 0; UlCnt < USER_RESERVE; UlCnt++ ) {

			CalcBlockChksum( ERASE_BLOCKS + UlCnt, &sumH, &sumL );

			Idx =  (ERASE_BLOCKS + UlCnt) * 2 * 5 + 1 + 40;
			NcDatVal = (UINT8 *)&sumH;

#ifdef _BIG_ENDIAN_
			
			UserMagicCode[Idx++] = *NcDatVal++;
			UserMagicCode[Idx++] = *NcDatVal++;
			UserMagicCode[Idx++] = *NcDatVal++;
			UserMagicCode[Idx++] = *NcDatVal++;
			Idx++;
			NcDatVal = (UINT8 *)&sumL;
			UserMagicCode[Idx++] = *NcDatVal++;
			UserMagicCode[Idx++] = *NcDatVal++;
			UserMagicCode[Idx++] = *NcDatVal++;
			UserMagicCode[Idx++] = *NcDatVal++;
#else
			
			UserMagicCode[Idx+3] = *NcDatVal++;
			UserMagicCode[Idx+2] = *NcDatVal++;
			UserMagicCode[Idx+1] = *NcDatVal++;
			UserMagicCode[Idx+0] = *NcDatVal++;
			Idx+=5;
			NcDatVal = (UINT8 *)&sumL;
			UserMagicCode[Idx+3] = *NcDatVal++;
			UserMagicCode[Idx+2] = *NcDatVal++;
			UserMagicCode[Idx+1] = *NcDatVal++;
			UserMagicCode[Idx+0] = *NcDatVal++;
#endif
		}
		NcDatVal = UserMagicCode;

	} else {
		NcDatVal = CcMagicCodeF40;
	}


	ans = UnlockCodeSet();
	if ( ans != 0 ) return (ans);							

	FlashBurstWriteF40( NcDatVal, sizeof(CcMagicCodeF40), 0x00010000 );					

	UnlockCodeClear();										

	RamWrite32A( 0xF00A , 0x00010000 ) ;				
	RamWrite32A( 0xF00B , 0x00000002 ) ;				
	RamWrite32A( 0xF00C , 0x00000100 ) ;				

	SiWrkVl0 = 0;
	for( UlCnt = 0; UlCnt < sizeof(CcMagicCodeF40) ;UlCnt++){
		SiWrkVl0 += *NcDatVal++;
	}
	for(; UlCnt < 320; UlCnt++){
		SiWrkVl0 += 0xFF;
	}

	UlCnt=0;
	do{
		if( UlCnt++ > 100 ) return ( 6 );					
		RamRead32A( 0xF00C, &UlReadVal );
	}while( UlReadVal != 0 );
	RamRead32A( 0xF00D, &SiWrkVl1 );						

	if(SiWrkVl0 != SiWrkVl1 ){
		return(0x30);
	}

	IOWrite32A( SYSDSP_REMAP, 0x00001000 ) ;				
	return( 0 );
}


void	FlashByteRead( UINT32 UlAddress, UINT8 *PucData, UINT8 UcByteAdd )
{
	UINT8	UcReadDat[ 4 ] ;

	IOWrite32A( FLASHROM_F40_ADR, UlAddress ) ;

	IOWrite32A( FLASHROM_F40_ACSCNT, 0 ) ;

	IOWrite32A( FLASHROM_F40_CMD, 1 ) ;  					

	if( UcByteAdd < 4 ) {
		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_F40_RDATL ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , UcReadDat ) ;

		*PucData	= UcReadDat[ UcByteAdd ] ;
	} else {
		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_F40_RDATH ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , UcReadDat ) ;
		*PucData	= UcReadDat[ 0 ] ;
		
		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_F40_RDATL ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , UcReadDat ) ;
	}
}

UINT8	FlashInt32Write( UINT32 UlAddress, UINT32 *PuiData, UINT8 UcLength )
{
	UINT8	UcIndex ;
	UINT8 * PucData = (UINT8 *)PuiData ;
	UINT8	UcResult = 0 ;
	UINT32	UlOffset = UlAddress & 0x3F ;
	UINT32	UlSecAddress = UlAddress & 0xFFFFFFC0 ;
	UINT8	SectorData[ SECTOR_SIZE ];

	
	if( (UlOffset + UcLength) > 0x40 )
		return 9;

	
	FlashSectorRead( UlSecAddress, SectorData );

	
	for( UcIndex = 0; UcIndex < UcLength; UcIndex++ )
	{
#ifdef _BIG_ENDIAN_
		SectorData[ (UlOffset + UcIndex) * 5 + 1 ] = *PucData++ ;
		SectorData[ (UlOffset + UcIndex) * 5 + 2 ] = *PucData++ ;
		SectorData[ (UlOffset + UcIndex) * 5 + 3 ] = *PucData++ ;
		SectorData[ (UlOffset + UcIndex) * 5 + 4 ] = *PucData++ ;
#else
		SectorData[ (UlOffset + UcIndex) * 5 + 4 ] = *PucData++ ;
		SectorData[ (UlOffset + UcIndex) * 5 + 3 ] = *PucData++ ;
		SectorData[ (UlOffset + UcIndex) * 5 + 2 ] = *PucData++ ;
		SectorData[ (UlOffset + UcIndex) * 5 + 1 ] = *PucData++ ;
#endif
	}

	UcResult = FlashSectorWrite( UlSecAddress, SectorData );
	if ( UcResult == 0 )
	{
		UINT32	UlSumH, UlSumL ;
		UINT8	UcBlockNum = (UlSecAddress >> 9) & 0x0F ;
		UINT8 *	NcDatVal ;

		
		CalcBlockChksum( UcBlockNum, &UlSumH, &UlSumL ) ;

		
		FlashSectorRead( 0x00010000, SectorData ) ;

		
		UcIndex =  UcBlockNum * 2 * 5 + 1 + 40;
		NcDatVal = (UINT8 *)&UlSumH;

#ifdef _BIG_ENDIAN_
			
			SectorData[UcIndex++] = *NcDatVal++;
			SectorData[UcIndex++] = *NcDatVal++;
			SectorData[UcIndex++] = *NcDatVal++;
			SectorData[UcIndex++] = *NcDatVal++;
			UcIndex++;
			NcDatVal = (UINT8 *)&UlSumL;
			SectorData[UcIndex++] = *NcDatVal++;
			SectorData[UcIndex++] = *NcDatVal++;
			SectorData[UcIndex++] = *NcDatVal++;
			SectorData[UcIndex++] = *NcDatVal++;
#else
			
			SectorData[UcIndex+3] = *NcDatVal++;
			SectorData[UcIndex+2] = *NcDatVal++;
			SectorData[UcIndex+1] = *NcDatVal++;
			SectorData[UcIndex+0] = *NcDatVal++;
			UcIndex += 5;
			NcDatVal = (UINT8 *)&UlSumL;
			SectorData[UcIndex+3] = *NcDatVal++;
			SectorData[UcIndex+2] = *NcDatVal++;
			SectorData[UcIndex+1] = *NcDatVal++;
			SectorData[UcIndex+0] = *NcDatVal++;
#endif

		
		UcResult = FlashSectorWrite( 0x00010000, SectorData ) ;
	}

	return UcResult ;
}

void	FlashSectorRead( UINT32 UlAddress, UINT8 *PucData )
{
	UINT8	UcIndex, UcNum ;
	UINT8	UcReadDat[ 4 ] ;

	IOWrite32A( FLASHROM_F40_ADR, ( UlAddress & 0xFFFFFFC0 ) ) ;

	IOWrite32A( FLASHROM_F40_ACSCNT, 63 ) ;
	UcNum	= 64 ;

	IOWrite32A( FLASHROM_F40_CMD, 1 ) ;  					

	for( UcIndex = 0 ; UcIndex < UcNum ; UcIndex++ )
	{
		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_F40_RDATH ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , UcReadDat ) ;
		*PucData++		= UcReadDat[ 0 ] ;
		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_F40_RDATL ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , UcReadDat ) ;
		*PucData++	= UcReadDat[ 3 ] ;
		*PucData++	= UcReadDat[ 2 ] ;
		*PucData++	= UcReadDat[ 1 ] ;
		*PucData++	= UcReadDat[ 0 ] ;
	}
}

void	FlashSectorRead_htc( UINT32 UlAddress, UINT8 *PucData )
{
	UINT8	UcIndex, UcNum ;
	UINT8	UcReadDat[ 4 ] ;

	IOWrite32A( FLASHROM_F40_ADR, ( UlAddress & 0xFFFFFFC0 ) ) ;

	IOWrite32A( FLASHROM_F40_ACSCNT, 63 ) ;
	UcNum	= 64 ;

	IOWrite32A( FLASHROM_F40_CMD, 1 ) ;  					

	for( UcIndex = 0 ; UcIndex < UcNum ; UcIndex++ )
	{
		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_F40_RDATH ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , UcReadDat ) ;
		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_F40_RDATL ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS , UcReadDat ) ;
		*PucData++	= UcReadDat[ 3 ] ;
		*PucData++	= UcReadDat[ 2 ] ;
		*PucData++	= UcReadDat[ 1 ] ;
		*PucData++	= UcReadDat[ 0 ] ;
	}
}


UINT8	FlashSectorRead_Burst( UINT32 UlAddress, UINT8 *PucData, UINT8 UcSectorNum )
{
	UINT8	UcIndex, UcStatus ;
	INT32	SiWrkVal ;

	
	IOWrite32A( SYSDSP_REMAP, 0x00001440 ) ;				
	WitTim( 1 ) ;											

	IORead32A( SYSDSP_SOFTRES, (UINT32 *)&SiWrkVal ) ;
	SiWrkVal	&= 0xFFFFEFFF ;
	IOWrite32A( SYSDSP_SOFTRES, SiWrkVal ) ;

	
	RamWrite32A( 0xF006 , 0x00000000 ) ;					
	IOWrite32A(  SYSDSP_DSPDIV, 1 ) ;						

	for( UcIndex = 0 ; UcIndex < UcSectorNum ; UcIndex++ )
	{
		UcStatus	= FlashBurstReadF40( &PucData[ UcIndex * SECTOR_SIZE ], SECTOR_SIZE, ( UINT32 )( UlAddress + ( UcIndex * ( SECTOR_SIZE / 5 ) ) ) ) ;
		if( UcStatus ) {
			return( UcStatus ) ;
		}
	}

	IOWrite32A( SYSDSP_REMAP, 0x00001000 ) ;				
	WitTim( 10 ) ;

	return( 0 ) ;

}

UINT8	FlashBlockErase( UINT32 SetAddress )
{
	UINT32	UlReadVal, UlCnt;
	UINT8	ans	= 0 ;

	
	
	if( SetAddress & 0x00010000 )
		return 9;

	
	ans	= UnlockCodeSet();
	if( ans != 0 )	return( ans ) ;							

	IOWrite32A( FLASHROM_F40_ADR, SetAddress & 0xFFFFFE00 ) ;
	
	IOWrite32A( FLASHROM_F40_CMD, 6	 ) ;

	WitTim( 5 ) ;

	UlCnt	= 0 ;

	do {
		if( UlCnt++ > 10 ){	ans = 2;	break;	} ;

		IORead32A( FLASHROM_F40_INT, &UlReadVal ) ;
	} while( ( UlReadVal & 0x00000080 ) != 0 ) ;

	UnlockCodeClear();										

	return( ans ) ;
}



UINT8	FlashSectorErase( UINT32 SetAddress )
{
	UINT32	UlReadVal, UlCnt;
	UINT8	ans	= 0 ;

	
	
	if( (SetAddress & 0x000100FF) >  0x0001007F )
		return 9;

	
	ans	= UnlockCodeSet();
	if( ans != 0 )	return( ans ) ;							

	IOWrite32A( FLASHROM_F40_ADR, SetAddress & 0xFFFFFFC0 ) ;
	
	IOWrite32A( FLASHROM_F40_CMD, 4 ) ;

	WitTim( 5 ) ;

	UlCnt	= 0 ;

	do {
		if( UlCnt++ > 10 ){	ans = 2;	break;	} ;

		IORead32A( FLASHROM_F40_INT, &UlReadVal ) ;
	} while( ( UlReadVal & 0x00000080 ) != 0 ) ;

	ans = UnlockCodeClear();								

	return( ans ) ;
}



UINT8	FlashSectorWrite( UINT32 UlAddress, UINT8 *PucData )
{
	UINT8	UcNum, UcStatus = 0, UcSize = 0 ;
	UnDwdVal		UnWriteDat ;

	
	
	if( (UlAddress & 0x000100FF) >  0x0001007F )
		return 9;

	UcStatus	= FlashSectorErase( UlAddress & 0xFFFFFFC0 ) ;
	if( UcStatus != 0 ) {
		return( UcStatus ) ;
	}

	
	UcStatus	= UnlockCodeSet();
	if( UcStatus != 0 ) {
		return( UcStatus ) ;											
	}

	do {
		
		IOWrite32A( FLASHROM_F40_ACSCNT, ( FLASH_ACCESS_SIZE - 1 ) ) ;	

		
		IOWrite32A( FLASHROM_F40_ADR, UlAddress + UcSize ) ;
		
		IOWrite32A( FLASHROM_F40_CMD, 2 ) ;  							

		for( UcNum = 0 ; UcNum < FLASH_ACCESS_SIZE ; UcNum++ )
		{
			UnWriteDat.StCdwVal.UcRamVa0	= *PucData++ ;
			IOWrite32A( FLASHROM_F40_WDATH, UnWriteDat.UlDwdVal ) ;

			UnWriteDat.StCdwVal.UcRamVa3	= *PucData++ ;
			UnWriteDat.StCdwVal.UcRamVa2	= *PucData++ ;
			UnWriteDat.StCdwVal.UcRamVa1	= *PucData++ ;
			UnWriteDat.StCdwVal.UcRamVa0	= *PucData++ ;
			IOWrite32A( FLASHROM_F40_WDATL, UnWriteDat.UlDwdVal ) ;
			UcSize++ ;
		}
	} while( UcSize < 64 ) ;											

	UcStatus = UnlockCodeClear() ;										

	return( UcStatus ) ;
}



UINT8	FlashSectorWrite_Burst( UINT32 UlAddress, UINT8 *PucData, UINT8 UcSectorNum )
{
	UINT8	UcStatus	= 0 ;
	UINT8	UcRemainSecNum, UcBlockErased	= 0 ;
	UINT32	UlCnt ;
	INT32	SiAdrVal, SiWrkVal ;

	
	IOWrite32A( SYSDSP_REMAP, 0x00001440 ) ;					
	WitTim( 1 ) ;												

	IORead32A( SYSDSP_SOFTRES, (UINT32 *)&SiWrkVal ) ;
	SiWrkVal	&= 0xFFFFEFFF ;
	IOWrite32A( SYSDSP_SOFTRES, SiWrkVal ) ;

	
	RamWrite32A( 0xF006 , 0x00000000 ) ;						
	IOWrite32A( SYSDSP_DSPDIV, 1 ) ;							

	
	
	RamWrite32A( 0x0344, 0x00000014 ) ;							
	SiAdrVal	= 0x00100000 ;

	for( UlCnt = 0 ; UlCnt < 25 ; UlCnt++ ) {
		RamWrite32A( 0x0340, SiAdrVal ) ;						
		SiAdrVal+= 0x00000008;									
		RamWrite32A( 0x0348, UlPmemCodeF40[ UlCnt * 5 ] ) ;		
		RamWrite32A( 0x034C, UlPmemCodeF40[ UlCnt * 5 + 1 ] ) ;	
		RamWrite32A( 0x0350, UlPmemCodeF40[ UlCnt * 5 + 2 ] ) ;	
		RamWrite32A( 0x0354, UlPmemCodeF40[ UlCnt * 5 + 3 ] ) ;	
		RamWrite32A( 0x0358, UlPmemCodeF40[ UlCnt * 5 + 4 ] ) ;	
		RamWrite32A( 0x033c, 0x00000001 ) ;						
	}

	
	for( UlCnt = 0 ; UlCnt < 9 ; UlCnt++ ) {
		CntWrt( ( char * )&UpData_CommandFromTable[ UlCnt * 6 ], 6 ) ;
	}

	UcRemainSecNum	= UcSectorNum ;
	for( UlCnt = 0 ; UlCnt < UcSectorNum ; UlCnt++ )
	{
		if( !( UlCnt % 8 ) ) {
			UcBlockErased	= 0 ;
		}

		if( !UcBlockErased ) {
			if( UcRemainSecNum / 8 ) {
				UcStatus	= FlashBlockErase( ( UlAddress & 0xFFFFFFC0 ) + ( UlCnt * ( SECTOR_SIZE / 5 ) ) ) ;
				if( UcStatus != 0 ) {
					return( UcStatus ) ;
				}
				UcRemainSecNum	-= 8 ;
				UcBlockErased	= 1 ;
			} else if( UcRemainSecNum < 8 ) {
				UcStatus	= FlashSectorErase( ( UlAddress & 0xFFFFFFC0 ) + ( UlCnt * ( SECTOR_SIZE / 5 ) ) ) ;
				if( UcStatus != 0 ) {
					return( UcStatus ) ;
				}
				UcRemainSecNum-- ;
			}
		}

		UcStatus	= UnlockCodeSet() ;											
		if( UcStatus != 0 ) {
			return( UcStatus ) ;
		}

		UcStatus	= FlashBurstWriteF40( &PucData[ UlCnt * SECTOR_SIZE ], SECTOR_SIZE, ( UINT32 )( UlAddress + ( UlCnt * ( SECTOR_SIZE / 5 ) ) ) ) ;
		if( UcStatus ) {
			return( UcStatus ) ;
		}

		UcStatus = UnlockCodeClear() ;											
	}

	IOWrite32A( SYSDSP_REMAP, 0x00001000 ) ;									
	WitTim( 10 ) ;
	return( 0 );
}



UINT8	FlashProtectStatus( void )
{
	UINT32	UlReadVal;

	IORead32A( FLASHROM_F40_WPB, &UlReadVal ) ;					

	UlReadVal &= 0x00000007;									

	if( !UlReadVal ){
		return ( 0 );											
	}else{
		return ( 1 );											
	}
}


UINT8 EraseCalDataF40( void )
{
	UINT32	UlReadVal, UlCnt;
	UINT8 ans = 0;

	
	ans = UnlockCodeSet();
	if ( ans != 0 ) return (ans);								

	
	IOWrite32A( FLASHROM_F40_ADR, 0x00010040 ) ;
	
	IOWrite32A( FLASHROM_F40_CMD, 4	 ) ;

	WitTim( 5 ) ;
	UlCnt=0;
	do{
		if( UlCnt++ > 10 ){	ans = 2;	break;	} ;
		IORead32A( FLASHROM_F40_INT, &UlReadVal );
	}while ( (UlReadVal & 0x00000080) != 0 );

	ans = UnlockCodeClear();									

	return(ans);
}


void ReadCalDataF40( UINT32 * BufDat, UINT32 * ChkSum )
{
	UINT16	UsSize = 0, UsNum;

	*ChkSum = 0;

	do{
		
		IOWrite32A( FLASHROM_F40_ACSCNT, (FLASH_ACCESS_SIZE-1) ) ;

		
		IOWrite32A( FLASHROM_F40_ADR, 0x00010040 + UsSize ) ;		
		
		IOWrite32A( FLASHROM_F40_CMD, 1 ) ;  						

		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_F40_RDATL ) ;		

		for( UsNum = 0; UsNum < FLASH_ACCESS_SIZE; UsNum++ )
		{
			RamRead32A(  CMD_IO_DAT_ACCESS , &(BufDat[ UsSize ]) ) ;
			*ChkSum += BufDat[ UsSize++ ];
		}
	}while (UsSize < 64);	
}

UINT8 WriteCalDataF40( UINT32 * BufDat, UINT32 * ChkSum )
{
	UINT16	UsSize = 0, UsNum;
	UINT8 ans = 0;
	UINT32	UlReadVal = 0;

	*ChkSum = 0;

	
	ans = UnlockCodeSet();
	if ( ans != 0 ) return (ans);									

	IOWrite32A( FLASHROM_F40_WDATH, 0x000000FF ) ;

	do{
		
		IOWrite32A( FLASHROM_F40_ACSCNT, (FLASH_ACCESS_SIZE - 1) ) ;
		
		IOWrite32A( FLASHROM_F40_ADR, 0x00010040 + UsSize ) ;
		
		IOWrite32A( FLASHROM_F40_CMD, 2) ;  						


		for( UsNum = 0; UsNum < FLASH_ACCESS_SIZE; UsNum++ )
		{
			IOWrite32A( FLASHROM_F40_WDATL,  BufDat[ UsSize ] ) ;
			do {
				IORead32A( FLASHROM_F40_INT, &UlReadVal );
			}while ( (UlReadVal & 0x00000020) != 0 );

			*ChkSum += BufDat[ UsSize++ ];							
		}
	}while (UsSize < 64);											

	ans = UnlockCodeClear();										

	return( ans );
}

void CalcChecksum( const UINT8 *pData, UINT32 len, UINT32 *pSumH, UINT32 *pSumL )
{
	UINT64 sum = 0;
	UINT32 dat;
	UINT16 i;

	for( i = 0; i < len / 5; i++ ) {
		sum  += (UINT64)*pData++ << 32;

		dat  = *pData++ << 24;
		dat += *pData++ << 16;
		dat += *pData++ << 8;
		dat += *pData++ ;
		sum += dat ;
	}

	*pSumH = (UINT32)(sum >> 32);
	*pSumL = (UINT32)(sum & 0xFFFFFFFF);
}

void CalcBlockChksum( UINT8 num, UINT32 *pSumH, UINT32 *pSumL )
{
	UINT8 SectorData[SECTOR_SIZE];
	UINT32 top;
	UINT16 sec;
	UINT64 sum = 0;
	UINT32 datH, datL;

	top = num * BLOCK_UNIT;		

	
	for( sec = 0; sec < (BLOCK_BYTE / SECTOR_SIZE); sec++ ) {
		FlashSectorRead( top + sec * 64, SectorData );

		CalcChecksum( SectorData, SECTOR_SIZE, &datH, &datL );
		sum += ((UINT64)datH << 32) + datL ;
	}

	*pSumH = (UINT32)(sum >> 32);
	*pSumL = (UINT32)(sum & 0xFFFFFFFF);
}

