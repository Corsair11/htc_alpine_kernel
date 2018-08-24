#define OTP_DRV_LSC_SIZE 186

struct otp_struct {
int flag;
int module_integrator_id;
int lens_id;
int production_year;
int production_month;
int production_day;
int rg_ratio; 
int bg_ratio;
unsigned char lenc[OTP_DRV_LSC_SIZE];
int checksumLSC;
int checksumOTP;
int checksumTotal;
int VCM_start;
int VCM_end;
int VCM_dir;
};

#define RG_Ratio_Typical 0x125
#define BG_Ratio_Typical 0x131

int read_otp(struct otp_struct *otp_ptr);
int apply_otp(struct otp_struct *otp_ptr);
int Decode_13855R2A(unsigned char*pInBuf, unsigned char* pOutBuf);
void otp_cali(unsigned char writeid);
void LumaDecoder(uint8_t *pData, uint8_t *pPara);
void ColorDecoder(uint8_t *pData, uint8_t *pPara);







